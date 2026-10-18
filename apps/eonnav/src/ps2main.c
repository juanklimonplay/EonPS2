/* ps2main.c
 * EonNav start-up on the PlayStation 2. SDL2main (linked in) resets the IOP and
 * mounts the boot device; this brings the network up with DHCP, builds the
 * Spanish start page and starts Links in graphics mode with the SDL driver.
 * This file is a part of EonNav, released under GPL (like Links).
 */

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ps2_eeip_driver.h>

int links_main(int argc, char *argv[]);
extern int ps2_net_up;

#define NET_TIMEOUT_SECONDS	8

static char net_line[160];

static void network_up(void)
{
	eeip_network_config_t cfg;
	struct ip4_addr ip, nm, gw;
	int r;

	if (init_eeip_driver(true) != EEIP_INIT_STATUS_OK) {
		snprintf(net_line, sizeof net_line, "No se pudo iniciar la placa de red.");
		return;
	}
	eeip_network_config_default_dhcp(&cfg);
	cfg.timeout_seconds = NET_TIMEOUT_SECONDS;
	r = configure_eeip_network(&cfg);
	if (r == EEIP_NET_STATUS_LINK_TIMEOUT) {
		snprintf(net_line, sizeof net_line, "Sin cable de red: conect&aacute; la PS2 al router y volv&eacute; a abrir EonNav.");
		return;
	}
	if (r != EEIP_NET_STATUS_OK) {
		snprintf(net_line, sizeof net_line, "El router no respondi&oacute; (DHCP, error %d).", r);
		return;
	}
	ps2_net_up = 1;
	eeip_get_current_config(&ip, &nm, &gw);
	snprintf(net_line, sizeof net_line, "Conectado. IP de la PS2: %s", ip4addr_ntoa(&ip));
}

static const char start_page[] =
	"<html><head><title>EonNav</title></head><body bgcolor=\"#101820\" text=\"#e8e8e8\" link=\"#7fc8ff\" vlink=\"#b8a0ff\">"
	"<h1>EonNav</h1><p>Navegador para PS2 (prototipo).</p>"
	"<p><b>Red:</b> %s</p>"
	"<h3>Para empezar</h3><ul>"
	"<li><a href=\"https://html.duckduckgo.com/html/\">DuckDuckGo</a> &mdash; buscador (versi&oacute;n simple)</li>"
	"<li><a href=\"https://es.m.wikipedia.org/\">Wikipedia</a> &mdash; en espa&ntilde;ol, versi&oacute;n para celulares</li>"
	"<li><a href=\"http://frogfind.com/\">FrogFind</a> &mdash; buscador para computadoras viejas (p&aacute;ginas en modo texto)</li>"
	"<li><a href=\"http://68k.news/\">68k.news</a> &mdash; noticias en modo liviano</li>"
	"<li><a href=\"http://info.cern.ch/\">info.cern.ch</a> &mdash; la primera p&aacute;gina web de la historia</li>"
	"</ul>"
	"<h3>Controles</h3><table border=\"1\" cellpadding=\"3\">"
	"<tr><td>Palanca izquierda</td><td>Mover el puntero</td></tr>"
	"<tr><td>Palanca derecha</td><td>Subir / bajar la p&aacute;gina</td></tr>"
	"<tr><td>X</td><td>Clic</td></tr>"
	"<tr><td>C&iacute;rculo</td><td>Volver atr&aacute;s</td></tr>"
	"<tr><td>Tri&aacute;ngulo</td><td>Escribir una direcci&oacute;n (con teclado USB)</td></tr>"
	"<tr><td>Cuadrado</td><td>Men&uacute;</td></tr>"
	"<tr><td>Cruz direccional</td><td>Pasar de un enlace a otro</td></tr>"
	"<tr><td>L1 / R1</td><td>P&aacute;gina anterior / siguiente</td></tr>"
	"<tr><td>START</td><td>Aceptar (Enter)</td></tr>"
	"</table></body></html>";

/* Links opens the start page as a data: URL, so it needs no file path syntax. */
static char *data_url(const char *html)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t n = strlen(html);
	char *url = malloc(sizeof "data:text/html;charset=iso-8859-1," + n * 3);
	char *p;
	if (!url) return NULL;
	p = url + sprintf(url, "data:text/html;charset=iso-8859-1,");
	for (; *html; html++) {
		unsigned char c = (unsigned char)*html;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			*p++ = c;
		} else {
			*p++ = '%';
			*p++ = hex[c >> 4];
			*p++ = hex[c & 15];
		}
	}
	*p = 0;
	return url;
}

/* The ELF's own folder (e.g. mass:/APPS/EONNAV) keeps Links' settings. */
static void set_home(const char *argv0)
{
	static char home[256];
	char *slash;
	if (!argv0 || !*argv0 || strlen(argv0) >= sizeof home) return;
	strcpy(home, argv0);
	slash = strrchr(home, '/');
	if (!slash) slash = strrchr(home, ':') ? strrchr(home, ':') + 1 : NULL;
	if (!slash) return;
	*slash = 0;
	setenv("HOME", home, 1);
}

int main(int argc, char *argv[])
{
	char *page, *url;
	char *largv[10];

	set_home(argc > 0 ? argv[0] : NULL);
	network_up();
	page = malloc(sizeof start_page + sizeof net_line);
	if (!page) return 1;
	sprintf(page, start_page, net_line);
	url = data_url(page);
	free(page);
	if (!url) return 1;

	largv[0] = "eonnav";
	largv[1] = "-g";
	largv[2] = "-driver";
	largv[3] = "sdl";
	largv[4] = "-language";
	largv[5] = "Spanish";
	/* No system certificate store on the PS2: use the root certificates built into Links. */
	largv[6] = "-ssl.builtin-certificates";
	largv[7] = "1";
	largv[8] = url;
	largv[9] = NULL;
	return links_main(9, largv);
}
