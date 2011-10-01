

#include "eeprom.h"
#include "cfg/cfg_control.h"

#include <avr/eeprom.h>

#include <drv/timer.h>

/* Define logging settings (for cfg/log.h module). */
#define LOG_LEVEL   LOG_LVL_INFO
#define LOG_FORMAT  LOG_FMT_TERSE
#include <cfg/log.h>

// The on EPROM conf (for persistency)
eeprom_conf_t EEMEM eeconf = {
	.sms_dest = {
		EMPTY_NUMBER"\0\0\0\0\0\0\0\0\0\0\0\0",
		EMPTY_NUMBER"\0\0\0\0\0\0\0\0\0\0\0\0",
		EMPTY_NUMBER"\0\0\0\0\0\0\0\0\0\0\0\0",
	},
	.sms_mesg =
			"Impianto RCT non configurato\0DEADBEEFDEADBEEFDEADB"
			"EEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBE\0",
	.enabledChannelsMask = 0x0000,
	.criticalChannelsMask = 0x0000,

	.faultSamples = CONFIG_FAULT_SAMPLES,
	.faultChecks = CONFIG_FAULT_CHECKS,
	.faultCheckTime = CONFIG_FAULT_CHECK_TIME,

	.faultLevel = (uint32_t)1000 * CONFIG_FAULT_LEVEL,
	.flCalibrationDiv = 8,
	.flDetectionDiv = 2,

	.calibWeeks = CONFIG_CALIBRATION_WEEKS,

	.notifyFlags = BV8(EE_NOTIFY_CALIBRATION),
};

// The on RAM configuration (for run-time use)
runtime_conf_t rt_conf;

// Pointer to on RAM configuration (for optimized access)
runtime_conf_t *pConf = &rt_conf;

//static int8_t ee_setBytes(uint8_t *eep, const uint8_t *buf, uint8_t size) {
//	uint8_t i;
//	for (i=0; i<size; i++) {
//		eeprom_update_byte(eep, (*buf));
//		buf++; eep++;
//	}
//	return i;
//}
//
//static int8_t ee_getBytes(const uint8_t *eep, uint8_t *buf, uint8_t size) {
//	uint8_t i;
//	for (i=0; i<size; i++) {
//		(*buf) = eeprom_read_byte(eep);
//		buf++; eep++;
//	}
//	return i;
//}

static int8_t ee_setString(uint8_t *eep, const char *str, uint8_t size) {
	uint8_t i;
	for (i=0; i<size; i++) {
		eeprom_update_byte(eep, (uint8_t)(*str));
		if (*str=='\0')
			break;
		str++; eep++;
	}
	return i;
}

static int8_t ee_getString(const uint8_t *eep, char *str, uint8_t size) {
	uint8_t i;
	for (i=0; i<size; i++) {
		(*str) = eeprom_read_byte(eep);
		if (*str=='\0')
			break;
		str++; eep++;
	}
	return i;
}

int8_t ee_getSmsDest(uint8_t pos, char *num, uint8_t count) {
	
	if (pos > MAX_SMS_DEST)
		return -1;

	if (count>MAX_SMS_NUM)
		count=MAX_SMS_NUM;

	return  ee_getString((const uint8_t*)eeconf.sms_dest[pos-1],
							num, count);

}


int8_t ee_setSmsDest(uint8_t pos, const char *num) {

	if (pos > MAX_SMS_DEST)
		return -1;

	return ee_setString((uint8_t*)eeconf.sms_dest[pos-1],
							num, MAX_SMS_NUM);

}



int8_t ee_getSmsText(char *buf, uint8_t count) {

	if (count>MAX_MSG_TEXT)
		count=MAX_MSG_TEXT;

	return  ee_getString((const uint8_t*)eeconf.sms_mesg,
							buf, count);
}

int8_t ee_setSmsText(const char *buf) {

	return ee_setString((uint8_t*)eeconf.sms_mesg,
							buf, MAX_MSG_TEXT);

}

void ee_setEnabledChMask(uint16_t chMask) {
	eeprom_update_word(
			(uint16_t*)&eeconf.enabledChannelsMask,
			chMask);
}

int16_t ee_getEnabledChMask(void) {
	return eeprom_read_word(
			(uint16_t*)&eeconf.enabledChannelsMask);
}

void ee_setCriticalChMask(uint16_t chMask) {
	eeprom_update_word(
			(uint16_t*)&eeconf.criticalChannelsMask,
			chMask);
}

int16_t ee_getCriticalChMask(void) {
	return eeprom_read_word(
			(uint16_t*)&eeconf.criticalChannelsMask);
}







uint8_t  ee_getFaultSamples(void) {
	return pConf->faultSamples;
}

void     ee_setFaultSamples(uint8_t fSamples) {
	eeprom_update_byte(
			(uint8_t*)&eeconf.faultSamples,
			fSamples);
	pConf->faultSamples = fSamples;
}


uint8_t  ee_getFaultChecks(void) {
	return pConf->faultChecks;
}

void     ee_setFaultChecks(uint8_t fChecks) {
	eeprom_update_byte(
			(uint8_t*)&eeconf.faultChecks,
			fChecks);
	pConf->faultChecks = fChecks;
}


uint16_t ee_getFaultCheckTime(void) {
	return pConf->faultCheckTime;
}

void     ee_setFaultCheckTime(uint16_t fCheckTime) {
	eeprom_update_word(
			(uint16_t*)&eeconf.faultCheckTime,
			fCheckTime);
	pConf->faultCheckTime = fCheckTime;
}


uint32_t ee_getFaultLevel(void) {
	return pConf->faultLevel;
}

void     ee_setFaultLevel(uint32_t fLevel) {
	eeprom_update_dword(
			(uint32_t*)&eeconf.faultLevel,
			fLevel);
	pConf->faultLevel = fLevel;
}

uint8_t ee_getFlCalibrationDiv(void) {
	return pConf->flCalibrationDiv;
}

void ee_setFlCalibrationDiv(uint8_t cdiv) {
	eeprom_update_byte(
			(uint8_t*)&eeconf.flCalibrationDiv,
			cdiv);
	pConf->flCalibrationDiv = cdiv;
}

uint8_t ee_getFlDetectionDiv(void) {
	return pConf->flDetectionDiv;
}

void ee_setFlDetectionDiv(uint8_t ddiv) {
	eeprom_update_byte(
			(uint8_t*)&eeconf.flDetectionDiv,
			ddiv);
	pConf->flDetectionDiv = ddiv;
}

uint8_t  ee_getCalibrationWeeks(void) {
	return pConf->calibWeeks;
}

void     ee_setCalibrationWeeks(uint8_t cWeeks) {
	eeprom_update_byte(
			(uint8_t*)&eeconf.calibWeeks,
			cWeeks);
	pConf->calibWeeks = cWeeks;
}


uint8_t  ee_getNotifyFlags(void) {
	return pConf->notifyFlags;
}

void     ee_setNotifyFlags(uint8_t mask) {
	eeprom_update_byte(
			(uint8_t*)&eeconf.notifyFlags,
			mask);
	pConf->notifyFlags = mask;
}


void ee_loadConf(void) {
	uint8_t i;
	char buff[MAX_MSG_TEXT];
	char space[] = " ";

	LOG_INFO("EEPROM Conf:\r\n");
	DELAY(5);

	// Dump SMS message
	ee_getSmsText(buff, MAX_MSG_TEXT);
	LOG_INFO(" SMS Text: %s\r\n", buff);
	DELAY(5);

	// Dump SMS destinations
	for (i=1; i<=MAX_SMS_DEST; i++) {
		ee_getSmsDest(i, buff, MAX_SMS_NUM);
		LOG_INFO(" GSM%d: %10s%s\r\n", i, space, buff);
		DELAY(5);
	}

	pConf->enabledChannelsMask = eeprom_read_word(
			(uint16_t*)&eeconf.enabledChannelsMask);
	LOG_INFO(" Enabled CHs:  %9s0x%04X\r\n",
			space, pConf->enabledChannelsMask);
	DELAY(5);

	pConf->criticalChannelsMask = eeprom_read_word(
			(uint16_t*)&eeconf.criticalChannelsMask);
	LOG_INFO(" Critical CHs: %9s0x%04X\r\n",
			space, pConf->criticalChannelsMask);
	DELAY(5);

	pConf->faultSamples = eeprom_read_byte(
			(uint8_t*)&eeconf.faultSamples);
	LOG_INFO(" Fault samples: %14hu\r\n",
			pConf->faultSamples);
	DELAY(5);

	pConf->faultChecks = eeprom_read_byte(
			(uint8_t*)&eeconf.faultChecks);
	LOG_INFO(" Fault checks: %15hu\r\n",
			pConf->faultChecks);
	DELAY(5);

	pConf->faultCheckTime = eeprom_read_word(
			(uint16_t*)&eeconf.faultCheckTime);
	LOG_INFO(" Fault check time: %11u [s]\r\n",
			pConf->faultCheckTime);
	DELAY(5);

	pConf->faultLevel = eeprom_read_dword(
			(uint32_t*)&eeconf.faultLevel);
	LOG_INFO(" Fault level: %16lu\r\n",
			pConf->faultLevel);
	DELAY(5);

	pConf->flCalibrationDiv = eeprom_read_byte(
			(uint8_t*)&eeconf.flCalibrationDiv);
	LOG_INFO(" Fault level CDIV: %11hu\r\n",
			pConf->flCalibrationDiv);
	DELAY(5);

	pConf->flDetectionDiv = eeprom_read_byte(
			(uint8_t*)&eeconf.flDetectionDiv);
	LOG_INFO(" Fault level DDIV: %11hu\r\n",
			pConf->flDetectionDiv);
	DELAY(5);

	pConf->calibWeeks = eeprom_read_byte(
			(uint8_t*)&eeconf.calibWeeks);
	LOG_INFO(" Calibration weeks: %10hu\r\n",
			pConf->calibWeeks);
	DELAY(5);

	pConf->notifyFlags = eeprom_read_byte(
			(uint8_t*)&eeconf.notifyFlags);
	LOG_INFO(" Notification flags: %5s0x%02X\r\n",
			space, pConf->notifyFlags);
	DELAY(5);

}

