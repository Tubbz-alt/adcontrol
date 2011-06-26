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
	uint8_t csq;

	gsmUpdateCSQ();
	csq = gsmCSQ();
	DB(LOG_INFO("GSM CSQ [%hu]\r\n", csq));
	if (csq==99 || csq<3) {
		csq = 0;
	} else {
		if (csq>2) csq = 1;
		if (csq>8) csq = 2;
		if (csq>16) csq = 3;
	}
	LED_GSM_CSQ(csq);

	// TODO if not network attached: force network scanning and attaching
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

	LOG_INFO("Notifing [%s]...\r\n", msg.from);
	GSM(gsmSMSSend(from, cmdBuff));

	// Wait for SMS being delivered
	timer_delay(10000);

}

// The task to process SMS events
static void sms_task(iptr_t timer) {
	//Silence "args not used" warning.
	(void)timer;
	int8_t smsIndex = 0;

	DB(LOG_INFO("\r\nChecking SMS...\r\n"));

	// Update signal level
	GSM(updateCSQ());

	// Retrive the first SMS into memory
	GSM(smsIndex = gsmSMSByIndex(&msg, 1));
	if (smsIndex==1) {
		command_parse(&dbg_port.fd, msg.text);
		timer_delay(500);
		GSM(gsmSMSDel(1));
	}

	// Process SMS commands
	smsSplitAndParse(msg.from, msg.text);

	// Reschedule this timer
	synctimer_add(&sms_tmr, &timers_lst);
}

//=====[ Console handling ]=====================================================

// The timer to schedule Console handling task
Timer cmd_tmr;
// The evento to handle Console events
Event cmd_evt;

// The task to process Console events
static void cmd_task(iptr_t timer) {
//static void cmd_task(struct Event *evt) {
	//Silence "args not used" warning.
	(void)timer;

	//kprintf("Parse CMD\n");
	//console_run((KFile*)(evt->Ev.Int.user_data));
	console_run(&dbg_port.fd);

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
		timer_delay(100);
	}

	// Button pressed BTN_CHECK_SEC < t < BTN_CHECK_SEC+BTN_RESET_SEC
	LED_NOTIFY_OFF();
	ERR_OFF();
	controlFlags &= ~CF_SPOILED;
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
#define chRstSample(CH)       (chData[CH].calSamples = CONFIG_CALIBRATION_SAMPLES);
#define chSetIrms(CH, IRMS)   (chData[CH].Irms = IRMS)
#define chSetImax(CH, IMAX)   (chData[CH].Imax = IMAX)
#define chSetVrms(CH, VRMS)   (chData[CH].Vrms = VRMS)
#define chGetIrms(CH)          chData[CH].Irms
#define chGetVrms(CH)          chData[CH].Vrms
#define chGetImax(CH)          chData[CH].Imax
#define chSetPwr(CH, PWR)     (chData[CH].Pwr = PWR)
#define chGetPwr(CH)           chData[CH].Pwr
#define chSetAE(CH, AE)       (chData[CH].ae = AE)
#define chIncFaults(CH)        chData[CH].fltSamples++
#define chGetFaults(CH)        chData[CH].fltSamples
#define chRstFaults(CH)        chData[CH].fltSamples=0
#define chFaulted(CH)         (chData[CH].fltSamples)
#define chMarkFault(CH)       (chFaulty |= BV16(CH))
#define chMarkGood(CH)        (chFaulty &= ~BV16(CH))

/** @brief The RFN running modes */
typedef enum running_modes {
	FAULT = 0,
	CALIBRATION,
	MONITORING,
} running_modes_t;

typedef struct chData {
	uint32_t Irms;
	uint32_t Vrms;
	uint32_t Imax;
	double Pwr;
	int32_t ae;
	uint8_t calSamples;
	uint8_t fltSamples;
} chData_t;

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

/** @brief The current running mode */
static running_modes_t rmode = CALIBRATION;

/** @brief The mask of channels in calibration mode */
uint16_t chCalib = 0xFFFF;

/** The vector of channels data */
static chData_t chData[MAX_CHANNELS];

/** The minimum load loss to notify a FAULT */
const uint32_t loadFault = ADE_IRMS_LOAD_FAULT;

/** The minimum load variation for calibration  */
const uint32_t minLoadVariation = (
		ADE_IRMS_LOAD_FAULT>>ADE_LOAD_CALIBRATION_FACTOR);

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

#if 0
static inline void resetMeter(void) {
	LOG_INFO("Reset ADE7753\r\n");
	// Reset the meter
	meter_ade7753_reset();
	timer_delay(20);
	// Set LCAE
	meter_ade7753_setLCEA(500);

}

static inline void readMeter(uint8_t ch) {
	static uint8_t prevCh = 17;
	int32_t ae;

	if (ch != prevCh) {
		resetMeter();
		prevCh = ch;
	}

	// Get Energy accumulation value
	ae = meter_ade7753_getEnergyLCAE();
	chSetAE(ch, ae);

	DB(kprintf("CH[%hd]: %08ld\r\n", ch, chData[ch].ae));

}
#else
static inline void resetMeter(void) {

	// Reset the meter
	meter_ade7753_reset();

	// Wait for a line cycle start
	signal_wait(SIGNAL_ADE_ZX);

	// Wait for a valid measure
	timer_delay(ADE_LINE_CYCLES_PERIOD
			*ADE_LINE_CYCLES_SAMPLE_COUNT);
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

#if CONFIG_CONTROL_TESTING
	kprintf("%02hd:%08ld:%08ld\r\n", ch+1, chGetIrms(ch), chGetVrms(ch));
	return;
#endif

	// Power convertion
	chSetPwr(ch, (((double)chGetIrms(ch)*chGetVrms(ch))/100000));


#if CONFIG_CONTROL_DEBUG
	DB(LOG_INFO("CH[%hd] %c%c: Irms %08ld, Vrms %08ld => Pwr %4.0fW (%08.3f)\r\n",
				ch+1, chUncalibrated(ch) ? 'C' : 'M',
				chFaulted(ch) ? 'F' : 'S',
				chGetIrms(ch), chGetVrms(ch),
				chGetPwr(ch)/ADE_PWR_RATIO,
				chGetPwr(ch)));
#else
	DB(LOG_INFO("CH[%hd] %c: %4.0f [W]\r\n",
				ch+1, chUncalibrated(ch) ? 'C' : 'M',
				chGetPwr(ch)/ADE_PWR_RATIO));
#endif

}
#endif

static uint8_t curCh = MAX_CHANNELS-1;
/** @brief Get a Power measure and return the measured channel */
static uint8_t sampleChannel(void) {
	uint16_t activeChs;

	// Get powered on (and enabled) channels
	activeChs = (getActiveChannels() & chEnabled);
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
		LOG_INFO("Uncalibrated CHs [0x%02X]\r\n", chCalib);
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
	chSetIrms(ch, 0);
	chMarkGood(ch);
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

#if 0
# warning USING INCREASING ONLY CALIBRATION
/** @brief Defines the calibration policy for each channel */
static void calibrate(uint8_t ch) {

	if (!chGetMoreSamples(ch) &&
			chUncalibrated(ch)) {
		// Mark channel as calibrated
		kprintf("CH[%hd]: calibration DONE %4.0f [W] (%08ld)\r\n",
				ch+1, chData[ch].Pwr/8500, chData[ch].Imax);
		chMarkCalibrated(ch);
		return;
	}
	
	kprintf("CH[%hd]: (max,cur)=(%08ld,%08ld)...\r\n",
			ch+1, chGetImax(ch), chGetIrms(ch));

	// Decrease calibration samples required
	chMrkSample(ch);

	if (chGetImax(ch) >= chGetIrms(ch)) {
		// TODO better verify to avoid being masked by a load peak
		return;
	}

	//----- Load increased... update current Imax -----
	kprintf("CH[%hd]: calibrating...\r\n", ch+1);
	chData[ch].calSamples = CONFIG_CALIBRATION_SAMPLES;

	// Avoid recording load peak
	if ((chGetIrms(ch)-chGetImax(ch)) > loadFault) {
		chData[ch].Imax += (loadFault>>2);
		return;
	}

	chData[ch].Imax = chGetIrms(ch);
}
#else
# warning USING BI-SEARCH CALIBRATION
/** @brief Defines the calibration policy for each channel */
static void calibrate(uint8_t ch) {
	uint32_t var;

	if (!chGetMoreSamples(ch) &&
			chUncalibrated(ch)) {
		// Mark channel as calibrated
		kprintf("CH[%hd]: calibration DONE %4.0f [W] (%08ld)\r\n",
				ch+1, chData[ch].Pwr/8500, chData[ch].Imax);
		chMarkCalibrated(ch);
		return;
	}
	
	kprintf("CH[%hd]: (max,cur)=(%08ld,%08ld)...\r\n",
			ch+1, chGetImax(ch), chGetIrms(ch));

	// Decrease calibration samples required
	chMrkSample(ch);

	// Update current measure (by half of the variation)
	if (chGetImax(ch) >= chGetIrms(ch)) {
		var = chGetImax(ch)-chGetIrms(ch);
		chData[ch].Imax -= var>>1;
	} else {
		var = chGetIrms(ch)-chGetImax(ch);
		chData[ch].Imax += var>>1;
	}

	// Mark calibration if measure is too noise
	if (var > minLoadVariation) {
		kprintf("CH[%hd]: calibrating...\r\n", ch+1);
		chData[ch].calSamples = CONFIG_CALIBRATION_SAMPLES;
	}


}
#endif

//=====[ Channel Monitoring ]===================================================

#if 0
#warning USING SINGLE-CHECK LOAD LOSS
static inline uint8_t chLoadLoss(uint8_t ch) {
	uint32_t loadLoss;

	if (chData[ch].Irms >= chData[ch].Imax)
		return 0;

	loadLoss = chData[ch].Imax-chData[ch].Irms;
	if ( loadLoss > loadFault)
		return 1;

	return 0;
}
#else
#warning USING MULTI-CHECK LOAD LOSS
static inline uint8_t chLoadLoss(uint8_t ch) {
	uint32_t loadLoss;

	// TODO we should consider increasing values, maybe to adapt the
	// calibration to drift values, or new loads
	if (chGetIrms(ch) >= chGetImax(ch)) {
		chMarkGood(ch);
		chRstFaults(ch);
		return 0;
	}

	// Computing LOAD loss
	loadLoss = chGetImax(ch)-chGetIrms(ch);
	if (loadLoss < loadFault) {
		chMarkGood(ch);
		chRstFaults(ch);
		return 0;
	}

	// Faults detection
	chMarkFault(ch);
	chIncFaults(ch);
	if (chGetFaults(ch)>CONFIG_FAULT_SAMPLES) {
		return 1;
	}
	
	return 0;
}
#endif

static inline uint8_t isCritical(uint8_t ch) {
	if (chCritical & BV16(ch))
		return 1;
	return 0;
}

static void notifyLoss(uint8_t ch) {
	char dst[MAX_SMS_NUM];
	char *msg = cmdBuff;
	uint8_t idx;
	uint8_t len;

	// Format SMS message
	len = ee_getSmsText(msg, MAX_MSG_TEXT);

	len += sprintf(msg+len, "\r\nAnomalia: CH%s(%hd)\r\nSemaforo: ",
			isCritical(ch) ? " CRITICO" : "",
			ch+1);
	if (controlCriticalSpoiled()) {
		len += sprintf(cmdBuff+len, "in LAMPEGGIO");
	} else if (controlGetSpoiledMask()) {
		len += sprintf(cmdBuff+len, "GUASTO");
	} else {
		// This should never happens
		len += sprintf(cmdBuff+len, "RFN FAULT?");
	}

	LOG_INFO("\r\nSMS:\r\n%s\r\n\n", msg);

	for (idx=0; idx<MAX_SMS_DEST; idx++) {
		ee_getSmsDest(idx, dst, MAX_SMS_NUM);

		// Jump disabled destination numbers
		if (dst[0] != '+')
			continue;

		LOG_INFO("Notifing [%s]...\r\n", dst);
		GSM(gsmSMSSend(dst, msg));
	}
		
	LOG_INFO("\n\n", dst);

	// Wait for SMS being delivered
	timer_delay(10000);
}

void controlNotifySpoiled(void) {
	// Light-up Fault LED (and switch the Relè)
	ERR_ON();
	controlFlags |= CF_SPOILED;
}

inline void controlSetSpoiled(uint8_t ch);
inline void controlSetSpoiled(uint8_t ch) {
	chSpoiled |= BV16(ch);
}

/** @brief Defines the monitoring policy for each channel */
static void monitor(uint8_t ch) {

	if (!chLoadLoss(ch))
		return;

	// Mark the channel as spoiled
	controlSetSpoiled(ch);

	// Notify if a CRITICAL channel is spoiled
	LOG_INFO("Crit: 0x%04X, ch: %d\r\n", chCritical, ch);
	if (chCritical & BV16(ch)) { //isCritical(ch)) {
		controlNotifySpoiled();
	}

	// Fault detected
	rmode = FAULT;
	kprintf("WARN: Load loss on CH[%hd] (%08ld => %08ld)\r\n",
		ch+1, chData[ch].Imax, chData[ch].Irms);

	// Mark channel for recalibration
	chRecalibrate(ch);

	// Send SMS notification
	notifyLoss(ch);

}

static void notifyFault(void) {
	char dst[MAX_SMS_NUM];
	char *msg = cmdBuff;
	uint8_t idx;
	uint8_t len;

	// Format SMS message
	len = ee_getSmsText(msg, MAX_MSG_TEXT);
	sprintf(msg+len, "\r\nAnomalia centralina RCT\r\n");
	LOG_INFO("SMS: %s", msg);

	for (idx=0; idx<MAX_SMS_DEST; idx++) {
		ee_getSmsDest(idx, dst, MAX_SMS_NUM);

		// Jump disabled destination numbers
		if (dst[0] != '+')
			continue;

		LOG_INFO("Notifing [%s]...\r\n", dst);
		GSM(gsmSMSSend(dst, msg));
	}

	LOG_INFO("\n\n");

	// Wait for SMS being delivered
	timer_delay(10000);
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
			!signal_status(SIGNAL_UNIT_IRQ)) {
		// Notify only on transitionn to LOW value
		notifyFault();
	}
	// Checking for BUTTON
	if (signal_pending(SIGNAL_PLAT_BUTTON)) {
		DB2(LOG_INFO("USR BUTTON [%d]\r\n\n", signal_status(SIGNAL_PLAT_BUTTON)));
		buttonHandler();
	}
}

static void notifyCalibrationCompleted(void) {
	char dst[MAX_SMS_NUM];
	char *msg = cmdBuff;
	uint8_t idx;
	uint8_t len;

	// Format SMS message
	len = ee_getSmsText(msg, MAX_MSG_TEXT);
	sprintf(msg+len, " - Calibrazione completata\r\n");
	LOG_INFO("SMS: %s", msg);

	for (idx=0; idx<MAX_SMS_DEST; idx++) {
		ee_getSmsDest(idx, dst, MAX_SMS_NUM);

		// Jump disabled destination numbers
		if (dst[0] != '+')
			continue;

		LOG_INFO("Notifing [%s]...\r\n", dst);
		GSM(gsmSMSSend(dst, msg));
	}

	LOG_INFO("\n\n");

	// Wait for SMS being delivered
	timer_delay(10000);
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

	// Dump EEPROM configuration
	ee_dumpConf();

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

}

static char progress[] = "/|\\-";

void controlLoop(void) {
	static uint8_t i = 0;
	uint8_t ch; // The currently selected channel

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
		timer_delay(500);
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

