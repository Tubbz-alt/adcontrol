
#include "command.h"
#include "control.h"
#include "eeprom.h"
#include "gsm.h"
#include "signals.h"

#include "cmd_ctor.h"  // MAKE_CMD, REGISTER_CMD
#include "verstag.h"

#include <drv/timer.h>

#include <avr/wdt.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h> // sprintf

// Define logging settingl (for cfg/log.h module).
#define LOG_LEVEL   LOG_LVL_INFO
#define LOG_FORMAT  LOG_FMT_TERSE
#include <cfg/log.h>

/** The buffer for command responces SMS formatting */
char cmdBuff[161];

/*
 * Commands.
 * TODO: Command declarations and definitions should be in another file(s).
 * Maybe we should use CMD_HUNK_TEMPLATE.
 */
MAKE_CMD(ver, "", "s",
({
	LOG_INFO("\n\nF/W Ver: %s\n\n", args[1].s);
	args[1].s = vers_tag;
	RC_OK;
}), 0);

/* Sleep. Example of declaring function body directly in macro call.  */
MAKE_CMD(sleep, "d", "",
({
	timer_delay((mtime_t)args[1].l);
	RC_OK;
}), 0)

/* Ping.  */
MAKE_CMD(ping, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	LOG_INFO("\n\nRFN - by Patrick Bellasi (derkling@gmail.com) - for RCT\n\n");
	RC_OK;
}), 0)

//----- CMD: PRINT HELP (console only)
MAKE_CMD(help, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	// FIXME provide an on-line help
	LOG_INFO("\n\nHelp: (To Be Done)\n\n");

	RC_OK;
}), 0)

/* Reset */
MAKE_CMD(rst, "", "",
({
	//Silence "args not used" warning.
	(void)args;

	LOG_INFO("\n\nReset in 2[s]...\n\n");
	wdt_enable(WDTO_2S);

	/*
	 * We want to have an infinite loop that lock access on watchdog timer.
	 * This piece of code it's equivalent to a while(true), but we have done this
	 * because gcc generate a warning message that suggest to use "noreturn"
	 * parameter in function reset.
	 */
	ASSERT(args);
	while(args);
	0;
}), 0)

//----- CMD: Test SMS commands from console
MAKE_CMD(test_sms, "t", "",
({
 	char *dst = args[1].s;
	char *msg = dst;

	// Lookup for DESTINATION end
	for ( ; *msg != ' ' && *msg; ++msg);
	if (*msg == '\0') {
		LOG_INFO("\n\n.:: Test SMS\nSyntax Error\n%s\n\n", args[1].s);
		RC_OK;
	}
	(*msg) = '\0';
	msg++;

	LOG_INFO("\n\n.:: Test SMS\nFrom: %s\nText: %s\n\n", dst, msg);
	smsSplitAndParse(dst, msg);

	RC_OK;
}), 0)

/*******************************************************************************
 * Configuration Commands
 ******************************************************************************/

//----- CMD: NUMBER ADD
MAKE_CMD(an, "ds", "",
({
	LOG_INFO("\n\n<= Aggiungi Numero %ld) %s)\r\n\n",
		args[1].l, args[2].s);
	ee_setSmsDest(args[1].l, args[2].s);
	RC_OK;
}), 0)

//----- CMD: NUMBER DEL
MAKE_CMD(cn, "d", "",
({
	LOG_INFO("\n\n<= Rimuovi numero %ld)\r\n\n",
		args[1].l);
	ee_setSmsDest(args[1].l, EMPTY_NUMBER);
	RC_OK;
}), 0)
;

//----- CMD: NUMBER SHOW
MAKE_CMD(vn, "", "s",
({
	char buff[MAX_SMS_NUM];
	uint8_t len = 0;

	sprintf(cmdBuff, "Destinatari SMS: ");
	for (uint8_t i=1; i<=MAX_SMS_DEST; i++) {
		ee_getSmsDest(i, buff, MAX_SMS_NUM);

		len = strlen(cmdBuff);
		sprintf(cmdBuff+len, "\n%d) %s;", i, buff);

		timer_delay(5);
	}

	LOG_INFO("\n\n%s\r\n\n", cmdBuff);

	args[1].s = cmdBuff;

	RC_OK;
}), 0)
;

//----- CMD: MESSAGE SET
MAKE_CMD(ii, "t", "",
({
	LOG_INFO("\n\n<= Imposta Identificazione: %s\r\n\n",
		args[1].s);
	ee_setSmsText(args[1].s);
	RC_OK;
}), 0)
;

//----- CMD: MESSAGE GET
MAKE_CMD(li, "", "s",
({
	char buff[MAX_MSG_TEXT];

	ee_getSmsText(buff, MAX_MSG_TEXT);
	sprintf(cmdBuff, "Identificazione: %s ", buff);
	LOG_INFO("\n\n=> %s\r\n\n", cmdBuff);

	args[1].s = cmdBuff;
	RC_OK;
}), 0)
;

static uint8_t parseChannelNumber(char const *buff);
static uint8_t parseChannelNumber(char const *buff) {
	char *c = buff;
	uint8_t ch = 0;

	for ( ; (*c != ' ' && *c != ';' && *c); ++c) {
		// Abort on un-expected input
		if ((*c < '0') || (*c > '9')) {
			return 0;
		}
		// Otherwise: update the current channel
		ch *= 10;
		ch += (*c-'0');
	}

	return ch;
}


/**
 * @brief Return the channel mask corresponding to the input string
 *
 * Inout string have this format: "([0-9][0-6]? )+", which essentially
 * correspond to a list of channels.
 * If the first number is 0, the rest of the string could be discarded and
 * 0xFFFF (all channels) is returned. Otherwise, a bitmask is returned where a
 * 1b is set for each channel in the list.
 *
 */
static uint16_t getChannelsMask(char const *buff) {
	uint16_t mask = 0x0000;
	char const *c = buff;
	int ch;

	//LOG_INFO("getChannelsMaks(%s)\n", buff);

	// Get next token
	while (*c) {
	
		//LOG_INFO(">%c<\t", *c);

		ch = 0;
		for ( ; (*c != ' ' && *c != ';' && *c); ++c) {
				// Abort on un-expected input
				if ((*c < '0') || (*c > '9')) {
						//LOG_ERR("Invalid parameter");
						return 0x0000;
				}
				// Otherwise: update the current channel
				ch *= 10;
				ch += (*c-'0');
		}

		// CH=0 => all channels are enabled
		if (ch == 0) {
			//LOG_WARN("Selecting all channels\n");
			return 0xFFFF;
		}

		// Otherwise: update the mask
		mask |= BV16(ch-1);
		LOG_INFO("ch=%d, mask: 0x%04X\r\n", ch, mask);

		// Go on with next channel
		if (*c)
			c++;
	}		

	return mask;
}


//----- CMD: ADD ENABLED CHANNELS
MAKE_CMD(aa, "t", "",
({
	uint16_t eCh, nCh;

	LOG_INFO("\n\n<= Aggiungi abilitati [%s]\r\n\n", args[1].s);

	nCh = getChannelsMask(args[1].s);
	if (nCh) {
		eCh = ee_getEnabledChMask();
		eCh |= nCh;
		ee_setEnabledChMask(eCh);
		controlSetEnabled(eCh);

		LOG_INFO(" (0x%04X, 0x%04X)\n\n", nCh, eCh);
	}

	RC_OK;
}), 0)
;

//----- CMD: REMOVE ENABLED CHANNELS
MAKE_CMD(ra, "t", "",
({
	uint16_t eCh, nCh;

	LOG_INFO("\n\n<= Rimuovi abilitati [%s]\r\n\n", args[1].s);

	nCh = getChannelsMask(args[1].s);
	if (nCh) {
		eCh = ee_getEnabledChMask();
		eCh &= ~nCh;
		ee_setEnabledChMask(eCh);
		controlSetEnabled(eCh);

		LOG_INFO(" (0x%04X, 0x%04X)\n\n", nCh, eCh);
	}

	RC_OK;
}), 0)
;

//----- CMD: ADD CRITICAL CHANNELS
MAKE_CMD(ac, "t", "",
({
	uint16_t cCh, nCh;

	LOG_INFO("\n\n<= Aggiungi critici [%s]\r\n\n", args[1].s);

	nCh = getChannelsMask(args[1].s);
	if (nCh) {
		cCh = ee_getCriticalChMask();
		cCh |= nCh;
		ee_setCriticalChMask(cCh);
		controlSetCritical(cCh);

		LOG_INFO(" (0x%04X, 0x%04X)\n\n", nCh, cCh);
	}

	RC_OK;
}), 0)
;

//----- CMD: REMOVE CRITICAL CHANNELS
MAKE_CMD(rc, "t", "",
({
	uint16_t cCh, nCh;

	LOG_INFO("\n\n<= Rimuovi critici [%s]\r\n\n", args[1].s);

	nCh = getChannelsMask(args[1].s);
	if (nCh) {
		cCh = ee_getCriticalChMask();
		cCh &= ~nCh;
		ee_setCriticalChMask(cCh);
		controlSetCritical(cCh);

		LOG_INFO(" (0x%04X, 0x%04X)\n\n", nCh, cCh);
	}

	RC_OK;
}), 0)
;

/*******************************************************************************
 * Control Commands
 ******************************************************************************/

//----- CMD: MODE CALIBRATION
MAKE_CMD(fc, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	LOG_INFO("\n\n<= Calibrazione forzata\n\n");
	controlCalibration();
	RC_OK;
}), 0)
;

//----- CMD: ENABLE MONITORING
MAKE_CMD(am, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	LOG_INFO("\n\n<= Monitoraggio abilitato\n\n");
	controlEnableMonitoring();
	RC_OK;
}), 0)
;

//----- CMD: DISABLE MONITORING
MAKE_CMD(dm, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	LOG_INFO("\n\n<= Monitoraggio disabilitato\n\n");
	controlDisableMonitoring();
	RC_OK;
}), 0)
;

//----- CMD: DISABLE MONITORING
MAKE_CMD(fl, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	LOG_INFO("\n\n<= Lampeggio Forzato\n\n");
	controlNotifySpoiled();
	RC_OK;
}), 0)
;

//----- CMD: SHOW CHANNEL STATUS
MAKE_CMD(sc, "s", "s",
({
	uint8_t ch;
	int len = 0;

	LOG_INFO("\n\n<= Stato canale [%s]\r\n\n", args[1].s);

	ch = parseChannelNumber(args[1].s);
	if (ch == 0)
		return RC_ERROR;
#if 0
	len += sprintf(cmdBuff+len, "\r\nStato CH%s(%d):",
			isCritical(ch) ? " CRITICO" : "", ch+1);
	len += sprintf(cmdBuff+len, "\r\nImax: %08ld, Irms: %08ld",
		chData[ch].Imax, chData[ch].Irms);

	LOG_INFO("\n\n##### Report Stato CH #######\n"
			"%s\n"
			"#############################\n\n",
			cmdBuff);
	args[1].s = cmdBuff;
#endif

	RC_OK;
}), 0)
;

//----- CMD: STATUS
MAKE_CMD(rs, "", "s",
({
	uint8_t csq = gsmCSQ();
	volatile uint16_t mask;
	uint8_t pos = 0;
	int len = 0;

	len += sprintf(cmdBuff+len, "STATO ");
	if (controlCriticalSpoiled()) {
		len += sprintf(cmdBuff+len, "LAMP");
	} else if (controlGetSpoiledMask() ||
				signal_status(SIGNAL_UNIT_IRQ)) {
		len += sprintf(cmdBuff+len, "GUAS");
	} else if (controlIsCalibrating()) {
		len += sprintf(cmdBuff+len, "CAL");
	} else if (controlMonitoringEnabled()) {
		len += sprintf(cmdBuff+len, "MON");
	} else {
		len += sprintf(cmdBuff+len, "DIS");
	}

	len += sprintf(cmdBuff+len, "\r\nCF");
	mask = controlGetSpoiledMask();
	if (!mask)
		len += sprintf(cmdBuff+len, " Nessuno");
	for (pos = 1; mask && pos <= 16; ++pos, mask>>=1) {
		//LOG_INFO("CF: 0x%04X\n", mask);
		if (mask & BV16(0)) {
			len += sprintf(cmdBuff+len, " %d", pos);
		}
	}

	len += sprintf(cmdBuff+len, "\r\nGSM %d (", csq);
	if (csq == 0 || csq == 99)
		len += sprintf(cmdBuff+len, "Scarso)");
	else if (csq<=4)
		len += sprintf(cmdBuff+len, "Basso)");
	else if (csq<=16)
		len += sprintf(cmdBuff+len, "Buono)");
	else
		len += sprintf(cmdBuff+len, "Ottimo)");


	len += sprintf(cmdBuff+len, "\r\nCA");
	mask = controlEnabled();
	if (!mask)
		len += sprintf(cmdBuff+len, " Nessuno");
	for (pos = 1; mask && pos<=16; ++pos, mask>>=1) {
		//LOG_INFO("CA: 0x%04X\n", mask);
		if (mask & BV16(0)) {
			len += sprintf(cmdBuff+len, " %d", pos);
		}
	}

	len += sprintf(cmdBuff+len, "\r\nCC");
	mask = controlCritical();
	if (!mask)
		len += sprintf(cmdBuff+len, " Nessuno");
	for (pos = 1; mask && pos <= 16; ++pos, mask>>=1) {
		//LOG_INFO("CC: 0x%04X\n", mask);
		if (mask & BV16(0)) {
			len += sprintf(cmdBuff+len, " %d", pos);
		}
	}

	LOG_INFO("\n\n##### Report Stato RFN #####\n"
			"%s\n"
			"#############################\n\n",
			cmdBuff);
	args[1].s = cmdBuff;

	RC_OK;
}), 0)
;


/* Register commands.  */
void command_init(void) {

//----- System commands
	REGISTER_CMD(ver);
	REGISTER_CMD(sleep);
	REGISTER_CMD(ping);
	REGISTER_CMD(help);

//----- Configuration commands
	REGISTER_CMD(an);
	REGISTER_CMD(cn);
	REGISTER_CMD(vn);
	REGISTER_CMD(ii);
	REGISTER_CMD(li);
	REGISTER_CMD(aa);
	REGISTER_CMD(ra);
	REGISTER_CMD(ac);
	REGISTER_CMD(rc);

//----- Control commands
	REGISTER_CMD(fc);
	REGISTER_CMD(am);
	REGISTER_CMD(dm);
	REGISTER_CMD(sc);
	REGISTER_CMD(rs);
	REGISTER_CMD(fl);
	REGISTER_CMD(rst);

//----- Test commands
	REGISTER_CMD(test_sms);
}

/**
 * Send a NAK asking the host to send the current message again.
 *
 * \a fd kfile handler for serial.
 * \a err  human-readable description of the error for debug purposes.
 */
INLINE void NAK(KFile *fd, const char *err) {
#ifdef _DEBUG
	kfile_printf(fd, "\nNAK \"%s\"\r\n", err);
#else
	kfile_printf(fd, "\nNAK\r\n");
#endif
}

/*
 * Print args on s, with format specified in t->result_fmt.
 * Return number of valid arguments or -1 in case of error.
 */
static bool command_reply(KFile *fd, const struct CmdTemplate *t,
			  const parms *args) {
	unsigned short offset = strlen(t->arg_fmt) + 1;
	unsigned short nres = strlen(t->result_fmt);

	for (unsigned short i = 0; i < nres; ++i) {
		if (t->result_fmt[i] == 'd') {
			kfile_printf(fd, " %ld", args[offset+i].l);
		} else if (t->result_fmt[i] == 's') {
			kfile_printf(fd, " %s", args[offset+i].s);
		} else {
			abort();
		}
	}
	kfile_printf(fd, "\r\n");
	return true;
}

void command_parse(KFile *fd, const char *buf) {

	const struct CmdTemplate *templ;
	parms args[PARSER_MAX_ARGS];

	/* Command check.  */
	templ = parser_get_cmd_template(buf);
	if (!templ) {
		kfile_print(fd, "\n-1 Invalid command.\r\n");
		return;
	}

	/* Args Check.  TODO: Handle different case. see doc/PROTOCOL .  */
	if (!parser_get_cmd_arguments(buf, templ, args)) {
		kfile_print(fd, "\n-2 Invalid arguments.\r\n");
		return;
	}

	/* Execute. */
	if(!parser_execute_cmd(templ, args)) {
		NAK(fd, "Error in executing command.");
	}

	//if (!command_reply(fd, templ, args)) {
	//	NAK(fd, "Invalid return format.");
	//}

	// Wait for console buffer to flush
	timer_delay(500);

	return;
}

