# EonPS2

**EonPS2** es nuestro propio sistema para las PS2 con chip Matrix. Lo armamos a partir de
**RiptOPL** (una versión mejorada y de código abierto del Open PS2 Loader, el famoso OPL). La idea
es tener algo parecido a BoliviaOS, pero con todo el código a la vista para poder arreglarlo y
mejorarlo nosotros.

> Etapa actual: **Etapa 1 – que compile bien**. Todavía no agregamos nada nuevo: el programa que se
> genera es RiptOPL tal cual, compilado por nosotros de forma prolija y repetible.

---

## ¿Qué hay en esta etapa?

- El código de RiptOPL sin cambios en el programa en sí.
- Un "robot" de GitHub (GitHub Actions) que, cada vez que se sube un cambio a cualquier rama,
  compila el programa solo y deja los archivos listos para bajar.
- Los "robots" de RiptOPL que publicaban versiones, subían cosas a MEGA o actualizaban la página
  web del proyecto original **quedaron desactivados** en esta copia (solo se pueden correr a mano,
  y no hace falta). Así nunca publicamos nada por error.

## Cómo bajar el programa ya compilado (lo más fácil)

1. Entrá a <https://github.com/juanklimonplay/EonPS2/actions/workflows/eonps2-build.yml>
   (tenés que estar logueado en GitHub).
2. Hacé clic en la ejecución más reciente que tenga el tilde verde ✅.
3. Bajá hasta abajo, a la sección **Artifacts**, y descargá **`EonPS2-build`**.
4. Es un `.zip` que adentro trae:
   - `RIPTOPL-v1.2.0-Beta-<número>-<commit>-EONPS2-PS2DEVPINNED.ELF` → el programa principal,
     solo (el nombre dice de qué versión y de qué commit salió).
   - `EONPS2-PS2DEVPINNED-bundle.zip` → el paquete completo para copiar al pendrive / SD:
     - `APPS/APP_RIPTOPL/RIPTOPL.ELF` (el programa, con su ícono)
     - `POPS/` (POPStarter, para juegos de PS1)
     - `EMBER/` (Ember, el otro emulador de PS1; **la BIOS de PS1 la tenés que poner vos**,
       no se puede repartir)
     - `neutrino/` (Neutrino, el otro "motor" para juegos de PS2)
     - `LNG/lang_Spanish.lng` (el menú en español, ver más abajo)
     - `ART/` y accesos directos a programas de PC
   - `APP_RIPTOPL.psu` → la misma app empaquetada para meter en una memory card con
     uLaunchELF/wLaunchELF.
   - `BUILD-MANIFEST.txt` → anota con qué herramientas se compiló (sirve si algo falla).

GitHub guarda estos archivos **30 días**. Si se vencieron, se puede volver a correr el robot
desde la misma página con el botón **Run workflow**.

### Probarlo en la PC

Abrí PCSX2 y cargá el `.ELF` con *Sistema → Ejecutar ELF*. En la consola real se usa igual que
cualquier OPL.

---

## Idioma: EonPS2 en español

El paquete trae el archivo de idioma **`LNG/lang_Spanish.lng`**, con **todos** los textos del menú
traducidos (los de OPL y también los que agrega RiptOPL: Neutrino, Ember, POPStarter, favoritos,
Coverflow, VMC por juego, trucos, red, etc.).

**Dónde va el archivo** (cualquiera de los dos sirve; OPL lo busca solo):

- En el **pendrive o la SD del MX4SIO**: carpeta `LNG` en la raíz → `mass0:/LNG/lang_Spanish.lng`.
  Es lo que ya trae el paquete: copiá la carpeta `LNG` tal cual al lado de `APPS`, `POPS`, etc.
  (Si en *Prefijo BDM* pusiste una subcarpeta, la carpeta `LNG` va adentro de esa subcarpeta.)
- En la **memory card**: carpeta `OPL` → `mc0:/OPL/lang_Spanish.lng` (o `mc1:`). Útil si querés el
  menú en español aunque no haya pendrive enchufado. Pesa unos 45 KB.

**Primer arranque en español:** si EonPS2 todavía no tiene configuración guardada (o la
configuración no dice ningún idioma) y encuentra `lang_Spanish.lng`, arranca directamente en
español. Si ya habías guardado otro idioma (por ejemplo una configuración vieja de OPL/RiptOPL con
*English*), se respeta lo que guardaste: cambialo a mano como se explica abajo. Con el archivo solo en
el pendrive, el menú pasa a español apenas se lee el USB (unos segundos después de arrancar); con
el archivo en la memory card, está en español desde el principio.

**Cómo cambiar el idioma a mano:**

1. En el menú, apretá **START** para abrir el menú de opciones.
2. Entrá a **Settings** (en español: **Configuración principal**). La misma opción también está en
   **Display Settings** (**Pantalla**).
3. En **Language** (**Idioma**) elegí **Spanish** con izquierda/derecha y confirmá con **OK**.
4. Volvé al menú y elegí **Save Changes** (**Guardar cambios**) para que lo recuerde la próxima vez.

Si en la lista solo aparece *English*, OPL no encontró el archivo: revisá que la carpeta se llame
`LNG` (en mayúsculas) y el archivo `lang_Spanish.lng`, y que el pendrive esté conectado.

No hace falta ninguna fuente (`font_Spanish.ttf`): la fuente que trae OPL adentro ya tiene la ñ,
los acentos y los signos ¿ ¡.

**De dónde salen las traducciones:** al compilar, el robot baja los idiomas oficiales de
[Open-PS2-Loader-lang](https://github.com/ps2homebrew/Open-PS2-Loader-lang) y les suma
`lng_fork/Spanish.yml`, que completa lo que falta. Si un texto queda mal o largo, se corrige en
ese archivo (el texto original en inglés está en `lng_tmpl/_base.yml`).

---

## Cómo compilarlo uno mismo

Tu PC no tiene Docker ni WSL, así que **lo normal es usar el robot de GitHub** (arriba). Esto es
para quien tenga Linux o para el futuro.

### Opción A: con Docker (igual que el robot)

```sh
docker run --rm -it -v "$PWD":/src -w /src \
  ghcr.io/ps2dev/ps2dev@sha256:8fba50ecc2229acd7f8da63d34302f12939b7d4fa6848dda1e6a0ce083321a11 sh
# adentro del contenedor:
apk add bash make git zip unzip python3 py3-yaml gawk curl 7zip
git config --global --add safe.directory /src
sh .github/scripts/install_coherent_mmce.sh
make clean && make LOCALVERSION=EONPS2-LOCAL release
sh .github/scripts/eonps2_package.sh RIPTOPL-*-EONPS2-LOCAL.ELF EONPS2-LOCAL out
```

### Opción B: Linux sin Docker (con el paquete ya armado de ps2dev)

```sh
curl -LO https://github.com/ps2dev/ps2dev/releases/download/latest/ps2dev-ubuntu-latest.tar.gz
sudo tar xzf ps2dev-ubuntu-latest.tar.gz -C /usr/local      # queda en /usr/local/ps2dev
# ps2-packer busca sus archivos en la ruta donde se fabricó el paquete:
sudo mkdir -p /__w/ps2dev/ps2dev && sudo ln -sfn /usr/local/ps2dev /__w/ps2dev/ps2dev/ps2dev
export PS2DEV=/usr/local/ps2dev PS2SDK=/usr/local/ps2dev/ps2sdk GSKIT=/usr/local/ps2dev/gsKit
export PATH=$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH
sudo apt install make git zip unzip python3 python3-yaml p7zip-full
sh .github/scripts/install_coherent_mmce.sh
make clean && make LOCALVERSION=EONPS2-LOCAL release
```

Ojo: **no uses `make -j`** (compilar en paralelo); el proyecto descarga los idiomas al principio y
en paralelo se pisa. Compilado de a uno tarda más o menos un minuto y medio.

---

## Versiones de las herramientas

| Dónde | Herramientas | Detalle |
| --- | --- | --- |
| Robot de GitHub (oficial) | Imagen Docker `ghcr.io/ps2dev/ps2dev` fijada por *digest* `sha256:8fba50ec…1a11` | La misma que RiptOPL usa para su versión "PS2DEVPINNED" (fijada el 2026-08-13). Siempre compila igual. Trae GCC 15.2.0 para EE e IOP (queda anotado en `BUILD-MANIFEST.txt`). |
| Prueba en la nube (Etapa 1) | `ps2dev-ubuntu-latest.tar.gz` (armado el 2026-10-13, sha256 `b9cf90d0…4b79`) | GCC 15.2.0 para EE (`mips64r5900el-ps2-elf`) e IOP (`mipsel-none-elf`), binutils 2.45.1. |
| Driver mmceman | ps2-mmce `db3e93f0` + parche del proyecto | Lo arma `install_coherent_mmce.sh`, igual que upstream. |
| Neutrino incluido | `v1.8.0` | Fijado a propósito (RiptOPL baja "la última", que cambia sola). |

Resultados de la Etapa 1:

- Robot de GitHub (imagen fijada): ELF de **1.538.772 bytes**; paquete completo de 6,8 MB.
- Prueba en la nube (tarball, código de RiptOPL `eab6c37`): ELF de **1.539.156 bytes**.

La pequeña diferencia es normal: el número de versión y el commit quedan grabados dentro del ELF.

---

## Qué se cambió respecto de RiptOPL (Etapa 1)

- **Nuevo** `.github/workflows/eonps2-build.yml`: compila en cada *push* a cualquier rama y sube
  el artifact `EonPS2-build`.
- **Nuevo** `.github/scripts/eonps2_package.sh`: arma el paquete (versión recortada del paso de
  empaquetado de `rolling-release.yml`, sin publicar nada).
- **Desactivados** (solo quedan con disparo manual; los disparadores originales quedaron
  anotados como comentario al principio de cada archivo): `rolling-release.yml`,
  `ra-release.yml`, `release-normalize.yml`, `promote-rolling-latest.yml`, `docs-sync.yml`,
  `korium-sync.yml`, `neutrino-watch.yml`, `orbit-neutrino-watch.yml`, `ember-watch.yml`.
- Sin tocar: `flavours.yml` (las pruebas y compilaciones de RiptOPL; corren en cada *pull
  request*), `check-format.yml` y los de Korium.
- **El código del programa no se tocó.**

## Qué se cambió (español)

- `lng_fork/Spanish.yml`: 327 textos nuevos traducidos (antes quedaban en inglés). Con eso el
  español queda completo.
- `.github/scripts/eonps2_package.sh`: el paquete incluye `LNG/lang_Spanish.lng` (y el robot
  falla si no está). Va solo el español, a propósito: OPL muestra como máximo 15 idiomas y con
  todos juntos el español podía quedar afuera.
- `src/lang.c`, `src/opl.c`, `include/lang.h`: si no hay idioma guardado, arranca en español
  (cuando encuentra `lang_Spanish.lng`). Con un idioma guardado no cambia nada.

---

## Hoja de ruta (lo que viene)

**a) Mini cargador en la memory card.** Un arrancador chiquito (PS2BBL o parecido) en la memory
card, que el chip Matrix ejecuta al prender, y que salta enseguida al pendrive. Así la memory card
casi no se toca y todo lo pesado vive en el USB.

**b) Menú de arranque en el pendrive.** Al prender, un menú simple para elegir:
EonPS2/OPL, Neutrino, POPStarter (PS1) o las herramientas de wLaunchELF.

**c) Extras estilo BoliviaOS, rehechos con código abierto:**
- explorador de archivos,
- reproductor de MP3/OGG,
- editor de texto,
- formateo de memory cards tipo "Memory Card Annihilator".

**d) Ajustes que nos sirven a nosotros por defecto:**
- memorias virtuales (VMC) en el USB por defecto,
- MX4SIO como dispositivo por defecto,
- que recuerde los últimos trucos (cheats) usados por juego.

**e) Red (las PS2 slim traen la entrada de red de fábrica):**
- **EonNav, navegador web** — prototipo hecho (ver [`apps/eonnav/README.md`](apps/eonnav/README.md)):
  el navegador Links adaptado a la PS2, con HTTPS moderno, joystick y páginas en castellano.
  Falta probarlo en PCSX2 y en la consola.
- Jugar desde la PC por red (SMB / UDPBD, ya vienen en RiptOPL): falta una guía en castellano.
- Asistente IA conectándose directo desde la PS2, usando la misma conexión HTTPS de EonNav.

---

## Créditos y licencia

EonPS2 se basa en [RiptOPL](https://github.com/NathanNeurotic/Open-PS2-Loader), que a su vez se
basa en [Open PS2 Loader](https://github.com/ps2homebrew/Open-PS2-Loader). Licencia: Academic
Free License 3.0 (ver `LICENSE`). Neutrino, POPStarter y Ember son de sus respectivos autores;
Ember se incluye con su licencia (`EMBER/LICENSE-BETA.txt`).
