/* ps2_compat.c
 * inet_pton() for wolfSSL on the PS2: IPv4 only, which is all the PS2 network
 * stack carries. Used when a certificate names an IP address.
 */
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int inet_pton(int af, const char *src, void *dst)
{
	if (af != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}
	return inet_aton(src, (struct in_addr *)dst) ? 1 : 0;
}
