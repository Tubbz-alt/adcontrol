
#include "console.h"
#include "command.h"

#include <mware/parser.h>

// Define logging settingl (for cfg/log.h module).
#define LOG_LEVEL   LOG_LVL_INFO
#define LOG_FORMAT  LOG_FMT_TERSE
#include <cfg/log.h>

#define CONSOLE_BUFFER_SIZE 100

void console_run(KFile *fd) {
	static char linebuf[CONSOLE_BUFFER_SIZE];

	kfile_gets(fd, linebuf, CONSOLE_BUFFER_SIZE);
	kfile_clearerr(fd);

	if (linebuf[0]!='\0' && linebuf[0]!='#')
		command_parse(fd, linebuf);
}


/* Initialization: readline context, parser and register commands.  */
void console_init(KFile *fd) {
	(void)fd;
	parser_init();
	command_init();
}

