

#include "eeprom.h"

#include <avr/eeprom.h>

#include <drv/timer.h>


eeprom_conf_t EEMEM eeconf = {
	.sms_dest = {
		EMPTY_NUMBER"\0\0\0\0\0\0\0\0\0\0\0\0",
		EMPTY_NUMBER"\0\0\0\0\0\0\0\0\0\0\0\0",
		EMPTY_NUMBER"\0\0\0\0\0\0\0\0\0\0\0\0",
	},
	.sms_mesg =
			"Allarme impianto RCT\0DEADBEEFDEADBEEFDEADBEEFDEADB"
			"EEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBE\0",

};

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
	
	if (pos >= MAX_SMS_DEST)
		return -1;

	if (count>MAX_SMS_NUM)
		count=MAX_SMS_NUM;

	return  ee_getString((const uint8_t*)eeconf.sms_dest[pos],
							num, count);

}


int8_t ee_setSmsDest(uint8_t pos, const char *num) {

	if (pos >= MAX_SMS_DEST)
		return -1;

	return ee_setString((uint8_t*)eeconf.sms_dest[pos],
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

void ee_dumpConf(KFile *fd) {
	uint8_t i;
	char buff[MAX_MSG_TEXT];

	kfile_print(fd, ".:: EEPROM Conf\n");
	timer_delay(5);

	// Dump SMS destinations
	for (i=0; i<MAX_SMS_DEST; i++) {
		ee_getSmsDest(i, buff, MAX_SMS_NUM);
		kfile_printf(fd, "SMS[%d]: %s\r\n", i, buff);
		timer_delay(5);
	}
	// Dump SMS message
	ee_getSmsText(buff, MAX_MSG_TEXT);
	kfile_print(fd, "SMS Text: ");
	kfile_print(fd, buff);
	kfile_print(fd, "\r\n");
	timer_delay(5);
}


