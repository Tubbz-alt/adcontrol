

#include "eeprom.h"

#include <avr/eeprom.h>

#include <drv/timer.h>

eeprom_conf_t EEMEM eeconf = {
	.sms_dest = {
		"-391231234567\0",
		"-391231234567\0",
		"-391231234567\0",
	},
	.sms_mesg = "Allarme impianto RCT\0",
};

int8_t ee_getSmsDest(uint8_t pos, char *num, uint8_t count) {
	
	if (pos >= MAX_SMS_DEST)
		return -1;

	if (count > MAX_SMS_NUM)
		count = MAX_SMS_NUM;


	eeprom_read_block((void*)num, (const void*)(eeconf.sms_dest[pos]), count);
	
	return 0;

}

int8_t ee_setSmsDest(uint8_t pos, char *num, uint8_t count) {

	if (pos >= MAX_SMS_DEST)
		return -1;

	if (count > MAX_SMS_NUM)
		count = MAX_SMS_NUM;

	eeprom_update_block((const void*)num, (void*)(eeconf.sms_dest[pos]), count);
	
	return count;

}

int8_t ee_getSmsText(char *buf, uint8_t count) {

	if (count > MAX_MSG_TEXT)
		count = MAX_MSG_TEXT;

	eeprom_read_block((void*)buf, (const void*)eeconf.sms_mesg, count);
	
	return count;

}

int8_t ee_setSmsText(char *buf, uint8_t count) {

	if (count > MAX_MSG_TEXT)
		count = MAX_MSG_TEXT;

	eeprom_update_block((const void*)buf, (void*)eeconf.sms_mesg, count);
	
	return count;

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


