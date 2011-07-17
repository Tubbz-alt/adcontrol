/**
 *       @file  gsm.c
 *      @brief  Interface to a serial GSM
 *
 * This provides a driver for the SIM900 GSM module by SIMCom.
 * The current implementation support basic SMS send/receive command as well
 * as GPRS connection setup and data exchange.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  05/18/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * ============================================================================
 */

#include "gsm.h"

#include "hw/hw_gsm.h"
#include "hw/hw_led.h"

#include <cfg/compiler.h>

#include <stdio.h> 			// sprintf
#include <string.h> 		// strstr
#include <avr/pgmspace.h>

/* Define logging settings (for cfg/log.h module). */
#define LOG_LEVEL   GSM_LOG_LEVEL
#define LOG_FORMAT  GSM_LOG_FORMAT
#include <cfg/log.h>

/* Define  debugging utility function */
#if CONFIG_GSM_DEBUG
# define gsmDebug(STR, ...)   LOG_INFO("GSM: "STR, ## __VA_ARGS__);
#else
# define gsmDebug(STR, ...)
#endif

/*----- GSM Locales -----*/

gsmConf_t gsmConf = {
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

static int8_t _gsmRead(char *resp, uint8_t size);
static int8_t _gsmReadResult(void);
static int8_t _gsmWrite(const char *cmd, size_t count);
static int8_t _gsmWriteLine(const char *cmd);

#if CONFIG_GSM_DEBUG
static void _gsmPrintResult(uint8_t result)
{
	switch(result) {
	case 0:
		gsmDebug("%d: OK\n", result);
		break;
	case 1:
		gsmDebug("%d: CONNECT\n", result);
		break;
	case 2:
		gsmDebug("%d: RING\n", result);
		break;
	case 3:
		gsmDebug("%d: NO CARRIER\n", result);
		break;
	case 4:
		gsmDebug("%d: ERROR\n", result);
		break;
	case 6:
		gsmDebug("%d: NO DIALTONE\n", result);
		break;
	case 7:
		gsmDebug("%d: BUSY\n", result);
		break;
	case 8:
		gsmDebug("%d: NO ANSWER\n", result);
		break;
	case 9:
		gsmDebug("%d: PROCEEDING\n", result);
		break;
	case 10:
		gsmDebug("%d: NO RESPONSE\n", result);
		break;
	default:
		gsmDebug("%d: UNDEF\n", result);
		break;
	}

}
#else
# define _gsmPrintResult(result)
#endif


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
	char buff[8];
	int8_t try;
	int8_t resp;

	// A HEX string such as “00 49 49 49 49 FF FF FF FF” will be sent out
	// through serial port at the baud rate of 115200 immediately after
	// SIM900 is powered on.

	// Send AT command for autobaud
	gsmDebug("Autobauding...\n");

	for(try=3; try; try--) {
		_gsmWriteLine("AT");
		_gsmRead(buff, 8);
		resp = _gsmReadResult();
		if (resp == OK) {
			gsmDebug("DONE\n");
			return resp;
		}
		gsmDebug("FAILED\n");
	}
	return ERROR;
}
#else
# define gsmAutobaud() OK
#endif

static int8_t gsmConfigure(void)
{
	char buff[8];
	int8_t resp;

	// Load configuration
	_gsmWriteLine("ATE0");
	_gsmRead(buff, 8);
	resp = _gsmReadResult();
	if (resp != OK) {
		return resp;
	}

	// Configure TA Response Format
	_gsmWriteLine("ATV0");
	resp = _gsmReadResult();
	if (resp != OK) {
		return resp;
	}

	return OK;
}

int8_t gsmPowerOn(void)
{
	int8_t result;

	// TODO: check if the modem is already on
	// either using the STATUS line or by sending an AT command
	if (gsm_status()) {
#if 0
		// Modem already ON... checking AT command
		_gsmWriteLine("AT");
		result = _gsmReadResult();
		if (result == OK)
			return result;
		// Not responding to AT comand... shut-down
		gsm_powerOff();
		timer_delay(10000);
#endif
		// Resetting the modem
		LOG_INFO("GSM: Resetting...\n");
		gsm_reset();
	} else {
		LOG_INFO("GSM: Powering-on...\n");
		gsm_on();
	}

	// When DCE powers on with the autobauding enabled, it is recommended
	// to wait 2 to 3 seconds before sending the first AT character.
	LOG_INFO("Wait (20s) for network attachment\n");
	timer_delay(20000);

	result = gsmAutobaud();
	if (result != OK)
		return result;

	result = gsmConfigure();
	return result;
}

void gsmPowerOff(void)
{
	LOG_INFO("GSM: Powering-off...\n");
	gsm_off();
	LED_GSM_OFF();
}


/*----- GSM ConfigurationInterface -----*/

int8_t gsmGetNetworkParameters(void)
{
	int8_t resp;
	char buff[64];

	// Request TA Serial Number Identification(IMEI)
	_gsmWriteLine("AT+CENG=1,1");
	_gsmRead(buff, 64);
	resp = _gsmReadResult();
	if (resp != OK )
		return -1;

	_gsmWriteLine("AT+CENG?");
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

	_gsmWriteLine("AT+CMGF=1");
	_gsmReadResult();

#if 0
#if CONFIG_GSM_DEBUG
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
uint8_t gsmUpdateCSQ(void)
{
	int8_t resp;
	char buff[16];

	_gsmWriteLine("AT+CSQ");
	_gsmRead(buff, 16);
	resp = _gsmReadResult();
	if (resp != OK) {
		gsmConf.rssi = 99;
		gsmConf.ber = 99;
		return resp;
	}

	sscanf(buff, "+CSQ: %hu,%hu",
			(short unsigned int*)&gsmConf.rssi,
			(short unsigned int*)&gsmConf.ber);

	gsmDebug("CSQ [%hu]\r\n", gsmConf.rssi);
	
	return resp;

}

void gsmUpdateConf(void)
{
	int8_t resp;

	// Request TA Serial Number Identification(IMEI)
	_gsmWriteLine("AT+GSN");
	_gsmRead(gsmConf.gsn, 16);
	resp = _gsmReadResult();
	if (resp != OK)
		gsmConf.gsn[0] = '!';

	// Request International Mobile Subscriber Identity
	_gsmWriteLine("AT+CIMI");
	_gsmRead(gsmConf.cimi, 16);
	resp = _gsmReadResult();
	if (resp != OK)
		gsmConf.cimi[0] = '!';

	// Subscriber Number
	_gsmWriteLine("AT+CCID");
	_gsmRead(gsmConf.ccid, 24);
	resp = _gsmReadResult();
	if (resp != OK)
		gsmConf.ccid[0] = '!';

	// Request TA Revision Identification Of Software Release
	_gsmWriteLine("AT+GMR");
	resp = _gsmRead(gsmConf.gmr, 32);
	if (resp<1)
		gsmConf.gmr[0] = '!';

	// Parse network parameters
	gsmGetNetworkParameters();

	// Signal Quality Report
	gsmUpdateCSQ();

}
/*----- GSM Private methods -----*/

static int8_t _gsmWrite(const char *cmd, size_t count)
{
	size_t i;

	// NOTE: debugging should no be mixed to modem command and response to
	// avoid timeing issues and discrepancy between debug and release
	// versions
	gsmDebug("TX [%s]\n", cmd);

	// Purge any buffered data before sending a new command
	ser_purge(gsm);

	// Sending the AT command
	for (i=0; cmd[i]!='\0' && count; i++, count--) {
		kfile_putc(cmd[i], &(gsm->fd));
	}
	return i;
}

static int8_t _gsmWriteLine(const char *cmd)
{
	size_t i;

	// NOTE: debugging should no be mixed to modem command and response to
	// avoid timeing issues and discrepancy between debug and release
	// versions
	gsmDebug("TX [%s]\n", cmd);

	// Purge any buffered data before sending a new command
	ser_purge(gsm);
	// Clear error flags
	ser_setstatus(gsm, 0);

	// Sending the AT command
	for (i=0; cmd[i]!='\0'; i++) {
		kfile_putc(cmd[i], &(gsm->fd));
	}
	kfile_write(&(gsm->fd), "\r\n", 2);

	return i;
}

static int8_t _gsmRead(char *resp, uint8_t size)
{
	int8_t len = 0;

	ASSERT(resp);

	// Init response vector
	resp[0]='\0';

	// NOTE: kfile_gets returns also "empty" lines.
	// Loop to get a (not null) text lines (or EOF, e.g. timeout)
	do {
		len = kfile_gets(&(gsm->fd), (void*)resp , size);
		if (len==-1) {
			gsmDebug("RX FAILED\n");
			return len;
		}
	} while (!len);

	gsmDebug("RX [%s]\n", resp);
	return len;
}

static int8_t _gsmReadResult()
{
	char resp[8];
	uint8_t result;

	result = _gsmRead(resp, 8);
	if (result == 0) {
		result = 15; /* print a "?" */
		return result;
	}

	result = (uint8_t)resp[0] - '0';
	if (resp[0]=='O' && resp[1]=='K') {
		/* Bugfix for wrong commands that return OK
		 * instead of the numeric code '0' */
		result = 0;
	}

	_gsmPrintResult(result);
	return result;
}

/*----- GSM Interface -----*/

uint8_t gsmUpdateCREG(void)
{
	int8_t resp;
	char buff[16];

	_gsmWriteLine("AT+CREG?");
	resp = _gsmRead(buff, 16);
	if (resp == -1) {
		gsmConf.creg_n = 0;
		gsmConf.creg_stat = UNKNOW;
		return ERROR;
	}

	sscanf(buff, "+CREG: %hhu,%hhu",
			&gsmConf.creg_n,
			&gsmConf.creg_stat);

	return OK;

}

uint8_t gsmRegistered(void) {

	if (gsmUpdateCREG() != OK)
		return 0;

	switch(gsmCREG()) {
	case 1:
		// 1 registered, home network
	case 2:
		// 2 not registered, but MT is currently searching a new
		//   operator to register to
	case 5:
		// 5 registered, roaming
		return 1;
	default:
		// 0 not registered, MT is not currently searching a new
		//   operator to register to
		// 3 registration denied
		// 4 unknown
		return 0;
	}

	return 0;
}

int8_t gsmRegisterNetwork(void)
{
	int8_t resp = ERROR;

	resp = gsmUpdateCREG();
	if (resp != OK)
		return ERROR;

	if (gsmConf.creg_stat == REGISTERED ||
			gsmConf.creg_stat == ROAMING) {
		return OK;
	}

	return ERROR;
}


/*----- GSM SMS Interface -----*/

int8_t gsmSMSConf(uint8_t load)
{
	char buff[16];
	int8_t resp;

	if (load) {
		// Restore SMS setting (profile 0)
		_gsmWriteLine("AT+CRES=0");
		resp = _gsmRead(buff, 16);
		if (resp==-1) {
			gsmDebug("Fail, loading SMS settings (profile 0)\n");
			return ERROR;
		}
		return OK;
	}

	// Set text mode
	_gsmWriteLine("AT+CMGF=1");
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebug("Fail, set Text Mode\n");
		return ERROR;
	}

#warning DISABLED SOME SMS CONFIGURATION SETTING
return OK;

	// Select TE (GMS) Character Set
	_gsmWriteLine("AT+CSCS=\"GSM\"");
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
	_gsmWriteLine("AT+CNMI=3,0,0,0,1");
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
	_gsmWriteLine("AT+CSAS=0");
	resp = _gsmRead(buff, 16);
	if (resp==-1) {
		gsmDebug("Fail, save settings\n");
		return ERROR;
	}

	return OK;
}
int8_t gsmSMSSend(const char *number, const char *message)
{
	int8_t resp, i;
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
	_gsmWriteLine(buff);
	timer_delay(1000);
	//_gsmWrite("AT+CMGS=\"", 9);
	//_gsmWrite(number, strlen(number));
	//_gsmWriteLine("\", 145");

	// Wait for modem message prompt
	//_gsmRead(buff, 32);
	for (i=0; i<15; i++) {
		(*buff) = kfile_getc(&(gsm->fd));
		if ((*buff) == EOF)
			return -1;
		if ((*buff) == '>')
			break;
	}
	if (i == 15)
		return -1;

	// Sending message
	_gsmWriteLine(message);
	timer_delay(1000);

	// Sending terminator
	_gsmWriteLine("\x1a");

	// Waiting send confirmation
	_gsmRead(buff, 32);
	resp = _gsmReadResult();

	return resp;
}

gsmSMSMessage_t msg;
char buff[256];


/**
 * @return 1 if a valid message has been retrived, 0 on no valid message, -1
 * on parsing error.
 */
int8_t gsmSMSByIndex(gsmSMSMessage_t * msg, uint8_t index) {
	char c;
	uint8_t i;
	char buff[13];
	char *text;

	// SMS indexes are 1..10
	if (!index || index>10)
		return 0;

	// Get the SMS message by the specified index
	sprintf(buff, "AT+CMGR=%d", index);
	_gsmWriteLine(buff);
	// Example responce:
	// +CMGR: "REC READ","+393357963938","","10/12/14,22:59:15+04"<0D><0A>
	// $NUM+393473153808$NUM+3355763944$RES$MSG:PiazzaleLargo e Lungo, Milano, Italy$12345<0D>
	// <0D><0A>
	// <0D><0A>
	// 0<0D>

	//***** Reading message Type, record format:
	// +CMGR: <TYPE>
	// wherem TYPE is:
	// Type          Bytes  Description
	// "REC UNREAD"     12  Received unread messages
	// "REC READ"       10  Received read messages
	// "STO UNSENT"     12  Stored unsent messages
	// "STO SENT"       10  Stored sent messages
	// Minimum unique substring: 6 Bytes
	// Thus, to parse the actual type, we read:
	//     "+CMGR: " + 6Bytes = 13Bytes


	// Check if this message index is empty
	// In this case it is returned just "0<0D>"
	c = kfile_getc(&(gsm->fd));
	if (c == EOF)
		goto parse_error;
	if (c == '0') {
		msg->from[0] = 0;
		msg->time[0] = 0;
		msg->text[0] = 0;
		LOG_INFO("SMS, P: %d, EMPTY\n", index);
		return 0;
	}

	// Read the rest of the initial record identifier
	if (!kfile_read(&(gsm->fd), buff, 12))
		goto parse_error;

	// TODO: Parse message type
	//

	//***** Sender number
	// Scanning for second '"', than parse the sender number
	for (i=2; i; ) {
		c = kfile_getc(&(gsm->fd));
		if (c == EOF)
			goto parse_error;
		if (c=='"')
			i--;
	}
	// Save the sender number, till next '"'
	for (i=0; i<15; i++) {
		c = kfile_getc(&(gsm->fd));
		if (c == EOF)
			goto parse_error;
		if (c=='"')
			break;
		msg->from[i] = c;
	}
	msg->from[i] = '\0';

	//***** Timestamp parsing
	// Scanning for three '"', than parse the timestamp
	for (i=3; i; ) {
		c = kfile_getc(&(gsm->fd));
		if (c == EOF)
			goto parse_error;
		if (c=='"')
			i--;
	}
	// Save the timestamp (20 Bytes + terminator)
	if (!kfile_read(&(gsm->fd), msg->time, 20))
		goto parse_error;
	msg->time[20] = '\0';
	// Discard remaining chars till line termination ["<0D><0A>]
	if (!kfile_read(&(gsm->fd), buff, 3))
		goto parse_error;

	//***** Message parsing
	text = msg->text;
	(*text) = kfile_getc(&(gsm->fd));
	if ((*text) == EOF)
		goto parse_error;

	if ((*text) == '$') {
		// Scanning for first ':', than parse the message
		for (i=0; i<64; i++) {
			c = kfile_getc(&(gsm->fd));
			if (c == EOF)
				goto parse_error;
			if (c==':')
				break;
		}
		// Save the message
		if (_gsmRead(text, 160) == -1)
			goto parse_error;
	} else {
		// We are already at the beginning of the message,
		// save the remainder of the text
		if (_gsmRead(text+1, 159) == -1)
			goto parse_error;
	}

	LOG_INFO("SMS, P: %d, T: %s, N: %s, M: %s\n",
			index, msg->time, msg->from, msg->text);

	return 1;

parse_error:
	gsmDebug("Parse FAILED\n");
	return -1;
}

inline int8_t gsmSMSLast(gsmSMSMessage_t * msg) {
	return gsmSMSByIndex(msg, 1);
}

int8_t gsmSMSDel(uint8_t index)
{
	char buff[16];

	// Delete selected message
	if (!index || index>10)
		return OK;

	sprintf(buff, "AT+CMGD=%d,0", index);
	_gsmWriteLine(buff);
	if (_gsmRead(buff, 16) == -1) {
		LOG_ERR("Fails, delete SMS %d\n", index);
		return ERROR;
	}

	return OK;
}


int8_t gsmSMSDelRead(void)
{
	char buff[16];

	_gsmWriteLine("AT+CMGD=1,3");
	if (_gsmRead(buff, 16) == -1) {
		LOG_ERR("Fails, delete readed SMS\n");
		return ERROR;
	}

	return OK;
}



int8_t gsmSMSList(void)
{
	_gsmWriteLine("AT+CMGL=\"ALL\",1");
	do {
		if (_gsmRead(buff, 255) == -1) {
			gsmDebug("Fail, get SMS list\n");
			return ERROR;
		}
	} while (buff[0]!='0');

	return OK;
}

static void gsmSMSListTesting(void) {
	int8_t resp;

	// Read SMS Message Storage
	// => +CPMS: "SM",2,3
	// ==> <mem1>,<used1>,<total1>
	_gsmWriteLine("AT+CPMS?");
	resp = _gsmRead(buff, 255);
	if (resp==-1)
		return;

	_gsmWriteLine("AT+CNMI?");
	resp = _gsmRead(buff, 255);
	if (resp==-1)
		return;
}


/**
 * @return the SMS index if found, 0 otherwise.
 */
int8_t gsmGetNewMessage(gsmSMSMessage_t * msg) {
	uint8_t idx;

	LOG_INFO("Scanning for new SMS...\n");
	for (idx=1; idx<10; idx++) {
		if ( gsmSMSByIndex(msg, idx)>0 )
			break;
	}

	// Return the message index if we found a valid SMS
	if (idx<10)
		return idx;

	// No new SMS messages available.
	return 0;
}

#if CONFIG_GSM_TESTING
# warning GSM TESTING ENABLED
void NORETURN gsmTesting(Serial *port) {

	gsmInit(port);
	gsmPowerOn();
	gsmSMSConf(0);

	//gsmUpdateConf();
	//timer_delay(1000);

	//timer_delay(5000);
	//gsmSMSSend("+393473153808", "Test message from RFN");
	while (1) {
		gsmUpdateCSQ();
		timer_delay(5000);
		//gsmSMSSend("+393473153808", "Test message from RFN");
		//gsmSMSSend("+393357963938", "Test message from RFN");
		//timer_delay(10000);
	}

//  Prova invio messaggio molto lungo, che impiega tutti i 160 caratteri a
//	disposizione per un singolo SMS. Vediamo se i buffer disponibili sono
//	sufficienti.  Ciao


	for(uint8_t i=1; i<100; i++) {

		LOG_INFO("Iteration %d/100\n", i);

		gsmSMSList();
		timer_delay(1000);

		//gsmSMSLast(&msg);
		for (uint8_t j=1; j<10; j++) {
			gsmSMSByIndex(&msg,j);
			timer_delay(500);
		}

		gsmSMSDelRead();

		if (!(i%3))
			gsmSMSSend("+393357963938", "Test message from RFN");

		timer_delay(10000);
	}

	timer_delay(15000);
	gsmPowerOff();
}
#endif





















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


#if CONFIG_GSM_DEBUG
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
#if CONFIG_GSM_DEBUG
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

#if CONFIG_GSM_DEBUG
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

#if CONFIG_GSM_DEBUG
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
#if CONFIG_GSM_DEBUG
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

#if CONFIG_GSM_DEBUG
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

#if CONFIG_GSM_DEBUG
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

