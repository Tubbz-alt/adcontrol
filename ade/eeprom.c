

#include "eeprom.h"

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

void ee_dumpConf(void) {
	uint8_t i;
	char buff[MAX_MSG_TEXT];
	uint16_t chMask;

	LOG_INFO(".:: EEPROM Conf\r\n");
	DELAY(5);

	// Dump SMS destinations
	for (i=1; i<=MAX_SMS_DEST; i++) {
		ee_getSmsDest(i, buff, MAX_SMS_NUM);
		LOG_INFO(" SMS[%d]: %s\r\n", i, buff);
		DELAY(5);
	}

	// Dump SMS message
	ee_getSmsText(buff, MAX_MSG_TEXT);
	LOG_INFO(" SMS Text: %s\r\n", buff);
	DELAY(5);

	chMask = ee_getEnabledChMask();
	LOG_INFO(" Enabled CHs: 0x%04X\r\n", chMask);
	DELAY(5);

	chMask = ee_getCriticalChMask();
	LOG_INFO(" Critical CHs: 0x%04X\r\n", chMask);
	DELAY(5);
}


