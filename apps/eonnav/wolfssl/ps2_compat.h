/* ps2_compat.h
 * Force-included into every wolfSSL source when building it for the PS2.
 * ps2sdk declares these functions in headers wolfSSL does not include on its own,
 * and has no inet_pton() (see ps2_compat.c). GCC 14+ makes the missing
 * declarations hard errors.
 */
#ifndef EONNAV_WOLFSSL_PS2_COMPAT_H
#define EONNAV_WOLFSSL_PS2_COMPAT_H
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>

int inet_pton(int af, const char *src, void *dst);
#endif
