#!/bin/bash
# One-time Linux setup for the fast e2e runner (run from anywhere).
#  - GBDK 4.2.0 Linux (same version as ZGB.zip's Windows one) -> $GBDK (~/gbdk)
#  - ZGB engine sources from ZGB.zip -> ZGB_linux/ (gitignored)
#  - Builds ZGB's gbr2c/gbr2png/gbm2c from the sources in ZGB.zip, so sprite,
#    tile and map edits in res/ regenerate on Linux too. Only music (.uge) is
#    reused from a Windows build (uge2source has no source).
set -e
HERE=$(cd "$(dirname "$0")" && pwd); RG=$(cd "$HERE/../../.." && pwd); GBDK=${GBDK:-$HOME/gbdk}
if [ ! -x "$GBDK/bin/sdcc" ]; then
  curl -sSL -o /tmp/gbdk-linux64.tar.gz https://github.com/gbdk-2020/gbdk-2020/releases/download/4.2.0/gbdk-linux64.tar.gz
  tar xzf /tmp/gbdk-linux64.tar.gz -C "$(dirname "$GBDK")"
fi
if [ ! -d "$RG/ZGB_linux/ZGB/common" ]; then
  mkdir -p "$RG/ZGB_linux" && unzip -q "$RG/ZGB.zip" 'ZGB/common/*' -d "$RG/ZGB_linux"
fi
mkdir -p "$RG/ZGB_linux/ZGB/env" && ln -sfn "$GBDK" "$RG/ZGB_linux/ZGB/env/gbdk"
# .uge music needs uge2source (Windows-only, no source): reuse Debug/res/*.uge.c
# from a Windows build. Sprites, tiles and maps are regenerated here.
[ -d "$RG/Debug/res" ] || { echo "Debug/res missing: build once on Windows (build_Debug.bat) first"; exit 1; }
find "$RG/Debug/res" \( -name "*.uge.c" -o -name "*.gbm.c" \) -exec touch {} +   # music + maps (map rule needs PowerShell)
mkdir -p "$RG/Debug" && touch "$RG/Debug/Makefile.uptodate"
gcc -O2 -o "$HERE/rgrun" "$HERE/rgrun.c"
# ZGB asset converters, built from the sources inside ZGB.zip (Windows ships .exe only)
T="$RG/ZGB_linux/ZGB/tools"; mkdir -p "$HERE/bin"
unzip -q -o "$RG/ZGB.zip" 'ZGB/tools/gbr2c/*' 'ZGB/tools/gbr2png/*' 'ZGB/tools/gbm2c/*' -d "$RG/ZGB_linux"
cp "$T/gbr2c/gbrParser .cpp" "$T/gbr2c/gbrParser.cpp"
g++ -O2 -w -o "$HERE/bin/gbr2c"   "$T/gbr2c/gbr2c.cpp"     "$T/gbr2c/gbrParser.cpp" -I"$T/gbr2c"
g++ -O2 -w -o "$HERE/bin/gbr2png" "$T/gbr2png/gbr2png.cpp" "$T/gbr2c/gbrParser.cpp" "$T/gbr2png/lodepng/lodepng.cpp" -I"$T/gbr2c" -I"$T/gbr2png"
g++ -O2 -w -o "$HERE/bin/gbm2c"   "$T/gbm2c/gbm2c.cpp"     "$T/gbr2c/gbrParser.cpp" -I"$T/gbr2c"
echo "ready: $HERE/e2e.sh <scenario>"
