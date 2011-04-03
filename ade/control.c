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
#include "gsm.h"

#include <cfg/compiler.h>
#include <cfg/macros.h>

#include <drv/timer.h>
#include <drv/meter_ade7753.h>
#include <mware/event.h>

#include <struct/list.h>

#include <stdio.h> // sprintf

/* Define logging settings (for cfg/log.h module). */
#define LOG_LEVEL   LOG_LVL_INFO
#define LOG_FORMAT  LOG_FMT_TERSE
#include <cfg/log.h>

#define DB(x) x
//#define DB(x)
#define SMS_(x) x
//#define SMS_(x)

// The time interval [s] for received SMS handling
#define SMS_CHECK_SEC	30

// The time interval [s] for console handling
#define CMD_CHECK_SEC	 1

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

//=====[ SMS handling ]=========================================================
// The timer to schedule SMS handling task
Timer sms_tmr;
// The event to handle SMS checking
Event sms_evt;
// The SMS message
gsmSMSMessage_t msg;
// The task to process SMS events
static void sms_task(iptr_t timer) {
	//Silence "args not used" warning.
	(void)timer;
	int8_t smsIndex = 0;

	kprintf("Check SMS\n");

	// Retrive the first SMS into memory
	SMS_(smsIndex = gsmSMSByIndex(&msg, 1));
	if (smsIndex==1) {
		command_parse(&dbg_port.fd, msg.text);
		timer_delay(500);
		SMS_(gsmSMSDel(1));
	}

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


//=====[ Channels Data ]========================================================

#define CALIBRATION_SAMPLES 32

#define chEnabled(CH) (chMask & BV16(CH))
#define chUncalibrated(CH) (chCalib & BV16(CH))
#define chMarkUncalibrated(CH) (chCalib |= BV16(CH))
#define chMarkCalibrated(CH) (chCalib &= ~BV16(CH))
#define chGetMoreSamples(CH) (chData[CH].calSamples)
#define chMarkSample(CH) (chData[CH].calSamples--)
#define chSetSample(CH, PWR) (chData[CH].Icur = PWR)

/** @brief The RFN running modes */
typedef enum running_modes {
	FAULT = 0,
	CALIBRATION,
	MONITORING,
} running_modes_t;

typedef struct chData {
	uint32_t Imax;
	uint32_t Icur;
	uint8_t calSamples;
} chData_t;

/** The mask of channels to be monitored */
//static uint16_t chMask = 0xFFFF;
static uint16_t chMask = 0xFFFF;
#define ee_getChMask(V) ((uint16_t)0x0001)
#warning MASKING ee_getChMask to return only ONE CHANNEL

/** The currently selected channel */
static uint8_t curCh = 15;

/** @brief The current running mode */
static running_modes_t rmode = CALIBRATION;

/** @brief The mask of channels in calibration mode */
static uint16_t chCalib = 0xFFFF;

/** The vector of channels data */
static chData_t chData[16];

/** The minimum load loss to notify a FAULT */
const uint32_t loadFault = 6000l;

/** The buffer defined by the GSM module */
extern char cmdBuff[161];

//=====[ Channel Selection ]====================================================

/** @brief Get the bitmaks of powered-on channels */ 
static inline uint16_t getActiveChannels(void) {
	uint16_t acm = 0xFFFF; // Active Channels Maks
	// TODO put here the code to get powered on channels
	// NOTE: this code is on the critical path, maybe we should exploit the
	// I2C interrupt to handle updates just once required...
	return acm;
}

static inline void switchAnalogMux(uint8_t ch) {
	// TODO select the required channel
	//Silence "args not used" warning.
	(void)ch;
}

static inline void readMeter(uint8_t ch) {
	static uint8_t prevCh = 0;
	uint32_t pwr;

	if (ch != prevCh) {
		// TODO Reset the meter
		// TODO wait for a valid measure
		timer_delay(250);
	}

	prevCh = ch;

	// TODO get Power value
	pwr = meter_ade7753_Irms();
	chSetSample(ch, pwr);

	DB(kprintf("CH[%hd]: %08ld\r\n", ch, chData[ch].Icur));

}

/** @brief Get a Power measure and return the measured channel */
static uint8_t sampleChannel(void) {
	uint16_t activeChs;

	// Get powered on (and enabled) channels
	activeChs = (getActiveChannels() & chMask);

	// Select next active channel (max one single scan)
	for (uint8_t i = 0; i<16; i++) {
		curCh++;
		if (curCh>15)
			curCh=0;
		if (BV16(curCh) & activeChs)
			break;
	};

	// Switch the analog MUX
	switchAnalogMux(curCh);

	// Read power from ADE7753 meter
	readMeter(curCh);

	return curCh;
}

//=====[ Channel Calibration ]==================================================

/** @brief Verify if the specified channel requries to be calibrated */
static inline uint8_t needCalibration(uint8_t ch) {

	// Avoid loading of (disabled) channels
	if (!chEnabled(ch))
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

/** @brief Setup the (initial) calibration data for the specified channel */
static void loadCalibrationData(uint8_t ch) {
	// TODO we could save calibration data to EEPROM and recover them at each
	// reboot... but right now we start with a dummy zero value.

	// Avoid loading of (disabled) channels
	if (!chEnabled(ch))
		return;

	LOG_INFO("Loading calibration data CH[%hd]\r\n", ch);

	chData[ch].Imax = 0;
	chData[ch].Icur = 0;
	chData[ch].calSamples = CALIBRATION_SAMPLES;
}

static inline void chRecalibrate(uint8_t ch) {
	// Avoid loading of (disabled) channels
	if (!chEnabled(ch))
		return;
	// Set required calibration points
	chData[ch].Imax = 0;
	chData[ch].Icur = 0;
	chData[ch].calSamples = CALIBRATION_SAMPLES;
	// Mark channel for calibration
	chMarkUncalibrated(ch);
}

void controlCalibration(void) {

	LOG_WARN("Forcing calibration\r\n");
	for (uint8_t ch=0; ch<16; ch++) {
		chRecalibrate(ch);
	}
}

/** @brief Defines the calibration policy for each channel */
static void calibrate(uint8_t ch) {

	if (!chGetMoreSamples(ch) &&
			chUncalibrated(ch)) {
		// Mark channel as calibrated
		kprintf("CH[%hd]: calibration DONE (%08ld)\r\n",
				ch, chData[ch].Imax);
		chMarkCalibrated(ch);
		return;
	}

	chMarkSample(ch);

	if (chData[ch].Imax > chData[ch].Icur) {
		// TODO better verify to avoid being masked by a load peak
		return;
	}

	//----- Load increased... update current Imax -----
	kprintf("CH[%hd]: calibrating...\r\n", ch);
	chData[ch].calSamples = CALIBRATION_SAMPLES;

	// Avoid recording load peak
	if ((chData[ch].Icur-chData[ch].Imax) > loadFault) {
		chData[ch].Imax += (loadFault>>2);
		return;
	}

	chData[ch].Imax = chData[ch].Icur;
}

//=====[ Channel Monitoring ]===================================================

static inline uint8_t chLoadLoss(uint8_t ch) {
	uint32_t loadLoss;

	// TODO we should consider increasing values, maybe to adapt the
	// calibration
	if (chData[ch].Icur >= chData[ch].Imax)
		return 0;

	loadLoss = chData[ch].Imax-chData[ch].Icur;
	if ( loadLoss > loadFault)
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
	sprintf(msg+len, " - Perdita carico CH[%hd]\r\n", ch);
	LOG_INFO("SMS: %s", msg);

	for (idx=0; idx<MAX_SMS_DEST; idx++) {
		ee_getSmsDest(idx, dst, MAX_SMS_NUM);

		// Jump disabled destination numbers
		if (dst[0] != '+')
			continue;

		LOG_INFO("Notifing [%s]...\r\n", dst);
		SMS_(gsmSMSSend(dst, msg));
	}

	// Wait for SMS being delivered
	timer_delay(10000);
}

/** @brief Defines the monitoring policy for each channel */
static void monitor(uint8_t ch) {

	if (!chLoadLoss(ch))
		return;

	// Fault detected
	rmode = FAULT;
	kprintf("WARN: Load loss on CH[%hd] (%08ld => %08ld)\r\n",
		ch, chData[ch].Imax, chData[ch].Icur);

	// Mark channel for recalibration
	chRecalibrate(ch);

	// Send SMS notification
	notifyLoss(ch);

}

//=====[ Control Loop ]=========================================================
void controlSetup(void) {

	// Init list of synchronous timers
	LIST_INIT(&timers_lst);

	// Schedule SMS handling task
	SMS_(gsmSMSDelRead());
	timer_setDelay(&sms_tmr, ms_to_ticks(SMS_CHECK_SEC*1000));
	timer_setSoftint(&sms_tmr, sms_task, (iptr_t)&sms_tmr);
	synctimer_add(&sms_tmr, &timers_lst);

	// Schedule Console handling task
	timer_setDelay(&cmd_tmr, ms_to_ticks(CMD_CHECK_SEC*1000));
	timer_setSoftint(&cmd_tmr, cmd_task, (iptr_t)&cmd_tmr);
	synctimer_add(&cmd_tmr, &timers_lst);

	// Setup console RX timeout
	console_init(&dbg_port.fd);
	ser_settimeouts(&dbg_port, 0, 1000);

	// Dump EEPROM configuration
	ee_dumpConf();

	// Get bitmask of enabled channels
	chMask = ee_getChMask();

	// Setup channels calibration data
	for (uint8_t ch=0; ch<16; ch++)
		loadCalibrationData(ch);

}

void controlLoop(void) {
	uint8_t ch; // The currently selected channel

	DB(timer_delay(100));

	// Select Channel to sample and get P measure
	ch = sampleChannel();

	if (needCalibration(ch)) {
		calibrate(ch);
	} else {
		monitor(ch);
	}

	// Schedule timer activities (SMS and Console checking)
	synctimer_poll(&timers_lst);

}

