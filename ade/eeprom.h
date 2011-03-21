
#ifndef EEPROMS_H
#define EEPROMS_H

#include <cpu/types.h>
#include <io/kfile.h>

#define MAX_SMS_DEST	  3
#define MAX_SMS_NUM		 14

#define MAX_MSG_TEXT	100

typedef struct eeprom_conf {
	char sms_dest[MAX_SMS_DEST][MAX_SMS_NUM];
	char sms_mesg[MAX_MSG_TEXT];
} eeprom_conf_t;

int8_t ee_getSmsDest(uint8_t pos, char *num, uint8_t count);
int8_t ee_setSmsDest(uint8_t pos, char *num, uint8_t count);
int8_t ee_getSmsText(char *buf, uint8_t count);
int8_t ee_setSmsText(char *buf, uint8_t count);

void ee_dumpConf(KFile *fd);

#endif /* end of include guard: EEPROMS_H */
