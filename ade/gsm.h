/*
  gsm.h - Interface to GSM
  Copyright (c) 2010 Patrick Bellasi.  All right reserved.
*/

#ifndef DRK_GSM_H_
#define DRK_GSM_H_

#include <io/kfile.h>
#include <drv/ser.h>

#include <string.h> // memset

#include "cfg/cfg_gsm.h"

/* Network Params */
typedef struct gsmCellNeighbor {
	uint16_t arfcn;
	uint8_t rxl;
	uint8_t bsic;
	uint16_t lac;
} gsmCellNeighbor_t;

#define GSM_MAX_CELLS 4

typedef struct gsmCell {
	uint8_t rxq;
	uint16_t mcc;
	uint16_t mnc;
	uint16_t cellid;
	uint8_t rla;
	uint8_t txp;
	uint8_t ta;
	gsmCellNeighbor_t neigh[GSM_MAX_CELLS];
} gsmCell_t;

/* The GSM configuration and status */
typedef struct gsmConf {
	/* Serial Number Identification(IMEI) */
	char gsn[16];
	/* International Mobile Subscriber Identity */
	char cimi[16];
	/* Integrated circuit card identifier (SIM serial) */
	char ccid[24];
	/* Revision Identification Of Software Release */
	char gmr[32];
	/* Received signal strength indication */
	uint8_t rssi;
	/* Valid network informations */
	uint8_t validCellInfo:1;
	/* Network cell infos */
	gsmCell_t cell;
#define DEFAULT_MIN_SAFE_RSSI 1
	/* Channel bit error rate */
	uint8_t ber;
#define DEFAULT_MAX_SAFE_BER 5
	/* Network registration unsolicited result code */
	uint8_t creg_n;
	/* Network registration status */
	uint8_t creg_stat;
#define DEFAULT_CREG_TRY 5
	uint8_t creg_try;
#define DEFAULT_CREG_WAIT 5000
	uint16_t creg_wait;
	/* Attached/Detached from GPRS service */
	uint8_t cgatt;
#define GPRS_DETACHED 0
#define GPRS_ATTACHED 1
	char apn[32];
#define DEFAULT_APN "ibox.tim.it"
	char state;
#define DEFAULT_STATE_TRY 5
	uint8_t state_try;
#define DEFAULT_STATE_WAIT 1000
	uint16_t state_wait;
	char ip[16];
	char proto[4];
#define DEFAULT_PROTO "TCP"
	char sip[32];
//#define DEFAULT_SERVER_IP "derkling.homeip.net"
#define DEFAULT_SERVER_IP "daricom.dyndns.org"
	char sport[6];
#define DEFAULT_SERVER_PORT "31000"
	char sms_server[16];
//#define DEFAULT_SMS_SERVER "+393357963944"
#define DEFAULT_SMS_SERVER "+393473153808"
} gsmConf_t;

extern gsmConf_t gsmConf;

/* Modem commands results */
typedef enum gsmConnectResult {
	OK = 0,
	CONNECT,
	RING,
	NO_CARRIER,
	ERROR,
	NO_DIALTONE = 6,
	BUSY,
	NO_ANSWER,
	PROCEEDING,
	NO_RESPONSE,
	UNDEF = 15,
} gsmConnectResult_t;

/* The AT+CREG return code */
typedef enum gsmCregStat {
	NOT_REGISTERED = 0,
	REGISTERED,
	SEARCHING,
	DENIED,
	UNKNOW,
	ROAMING,
} gsmCregStat_t;

/* The GPRS data connection status */
typedef enum gsmStatus {
	INITIAL = 0,
	START,
	CONFIG,
	GPRSACT,
	STATUS,
	CONNECTING,
	CONNECTED,
	CLOSING,
	CLOSED,
	PDP_DEACT,
} gsmStatus_t;

/* The SMS return code */
typedef enum gsmSMSStatus {
	SMS_SEND_OK = 0,
	SMS_SEND_FAILED,
	SMS_NO_CELLS_INFO,
} gsmSMSStatus_t;

typedef struct gsmSMSMessage {
	char from[16];
	char time[21];
	char text[161];
} gsmSMSMessage_t;

inline void gsmBufferCleanup(gsmSMSMessage_t *msg);
inline void gsmBufferCleanup(gsmSMSMessage_t *msg) {
	if (msg->from[0]) {
		memset(msg->from, 0, 16);
		memset(msg->time, 0, 21);
		memset(msg->text, 0, 161);
	}
}

#if CONFIG_GSM_TESTING
void gsmTesting(Serial *port);
#else
# define gsmTesting(port)
#endif

/* GSM Control Interface */
void gsmInit(Serial *port);
void gsmReset(void);
int8_t gsmPowerOn(void);
void gsmPowerOff(void);
void gsmUpdateConf(void);
int8_t gsmGetNetworkParameters(void);


/*----- GSM Status Interface -----*/
#define gsmCSQ() gsmConf.rssi
uint8_t gsmUpdateCSQ(void);
#define gsmCREG() gsmConf.creg_stat
uint8_t gsmUpdateCREG(void);
uint8_t gsmRegistered(void);
int8_t gsmRegisterNetwork(void);

/*----- GSM SMS Interface -----*/
int8_t gsmSMSConf(uint8_t load);
int8_t gsmSMSSend(const char *number, const char *message);
int8_t gsmSMSParse(char *buff, gsmSMSMessage_t * msg);
int8_t gsmSMSLast(gsmSMSMessage_t * msg);

int8_t gsmSMSByIndex(gsmSMSMessage_t * msg, uint8_t index);
int8_t gsmSMSDel(uint8_t index);
int8_t gsmSMSDelRead(void);
int8_t gsmSMSList(void);
int8_t gsmGetNewMessage(gsmSMSMessage_t * msg);

#if 0
/* GSM Configuration Interface */
void gsmUpdateConf(void);
int8_t gsmGetNetworkParameters(void);
int8_t gsmInitNet(void);
int8_t gsmConnect(void);
int8_t gsmDisconnect(void);

/* GSM Data Interface */
int8_t gsmSendData(uint8_t *data, uint8_t len);
int8_t gsmSendStr(char *str);

/* GSM SMS Interface  */
int8_t gsmSMSSend(char *number, char *message);
int8_t gsmSMSSendCells(void);
int8_t gsmSMSConf(uint8_t load);
int8_t gsmSMSList(void);
int8_t gsmSMSLast(gsmSMSMessage_t * msg);

int8_t gsmSMSCount(void) ;
int8_t gsmSMSParse(char *buff, gsmSMSMessage_t * msg);

void gsmSMSTesting(void);
#endif

#endif // DRK_GSM_H_

