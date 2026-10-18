# EonNav — navegador web para PS2

EonNav es el navegador **Links 2.29** (de código abierto, licencia GPL) adaptado a la PS2 slim:
se maneja con el joystick, usa la red de la consola y abre páginas con **HTTPS moderno**
(TLS 1.3, con la librería wolfSSL que trae el kit de PS2). Es una aplicación aparte de EonPS2:
se abre desde la pestaña **Aplicaciones**.

> **Estado: prototipo.** Compila para PS2 y fue probado en la PC con el mismo código
> (sin pantalla, con capturas). **Todavía no se probó en PCSX2 ni en una PS2 real.**

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
| `tests/` | Pruebas automáticas en PC: HTTP, HTTPS, rechazo de certificados falsos y captura de pantalla. |

Para compilar en Linux: `sh apps/eonnav/build.sh host` (necesita `libsdl2-dev libpng-dev
libwolfssl-dev`), y para PS2 `sh apps/eonnav/build.sh ps2` con el kit ps2dev instalado.

## Próximos pasos

1. Probarlo en PCSX2 (con la red del emulador activada) y en una PS2 real.
2. Teclado en pantalla manejado con el joystick, para escribir direcciones sin teclado USB.
3. Marcadores en castellano y página de inicio guardada en el pendrive.
4. Modo "liviano" a través de FrogFind para páginas que no entran en memoria.
5. El asistente IA usando la misma conexión HTTPS.

## Licencia

Links es © Mikuláš Patočka y colaboradores, GPL versión 2 o posterior. Los archivos de
`apps/eonnav/src` también son GPL. Por eso EonNav es un programa aparte y no se mezcla con el
código de OPL (que tiene otra licencia).
