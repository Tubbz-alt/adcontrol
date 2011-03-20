
#include "console.h"
#include "command.h"

#include <mware/readline.h>
#include <mware/parser.h>

// Define logging settingl (for cfg/log.h module).
#define LOG_LEVEL   LOG_LVL_INFO
#define LOG_FORMAT  LOG_FMT_TERSE
#include <cfg/log.h>


// DEBUG: set to 1 to force interactive mode
#define FORCE_INTERACTIVE         0

/**
 * True if we are in interactive mode, false if we are in console mode.
 * In interactive mode, commands are read through readline() (prompt,
 * completion, history) without IDs, and replies/errors are sent to the serial
 * output.
 * In protocol mode, we implement the default protocol
 */
static bool interactive;

/* Readline context, used for interactive mode. */
static struct RLContext rl_ctx;

static void console_prompt(KFile *fd) {
	kfile_print(fd, "$ ");
}

static void console_batch(KFile *fd, char *linebuf) {

	if (linebuf[0]=='\0' || linebuf[0]=='#')
		return;

	if (linebuf[0] == 0x1B && linebuf[1] == 0x1B) {
		interactive = true;
		kfile_printf(fd, "Entering interactive mode\r\n");
		return;
	}

	command_parse(fd, linebuf);
}


void console_run(KFile *fd) {
	/**
	 * \todo to be removed, we could probably access the serial FIFO
	 * directly
	 */
	static char linebuf[80];
	const char *buf;

	if (!interactive) {
		kfile_gets(fd, linebuf, sizeof(linebuf));
		// reset serial port error anyway
		kfile_clearerr(fd);
		console_batch(fd, linebuf);
		console_prompt(fd);
		return;
	}

	/*
	 * Read a line from serial. We use a temporary buffer
	 * because otherwise we would have to extract a message
	 * from the port immediately: there might not be any
	 * available, and one might get free while we read
	 * the line. We also add a fake ID at the start to
	 * fool the parser.
	 */
	buf = rl_readline(&rl_ctx);

	/* If we enter lines beginning with sharp(#)
	   they are stripped out from commands */
	if(!buf || buf[0]=='\0' || buf[0]=='#')
		return;

	// exit special case to immediately change serial input
	if (!strcmp(buf, "exit") || !strcmp(buf, "quit")) {
		rl_clear_history(&rl_ctx);
		kfile_printf(fd, "Leaving interactive mode...\r\n");
		interactive = FORCE_INTERACTIVE;
		return;
	}

	//TODO: remove sequence numbers
	linebuf[0] = '0';
	linebuf[1] = ' ';
	strncpy(linebuf + 2, buf, sizeof(linebuf) - 3);
	linebuf[sizeof(linebuf) - 1] = '\0';

	command_parse(fd, linebuf);
	console_prompt(fd);
}



/* Initialization: readline context, parser and register commands.  */
void console_init(KFile *fd) {

	interactive = FORCE_INTERACTIVE;

	rl_init_ctx(&rl_ctx);
	//rl_setprompt(&rl_ctx, "> ");
	rl_sethook_get(&rl_ctx, (getc_hook)kfile_getc, fd);
	rl_sethook_put(&rl_ctx, (putc_hook)kfile_putc, fd);
	rl_sethook_match(&rl_ctx, parser_rl_match, NULL);
	rl_sethook_clear(&rl_ctx, (clear_hook)kfile_clearerr,fd);

	parser_init();
	command_init();

	console_prompt(fd);
}

