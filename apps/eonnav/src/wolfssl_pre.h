/* wolfssl_pre.h
 * Force-included before every EonNav source when HTTPS comes from wolfSSL's
 * OpenSSL compatibility layer. wolfSSL's headers use "G" as a parameter name,
 * and Links #defines G (graphics mode), so they must be read before config.h.
 */
#ifndef EONNAV_WOLFSSL_PRE_H
#define EONNAV_WOLFSSL_PRE_H
#include <sys/types.h>
#include <sys/socket.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>

/* OpenSSL checks the server's certificate chain even in SSL_VERIFY_NONE mode and
 * Links reads the outcome with SSL_get_verify_result(). wolfSSL skips the check
 * entirely in that mode and always reports success, so every certificate would be
 * accepted. build.sh makes Links' SSL context use SSL_VERIFY_PEER with this
 * callback: wolfSSL checks and records the result, the handshake continues, and
 * Links decides as it does with OpenSSL (reject, or warn and ask). */
static inline int eonnav_verify_cb(int preverify_ok, WOLFSSL_X509_STORE_CTX *store)
{
	(void)preverify_ok;
	(void)store;
	return 1;
}

/* A server that closes the TCP connection after the body (HTTP/1.0 style, no TLS
 * close_notify) makes OpenSSL's SSL_read() return 0, which Links reads as the end
 * of the page. wolfSSL returns -1 with a "peer closed" error instead, which Links
 * would show as "Unknown error" after a complete page. Report it as 0 like OpenSSL. */
static inline int eonnav_ssl_read(WOLFSSL *ssl, void *buf, int len)
{
	int r = wolfSSL_read(ssl, buf, len);
	if (r < 0) {
		int e = wolfSSL_get_error(ssl, r);
		if (e == WOLFSSL_ERROR_ZERO_RETURN)
			return 0;
		if (e == WOLFSSL_ERROR_SYSCALL) {
			/* wolfSSL folds "peer closed" into SYSCALL; a peek that reads 0 bytes
			 * tells an orderly close apart from a real socket error. */
			char c;
			if (recv(wolfSSL_get_fd(ssl), &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0)
				return 0;
		}
	}
	return r;
}
#undef SSL_read
#define SSL_read	eonnav_ssl_read

/* Only defined by wolfSSL for OPENSSL_ALL builds. */
#ifndef SSL_get_cipher_bits
#define SSL_get_cipher_bits(s, np)	wolfSSL_CIPHER_get_bits(wolfSSL_get_current_cipher(s), (np))
#endif

/* The ps2sdk wolfSSL is built without SESSION_CERTS, so the peer chain is not kept.
 * Links uses it only to show the issuer names in the page info dialog; the
 * certificate itself is still verified (verify result + X509_check_host). */
#undef SSL_get_peer_cert_chain
#define SSL_get_peer_cert_chain(s)	((WOLF_STACK_OF(WOLFSSL_X509) *)NULL)
#endif
