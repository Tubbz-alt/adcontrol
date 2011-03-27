/*
  gms.c - Interface to a serial GSM
  Copyright (c) 201o Patrick Bellasi.  All right reserved.
*/

#include "gsm.h"

#include "cfg/cfg_gsm.h"
#include "hw/hw_gsm.h"

#include <cfg/compiler.h>

#include <mware/sprintf.h>

//#include <string.h>
#include <avr/pgmspace.h>

/* Define logging settings (for cfg/log.h module). */
#define LOG_LEVEL   GSM_LOG_LEVEL
#define LOG_FORMAT  GSM_LOG_FORMAT
#include <cfg/log.h>

/* Define  debugging utility function */
#ifdef CONFIG_GSM_DEBUG
# define gsmDebug(STR, ...)   LOG_INFO("GSM: "STR, ## __VA_ARGS__);
#else
# define gsmDebug(STR, ...)
#endif

#define gsmFlush() ser_purge(gsm);


/*----- GSM Locales -----*/

static gsmConf_t gsmConf = {
	.creg_stat = UNKNOW,
	.creg_try = DEFAULT_CREG_TRY,
	.creg_wait = DEFAULT_CREG_WAIT,
	.apn = DEFAULT_APN,
	.state = INITIAL,
	.state_try = DEFAULT_STATE_TRY,
	.state_wait = DEFAULT_STATE_WAIT,
	.proto = DEFAULT_PROTO,
	.sip = DEFAULT_SERVER_IP,
	.sport = DEFAULT_SERVER_PORT,
	.sms_server = DEFAULT_SMS_SERVER,
};

Serial *gsm;


static int8_t _gsmWriteLine(const char *cmd, uint16_t rdelay);
static int8_t _gsmReadTo(char *resp, uint8_t size, mtime_t to);
static int8_t _gsmRead(char *resp, uint8_t size);
static int8_t _gsmReadResultTo(mtime_t to);
static int8_t _gsmReadResult(void);
static void _gsmPrintResult(uint8_t result);

/*----- GSM Control Interface -----*/

void gsmInit(Serial *port) {
	
	// Saving UART port device
	ASSERT(port);
	gsm = port;
		
	LOG_INFO("GSM: Init\n");
	gsm_init();
}

void gsmReset(void)
{
	LOG_INFO("GSM: Reset\n");
	gsm_reset();
}



#ifdef CONFIG_GSM_AUTOBAUD
static int8_t gsmAutobaud(void)
{
	char buff[16];
	int8_t try;
	int8_t resp;

	// A HEX string such as “00 49 49 49 49 FF FF FF FF” will be sent out
	// through serial port at the baud rate of 115200 immediately after
	// SIM900 is powered on.

	// Send AT command for autobaud
	gsmDebug("Autobauding...\n");
	try = 3;
	while(try--) {

		_gsmWriteLine("AT", 1000);
		_gsmRead(buff, 16);
		resp = _gsmReadResult();
		if (resp != OK) {
			gsmDebug("FAILED\n");
			continue;
		}
		gsmDebug("DONE\n");
		break;
	}
	if (try==0)
		return ERROR;

	return OK;
}
#else
#define gsmAutobaud() OK
#endif

static int8_t gsmConfigure(void)
{
	char buff[16];
	int8_t resp;

	// Load configuration
	_gsmWriteLine("ATE0", 1000);
	_gsmRead(buff, 16);
	resp = _gsmReadResult();
	if (resp != OK) {
		return resp;
	}

	// Configure TA Response Format
	_gsmWriteLine("ATV0", 1000);
	resp = _gsmReadResult();
	if (resp != OK) {
		return resp;
	}
	
	gsmFlush();
	return OK;
}

int8_t gsmPowerOn(void)
{
	int8_t result;

	LOG_INFO("GSM: Powering-on...\n");
	gsm_powerOn();
	gsmDebug("DONE\n");

	// When DCE powers on with the autobauding enabled, it is recommended
	// to wait 2 to 3 seconds before sending the first AT character.
	LOG_INFO("Wait (30s) for network attachment\n");
	timer_delay(30000);

	result = gsmAutobaud();
	if (result != OK)
		return result;

	result = gsmConfigure();

	return result;
}

void gsmPowerOff(void)
{
	LOG_INFO("GSM: Powering-off...\n");
	gsm_powerOff();
	gsmDebug("DONE\n");
}


/*----- GSM ConfigurationInterface -----*/

int8_t gsmGetNetworkParameters(void)
{
	int8_t resp;
	char buff[64];

	// Request TA Serial Number Identification(IMEI)
	_gsmWriteLine("AT+CENG=1,1", 1000);
	_gsmRead(buff, 64);
	resp = _gsmReadResult();
	if (resp != OK )
		return -1;

	_gsmWriteLine("AT+CENG?", 1000);
	// Blow-up unnedded lines
	_gsmRead(buff, 64);
	// Get current cell
	_gsmRead(buff, 64);
#if 0
	resp = sscanf(buff+9,
			"%5u,%3hhu,%3hhu,%5u,%5u,%3hhu,%4x,%5u,%3hhu,%3hhu,%3hhu",
			&gsmConf.cell.neigh[0].arfcn,
			&gsmConf.cell.neigh[0].rxl,
			&gsmConf.cell.rxq,
			&gsmConf.cell.mcc,
			&gsmConf.cell.mnc,
			&gsmConf.cell.neigh[0].bsic,
			&gsmConf.cell.cellid,
			&gsmConf.cell.neigh[0].lac,
			&gsmConf.cell.rla,
			&gsmConf.cell.txp,
			&gsmConf.cell.ta
		);
	if (resp>=11)
		gsmConf.validCellInfo = 1;
	else
		gsmConf.validCellInfo = 0;
#else
#warning FIXME update valid cells info
#endif
	// get neighburing cell
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	// NOTE: the SIM900 seem to be broken, the 'OK' string is not returned
	// as expected at the end of the neighbors inforamtion
	_gsmReadResult();

	_gsmWriteLine("AT+CMGF=1", 500);
	_gsmReadResult();

#if 0
#ifdef CONFIG_GSM_DEBUG
	snprintf(buff, 64, "lac: %05u, mnc: %02u, cid: 0x%02x, mcc: %u",
			gsmConf.cell.neigh[0].lac,
			gsmConf.cell.mnc,
			gsmConf.cell.cellid,
			gsmConf.cell.mcc);
	gsmDebugLine(buff);
#endif
#else
#warning FIXME add debuggin sentence
#endif
	return resp;
}

// Update the "Signal Quality Report"
static uint8_t gsmUpdateCSQ(void)
{
	int8_t resp;
	char buff[16];

	_gsmWriteLine("AT+CSQ", 500);
	_gsmRead(buff, 16);
	resp = _gsmReadResult();
	if (resp != OK) {
		gsmConf.rssi = 99;
		gsmConf.ber = 99;
		return resp;
	}
#if 0
	sscanf(buff, "+CSQ: %hhu,%hhu",
			&gsmConf.rssi,
			&gsmConf.ber);

#ifdef CONFIG_GSM_DEBUG
	sprintf(buff, "CSQ<=%02hhu,%02hhu", gsmConf.rssi, gsmConf.ber);
	gsmDebugLine(buff);
#endif
#else
#warning FIXME scan RSSI and BER values
#endif

	return resp;

}

void gsmUpdateConf(void)
{
	int8_t resp;

	// Request TA Serial Number Identification(IMEI)
	_gsmWriteLine("AT+GSN", 500);
	_gsmRead(gsmConf.gsn, 16);
	resp = _gsmReadResult();
	if (resp != OK)
		gsmConf.gsn[0] = '!';

	// Request International Mobile Subscriber Identity
	_gsmWriteLine("AT+CIMI", 500);
	_gsmRead(gsmConf.cimi, 16);
	resp = _gsmReadResult();
	if (resp != OK)
		gsmConf.cimi[0] = '!';

	// Subscriber Number
	_gsmWriteLine("AT+CCID", 500);
	_gsmRead(gsmConf.ccid, 24);
	resp = _gsmReadResult();
	if (resp != OK)
		gsmConf.ccid[0] = '!';

	// Request TA Revision Identification Of Software Release
	_gsmWriteLine("AT+GMR", 500);
	resp = _gsmRead(gsmConf.gmr, 32);
	if (resp<1)
		gsmConf.gmr[0] = '!';

	// Parse network parameters
	gsmGetNetworkParameters();

	// Signal Quality Report
	gsmUpdateCSQ();

}
/*----- GSM Private methods -----*/

static int8_t _gsmWriteLine(const char *cmd, uint16_t rdelay)
{
	// NOTE: debugging should no be mixed to modem command and response to
	// avoid timeing issues and discrepancy between debug and release
	// versions
	gsmDebug("TX [%s]\n", cmd);

	// Purge any buffered data before sending a new command
	gsmFlush();

	// Sending the AT command
	for (size_t i=0; cmd[i]!='\0'; i++) {
		kfile_putc(cmd[i], &(gsm->fd));
	}
	kfile_write(&(gsm->fd), "\r\n", 2);

	// Some commands could require a fixed delay to have a complete
	// response; for example the intial autobauding command
	if (rdelay)
		timer_delay(rdelay);

	return 0;
}

static int8_t _gsmReadTo(char *resp, uint8_t size, mtime_t to)
{
	int8_t len = 0;

	ASSERT(resp);

	// Init response vector
	resp[0]='\0';

	// Set the RX timeout
	ser_settimeouts(gsm, to, 0);

	// Loop to get a data lines (or EOF)
	do {
		len = kfile_gets(&(gsm->fd), (void*)resp , size);
		if (len==-1) {
			break;
		}
	} while (!len);
	//len = gsmReadLine(resp, size);

	gsmDebug("RX [%s]\n", resp);

	return len;

}

static inline int8_t _gsmRead(char *resp, uint8_t size)
{
	// get a response with the default timeout of 3s
	return _gsmReadTo(resp, size, 3000);
}

static int8_t _gsmReadResultTo(mtime_t to)
{
	char resp[8];
	uint8_t result;

	result = _gsmReadTo(resp, 8, to);
	if (result == 0) {
		result = 15; /* print a "?" */
	} else {
		result = (uint8_t)resp[0] - '0';
		if (resp[0]=='O' && resp[1]=='K') {
			/* Bugfix for wrong commands that return OK
			 * instead of the numeric code '0' */
			result = 0;
		}
	}

	_gsmPrintResult(result);

	return result;
}

static inline int8_t _gsmReadResult(void)
{
	// get a result with the default timeout of 3s
	return _gsmReadResultTo(3000);
}

#ifdef CONFIG_GSM_DEBUG
static void _gsmPrintResult(uint8_t result)
{
	switch(result) {
	case 0:
		gsmDebug("%02d: OK\n", result);
		break;
	case 1:
		gsmDebug("%02d: CONNECT\n", result);
		break;
	case 2:
		gsmDebug("%02d: RING\n", result);
		break;
	case 3:
		gsmDebug("%02d: NO CARRIER\n", result);
		break;
	case 4:
		gsmDebug("%02d: ERROR\n", result);
		break;
	case 6:
		gsmDebug("%02d: NO DIALTONE\n", result);
		break;
	case 7:
		gsmDebug("%02d: BUSY\n", result);
		break;
	case 8:
		gsmDebug("%02d: NO ANSWER\n", result);
		break;
	case 9:
		gsmDebug("%02d: PROCEEDING\n", result);
		break;
	case 10:
		gsmDebug("%02d: NO RESPONSE\n", result);
		break;
	default:
		gsmDebug("%02d: UNDEF\n", result);
		break;
	}

}
#else
# define _gsmPrintResult(result) do { } while(0)
#endif


/*----- GSM SMS Interface -----*/

int8_t gsmSMSConf(uint8_t load)
{
	char buff[16];
	int8_t resp;

	if (load) {

		// Restore SMS setting (profile 0)
		_gsmWriteLine("AT+CRES=0", 1000);
		resp = _gsmRead(buff, 16);
		if (resp==-1) {
			gsmDebug("Fail, loading SMS settings (profile 0)\n");
			return ERROR;
		}

		return OK;
	}

	// Set text mode
	_gsmWriteLine("AT+CMGF=1", 1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebug("Fail, set Text Mode\n");
		return ERROR;
	}

	// Select TE (GMS) Character Set
	_gsmWriteLine("AT+CSCS=\"GSM\"", 1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebug("Fail, set char set\n");
		return ERROR;
	}

	// Set New SMS Message Indications
	// - Forward unsolicited result codes directly to the TE
	// - (Polling mode): No SMS-DELIVER indications are routed to the TE
	// - No CBM indications are routed to the TE
	// - No SMS-STATUS-REPORTs are routed to the TE
	// - Celar TA buffer of unsolicited result codes
	_gsmWriteLine("AT+CNMI=3,0,0,0,1", 1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebug("Fail, set Indications\n");
		return ERROR;
	}

/*
	// Preferred SMS Message Storage
	_gsmWriteLine("AT+CPMS=1",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Fail, set Indications");
		return ERROR;
	}
*/

	// Set +CSCB

	// Save SMS Settings (profile "0")
	_gsmWriteLine("AT+CSAS=0", 1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebug("Fail, save settings\n");
		return ERROR;
	}

	return OK;
}
int8_t gsmSMSSend(const char *number, const char *message)
{
	int8_t resp;
	char buff[32];

	gsmDebug("Sending SMS\n");
#if 0
	if (strlen(message)>160)
		message[160] = 0;
#else
#warning CHECK for message max length!!!
#endif
	// Sending destination number
	sprintf(buff, "AT+CMGS=\"%s\", 145", number);
	_gsmWriteLine(buff, 1000);
	//_gsmWriteLine("AT+CMGS=\"", 0);
	//_gsmWriteLine(number, 0);
	//_gsmWriteLine("\", 145", 1000);
	_gsmRead(buff, 32);

	// Sending message
	_gsmWriteLine(message, 1000);

	// Sending terminator
	_gsmWriteLine("\x1a", 1000);

	// Waiting send confirmation
	_gsmReadTo(buff, 32, 15000);
	resp = _gsmReadResult();

	return resp;
}








void gsmPortingTest(Serial *port) {
	gsmInit(port);
	gsmPowerOn();

	//gsmUpdateConf();
	//timer_delay(1000);

	gsmSMSConf(0);
	timer_delay(5000);
	gsmSMSSend("+393473153808", "Test message from RFN");

	timer_delay(15000);
	gsmPowerOff();
}






















#if 0



static int8_t _gsmWriteData(uint8_t *data, uint8_t len, uint16_t rdelay)
{
	// NOTE: debugging should no be mixed to modem command and response to
	// avoid timeing issues and discrepancy between debug and release
	// versions
	gsmDebugStr("TXb [");
	_gsmDebugStr((char*)data);
	_gsmDebugLine("]");

	// Sending the AT command
	gsmPrintData(data, len);

	// Some commands could require a fixed delay to have a complete
	// response; for example the intial autobauding command
	if (rdelay)
		delay(rdelay);

	return 0;
}


#ifdef CONFIG_GSM_DEBUG
// This is an endless loop polling the modem for generated messages
static void _gsmReadLoop(void)
{
	char buff[48];

	gsmDebugLine("Entering endless read loop...");
	while(1) {
		_gsmRead(buff, 48);
	}
}

#else
# define _gsmReadLoop \
#error endless loops still present into the code
#endif


static uint8_t gsmUpdateCREG(void)
{
	int8_t resp;
	char buff[16];

	_gsmWriteLine("AT+CREG?", 500);
	_gsmRead(buff, 16);
	resp = _gsmReadResult();
	if (resp != OK) {
		gsmConf.creg_n = 0;
		gsmConf.creg_stat = UNKNOW;
		return resp;
	}

	sscanf(buff, "+CREG: %hhu,%hhu",
			&gsmConf.creg_n,
			&gsmConf.creg_stat);
#ifdef CONFIG_GSM_DEBUG
	sprintf(buff, "CREG<=%02hhu,%02hhu", gsmConf.creg_n, gsmConf.creg_stat);
	gsmDebugLine(buff);
#endif

	return resp;

}

static int8_t gsmUnlockSim(void)
{
	int8_t resp;
	char buff[16];

	// Check SIM PIN
	_gsmWriteLine("AT+CPIN?", 500);
	_gsmRead(buff, 16);
	resp = _gsmReadResult();
	if (resp == OK)
		return resp;

	gsmDebugLine("SIM LOCKED");
	/* TODO: add support for SIM unlocking  */

	return ERROR;
}

static int8_t gsmRegisterNetwork(void)
{
	int8_t resp;
	uint8_t try;

	try = gsmConf.creg_try;
	for ( ; try; try--) {

		resp = gsmUpdateCREG();
		if (resp!=OK)
			break; /* Exit with error */

		if (gsmConf.creg_stat == REGISTERED ||
			gsmConf.creg_stat == ROAMING) {
			break; /* Exit OK */
		}

		if (gsmConf.creg_stat == SEARCHING ||
			gsmConf.creg_stat == NOT_REGISTERED) {
			delay(gsmConf.creg_wait);
			continue;
		}

		if (gsmConf.creg_stat ==  DENIED ||
			gsmConf.creg_stat == UNKNOW) {
			resp=ERROR;
			break;	/* Exit with error */
		}
	}

	if (try==0)
		resp=ERROR;

	return resp;
}

static int8_t gsmAttachGPRS(void)
{
	int8_t resp;
	char buff[16];

	// Check GPRS attach status
	_gsmWriteLine("AT+CGATT?", 1000);
	_gsmRead(buff, 16);
	resp = _gsmReadResult();
	if (resp != OK) {
		gsmConf.creg_n = GPRS_DETACHED;
		return resp;
	}

	sscanf(buff, "+CGATT: %hhu",
			&gsmConf.cgatt);

	/* TODO: handle GPRS attach if in detached mode */

#ifdef CONFIG_GSM_DEBUG
	sprintf(buff, "CGATT<=%02hhu", gsmConf.cgatt);
	gsmDebugLine(buff);
#endif

	return resp;

}

static int8_t gsmSetupAPN(void)
{
	int8_t resp;
	char buff[32];

	// START task and Set APN
	snprintf(buff, 32, "AT+CSTT=\"%s\"", gsmConf.apn);
	_gsmWriteLine(buff, 500);
	resp = _gsmReadResult();

	/* TODO: handle configuration problesm */

	return resp;

}

static int8_t gsmUpdateStatus(void)
{
	int8_t resp;
	char buff[32];

	_gsmWriteLine("AT+CIPSTATUS", 500);
	resp = _gsmReadResult();
	if (resp != OK) {
		return ERROR;
	}

	_gsmRead(buff, 32);
	// buff => "STATE: <state>"
	switch(buff[7]) {
	case 'C':
		gsmConf.state = CONNECTED;
		break;
	case 'I':
		switch(buff[10]) {
		case 'C':
			gsmConf.state = CONFIG;
			break;
		case 'G':
			gsmConf.state = GPRSACT;
			break;
		case 'I':
			gsmConf.state = INITIAL;
			break;
		case 'S':
			gsmConf.state = (buff[13] == 'R') ? START : STATUS;
			break;
		}
		break;
	case 'P':
		gsmConf.state = PDP_DEACT;
		break;
	case 'T':
		switch(buff[16]) {
		case 'C':
			gsmConf.state = CONNECTING;
			break;
		case 'D':
			gsmConf.state = CLOSED;
			break;
		case 'N':
			gsmConf.state = CLOSING;
			break;
		}
		break;
	}

#ifdef CONFIG_GSM_DEBUG
	sprintf(buff, "STATE<=%02hhu", gsmConf.state);
	gsmDebugLine(buff);
#endif

	return resp;

}

// Bring Up Wireless GPRS Connection
static int8_t gsmWirelessUp(void)
{
	int8_t resp;
	uint8_t try;

// AT+CIICR only activates moving scene at the status of IP START,
// after operating this Command, the state will be changed to IP CONFIG.
// After module accepting the activated operation, if activate successfully,
// the state will be changed to IP GPRSACT, response OK, otherwise
// response ERROR.

	// START task and Set APN
	_gsmWriteLine("AT+CIICR", 1000);
	resp = _gsmReadResultTo(10000);
	if (resp != OK)
		return resp;

	try = gsmConf.state_try;
	do {
		resp = gsmUpdateStatus();
		if (resp!=OK)
			return ERROR;

		if (gsmConf.state == GPRSACT) {
			break; /* Exit OK */
		}

		delay(gsmConf.state_wait);

	} while(--try);
	if (try == 0)
		resp = ERROR;

	/* TODO: handle error problems */

	return resp;

}

// Update the "Local IP Address"
static uint8_t gsmUpdateIP(void)
{
	int8_t resp;
#ifdef CONFIG_GSM_DEBUG
	char buff[32];
#endif

// Only at the status of activated the moving scene: IP GPRSACT、
// TCP/UDP CONNECTING、CONNECT OK、IP CLOSE can get local
// IP Address by AT+CIFSR, otherwise response ERROR.

	_gsmWriteLine("AT+CIFSR", 1000);
	resp = _gsmRead(gsmConf.ip, 16);
	if (resp==-1) {
		gsmConf.ip[0] = 0;
		return ERROR;
	}

	if (gsmConf.ip[0] == 'E') {
		gsmConf.ip[0] = 0;
		return ERROR;
	}

#ifdef CONFIG_GSM_DEBUG
	snprintf(buff, 32, "IP<=%s", gsmConf.ip);
	gsmDebugLine(buff);
#endif

	return OK;

}

// Start Up TCP Connection
static int8_t gsmStartTCP(void)
{
	int8_t resp;
	char buff[48];

// This command is allowed to establish a TCP/UDP connection only
// when the state is IP INITIAL or IP STATUS when it is in single state.
// In multi-IP state, the state is in IP STATUS only. So it is necessary to
// process “AT+CIPSHUT” before establish a TCP/UDP connection with
// this command when the state is not IP INITIAL or IP STATUS.
// When in multi-IP state, before executing this command, it is necessary
// to process” AT+CSTT, AT+CIICR, AT+CIFSR”

// This command generate 2 or 3 responce sentence:
//
// a) command format
// If format is right:
//   OK
// otherwise:
//  +CME ERROR <err>
//
// b) command result:
// If connection exists, response:
//  ALREAY CONNECT
// If connected successfully response:
//  CONNECT OK
// Otherwise, response:
//  STATE: <state>
//  CONNECT FAIL


	snprintf(buff, 48, "AT+CIPSTART=\"%s\",\"%s\",\"%s\"",
		gsmConf.proto,
		gsmConf.sip,
		gsmConf.sport);

	_gsmWriteLine(buff, 1000);

	// a) reading FORMAT response
	resp = _gsmReadResult();
	if (resp!=OK) {
		gsmDebugLine("Wrong format on AT+CIPSTART command");
		return resp;
	}

	// b) reading RESULT response
	resp = _gsmReadTo(buff, 48, 15000);
	if (resp==-1) {
		gsmDebugLine("Failed reading RESULT response");
		return ERROR;
	}

	if (buff[0]=='S') {
		// Connection failed: reading back the last sentence
		gsmDebugLine(buff);
		_gsmRead(buff, 48);
		return ERROR;
	}

	// Connection successful
	gsmDebugLine("STATE<=(06)CONNECTED");
	gsmConf.state = CONNECTED;

	return OK;

}

static int8_t gsmConfSendMode(void)
{
	int8_t resp;

	// Set Prompt Of ‘>’ When Sending Data
	// 0: no prompt and show "send ok" when send successfully
	// 1: echo ‘>’ prompt and show "send ok" when send successfully
	_gsmWriteLine("AT+CIPSPRT=0", 700);
	resp = _gsmReadResult();
	if (resp!=OK) {
		return resp;
	}

	// Select 'NORMAL' Data Transmitting Mode
	// when the server receives TCP data, it will response SEND OK
	_gsmWriteLine("AT+CIPQSEND=0", 700);
	resp = _gsmReadResult();
	if (resp!=OK) {
		return resp;
	}

	//  Set Auto Sending Timer
	//  0: not set timer when sending data
	//  1: set timer when sending data
	_gsmWriteLine("AT+CIPATS=0", 700);
	resp = _gsmReadResult();
	if (resp!=OK) {
		return resp;
	}

	return OK;
}




/*----- GSM ConfigurationInterface -----*/

int8_t gsmGetNetworkParameters(void)
{
	int8_t resp;
	char buff[64];

	// Request TA Serial Number Identification(IMEI)
	_gsmWriteLine("AT+CENG=1,1", 0);
	_gsmRead(buff, 64);
	resp = _gsmReadResult();
	if (resp != OK )
		return -1;

	_gsmWriteLine("AT+CENG?", 0);
	// Blow-up unnedded lines
	_gsmRead(buff, 64);
	// Get current cell
	_gsmRead(buff, 64);
	resp = sscanf(buff+9,
			"%5u,%3hhu,%3hhu,%5u,%5u,%3hhu,%4x,%5u,%3hhu,%3hhu,%3hhu",
			&gsmConf.cell.neigh[0].arfcn,
			&gsmConf.cell.neigh[0].rxl,
			&gsmConf.cell.rxq,
			&gsmConf.cell.mcc,
			&gsmConf.cell.mnc,
			&gsmConf.cell.neigh[0].bsic,
			&gsmConf.cell.cellid,
			&gsmConf.cell.neigh[0].lac,
			&gsmConf.cell.rla,
			&gsmConf.cell.txp,
			&gsmConf.cell.ta
		);
	if (resp>=11)
		gsmConf.validCellInfo = 1;
	else
		gsmConf.validCellInfo = 0;

	// get neighburing cell
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	_gsmRead(buff, 64);
	// NOTE: the SIM900 seem to be broken, the 'OK' string is not returned
	// as expected at the end of the neighbors inforamtion
	_gsmReadResult();

	_gsmWriteLine("AT+CMGF=1", 500);
	_gsmReadResult();

#ifdef CONFIG_GSM_DEBUG
	snprintf(buff, 64, "lac: %05u, mnc: %02u, cid: 0x%02x, mcc: %u",
			gsmConf.cell.neigh[0].lac,
			gsmConf.cell.mnc,
			gsmConf.cell.cellid,
			gsmConf.cell.mcc);
	gsmDebugLine(buff);
#endif

	return resp;
}




int8_t gsmInitNet(void)
{
	int8_t resp;

	gsmDebugLine("net init...");

	// Check SIM PIN
	resp = gsmUnlockSim();
	if (resp!=OK)
		return resp;

	// Check Signal Quality Report
	resp = gsmUpdateCSQ();
	if (resp!=OK || gsmConf.rssi==99)
		return NO_CARRIER;
	if (gsmConf.rssi<DEFAULT_MIN_SAFE_RSSI)
		return NO_CARRIER;

	// GSM network registration
	resp = gsmRegisterNetwork();
	if (resp!=OK)
		return resp;

	// Attach GPRS Service
	resp = gsmAttachGPRS();
	if (resp!=OK)
		return ERROR;

	// START task and Set APN
	resp = gsmSetupAPN();
	if (resp!=OK)
		return ERROR;

	return OK;
}

int8_t gsmConnect(void)
{
	int8_t resp;

	gsmDebugLine("connect...");

	// Bring up wireless connection with GPRS
	resp = gsmWirelessUp();
	if (resp!=OK)
		return ERROR;

	// Get local IP address
	resp = gsmUpdateIP();
	if (resp!=OK)
		return ERROR;

	// Start up TCP connection:
	resp = gsmStartTCP();
	if (resp!=OK)
		return ERROR;

	resp = gsmConfSendMode();
	if (resp!=OK)
		return ERROR;

	return OK;
}

int8_t gsmDisconnect()
{
	char buff[16];
	int8_t resp;

	gsmDebugLine("disconnect...");

// If close successfully:
//  CLOSE OK
// If close fail:
//  ERROR

	// Close TCP Connection
	_gsmWriteLine("AT+CIPCLOSE",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Failed SEND data to TE");
		return ERROR;
	}

	if (buff[0]=='E') // ERROR
		return ERROR;

	return OK;

}


/*----- GSM Data Interface -----*/

static int8_t _gsmSend(uint8_t *data, uint8_t len, uint8_t ascii)
{
	char buff[16];
	int8_t resp;

	gsmDebugLine("send data...");

// If sending successfully:
//  When +CIPQSEND=0:
//   SEND OK
//  When +CIPQSEND=1:
//   DATA ACCEPT:<length>
// If sending fail:
//  SEND FAIL

	// Send data through TCP connection
	snprintf(buff, 16, "AT+CIPSEND=%d", len);
	_gsmWriteLine(buff,500);

	// Send data, Ctrl-Z (0x1A==26) is used as a termination symbol
	if (ascii)
		_gsmWriteLine((char*)data, 2000);
	else
		_gsmWriteData(data, len, 2000);

	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Failed SEND data to TE");
		return ERROR;
	}

	if (buff[5]=='F') // SEND FAIL
		return ERROR;

	return OK;
}

int8_t gsmSendData(uint8_t *data, uint8_t len)
{
	return _gsmSend(data, len, 0);
}

int8_t gsmSendStr(char *str)
{
	uint8_t len;

	len = strlen(str);
	return _gsmSend((uint8_t*)str, len, 1);

}

/*----- GSM SMS Interface -----*/

int8_t gsmSMSSend(char *number, char *message)
{
	int8_t resp;
	char buff[32];

	gsmDebugLine("sending SMS...");

	if (strlen(message)>160)
		message[160] = 0;

	// Sending destination number
	sprintf(buff, "AT+CMGS=\"%s\", 145", number);
	_gsmWriteLine(buff, 1000);
	_gsmRead(buff, 32);

	// Sending message
	_gsmWriteLine(message, 1000);

	// Sending terminator
	_gsmWriteLine("\x1a", 1000);

	// Waiting send confirmation
	_gsmReadTo(buff, 32, 15000);
	resp = _gsmReadResult();

	return resp;
}

int8_t gsmSMSSendCells(void)
{
	char buff[36];
	int8_t resp;

	gsmDebugLine("sending cells...");

	if (!gsmConf.validCellInfo) {
		gsmDebugLine("Abort: no valid cell info");
		return SMS_NO_CELLS_INFO;
	}

	snprintf(buff, 36, "ATrack@%05u,%02u,%02x,%u",
			gsmConf.cell.neigh[0].lac,
			gsmConf.cell.mnc,
			gsmConf.cell.cellid,
			gsmConf.cell.mcc);
	resp = gsmSMSSend(gsmConf.sms_server, buff);
	if (resp != OK)
		resp = SMS_SEND_FAILED;

	return SMS_SEND_OK;
}

int8_t gsmSMSConf(uint8_t load)
{
	char buff[16];
	int8_t resp;

	if (load) {

		// Restore SMS setting (profile 0)
		_gsmWriteLine("AT+CRES=0",1000);
		resp = _gsmRead(buff, 16);
		if (resp==-1) {
			gsmDebugLine("Fail, loading SMS settings (profile 0)");
			return ERROR;
		}

		return OK;
	}

	// Set text mode
	_gsmWriteLine("AT+CMGF=1",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Fail, set Text Mode");
		return ERROR;
	}

	// Select TE (GMS) Character Set
	_gsmWriteLine("AT+CSCS=\"GSM\"",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Fail, set char set");
		return ERROR;
	}

	// Set New SMS Message Indications
	// - Forward unsolicited result codes directly to the TE
	// - (Polling mode): No SMS-DELIVER indications are routed to the TE
	// - No CBM indications are routed to the TE
	// - No SMS-STATUS-REPORTs are routed to the TE
	// - Celar TA buffer of unsolicited result codes
	_gsmWriteLine("AT+CNMI=3,0,0,0,1",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Fail, set Indications");
		return ERROR;
	}

/*
	// Preferred SMS Message Storage
	_gsmWriteLine("AT+CPMS=1",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Fail, set Indications");
		return ERROR;
	}
*/

	// Set +CSCB

	// Save SMS Settings (profile "0")
	_gsmWriteLine("AT+CSAS=0",1000);
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebugLine("Fail, save settings");
		return ERROR;
	}

	return OK;
}

/*
int8_t gsmSMSList(void)
{
	char buff[256];
	int8_t resp;

	// Get message list
	_gsmWriteLine("AT+CMGL=\"ALL\",1",1000);
	do {
		resp = _gsmReadTo(buff, 255, 5000);
		if (resp<0)
			break;
	} while (strcmp(buff,"OK") &&
			strcmp(buff,"+CMS"));
	if (resp==-1) {
		gsmDebugLine("Fail, get SMS list");
		return ERROR;
	}

	return OK;
}
*/

int8_t gsmSMSParse(char *buff, gsmSMSMessage_t * msg) {
	char * p1;
	char * p2;
	int8_t i;

	// Example message buffer:
	// +CMGR: "REC READ","+393357963938","","10/12/14,22:59:15+04"
	// NUM+<from>NUM+<to>RESMSG:<text>

	// parsing TIME
	p1 = strstr(buff, "/");
	strncpy(msg->time, p1-2, 20);

	// parsing FROM number
	p1 = strstr(buff, "NUM+");
	p1 += 3;
	p2 = strstr(p1, "NUM+");
	i = (p2-p1 > 15) ? 15 : p2-p1;
	strncpy(msg->from, p1, i);

	// parsing TEXT message
	p1 = strstr(p1+26, "MSG:");
	strncpy(msg->text, p1+4, 160);

	return 0;

}

int8_t gsmSMSLast(gsmSMSMessage_t * msg) {
	char buff[256];
	int8_t resp = 0;
	uint8_t len = 0;
	char *pos;

	// Get message list
	_gsmWriteLine("AT+CMGR=1", 2000);
	pos = buff;
	do {
		len += resp;
		resp = _gsmReadTo(pos+len, 255-len, 5000);
		if (resp<0)
			break;

		if (pos[len]=='0')
			break;

	} while (strcmp(pos+len,"OK") &&
			strcmp(pos+len,"+CMS"));

	if (resp==-1) {
		gsmDebugLine("Fail, get last SMS");
		return ERROR;
	}

	gsmDebugStr("SMS_BUFF: [");
	_gsmDebugStr(buff);
	_gsmDebugLine("]");

	gsmSMSParse(buff, msg);

	return OK;
}

/*
int8_t gsmSMSCount(void) {
	char buff[64];
	int8_t resp;
	int8_t count = 0;

	// Read SMS Message Storage
	// => +CPMS: "SM",2,3
	// ==> <mem1>,<used1>,<total1>
	_gsmWriteLine("AT+CPMS?", 2000);
	resp = _gsmRead(buff, 255);
	if (resp==-1)
		return -1;

	// Example response:
	// +CPMS: "SM",3,30,"SM",3,30,"SM",3,30
	//             ^--- sms count
	// message count start at pos 12
	if (resp>13)
		sscanf(buff+12, "%hd", &count);

	return count;

}
*/

void gsmSMSTesting(void) {
	char buff[256];
	int8_t resp;

	// Read SMS Message Storage
	// => +CPMS: "SM",2,3
	// ==> <mem1>,<used1>,<total1>
	_gsmWriteLine("AT+CPMS?", 5000);
	resp = _gsmRead(buff, 255);
	if (resp==-1)
		return;

	_gsmWriteLine("AT+CNMI?", 5000);
	resp = _gsmRead(buff, 255);
	if (resp==-1)
		return;


}

#endif

