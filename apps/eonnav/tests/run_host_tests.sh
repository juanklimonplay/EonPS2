#!/bin/sh
# Host smoke tests for EonNav (no console needed). Needs the "host" and "glue" builds:
#   sh apps/eonnav/build.sh host && sh apps/eonnav/build.sh glue
#   sh apps/eonnav/tests/run_host_tests.sh <out-dir>
# Serves a test page over HTTP and HTTPS (self-signed) on localhost, then checks that:
#   - the PS2 pipe/select glue loads a page over HTTP,
#   - a self-signed certificate is rejected when certificates are enforced,
#   - an HTTPS page that ends with a plain TCP close loads without "Unknown error",
#   - the SDL driver renders a page headless (screenshot saved to <out-dir>).
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/../out"
OUT="${1:-$HERE/../out/tests}"
mkdir -p "$OUT"
T="$(mktemp -d)"
trap 'kill $HTTP_PID $HTTPS_PID 2>/dev/null || true; rm -rf "$T"' EXIT INT TERM

cp "$HERE/pagina.html" "$T/pagina.html"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/key.pem" -out "$T/cert.pem" -days 1 \
  -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost" >/dev/null 2>&1
python3 -I "$HERE/serve.py" http 18765 "$T" >"$T/http.log" 2>&1 &
HTTP_PID=$!
python3 -I "$HERE/serve.py" https 18443 "$T" >"$T/https.log" 2>&1 &
HTTPS_PID=$!
sleep 2

export HOME="$T/home"
mkdir -p "$HOME"
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY
FAIL=0
check() {
  if [ "$1" = ok ]; then echo "PASS: $2"; else echo "FAIL: $2"; FAIL=1; fi
}

R="$("$BIN/eonnav-glue" -dump http://127.0.0.1:18765/pagina.html 2>&1 || true)"
echo "$R" | grep -q "Prueba de EonNav" && check ok "glue build loads a page over HTTP" || check bad "glue build loads a page over HTTP"

R="$("$BIN/eonnav-host" -ssl.builtin-certificates 1 -ssl.certificates 2 -dump https://localhost:18443/pagina.html 2>&1 || true)"
echo "$R" | grep -q "Invalid certificate" && check ok "self-signed certificate is rejected" || check bad "self-signed certificate is rejected"

if R="$("$BIN/eonnav-host" -ssl.builtin-certificates 1 -ssl.certificates 0 -dump https://localhost:18443/pagina.html 2>&1)"; then RC=0; else RC=$?; fi
{ [ "$RC" = 0 ] && echo "$R" | grep -q "Prueba de EonNav" && ! echo "$R" | grep -q "Unknown error"; } \
  && check ok "HTTPS page with TCP close loads cleanly" || check bad "HTTPS page with TCP close loads cleanly (rc=$RC)"

SDL_VIDEODRIVER=offscreen EONNAV_SHOT="$OUT/pagina.ppm" EONNAV_SHOT_MS=3000 \
  timeout 60 "$BIN/eonnav-glue" -g -driver sdl -language Spanish http://127.0.0.1:18765/pagina.html || true
python3 -I "$HERE/ppm2png.py" "$OUT/pagina.ppm" "$OUT/pagina.png" && check ok "SDL driver renders a screenshot" || check bad "SDL driver renders a screenshot"

exit $FAIL
