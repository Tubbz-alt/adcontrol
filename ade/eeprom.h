
#ifndef EEPROMS_H
#define EEPROMS_H

#include <cpu/types.h>
#include <io/kfile.h>

#define MAX_SMS_DEST	  3
#define MAX_SMS_NUM		 14

#define MAX_MSG_TEXT	100

#define EMPTY_NUMBER "-\0"

typedef struct eeprom_conf {
	char sms_dest[MAX_SMS_DEST][MAX_SMS_NUM];
	char sms_mesg[MAX_MSG_TEXT];

	/** A bitmask of ENABLED input channel which should be monitored */
	uint16_t enabledChannelsMask;
	/** A bitmask of CRITICAL input channel which should generate
	 * an EXT alarm (by switching the Relé) */
	uint16_t criticalChannelsMask;

	/** Number of FAULT samples before alarm notification */
	uint8_t faultSamples;
	/** Number of suspension/recheck on faulty detection */
	uint8_t faultChecks;
	/** Number of seconds between faults checks */
	uint16_t faultCheckTime;
	/** Fault (Power) detection level */
	uint32_t faultLevel;

	/** Weeks between recalibrations */
	uint8_t calibWeeks;

	/** Notification flags */
#define EE_NOTIFY_REBOOT 		0
#define EE_NOTIFY_CALIBRATION 	1
	uint8_t notifyFlags;

} eeprom_conf_t;


/**
 * The Set of EEPROM data mirrored on RAM at run-time
 */
typedef struct runtime_conf {

	/** A bitmask of ENABLED input channel which should be monitored */
	uint16_t enabledChannelsMask;
	/** A bitmask of CRITICAL input channel which should generate
	 * an EXT alarm (by switching the Relé) */
	uint16_t criticalChannelsMask;

	/** Number of FAULT samples before alarm notification */
	uint8_t faultSamples;
	/** Number of suspension/recheck on faulty detection */
	uint8_t faultChecks;
	/** Number of seconds between faults checks */
	uint16_t faultCheckTime;
	/** Fault (Power) detection level */
	uint32_t faultLevel;

	/** Weeks between recalibrations */
	uint8_t calibWeeks;

	/** Notification flags */
	uint8_t notifyFlags;

} runtime_conf_t;

int8_t  ee_getSmsDest(uint8_t pos, char *num, uint8_t count);
int8_t  ee_setSmsDest(uint8_t pos, const char *num);
int8_t  ee_getSmsText(char *buf, uint8_t count);
int8_t  ee_setSmsText(const char *buf);
int16_t ee_getEnabledChMask(void);
void    ee_setEnabledChMask(uint16_t chMask);
int16_t ee_getCriticalChMask(void);
void    ee_setCriticalChMask(uint16_t chMask);

uint8_t  ee_getFaultSamples(void);
void     ee_setFaultSamples(uint8_t);
uint8_t  ee_getFaultChecks(void);
void     ee_setFaultChecks(uint8_t);
uint16_t ee_getFaultCheckTime(void);
void     ee_setFaultCheckTime(uint16_t);
uint32_t ee_getFaultLevel(void);
void     ee_setFaultLevel(uint32_t);
uint8_t  ee_getCalibrationWeeks(void);
void     ee_setCalibrationWeeks(uint8_t);


uint8_t  ee_getNotifyFlags(void);
void     ee_setNotifyFlags(uint8_t mask);
inline uint8_t  ee_onNotifyReboot(void);
inline uint8_t  ee_onNotifyReboot(void) {
	return (ee_getNotifyFlags() & BV8(EE_NOTIFY_REBOOT));
}
inline uint8_t  ee_onNotifyCalibration(void);
inline uint8_t  ee_onNotifyCalibration(void) {
	return (ee_getNotifyFlags() & BV8(EE_NOTIFY_CALIBRATION));
}

extern runtime_conf_t *pConf;

void ee_loadConf(void);

#endif /* end of include guard: EEPROMS_H */
