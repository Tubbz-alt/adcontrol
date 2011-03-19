
#ifndef COMMANDS_H
#define COMMANDS_H

#include <io/kfile.h>

void command_init(void);
void command_parse(KFile *fd, const char *buf);

#endif /* end of include guard: COMMANDS_H */
