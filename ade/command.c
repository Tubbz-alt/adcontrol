
#include "command.h"

#include "cmd_ctor.h"  // MAKE_CMD, REGISTER_CMD
#include "verstag.h"

#include <drv/timer.h>

#include <avr/wdt.h>

#include <stdlib.h>
#include <string.h>

/*
 * Commands.
 * TODO: Command declarations and definitions should be in another file(s).
 * Maybe we should use CMD_HUNK_TEMPLATE.
 */
MAKE_CMD(ver, "", "s",
({
	args[1].s = vers_tag;
	0;
}), 0);

/* Sleep. Example of declaring function body directly in macro call.  */
MAKE_CMD(sleep, "d", "",
({
	timer_delay((mtime_t)args[1].l);
	0;
}), 0)

/* Ping.  */
MAKE_CMD(ping, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	0;
}), 0)

/* Reset */
MAKE_CMD(reset, "", "",
({
	//Silence "args not used" warning.
	(void)args;
	wdt_enable(WDTO_2S);

	/*We want to have an infinite loop that lock access on watchdog timer.
	This piece of code it's equivalent to a while(true), but we have done this because
	gcc generate a warning message that suggest to use "noreturn" parameter in function reset.*/
	ASSERT(args);
	while(args);
	0;
}), 0)


/* Register commands.  */
void command_init(void) {
	REGISTER_CMD(ver);
	REGISTER_CMD(sleep);
	REGISTER_CMD(ping);
	REGISTER_CMD(reset);
}

/**
 * Send a NAK asking the host to send the current message again.
 *
 * \a fd kfile handler for serial.
 * \a err  human-readable description of the error for debug purposes.
 */
INLINE void NAK(KFile *fd, const char *err) {
#ifdef _DEBUG
	kfile_printf(fd, "NAK \"%s\"\r\n", err);
#else
	kfile_printf(fd, "NAK\r\n");
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

	/* Command check.  */
	templ = parser_get_cmd_template(buf);
	if (!templ) {
		kfile_print(fd, "-1 Invalid command.\r\n");
		return;
	}

	parms args[PARSER_MAX_ARGS];

	/* Args Check.  TODO: Handle different case. see doc/PROTOCOL .  */
	if (!parser_get_cmd_arguments(buf, templ, args)) {
		kfile_print(fd, "-2 Invalid arguments.\r\n");
		return;
	}

	/* Execute. */
	if(!parser_execute_cmd(templ, args)) {
		NAK(fd, "Error in executing command.");
	}
	if (!command_reply(fd, templ, args)) {
		NAK(fd, "Invalid return format.");
	}

	return;
}








#if 0

// Command Tempaltes
//ResultCode cmd_add_hunk(params argv[], params results[]) {
//   return cmd_add(argv[0].l, argv[1].l, &results[0].l);
//}
//
//const struct CmdTemplate cmd_add_template =  {
//   "add", "dd", "d", cmd_add_hunk
//};

//----- CMD: PRINT HELP (console only)
ResultCode cmd_help_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_help_template = {
	.name = "help",
	.arg_fmt = "",
	.result_fmt = "",
	.func = cmd_help_hunk
};

//----- CMD: NUMBER ADD
ResultCode cmd_number_add_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_number_add_template = {
	.name = "ADD",
	.arg_fmt = "ds",
	.result_fmt = "",
	.func = cmd_number_add_hunk
};

//----- CMD: NUMBER DEL
ResultCode cmd_number_del_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_number_del_template = {
	.name = "DEL",
	.arg_fmt = "d",
	.result_fmt = "",
	.func = cmd_number_del_hunk
};

//----- CMD: NUMBER SHOW
ResultCode cmd_number_show_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_number_show_template = {
	.name = "NUM",
	.arg_fmt = "",
	.result_fmt = "s",
	.func = cmd_number_show_hunk
};

//----- CMD: MESSAGE SET
ResultCode cmd_message_set_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_message_set_template = {
	.name = "MSG",
	.arg_fmt = "s",
	.result_fmt = "",
	.func = cmd_message_set_hunk
};

//----- CMD: RESET
ResultCode cmd_reset_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_reset_template = {
	.name = "RST",
	.arg_fmt = "",
	.result_fmt = "",
	.func = cmd_reset_hunk
};

//----- CMD: MODE CALIBRATION
ResultCode cmd_mode_calibration_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_mode_calibration_template = {
	.name = "CAL",
	.arg_fmt = "d",
	.result_fmt = "",
	.func = cmd_mode_calibration_hunk
};

//----- CMD: MODE MONITORING
ResultCode cmd_mode_monitoring_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_mode_monitoring_template = {
	.name = "GO",
	.arg_fmt = "",
	.result_fmt = "",
	.func = cmd_mode_monitoring_hunk
};

//----- CMD: STATUS
ResultCode cmd_status_hunk(parms args_results[]) {
	LOG_INFO("%sn", __func__);
	return RC_OK;
}
const struct CmdTemplate cmd_status_template = {
	.name = "STA",
	.arg_fmt = "",
	.result_fmt = "s",
	.func = cmd_status_hunk
};

void cmd_parser_init(void) {

	parser_init();

	parser_register_cmd(&cmd_help_template);
 	parser_register_cmd(&cmd_help_template);
 	parser_register_cmd(&cmd_number_add_template);
 	parser_register_cmd(&cmd_number_del_template);
 	parser_register_cmd(&cmd_number_show_template);
 	parser_register_cmd(&cmd_message_set_template);
 	parser_register_cmd(&cmd_reset_template);
 	parser_register_cmd(&cmd_mode_calibration_template);
 	parser_register_cmd(&cmd_mode_monitoring_template);
 	parser_register_cmd(&cmd_status_template);

}

#endif


