/* ps2glue.h
 * EonNav on the PlayStation 2: Links expects Unix pipes, and select() that waits on
 * them. ps2sdk has neither, so pipes become in-memory "virtual pipes" (Links' own
 * vpipe.inc, as in its DOS port) and select() is wrapped to understand them.
 * Included from os_depx.h when EONNAV_PS2 is defined.
 * This file is a part of EonNav, released under GPL (like Links).
 */

#ifndef EONNAV_PS2GLUE_H
#define EONNAV_PS2GLUE_H

/* No fork and no threads for name lookups: resolve synchronously. */
#ifndef NO_ASYNC_LOOKUP
#define NO_ASYNC_LOOKUP
#endif

int ps2_read(int fd, void *buf, size_t size);
int ps2_write(int fd, const void *buf, size_t size);
int ps2_pipe(int fd[2]);
int ps2_close(int fd);
int ps2_fcntl(int fd, int cmd, ...);
int ps2_select(int n, fd_set *rs, fd_set *ws, fd_set *es, struct timeval *t);

/* Set by ps2main.c once the network stack is up; select() only reaches it then. */
extern int ps2_net_up;

#define read ps2_read
#define write ps2_write
#define pipe ps2_pipe
#define close ps2_close
#define fcntl ps2_fcntl
#define select ps2_select

#endif
