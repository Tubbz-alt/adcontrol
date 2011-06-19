
#ifndef COMMANDS_H
#define COMMANDS_H

#include <io/kfile.h>

void command_init(void);
void command_parse(KFile *fd, const char *buf);

/** The buffer defined by the GSM module */
extern char cmdBuff[161];

#endif /* end of include guard: COMMANDS_H */
