/**
 *       @file  control.h
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

#ifndef DRK_CONTROL_H_
#define DRK_CONTROL_H_

#include "console.h"

#include "cfg/cfg_control.h"

#include <io/kfile.h>

// The time interval [s] for received SMS handling
#define SMS_CHECK_SEC	30

// The time interval [h] for GSM restart
#define GSM_RESTART_HOURES	24

// The time interval [s] for console handling
#define CMD_CHECK_SEC	 1

// The time interval [s] for console handling
#define CH_SUSPEND_SEC	 60

// The time interval [s] for button handling
#define BTN_CHECK_SEC	 3

// The time interval [s] for reset button handling
#define BTN_RESET_SEC	 5

// Line cycle period [ms]
// Power line @50Hz => 1 cycles = 20ms
#define ADE_LINE_CYCLES_PERIOD 20

// The number of line cycles to wait before getting a sample
#define ADE_LINE_CYCLES_SAMPLE_COUNT 16

// The Irms offset
#define ADE_IRMS_OFFSET 0

// The Power constant [1W]
// - (max) power sample
// - multimeter readed Power
#define ADE_PWR_RATIO (19700)

// The I LOAD FAULT level
#define ADE_IRMS_LOAD_FAULT 3000l

// The P LOAD FAULT level
#define ADE_PRMS_LOAD_FAULT 160000.0

// The Load FAULT sensitivity factor (power of 2)
#define ADE_LOAD_CALIBRATION_FACTOR 1


// The maximum number of channels
#define MAX_CHANNELS 16

/** The CHs load value */
typedef uint32_t chLoad_t;

typedef struct chData {
	uint32_t Irms;
	uint32_t Vrms;
	chLoad_t Prms;
	uint32_t Imax;
	uint32_t Vmax;
	chLoad_t Pmax;
	uint8_t calSamples;
	uint8_t fltChecks;
	uint8_t fltSamples;
} chData_t;

extern chData_t chData[MAX_CHANNELS];




void controlCalibration(void);
void controlSetup(void);
void controlLoop(void);

extern uint8_t controlFlags;
#define CF_MONITORING 		0x01
#define CF_SPOILED 			0x02

inline void controlEnableMonitoring(void);
inline void controlEnableMonitoring(void) {
	controlFlags |=  CF_MONITORING;
}
inline void controlDisableMonitoring(void);
inline void controlDisableMonitoring(void) {
	controlFlags &= ~CF_MONITORING;
}
inline uint8_t controlMonitoringEnabled(void);
inline uint8_t controlMonitoringEnabled(void) {
	return (controlFlags & CF_MONITORING);
}

extern uint16_t chSpoiled;
inline uint16_t controlGetSpoiledMask(void);
inline uint16_t controlGetSpoiledMask(void) {
	return chSpoiled;
}

inline uint8_t controlCriticalSpoiled(void);
inline uint8_t controlCriticalSpoiled(void) {
	return (controlFlags & CF_SPOILED);
}

void controlSetEnabled(uint16_t mask);

extern uint16_t chEnabled;
inline uint16_t controlEnabled(void);
inline uint16_t controlEnabled(void) {
	return chEnabled;
}

extern uint16_t chCalib;
inline uint8_t controlIsCalibrating(void);
inline uint8_t controlIsCalibrating(void) {
	if (chCalib & chEnabled)
		return 1;
	return 0;
}


extern uint16_t chCritical;
inline void controlSetCritical(uint16_t mask);
inline void controlSetCritical(uint16_t mask) {
	chCritical = mask;
}
inline uint16_t controlCritical(void);
inline uint16_t controlCritical(void) {
	return chCritical;
}
inline uint8_t isCritical(uint8_t ch);
inline uint8_t isCritical(uint8_t ch) {
	if (chCritical & BV16(ch))
		return 1;
	return 0;
}


void controlNotifySpoiled(void);

void smsSplitAndParse(char const *from, char *sms);

int8_t controlNotifyBySMS(const char *dest, const char *buff);

#if CONFIG_CONTROL_TESTING 
void NORETURN chsTesting(void);
#else
#define chsTesting()
#endif

#endif // DRK_CONTROL_H_

