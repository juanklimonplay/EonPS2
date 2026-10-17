#!/bin/sh
# EonPS2 stage 1: package an already-built ELF into an installable bundle.
#
# A trimmed version of the "Build RIPTOPL release package" step in rolling-release.yml, without
# the multi-flavour folders, release publishing or MEGA upload. Run from the repository root
# after `make release`:
#
#   sh .github/scripts/eonps2_package.sh <built.ELF> <flavour> <out-dir>
#
# Produces <out-dir>/<flavour>-bundle.zip (plus a copy of the ELF and the PSU) containing:
#   APPS/APP_RIPTOPL/RIPTOPL.ELF (+ icons, title.cfg), ART/, APP_RIPTOPL.psu,
#   POPS/ (POPStarter + BDMA pairs), EMBER/ (no BIOS), neutrino/ (pinned release),
#   and the upstream PC-tool .url shortcuts.
#
# NEUTRINO_TAG pins the bundled Neutrino release (upstream RiptOPL uses the newest pre-release,
# which moves; a pin keeps two builds of the same commit identical).
set -eu

ELF="$1"
FLAVOUR="$2"
OUT="$3"
NEUTRINO_TAG="${NEUTRINO_TAG:-v1.8.0}"
VERSION="${EONPS2_VERSION:-stage1}"

test -s "$ELF"
mkdir -p "$OUT"
PKG="$OUT/pkg"
rm -rf "$PKG"
mkdir -p "$PKG/POPS" "$PKG/EMBER"

python3 .github/scripts/package_release_app.py stage \
  --package "$PKG" --source-elf "$ELF" \
  --app-name APP_RIPTOPL --elf-name RIPTOPL.ELF \
  --title '[APP] EonPS2 (RiptOPL)' --pbt-title '[APP] EonPS2' \
  --version "$VERSION" --flavour "$FLAVOUR" \
  --channel EonPS2 --description 'Open PS2 Loader for PS2 and PS3'

# POPStarter: tracked files plus the extras the upstream release adds from the Example kit.
cp -rf POPS/. "$PKG/POPS/"
EXAMPLE=Example/APP_RIPTOPL-OFFICIALROLLING-VERSION.ZIP
for f in CHEATS.TXT IGR_BG.TM2 IGR_NO.TM2 IGR_YES.TM2 POPSTARTER.KELF TROJAN_7.BIN; do
  test -s "$EXAMPLE/POPS/$f"
  cp -f "$EXAMPLE/POPS/$f" "$PKG/POPS/$f"
done

# Ember: its licence must ship with it, and a PS1 BIOS must never be redistributed.
test -f EMBER/ember.elf
test -f EMBER/LICENSE-BETA.txt
cp -rf EMBER/. "$PKG/EMBER/"
mkdir -p "$PKG/EMBER/games/Per-Game-Folder"
cp -f "$EXAMPLE/EMBER/games/Per-Game-Folder/GAME FILES" "$PKG/EMBER/games/Per-Game-Folder/GAME FILES"
if [ -e "$PKG/EMBER/bios.bin" ]; then
  echo "ERROR: EMBER/bios.bin is present -- a PS1 BIOS must never be redistributed" >&2
  exit 1
fi

# Neutrino: extract the release archive so the folder copies straight to the device, then add
# the fork-private UDPFS config exactly as upstream does.
ARCHIVE="$OUT/neutrino_${NEUTRINO_TAG}.7z"
if [ ! -s "$ARCHIVE" ]; then
  curl -fsSL --retry 3 -o "$ARCHIVE" \
    "https://github.com/rickgaiser/neutrino/releases/download/${NEUTRINO_TAG}/neutrino_${NEUTRINO_TAG}.7z"
fi
SEVENZ="$(command -v 7z || command -v 7za || true)"
test -n "$SEVENZ"
"$SEVENZ" x -y -o"$PKG/neutrino" "$ARCHIVE" >/dev/null
test -s "$PKG/neutrino/neutrino.elf"
mkdir -p "$PKG/neutrino/config"
cp -f neutrino/bsd-udpfsbd.toml "$PKG/neutrino/config/bsd-udpfsbd.toml"

# PC companion-tool shortcuts (links only, no binaries); the package verifier expects them.
cp -f pc/PS2-Servers.url pc/OrbitPS2-Manager.url pc/OPL-PS1-AIO-Converter-GUI.url pc/PS2RD-CHT-Manager.url "$PKG/"
cp -f pc/udpfs-server.url "$PKG/android-udpfs-server.url"

cp -f "$ELF" "$OUT/$(basename "$ELF")"
cp -f "$PKG/APP_RIPTOPL.psu" "$OUT/APP_RIPTOPL.psu"
ZIP="${FLAVOUR}-bundle.zip"
rm -f "$OUT/$ZIP"
(
  cd "$PKG"
  zip -r -X "../$ZIP" APPS ART APP_RIPTOPL.psu POPS EMBER neutrino \
    PS2-Servers.url android-udpfs-server.url OrbitPS2-Manager.url \
    OPL-PS1-AIO-Converter-GUI.url PS2RD-CHT-Manager.url >/dev/null
)
python3 .github/scripts/package_release_app.py verify \
  --zip "$OUT/$ZIP" --source-elf "$ELF" \
  --app-name APP_RIPTOPL --elf-name RIPTOPL.ELF
rm -rf "$PKG"
echo "Built $OUT/$ZIP (Neutrino $NEUTRINO_TAG)"
