
#ifndef COMMANDS_H
#define COMMANDS_H

#include <io/kfile.h>

void command_init(void);
void command_parse(KFile *fd, const char *buf);

/** The buffer defined by the GSM module */
#define CMD_BUFFER_SIZE 161
extern char cmdBuff[CMD_BUFFER_SIZE];

#endif /* end of include guard: COMMANDS_H */
