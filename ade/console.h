#ifndef CONSOLE_H
#define CONSOLE_H

#include <io/kfile.h>

void console_init(KFile *fd);
void console_run(KFile *fd);

#endif /* end of include guard: CONSOLE_H */
