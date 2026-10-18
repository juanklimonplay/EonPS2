#!/bin/sh
# Build EonNav: the Links 2 browser with an SDL2 graphics driver (src/sdl.c).
#
#   sh apps/eonnav/build.sh ps2    -> apps/eonnav/out/EONNAV.ELF   (needs PS2DEV/PS2SDK)
#   sh apps/eonnav/build.sh host   -> apps/eonnav/out/eonnav-host  (needs libsdl2-dev, libpng-dev, libwolfssl-dev)
#   sh apps/eonnav/build.sh glue   -> apps/eonnav/out/eonnav-glue  (host build that uses the PS2 pipe/select glue)
#
# The host build is the same browser on Linux. With SDL_VIDEODRIVER=offscreen and
# EONNAV_SHOT=file.ppm it renders a page headless and saves a screenshot, so pages can be
# checked without a console or an emulator.
#
# HTTPS comes from wolfSSL's OpenSSL compatibility layer (the PS2 SDK ships wolfSSL with
# TLS 1.3). Links finds it through a generated "openssl" pkg-config file, and
# src/wolfssl_pre.h is force-included so wolfSSL's headers are read before Links' own.
set -eu

TARGET="${1:-ps2}"
HERE="$(cd "$(dirname "$0")" && pwd)"
LINKS_VER=2.29
LINKS_TAR="links2_${LINKS_VER}.orig.tar.bz2"
LINKS_SHA256=22aa96c0b38e1a6f8f7ed9d7a4167a47fc37246097759ef6059ecf8f9ead7998
# Ubuntu's copy of the upstream release tarball (byte-identical "orig" tarball).
LINKS_URL="http://archive.ubuntu.com/ubuntu/pool/universe/l/links2/${LINKS_TAR}"

WORK="$HERE/build/$TARGET"
OUT="$HERE/out"
mkdir -p "$HERE/build" "$OUT"

if [ ! -s "$HERE/build/$LINKS_TAR" ]; then
  curl -fsSL --retry 3 -o "$HERE/build/$LINKS_TAR" "$LINKS_URL"
fi
echo "$LINKS_SHA256  $HERE/build/$LINKS_TAR" | sha256sum -c -

rm -rf "$WORK"
mkdir -p "$WORK"
tar xjf "$HERE/build/$LINKS_TAR" -C "$WORK"
SRC="$WORK/links-$LINKS_VER"
cp "$HERE/src/sdl.c" "$SRC/sdl.c"

# 1. The SDL driver uses the shared virtual-device code, like the framebuffer drivers.
sed -i 's/^#if defined(GRDRV_SVGALIB) || defined(GRDRV_FB) || defined(GRDRV_GRX)$/& || defined(GRDRV_SDL)/' "$SRC/links.h"
grep -q 'defined(GRDRV_GRX) || defined(GRDRV_SDL)' "$SRC/links.h"
# 2. configure knows no SDL driver; let graphics mode through with none detected (we add ours).
sed -i 's/{ echo "configure: error: No graphics drivers found." 1>&2; exit 1; }/:/' "$SRC/configure"
# 3. No English "Welcome to Links" box: EonNav's start page explains the controls.
sed -i 's/^\tif (first_use) {$/\tif (0 \&\& first_use) {/' "$SRC/session.c"
grep -q 'if (0 && first_use) {' "$SRC/session.c"
# 4. wolfSSL only checks certificates in SSL_VERIFY_PEER mode (see src/wolfssl_pre.h).
sed -i 's/^\t\tSSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_COMPRESSION);$/&\n\t\tSSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, eonnav_verify_cb);/' "$SRC/https.c"
grep -q 'SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, eonnav_verify_cb);' "$SRC/https.c"
# 5. Build sdl.o with the rest.
sed -i 's/^links_OBJECTS = /links_OBJECTS = sdl.o /' "$SRC/Makefile.in"
# 6. PS2 only: virtual pipes + select (ps2.c), and our own main (ps2main.c) that brings the
#    network up and starts Links in graphics mode.
if [ "$TARGET" = ps2 ] || [ "$TARGET" = glue ]; then
  cp "$HERE/src/ps2.c" "$HERE/src/ps2glue.h" "$SRC/"
  sed -i '1i #ifdef EONNAV_PS2\n#include "ps2glue.h"\n#endif' "$SRC/os_depx.h"
  sed -i 's/^links_OBJECTS = /links_OBJECTS = ps2.o /' "$SRC/Makefile.in"
fi
if [ "$TARGET" = ps2 ]; then
  cp "$HERE/src/ps2main.c" "$SRC/"
  sed -i 's/^int main(int argc, char \*argv\[\])$/int links_main(int argc, char *argv[])/' "$SRC/main.c"
  grep -q '^int links_main(int argc, char \*argv\[\])$' "$SRC/main.c"
  sed -i 's/^links_OBJECTS = /links_OBJECTS = ps2main.o /' "$SRC/Makefile.in"
fi

# wolfSSL presented to configure as "openssl".
PCDIR="$WORK/pkgconfig"
mkdir -p "$PCDIR"
# The forced include is also seen by configure's preprocessor-only probes, which get no
# CFLAGS, so the PS2 entry carries every include path wolfssl_pre.h needs.
if [ "$TARGET" = ps2 ]; then
  WOLF_INC="$PS2SDK/ee/include -D_EE -I$PS2SDK/common/include -I$PS2SDK/ports/include -I$PS2SDK/ports/include/wolfssl"
else
  WOLF_INC=/usr/include/wolfssl
fi
printf 'Name: openssl\nDescription: wolfSSL OpenSSL compatibility layer\nVersion: 3.0.0\nCflags: -I%s -include %s\nLibs: -lwolfssl\n' \
  "$WOLF_INC" "$HERE/src/wolfssl_pre.h" > "$PCDIR/openssl.pc"

COMMON_CONF="--with-ssl --enable-graphics --without-x --without-fb --without-directfb --without-svgalib \
  --without-pmshell --without-atheos --without-gpm --without-freetype --without-bzip2 \
  --without-lzma --without-brotli --without-zstd --without-libtiff --without-librsvg \
  --without-libwebp --without-libavif --without-libjpeg --disable-javascript"

cd "$SRC"
case "$TARGET" in
  host|glue)
    GLUE=""
    [ "$TARGET" = glue ] && GLUE="-DEONNAV_PS2 -DEONNAV_GLUE_TEST"
    PKG_CONFIG_PATH="$PCDIR" \
    CFLAGS="-O2 -fpermissive $GLUE $(sdl2-config --cflags)" \
    LIBS="$(sdl2-config --libs)" \
    ./configure $COMMON_CONF > "$WORK/configure.log" 2>&1 || { tail -30 "$WORK/configure.log"; exit 1; }
    ;;
  ps2)
    : "${PS2SDK:?PS2SDK must be set}"
    P="$PS2SDK/ports"
    # lwIP (inside libps2_drivers) also exports mem_free; rename Links' allocator.
    # pkg-config must only see the PS2 ports, never the host libraries.
    PKG_CONFIG_LIBDIR="$PCDIR:$P/lib/pkgconfig" PKG_CONFIG_PATH= \
    CC=mips64r5900el-ps2-elf-gcc \
    CFLAGS="-O2 -G0 -D_EE -DPS2 -D__PS2__ -DEONNAV_PS2 -Dmem_free=links_mem_free -fpermissive -I$PS2SDK/ee/include -I$PS2SDK/common/include -I$P/include -I$P/include/SDL2" \
    LDFLAGS="-L$PS2SDK/ee/lib -L$P/lib -L$PS2DEV/gsKit/lib -T$PS2SDK/ee/startup/linkfile" \
    LIBS="-lSDL2main -lSDL2 -lgskit -ldmakit -lps2_drivers -lpatches -lps2ip -lnetman -lpad -laudsrv -lmtap -lpatches -lkernel -lz" \
    ./configure --host=mips64r5900el-ps2-elf $COMMON_CONF > "$WORK/configure.log" 2>&1 || { tail -30 "$WORK/configure.log"; exit 1; }
    ;;
  *)
    echo "usage: $0 ps2|host" >&2
    exit 2
    ;;
esac

printf '#define GRDRV_SDL 1\n' >> config.h
# configure's function probes miss wolfSSL's macro-mapped names; these exist in it.
printf '#define HAVE_OPENSSL_X509V3_H 1\n#define HAVE_X509_CHECK_HOST 1\n#define HAVE_SSL_GET1_SESSION 1\n#define HAVE_SSL_LOAD_ERROR_STRINGS 1\n#define HAVE_ASN1_STRING_TO_UTF8 1\n#define HAVE_X509_VERIFY_PARAM_SET_FLAGS 1\n' >> config.h
grep -q '^#define G 1' config.h
grep -q '^#define HAVE_SSL 1' config.h
grep -q '^#define HAVE_PNG_H 1' config.h || grep -q '^#define HAVE_LIBPNG_PNG_H 1' config.h

make -j"$(nproc 2>/dev/null || echo 2)" > "$WORK/make.log" 2>&1 || { grep -E 'error|undefined' "$WORK/make.log" | head -40; exit 1; }

case "$TARGET" in
  host) cp -f links "$OUT/eonnav-host" ;;
  glue) cp -f links "$OUT/eonnav-glue" ;;
  ps2)
    mips64r5900el-ps2-elf-strip -o "$OUT/EONNAV.ELF" links
    # Ready-to-copy kit: APPS/EONNAV shows up in EonPS2's Apps tab through title.cfg.
    PKG="$WORK/pkg"
    rm -rf "$PKG"
    mkdir -p "$PKG/APPS/EONNAV"
    cp "$OUT/EONNAV.ELF" "$PKG/APPS/EONNAV/EONNAV.ELF"
    printf 'title=EonNav (navegador)\nboot=EONNAV.ELF\n' > "$PKG/APPS/EONNAV/title.cfg"
    cp "$HERE/package/LEEME.txt" "$PKG/APPS/EONNAV/LEEME.txt"
    cp COPYING "$PKG/APPS/EONNAV/COPYING"
    rm -f "$OUT/EONNAV-bundle.zip"
    (cd "$PKG" && zip -r -X "$OUT/EONNAV-bundle.zip" APPS >/dev/null)
    ;;
esac
ls -l "$OUT"
