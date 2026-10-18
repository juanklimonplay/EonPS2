/* ps2.c
 * Virtual pipes and select() for EonNav on the PlayStation 2 (see ps2glue.h).
 * This file is a part of EonNav, released under GPL (like Links).
 */

#include "cfg.h"

#ifdef EONNAV_PS2

#include "links.h"

#include <stdarg.h>

/* This file implements the wrappers, so it needs the real calls. */
#undef read
#undef write
#undef pipe
#undef close
#undef fcntl
#undef select

#ifdef EONNAV_GLUE_TEST
int ps2_net_up = 1;
#else
int ps2_net_up = 0;
#endif

/* Pipe ends get descriptor numbers from the top of the fd_set range, far above
 * the small numbers ps2sdk hands out for files and sockets. */
#define FAKE_FD_BASE	(FD_SETSIZE - 64)
#define FAKE_FD_COUNT	64
static unsigned char fake_fd_used[FAKE_FD_COUNT];

static int fake_fd_open(void)
{
	int i;
	for (i = 0; i < FAKE_FD_COUNT; i++)
		if (!fake_fd_used[i]) {
			fake_fd_used[i] = 1;
			return FAKE_FD_BASE + i;
		}
	errno = EMFILE;
	return -1;
}

static int fake_fd_close(int fd)
{
	if (fd >= FAKE_FD_BASE && fd < FAKE_FD_BASE + FAKE_FD_COUNT)
		fake_fd_used[fd - FAKE_FD_BASE] = 0;
	return 0;
}

/* Single-threaded: Links never blocks on a pipe (every end is non-blocking). */
static inline void pipe_lock(void) { }
static inline void pipe_unlock(void) { }
static inline void pipe_unlock_wait(void) { }
static inline void pipe_wake(void) { }

#define VIRTUAL_PIPE_SIZE	512
#define open(path, flags)	fake_fd_open()
#define close(fd)		fake_fd_close(fd)
#include "vpipe.inc"
#undef open
#undef close

int ps2_pipe(int fd[2])
{
	if (vpipe_create(fd))
		return -1;
	pipe_flags[fd[0]] |= VIRTUAL_PIPE_FLAG_NONBLOCK;
	pipe_flags[fd[1]] |= VIRTUAL_PIPE_FLAG_NONBLOCK;
	return 0;
}

int ps2_read(int fd, void *buf, size_t size)
{
	int r = vpipe_read(fd, buf, size);
	if (r != -2) return r;
	return read(fd, buf, size);
}

int ps2_write(int fd, const void *buf, size_t size)
{
	int r = vpipe_write(fd, buf, size);
	if (r != -2) return r;
	return write(fd, buf, size);
}

int ps2_close(int fd)
{
	int r = vpipe_close(fd);
	if (r != -2) return r;
	return close(fd);
}

int ps2_fcntl(int fd, int cmd, ...)
{
	va_list ap;
	long arg;
	if (fd < FD_SETSIZE && pipe_desc[fd])
		return 0;
	va_start(ap, cmd);
	arg = va_arg(ap, long);
	va_end(ap);
	return fcntl(fd, cmd, arg);
}

/* Ready pipe ends answer at once; otherwise real sockets go to the network
 * stack's select(), and with no sockets (or no network) we just sleep out the
 * timeout, which is what the timer-driven main loop asks for. */
int ps2_select(int n, fd_set *rs, fd_set *ws, fd_set *es, struct timeval *t)
{
	int i, ready = 0, sockets = 0;
	fd_set rr, ww, ee;

	FD_ZERO(&rr);
	FD_ZERO(&ww);
	FD_ZERO(&ee);
	for (i = 0; i < n; i++) {
		int rd = rs && FD_ISSET(i, rs);
		int wr = ws && FD_ISSET(i, ws);
		int ex = es && FD_ISSET(i, es);
		if (pipe_desc[i]) {
			if (rd && vpipe_may_read(i)) FD_SET(i, &rr), ready++;
			if (wr && vpipe_may_write(i)) FD_SET(i, &ww), ready++;
		} else if (rd || wr || ex) {
			sockets = 1;
		}
	}
	if (ready) {
		if (rs) *rs = rr;
		if (ws) *ws = ww;
		if (es) *es = ee;
		return ready;
	}
	if (sockets && ps2_net_up) {
		/* Pipe descriptors are all above the sockets; strip them for the real call. */
		for (i = FAKE_FD_BASE; i < n; i++) {
			if (rs) FD_CLR(i, rs);
			if (ws) FD_CLR(i, ws);
			if (es) FD_CLR(i, es);
		}
		return select(n < FAKE_FD_BASE ? n : FAKE_FD_BASE, rs, ws, es, t);
	}
	if (rs) FD_ZERO(rs);
	if (ws) FD_ZERO(ws);
	if (es) FD_ZERO(es);
	if (t) {
		unsigned long us = (unsigned long)t->tv_sec * 1000000UL + t->tv_usec;
		if (us > 50000) us = 50000;
		if (us) usleep(us);
	} else {
		usleep(20000);
	}
	return 0;
}

#endif
