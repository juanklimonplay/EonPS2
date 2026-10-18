# EonNav — navegador web para PS2

EonNav es el navegador **Links 2.29** (de código abierto, licencia GPL) adaptado a la PS2 slim:
se maneja con el joystick, usa la red de la consola y abre páginas con **HTTPS moderno**
(TLS 1.3, con la librería wolfSSL que trae el kit de PS2). Es una aplicación aparte de EonPS2:
se abre desde la pestaña **Aplicaciones**.

> **Estado: prototipo.** Primera prueba en PCSX2 hecha el 18/10/2026 (ver abajo): arranca,
> muestra la página de inicio y conecta a la red. Encontró dos fallas, que ya están
> corregidas en esta versión pero **todavía no se volvieron a probar en PCSX2**.
> **Todavía no se probó en una PS2 real.**

## Prueba en PCSX2 (18/10/2026) y correcciones

Probado con PCSX2 2.8.2, BIOS SCPH-70004, red del emulador en modo Sockets, con el
artefacto del commit `04f53b6`.

| Qué | Resultado | Qué se hizo |
|---|---|---|
| Arranque y página de inicio en castellano | Anduvo | — |
| Red por DHCP (`IP de la PS2: 192.0.2.100`), DNS y conexión TCP al puerto 443 | Anduvo | — |
| HTTPS | **Falló**: "Error al inicializar los certificados incorporados: PEM_read_bio_X509, failed 10/126" | **Corregido** (ver 1) |
| Palanca izquierda (puntero) | **No se movió** | **Corregido** (ver 2) |
| Cruz abajo | Activó la barra de menú en vez de pasar al próximo enlace | Probablemente la misma causa que la palanca (ver 2) |
| Páginas HTTP, scroll, volver atrás | No se pudo probar (sin poder elegir enlaces) | Repetir la prueba |

**1. Certificados.** "failed 10/126" quiere decir que 10 de los 126 certificados raíz no se
cargaron (los otros 116 sí). Los 10 tienen número de serie 0: GoDaddy Class 2, GoDaddy G2,
Starfield Class 2, Starfield G2, Starfield Services G2, SECOM RootCA1 y RootCA2, y HARICA
2011, 2015 y ECC 2015. El wolfSSL 5.8.2 que trae el kit de PS2 los rechaza por una regla
estricta. La versión 5.6.6 que se usaba para las pruebas en PC no tiene esa regla, y por eso
las pruebas no lo vieron. Además, ese wolfSSL del kit calcula mal el tamaño de los enteros
de 64 bits en la PS2 (los toma de 4 bytes en vez de 8), lo que puede romper SHA-384/512.
Arreglo: `build.sh` ahora compila wolfSSL 5.8.2 desde el código fuente (commit fijo), con
las mismas opciones del kit (`wolfssl/options.h`) más dos cambios: aceptar serie 0 y los
tamaños correctos para PS2. Las pruebas de PC usan ese mismo wolfSSL. Ahora hay una prueba
que falla con "failed 10/126" si vuelve a pasar, igual que en PCSX2.

**2. Mando.** El SDL de PS2 nunca pone el DualShock 2 en modo analógico. En modo digital
igual informa las palancas con valores viejos, que pueden dejar el puntero pegado arriba a la
izquierda, justo sobre la barra de menú. Arreglo: EonNav le pide al mando el modo analógico
(como los juegos) e ignora las palancas mientras no esté en ese modo. El mapa de botones de
SDL coincide con el del mando y, en la PC, la cruz abajo pasa de un enlace al otro (probado
con capturas simulando el mando: `EONNAV_INPUT=down,down`). En PCSX2 falta confirmarlo.

**Para la próxima prueba en PCSX2:** abrir DuckDuckGo y Wikipedia (HTTPS), mover el puntero
con la palanca izquierda, la cruz abajo/arriba entre enlaces, X sobre un enlace, Círculo para
volver, y una página HTTP (info.cern.ch). Si el puntero sigue quieto, probar con el botón
ANALOG del mando (en PCSX2, el que tenga asignado).

## Qué anda y qué no

| Anda | No anda |
|---|---|
| Wikipedia (versión celular), noticias, foros, blogs | YouTube, Facebook, Instagram |
| DuckDuckGo en su versión simple (html.duckduckgo.com) | Google (exige JavaScript) |
| Imágenes PNG y GIF, tablas, tildes y eñes | Videos, sonido, JavaScript |
| HTTPS con verificación de certificados | Páginas muy pesadas (la PS2 tiene 32 MB) |

## Cómo se instala

1. Bajá el artifact **`EonNav-build`** de
   <https://github.com/juanklimonplay/EonPS2/actions/workflows/eonnav-build.yml>
   (la ejecución más nueva con tilde verde).
2. Adentro está `EONNAV-bundle.zip`. Copiá su carpeta `APPS/EONNAV` al pendrive o a la SD.
3. Conectá la PS2 al router con un cable de red y abrí **EonNav** desde Aplicaciones.

El archivo `LEEME.txt` que viene en el paquete explica los controles.

## Cómo está hecho (para quien siga programando)

| Archivo | Qué hace |
|---|---|
| `build.sh` | Baja Links 2.29 (con verificación sha256), le aplica los cambios y compila. `ps2` arma `EONNAV.ELF` y el paquete; `host` y `glue` arman la misma versión para Linux. |
| `src/sdl.c` | "Driver" de pantalla de Links con SDL2: dibuja en memoria y lo muestra; joystick, mouse y teclado USB. En Linux puede sacar capturas (`EONNAV_SHOT`). |
| `src/ps2.c`, `src/ps2glue.h` | Lo que Links espera de Linux y la PS2 no tiene: "pipes" virtuales y `select()`. Es la misma idea que usa Links en DOS. |
| `src/ps2main.c` | Arranque en PS2: red por DHCP, página de inicio en castellano, Links en modo gráfico. |
| `src/wolfssl_pre.h` | Adapta wolfSSL a Links: obliga a verificar certificados y maneja bien el fin de conexión. |
| `wolfssl/` | Opciones de wolfSSL (las mismas del kit de PS2) y `inet_pton()` para PS2. `build.sh` compila wolfSSL 5.8.2 con ellas. |
| `tests/` | Pruebas automáticas en PC: HTTP, HTTPS, rechazo de certificados falsos y captura de pantalla. |

Para compilar en Linux: `sh apps/eonnav/build.sh host` (necesita `libsdl2-dev libpng-dev`
y `git`), y para PS2 `sh apps/eonnav/build.sh ps2` con el kit ps2dev instalado.

## Próximos pasos

1. Repetir la prueba en PCSX2 con esta versión, y probarlo en una PS2 real.
2. Teclado en pantalla manejado con el joystick, para escribir direcciones sin teclado USB.
3. Marcadores en castellano y página de inicio guardada en el pendrive.
4. Modo "liviano" a través de FrogFind para páginas que no entran en memoria.
5. El asistente IA usando la misma conexión HTTPS.

## Licencia

Links es © Mikuláš Patočka y colaboradores, GPL versión 2 o posterior. wolfSSL es GPL
versión 3 o posterior, así que el programa armado (`EONNAV.ELF`) queda bajo GPL 3. Los
archivos de `apps/eonnav/src` y `apps/eonnav/wolfssl` también son GPL. Por eso EonNav es un programa aparte y no se mezcla con el
código de OPL (que tiene otra licencia).
