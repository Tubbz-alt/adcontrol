/**
 *       @file  control.c
 *      @brief  The FRN control loop
 *
 * This module provides the definition of the main control loop for the Remote
 * Fault Notifier device.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  03/31/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * ============================================================================
 */

#include "control.h"

#include "console.h"
#include "command.h"
#include "eeprom.h"
#include "signals.h"
#include "gsm.h"

#include "hw/hw_led.h"

#include <cfg/compiler.h>
#include <cfg/macros.h>

#include <drv/timer.h>
#include <drv/meter_ade7753.h>
#include <drv/pca9555.h>
#include <mware/event.h>

#include <struct/list.h>

#include <avr/wdt.h>

#include <stdio.h> // sprintf

/* Define logging settings (for cfg/log.h module). */
#define LOG_LEVEL   CONTROL_LOG_LEVEL
#define LOG_FORMAT  CONTROL_LOG_FORMAT
#include <cfg/log.h>

//#define DB(x)
#define DB(x) x
#define DB2(x)
//#define DB2(x) x
#define GSM(x) x
//#define GSM(x)

static void sms_task(iptr_t timer);
static void cmd_task(iptr_t timer);
static uint8_t sampleChannel(void);
static uint8_t needCalibration(uint8_t ch);
static void calibrate(uint8_t ch);
static void resetCalibrationCountdown(void);

static void notifyAllBySMS(const char *msg);
static uint8_t chLoadLoss(uint8_t ch);
static void chSetSuspendCountdown(void);
static void chSetSpoiled(uint8_t ch);
static uint8_t chCheckFault(uint8_t ch);
static void monitor(uint8_t ch);

// The list of timer shcedulated tasks
List timers_lst;

// The serial port used for console commands
extern Serial dbg_port;
// The I2C bus used to access the port expander
extern I2c i2c_bus;
// The PCA9995 port expander
extern Pca9555 pe;

//=====[ SMS handling ]=========================================================
// The timer to schedule SMS handling task
Timer sms_tmr;
// The event to handle SMS checking
Event sms_evt;
// The SMS message
gsmSMSMessage_t msg;

// Update the CSQ signal level indicator
static void updateCSQ(void) {
	uint8_t csq, level;

	// Prevently switch-off all CSQ LEDs
	LED_GSM_OFF();

	// Checking for network availability
	if (!gsmRegistered()) {
		// Forcing GMS reset
		gsmPowerOn();
	}

	// Update the signal level
	gsmUpdateCSQ();
	csq = gsmCSQ();
	DB(LOG_INFO("GSM CSQ [%hu]\r\n", csq));
	if (csq == 99)
		return;

	// Set the CSQ ledbar according to the CSQ signal level
	level = 0;
	if (csq > 2)
		level = 1;
	if (csq > 16)
		level = 2;
	if (csq > 24)
		level = 3;
	LED_GSM_CSQ(level);

	// TODO if not network attached: force network scanning and attaching
}

int8_t controlNotifyBySMS(const char *dest, const char *buff) { 
	int8_t result;
	uint8_t try;
	uint8_t timeout;

	LOG_INFO("Notify by SMS\nDest: %s\nText: %s\r\n", dest, buff);

	// Checking for Network availability
	try = 0; timeout = 10;
	result = gsmRegisterNetwork();
	while (result != OK) {
		LOG_WARN("Network not available\r\n");
		if (try % timeout) {
			LOG_WARN("Trying again in 60s\r\n");
			DELAY(60000);
		} else {
			// Resetting modem once every (10*timeout) mins
			gsmPowerOn();
			timeout += 10;
			if (timeout >= 250)
				timeout = 10;
		}
		result = gsmRegisterNetwork();
		try++;
	}
	
	// Checking for signal quality
	try = 0; timeout = 20;
	result = gsmUpdateCSQ();
	while (result != OK ||
			gsmCSQ() == 99 || gsmCSQ() == 0) {
		LOG_WARN("Low network signal [%d]\r\n", gsmCSQ());
		if (try % timeout) {
			LOG_WARN("Trying again in 60s\r\n");
			DELAY(60000);
		} else {
			// Resetting modem once every (20*timeout) mins
			gsmPowerOn();
			timeout += 20;
			if (timeout >= 240)
				timeout = 20;
		}
		result = gsmRegisterNetwork();
		try++;
	}

	// Trying to send the SMS
	result = 0;
	GSM(result = gsmSMSSend(dest, buff));
	return result;
}
void smsSplitAndParse(char const *from, char *sms) {
	char *cmd = sms;
	char *cmdEnd = sms;

	// Reset response buffer
	cmdBuff[0] = '\0';

	while (*cmdEnd) {
		
		// Find command separator, or end of SMS
		for ( ; (*cmdEnd && *cmdEnd != ';'); ++cmdEnd)
			;// nop
		
		// 
		if (*cmdEnd == ';') {
			*cmdEnd = '\0';
		}

		// lowercase current command
		for (char *p = cmd; *p != ' ' && *p; ++p) {
			if (*p >= 'A' && *p <= 'Z')
				*p += 'a'-'A';
		}

		//DB2(LOG_INFO("CMD: %s\r\n", cmd));

		// Parse current command
		command_parse(&dbg_port.fd, cmd);

		// Go on with next command
		*cmdEnd = ';';
		for ( ; *cmdEnd; ++cmdEnd) {
			if ((*cmdEnd != ';') && (*cmdEnd != ' '))
				break;
		}
		cmd = cmdEnd;

	}

	// If a non empty buffer has been setup: send it as response
	if (cmdBuff[0] == '\0')
		return;

	controlNotifyBySMS(from, cmdBuff);

	// Wait for SMS being delivered
	DELAY(10000);

}

// The countdown to GSM restat
#define GSM_RESTART_COUNTDOWN (\
		(uint32_t)GSM_RESTART_HOURES * 3600 / SMS_CHECK_SEC)
static uint32_t gsmRestartCountdown = GSM_RESTART_COUNTDOWN;

// The task to process SMS events
static void sms_task(iptr_t timer) {
	//Silence "args not used" warning.
	(void)timer;
	int8_t smsIndex = 0;

	DB(LOG_INFO("\r\nChecking SMS...\r\n"));

	// Update signal level
	GSM(updateCSQ());

	// Flush SMS buffer
	GSM(gsmBufferCleanup(&msg));

	// Retrive the first SMS into memory
	GSM(smsIndex = gsmSMSByIndex(&msg, 1));
	if (smsIndex==1) {
		command_parse(&dbg_port.fd, msg.text);
		DELAY(500);
		GSM(gsmSMSDel(1));
	}

	// Process SMS commands
	smsSplitAndParse(msg.from, msg.text);

	// Restart GSM at each countdown
	if (--gsmRestartCountdown == 0) {
		LOG_INFO("\r\nRestarting GSM...");
		GSM(gsmPowerOff());
		gsmRestartCountdown = GSM_RESTART_COUNTDOWN;
	}

	// Reschedule this timer
	synctimer_add(&sms_tmr, &timers_lst);
}

//=====[ Console handling ]=====================================================

// The timer to schedule Console handling task
Timer cmd_tmr;
// The evento to handle Console events
Event cmd_evt;

// Forward declaration
extern uint16_t chSuspended;
static uint16_t chResumeCountdown = 0;

// The countdown to GSM restat
#define CLB_COUNTDOWN(WEEKS) (\
		(uint32_t)WEEKS * 604800 / CMD_CHECK_SEC)
static uint32_t recalibrationCountdown;

static void resetCalibrationCountdown(void) {
	uint8_t weeks = ee_getCalibrationWeeks();
	// Try avoid misconfigured recaliations
	if (!weeks)
		weeks = 0xFF;
	recalibrationCountdown = CLB_COUNTDOWN(weeks);
}

// The task to process Console events
static void cmd_task(iptr_t timer) {
//static void cmd_task(struct Event *evt) {
	//Silence "args not used" warning.
	(void)timer;

	//kprintf("Parse CMD\n");
	//console_run((KFile*)(evt->Ev.Int.user_data));
	console_run(&dbg_port.fd);

	// Reset suspended CHs mask
	if (!chResumeCountdown)
		chSuspended = 0x0000;
	else
		chResumeCountdown--;

	// Check for periodic re-calibration
	recalibrationCountdown--;
	if (!recalibrationCountdown) {
		resetCalibrationCountdown();
		LOG_INFO("\n\n!!!!! Ri-calibrazione periodica !!!!!\n\n");
		controlCalibration();
	}

	// Reschedule this timer
	synctimer_add(&cmd_tmr, &timers_lst);
}


//=====[ Button handling ]=====================================================

// The timer to schedule Button handling task
Timer btn_tmr;

static void reset_board(void) {
	// Shutdons all LEDs to notify reset
	LED_NOTIFY_OFF();
	LOG_INFO("Forced reset...\r\n");
	wdt_enable(WDTO_2S);
	while(1);
}

// The task to process Console events
static void btn_task(iptr_t timer) {
	ticks_t start = timer_clock();
	ticks_t elapsed;
	(void)timer;

	DB2(LOG_INFO("Button timer...\r\n"));

	// LightUp all LEDS to notify calibration/reset pending
	LED_NOTIFY_ON();

	// Wait for button release or reset timeout
	while (!signal_status(SIGNAL_PLAT_BUTTON)) {
		elapsed = timer_clock() - start;
		if ( ms_to_ticks(BTN_RESET_SEC*1000) <= elapsed ) {
			// Button pressed fot t > BTN_CHECK_SEC+BTN_RESET_SEC
			reset_board();
		}
		DELAY(100);
	}

	// Button pressed BTN_CHECK_SEC < t < BTN_CHECK_SEC+BTN_RESET_SEC
	LED_NOTIFY_OFF();
	ERR_OFF();

	// Reset spoiled channels mask
	controlFlags &= ~CF_SPOILED;
	chSpoiled = 0x0000;

	controlCalibration();

}

//=====[ Channels Data ]========================================================

#define isEnabled(CH)         (chEnabled & BV16(CH))
#define chUncalibrated(CH)    (chCalib & BV16(CH))
#define chMarkUncalibrated(CH)(chCalib |= BV16(CH))
#define chMarkCalibrated(CH)  (chCalib &= ~BV16(CH))
#define CalibrationDone()     (!(chCalib & chEnabled))
#define chGetMoreSamples(CH)  (chData[CH].calSamples)
#define chMrkSample(CH)       (chData[CH].calSamples--)
#define chRstSample(CH)        chData[CH].calSamples = ee_getFaultSamples()

#define chSetIrms(CH, IRMS)   (chData[CH].Irms = IRMS)
#define chSetImax(CH, IMAX)   (chData[CH].Imax = IMAX)
#define chSetVrms(CH, VRMS)   (chData[CH].Vrms = VRMS)

#define chSetVmax(CH, VMAX)   (chData[CH].Vmax = VMAX)
#define chSetPrms(CH, PRMS)   (chData[CH].Prms = PRMS)
#define chSetPmax(CH, PMAX)   (chData[CH].Pmax = PMAX)

#define chGetIrms(CH)          chData[CH].Irms
#define chGetVrms(CH)          chData[CH].Vrms
#define chGetPrms(CH)          chData[CH].Prms

#define chGetImax(CH)          chData[CH].Imax
#define chGetVmax(CH)          chData[CH].Vmax
#define chGetPmax(CH)          chData[CH].Pmax

#define chSetAE(CH, AE)       (chData[CH].ae = AE)
#define chIncFaults(CH)        chData[CH].fltSamples++
#define chGetFaults(CH)        chData[CH].fltSamples
#define chRstFaults(CH)        chData[CH].fltSamples = 0
#define chFaulted(CH)         (chData[CH].fltSamples)

#define chIncChecks(CH)        chData[CH].fltChecks++
#define chGetChecks(CH)        chData[CH].fltChecks
#define chRstChecks(CH)        chData[CH].fltChecks = 0
#define chChecks(CH)          (chData[CH].fltChecks)

#define chMarkFault(CH)       (chFaulty |= BV16(CH))
#define chMarkGood(CH)        (chFaulty &= ~BV16(CH))
#define chSuspend(CH)         (chSuspended |= BV16(CH))

/** The CHs load value */
typedef double chLoad_t;
/** The minimum load loss to notify a FAULT */
const double loadFault = ADE_PRMS_LOAD_FAULT/2;
/** The minimum load variation for calibration  */
const double minLoadVariation = ADE_PRMS_LOAD_FAULT/2;

/** @brief The RFN running modes */
typedef enum running_modes {
	FAULT = 0,
	CALIBRATION,
	MONITORING,
} running_modes_t;

/** The mask of channels to be monitored */
uint16_t chEnabled = 0x0000;
//#define ee_getEnabledChMask(V) ((uint16_t)0xFF00)
//#warning MASKING ee_getEnabledChMask FORCING ENABLED CHANNELS (0xFF00)

/** The mask of critical channels (which activiate AUX) */
uint16_t chCritical = 0x0000;

/** The mask of channels with fault samples */
static uint16_t chFaulty = 0x0000;

/** The mask of channels in fault state */
uint16_t chSpoiled = 0x0000;

/** The mask of suspended channels */
uint16_t chSuspended = 0x0000;

/** @brief The current running mode */
static running_modes_t rmode = CALIBRATION;

/** @brief The mask of channels in calibration mode */
uint16_t chCalib = 0xFFFF;

/** The vector of channels data */
chData_t chData[MAX_CHANNELS];

/** Control flasg */
uint8_t controlFlags = CF_MONITORING;

//=====[ Channel Selection ]====================================================

uint8_t chSelectionMap[] = {
	0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
	0x00, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09
};

/** @brief Get the bitmaks of powered-on channels */ 
static inline uint16_t getActiveChannels(void) {
	static uint16_t acm = 0x0000; // Active Channels Maks

	// TODO put here the code to get powered on channels
	// NOTE: this code is on the critical path, maybe we should exploit the
	// I2C interrupt to handle updates just once required...
	if (signal_pending(SIGNAL_PLAT_I2C)) {
		pca9555_in(&i2c_bus, &pe, &acm);
		acm = ~acm;
		DB2(LOG_INFO("Active Channels: 0x%04X\r\n", acm));
	}

	return acm;
}

static inline void switchAnalogMux(uint8_t ch) {
	// Set an invalid channel to force actual initialization at first call
	static uint8_t prevAmuxCh = 0xFF;
	uint8_t chSel;

	// Avoid unnecessary switch if channel has not changed
	if (ch == prevAmuxCh)
		return;

	prevAmuxCh = ch;
	chSel  = (0xF0 & PORTA);
	chSel |= chSelectionMap[ch];
	PORTA  = chSel;

	DB2(LOG_INFO("Switch Ch: %d => 0x%02X\r\n",
				ch, chSelectionMap[ch]));

}

static inline void resetMeter(void) {

	// Reset the meter
	meter_ade7753_reset();

	// Wait for a line cycle start
	signal_wait(SIGNAL_ADE_ZX);

	// Wait for a valid measure
	DELAY(ADE_LINE_CYCLES_PERIOD
			*ADE_LINE_CYCLES_SAMPLE_COUNT);
}

static inline void setPower(uint8_t ch) {
#if CONFIG_MONITOR_POWER
	// Compute RMS Power from V and I
	chSetPrms(ch, (((double)chGetIrms(ch)*chGetVrms(ch))/100000));
#else
#warning Monitoring RMS Current (Irms) only
	// Compute RMS Power from V and I
	// 230V => Vrms~=1M 
	chSetPrms(ch, ((double)chGetIrms(ch)*10));
#endif
}

static inline void readMeter(uint8_t ch) {
	static uint8_t prevAdeCh = 0xFF;

	if (ch != prevAdeCh) {
		prevAdeCh = ch;
		resetMeter();
	}

	// TODO get Power value
	chSetIrms(ch, meter_ade7753_Irms());
	chSetVrms(ch, meter_ade7753_Vrms());

#if ADE_IRMS_OFFSET
	// Fix Offset on Irms
	if (chGetIrms(ch)<ADE_IRMS_OFFSET)
		chSetIrms(ch) = 0;
	else
		chSetIrms(ch) = chGetIrms(ch)-ADE_IRMS_OFFSET;
#endif

	// Update the Power RMS value for this channel
	setPower(ch);

#if CONFIG_CONTROL_TESTING
	kprintf("CH: %02hd, Irms: %08ld, Vrms: %08ld, Prms: %4.0f (%08.3f)\r\n",
			ch+1, chGetIrms(ch), chGetVrms(ch),
			chGetPrms(ch)/ADE_PWR_RATIO, chGetPrms(ch));
	return;
#endif


#if CONFIG_CONTROL_DEBUG
	DB(LOG_INFO("CH[%02hd] %c%c: Irms %08ld, Vrms %08ld => Prms %4.0fW (%08.3f)\r\n",
				ch+1, chUncalibrated(ch) ? 'C' : 'M',
				chFaulted(ch) ? 'F' : 'S',
				chGetIrms(ch), chGetVrms(ch),
				chGetPrms(ch)/ADE_PWR_RATIO,
				chGetPrms(ch)));
#else
	DB(LOG_INFO("CH[%02hd] %c: %4.0f [W]\r\n",
				ch+1, chUncalibrated(ch) ? 'C' : 'M',
				chGetPrms(ch)/ADE_PWR_RATIO));
#endif

}

static uint8_t curCh = MAX_CHANNELS-1;
/** @brief Get a Power measure and return the measured channel */
static uint8_t sampleChannel(void) {
	uint16_t activeChs;

	// Get powered on (and enabled) channels
	activeChs = (getActiveChannels() & chEnabled & ~chSuspended);
	if (!activeChs)
		return MAX_CHANNELS;

	// If some _active_ channels are in fault mode: focus just on them only
	// This allows to reduce FAULTS DETECTION time perhaps also avoiding channel
	// switching
	if (chFaulty &&
			(activeChs & chFaulty)) {
		LOG_INFO("Faulty CHs [0x%02X]\r\n", chFaulty);
		activeChs &= chFaulty;
		// Remain on current channel if it is _active_ and _faulty_
		if (activeChs & BV16(curCh))
			goto sample;
		// Switch to the next _active_ abd _faulty_ channel
		goto select;
	}

	// If some _active_ channels are in calibration mode: focus just on them only
	// This allows to reduce CALIBRATION time perhaps also avoiding channel
	// switching
	if (!CalibrationDone() &&
			(activeChs & chCalib)) {
		DB2(LOG_INFO("Uncalibrated CHs [0x%02X]\r\n", chCalib));
		activeChs &= chCalib;
		// Remain on current channel if it is _active_ and _uncalibrated_
		if (activeChs & BV16(curCh))
			goto sample;
		// Switch to the next _active_ abd _uncalibrated_ channel
		goto select;
	}

select:

	// Select next active channel (max one single scan)
	// TODO optimize selection considering the board schematic
	for (uint8_t i = 0; i<MAX_CHANNELS; i++) {
		curCh++;
		if (curCh>15)
			curCh=0;
		if (BV16(curCh) & activeChs)
			break;
	}

	// Switch the analog MUX
	switchAnalogMux(curCh);

sample:

	// Read power from ADE7753 meter
	readMeter(curCh);

	return curCh;
}

//=====[ Channel Calibration ]==================================================

/** @brief Verify if the specified channel requries to be calibrated */
static inline uint8_t needCalibration(uint8_t ch) {

	// Avoid loading of (disabled) channels
	if (!isEnabled(ch))
		return 0;

	// Abort if we are not on calibration mode
	if (rmode > CALIBRATION)
		return 0;

	// Check if this channel must be calibrated
	if (chUncalibrated(ch))
		return 1;

	// By default we assume calibration as completed
	return 0;
}

static inline void chRecalibrate(uint8_t ch) {
	// Avoid loading of (disabled) channels
	if (!isEnabled(ch))
		return;
	// Set required calibration points
	chSetImax(ch, 0);
	chSetVmax(ch, 0);
	chSetPmax(ch, 0);
	chSetIrms(ch, 0);
	chSetVrms(ch, 0);
	chSetPrms(ch, 0);
	chMarkGood(ch);
	chRstChecks(ch);
	chRstFaults(ch);
	chRstSample(ch);
	chMarkUncalibrated(ch);
}

/** @brief Setup the (initial) calibration data for the specified channel */
static void loadCalibrationData(uint8_t ch) {
	// TODO we could save calibration data to EEPROM and recover them at each
	// reboot... but right now we start with a dummy zero value.

	// Avoid loading of (disabled) channels
	if (!isEnabled(ch))
		return;

	LOG_INFO("Loading calibration data CH[%02hd]\r\n", ch+1);
	chRecalibrate(ch);
}

void controlSetEnabled(uint16_t mask) {
	uint16_t chNew = (mask & (chEnabled ^ mask));
	
	LOG_INFO("New ENABLED Channels 0x%04X\r\n", chNew);

	chCalib &= mask; // Remove disabled CHs
	chCalib |= chNew; // Newly enabled channels
	chEnabled = mask;

	// Load calibration data for new channels
	for (uint8_t pos = 0; chNew && pos < 16; ++pos, chNew>>=1) {
		if (chNew & BV16(0)) {
			loadCalibrationData(pos);
		}
	}

}


void controlCalibration(void) {

	LOG_WARN("Forcing re-calibration\r\n\n");
	for (uint8_t ch=0; ch<16; ch++) {
		chRecalibrate(ch);
	}
}

/** @brief Defines the calibration policy for each channel */
static void calibrate(uint8_t ch) {
	chLoad_t var;

	if (!chGetMoreSamples(ch) &&
			chUncalibrated(ch)) {
		// Mark channel as calibrated
		DB(LOG_INFO("CH[%02hd] Calibration DONE, %c: %08.3f\r\n",
				ch+1, CONFIG_MONITOR_POWER ? 'P' : 'I',
				chGetPrms(ch)));
		chMarkCalibrated(ch);
		return;
	}

	DB2(LOG_INFO("CH[%02hd] %c(max,cur)=(%08.3f, %08.3f)...\r\n",
			ch+1, CONFIG_MONITOR_POWER ? 'P' : 'I',
			chGetPmax(ch), chGetPrms(ch)));

	// Decrease calibration samples required
	chMrkSample(ch);

	// Update load measure (by half of the variation)
	if (chGetPmax(ch) >= chGetPrms(ch)) {
		var = chGetPmax(ch)-chGetPrms(ch);
		chGetPmax(ch) -= (var/2);
	} else {
		var = chGetPrms(ch)-chGetPmax(ch);
		chGetPmax(ch) += (var/2);
	}

	// Mark calibration if measure is too noise
	if (var > (ee_getFaultLevel()/4)) {
		DB(LOG_INFO("CH[%02hd] Calibrating...\r\n", ch+1));
		chData[ch].calSamples = ee_getFaultSamples();
	}

	// Keep track of current RMS values
	chSetImax(ch, chGetIrms(ch));
	chSetPmax(ch, chGetPrms(ch));
	chSetVmax(ch, chGetVrms(ch));

}

//=====[ Channel Monitoring ]===================================================

static uint8_t chLoadLoss(uint8_t ch) {
	chLoad_t loadLoss;

	// TODO we should consider increasing values, maybe to adapt the
	// calibration to drift values, or new loads
	if (chGetPrms(ch) >= chGetPmax(ch)) {
		chMarkGood(ch);
		chRstChecks(ch);
		chRstFaults(ch);
		return 0;
	}

	// Computing LOAD loss
	loadLoss = chGetPmax(ch)-chGetPrms(ch);
	if (loadLoss < ee_getFaultLevel()) {
		chMarkGood(ch);
		chRstChecks(ch);
		chRstFaults(ch);
		return 0;
	}

	// Faults detection
	chMarkFault(ch);
	chIncFaults(ch);

	// Notify on FAULTS count overflows
	if (chGetFaults(ch) >= ee_getFaultSamples())
		return 1;
	
	return 0;
}

static void notifyAllBySMS(const char *msg) {
	char dst[MAX_SMS_NUM];

	LOG_INFO("\r\nSMS:\r\n%s\r\n\n", msg);

	for (uint8_t idx = 1; idx <= MAX_SMS_DEST; ++idx) {
		ee_getSmsDest(idx, dst, MAX_SMS_NUM);

		// Jump disabled destination numbers
		if (dst[0] != '+')
			continue;

		controlNotifyBySMS(dst, msg);
	}

	LOG_INFO("\n\n");

	// Wait for SMS being delivered
	DELAY(10000);

}

static void notifyLoss(uint8_t ch) {
	char *msg = cmdBuff;
	uint8_t len = 0;

	// Format SMS message
	len = ee_getSmsText(msg, MAX_MSG_TEXT);

	len += snprintf(msg+len,
			CMD_BUFFER_SIZE-len,
			"\r\nAnomalia: CH%s(%hd)\r\nSemaforo: ",
			isCritical(ch) ? " CRITICO" : "",
			ch+1);
	if (controlCriticalSpoiled()) {
		len += snprintf(cmdBuff+len,
				CMD_BUFFER_SIZE-len,
				"in LAMPEGGIO");
	} else if (controlGetSpoiledMask()) {
		len += snprintf(cmdBuff+len,
				CMD_BUFFER_SIZE-len,
				"GUASTO");
	} else {
		// This should never happens
		len += snprintf(cmdBuff+len,
				CMD_BUFFER_SIZE-len,
				"RFN FAULT?");
	}

#if CONFIG_REPORT_FAULT_LEVELS
#warning Reporting FAULTS LEVELS enabled
	if (len+27 < CMD_BUFFER_SIZE) {
		len += snprintf(msg+len,
				CMD_BUFFER_SIZE-len,
				"\r\n"
				"P: %8.3f => %8.3f\r\n",
				chData[ch].Pmax, chData[ch].Prms);
	}
	if (len+25 < CMD_BUFFER_SIZE) {
		len += snprintf(msg+len,
				CMD_BUFFER_SIZE-len,
				"I: %8ld => %8ld\r\n",
				chData[ch].Imax, chData[ch].Irms);
	}
	if (len+25 < CMD_BUFFER_SIZE) {
		len += snprintf(msg+len,
				CMD_BUFFER_SIZE-len,
				"V: %8ld => %8ld\r\n",
				chData[ch].Vmax, chData[ch].Vrms);
	}
#endif

	// Send message by SMS to all enabled destination
	notifyAllBySMS(msg);

}

void controlNotifySpoiled(void) {
	// Light-up Fault LED (and switch the Relè)
	ERR_ON();
	controlFlags |= CF_SPOILED;
}

static void chSetSuspendCountdown(void) {
	// Set the channels resume countdown ONLY if it is not already running
	// This grants to resume in time with respect to the first faulty channel
	// noticied.
	if (chResumeCountdown)
		return;
	chResumeCountdown = (ee_getFaultCheckTime() / CMD_CHECK_SEC);
}

static void chSetSpoiled(uint8_t ch) {
	// Suspend the channel until the next CHECK
	chSuspend(ch);
	// Schedule channel resume resume
	chSetSuspendCountdown();
	// Mark the channel as spoiled
	chSpoiled |= BV16(ch);
}

static uint8_t chCheckFault(uint8_t ch) {

	// Increas the fault CHECKS count
	chIncChecks(ch);

	// Notify on CHECKS count overflows 
	if (chGetChecks(ch) >= ee_getFaultChecks())
		return 1;

	// Mark this channel as spoiled
	chSetSpoiled(ch);

	// Reset Samples count for next check
	chRstFaults(ch);

	// Notify the channel is not yet in FAULT
	return 0;
}

/** @brief Defines the monitoring policy for each channel */
static void monitor(uint8_t ch) {

	if (!chLoadLoss(ch))
		return;

	if (!chCheckFault(ch))
		return;

	// Notify if a CRITICAL channel is spoiled
	LOG_INFO("Crit: 0x%04X, ch: %d\r\n", chCritical, ch);
	if (chCritical & BV16(ch)) { //isCritical(ch)) {
		controlNotifySpoiled();
	}

	// Fault detected
	rmode = FAULT;
	kprintf("\nWARN: Load loss on CH[%02hd] (%08.3f => %08.3f)\r\n",
		ch+1, chGetPmax(ch), chGetPrms(ch));

	// Send SMS notification
	notifyLoss(ch);

	// Mark channel for recalibration
	chRecalibrate(ch);

}

static void notifyFault(void) {
	char *msg = cmdBuff;
	uint8_t len;

	// Format SMS message
	len = ee_getSmsText(msg, MAX_MSG_TEXT);
	sprintf(msg+len, "\r\nGuasto centralina RCT\r\n");

	// Send message by SMS to all enabled destination
	notifyAllBySMS(msg);

}

static void buttonHandler(void) {

	// Button pressed
	if (!signal_status(SIGNAL_PLAT_BUTTON)) {
		// Schedule timer task
		synctimer_add(&btn_tmr, &timers_lst);
		return;
	}

	// Button released
	synctimer_abort(&btn_tmr);

}

static void checkSignals(void) {
	// Check for UNIT IRQ
	if (signal_pending(SIGNAL_UNIT_IRQ) &&
			signal_status(SIGNAL_UNIT_IRQ)) {
		// Notify only on transitionn to LOW value
		notifyFault();
	}
	// Checking for BUTTON
	if (signal_pending(SIGNAL_PLAT_BUTTON)) {
		DB2(LOG_INFO("USR BUTTON [%d]\r\n\n",
					signal_status(SIGNAL_PLAT_BUTTON)));
		buttonHandler();
	}
}

static void notifyCalibrationCompleted(void) {
	char *msg = cmdBuff;
	uint8_t len;

	// Format SMS message
	len = ee_getSmsText(msg, MAX_MSG_TEXT);
	len += sprintf(msg+len, "\nCalibrazione completata\nSemaforo ");
	if (controlCriticalSpoiled()) {
		len += sprintf(cmdBuff+len, "in LAMPEGGIO");
	} else if (controlGetSpoiledMask()) {
		len += sprintf(cmdBuff+len, "GUASTO");
	} else if (controlMonitoringEnabled()) {
		len += sprintf(cmdBuff+len, "in MONITORAGGIO");
	} else {
		len += sprintf(cmdBuff+len, "NON monitorato");
	}

	// Send message by SMS to all enabled destination
	notifyAllBySMS(msg);

}

#if CONFIG_CONTROL_TESTING 
# warning CONTROL TESTING ENABLED
void NORETURN chsTesting(void) {

	LOG_INFO(".:: CHs Testing\r\n");

	// Enabling all channels
	chEnabled = 0xFFFF;
	// Starting from CH[0]
	curCh = 0;

	while(1) {

		// Read power from ADE7753 meter
		readMeter(curCh);

		// Switch channel on Button push
		if (signal_pending(SIGNAL_PLAT_BUTTON) &&
				signal_status(SIGNAL_PLAT_BUTTON)) {
			curCh++;
			if (curCh>15)
				curCh=0;
			switchAnalogMux(curCh);
		}
	}

}
#endif

//=====[ Control Loop ]=========================================================
void controlSetup(void) {

	// Init list of synchronous timers
	LIST_INIT(&timers_lst);

	// Schedule SMS handling task
	GSM(gsmSMSDelRead());
	timer_setDelay(&sms_tmr, ms_to_ticks(SMS_CHECK_SEC*1000));
	timer_setSoftint(&sms_tmr, sms_task, (iptr_t)&sms_tmr);
	synctimer_add(&sms_tmr, &timers_lst);

	// Schedule Console handling task
	timer_setDelay(&cmd_tmr, ms_to_ticks(CMD_CHECK_SEC*1000));
	timer_setSoftint(&cmd_tmr, cmd_task, (iptr_t)&cmd_tmr);
	synctimer_add(&cmd_tmr, &timers_lst);

	// Setup Button handling task
	timer_setDelay(&btn_tmr, ms_to_ticks(BTN_CHECK_SEC*1000));
	timer_setSoftint(&btn_tmr, btn_task, (iptr_t)&btn_tmr);

	// Setup console RX timeout
	console_init(&dbg_port.fd);
	ser_settimeouts(&dbg_port, 0, 1000);

	// Dump ADE7753 configuration
	meter_ade7753_dumpConf();

	// Get bitmask of enabled channels
	chEnabled = ee_getEnabledChMask();

	// Get bitmask of enabled channels
	chCritical = ee_getCriticalChMask();

	// Enabling calibration only for enabled channels
	chCalib = chEnabled;

	// Setup channels calibration data
	for (uint8_t ch=0; ch<16; ch++)
		loadCalibrationData(ch);

	// Update signal level
	GSM(updateCSQ());

	// Setup Re-Calibration Weeks
	resetCalibrationCountdown();

	// Enabling the watchdog for the control loop
	WATCHDOG_ENABLE();

}

static char progress[] = "/|\\-";

void controlLoop(void) {
	static uint8_t i = 0;
	uint8_t ch; // The currently selected channel

	// Keep quite the dog at each iteration
	WATCHDOG_RESET();

	// Set device status led
	if (CalibrationDone())
		LED_ON();
	else
		LED_SWITCH();

	// Schedule timer activities (SMS and Console checking)
	synctimer_poll(&timers_lst);

	// Checking for pending signals to serve
	checkSignals();

	// Select Channel to sample and get P measure
	ch = sampleChannel();
	if (ch==MAX_CHANNELS) {
		// No channels enabled... avoid calibration/monitoring
		if (chCalib) {
			DB(LOG_INFO("Idle (%s, Fault: 0x%04X, Cal: 0x%04X) %c\r",
						controlMonitoringEnabled() ? "Mon" : "Dis",
						chSpoiled,
						chCalib, progress[i++%4]));
		} else {
			DB(LOG_INFO("Idle (%s, Fault: 0x%04X) %c\r",
						controlMonitoringEnabled() ? "Mon" : "Dis",
						chSpoiled,
						progress[i++%4]));
		}
		DELAY(500);
		return;
	}

	if (needCalibration(ch)) {

		calibrate(ch);
		
		// Check if all channels has been calibrated
		// So that we can notify calibration completion (just one time)
		if (!CalibrationDone())
			return;

		// Notify calibration completion
		LOG_INFO("\n\nCALIBRATION COMPLETED\r\n\n");
		LED_ON();
		notifyCalibrationCompleted();
		return;
	}

	// Monitor the current channel
	if (controlMonitoringEnabled())
		monitor(ch);

}

