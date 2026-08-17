#!/bin/sh
set -eu

# Plne staticky 32bit Linux build pro Alpine/musl.
# FLTK a ALSA se ocekavaji v /opt/jondra-static.
JOBS=${JOBS:-2}
STATIC_PREFIX=${STATIC_PREFIX:-/opt/jondra-static}

SCRIPT_DIR=`dirname "$0"`
cd "$SCRIPT_DIR"
ROOT_DIR=`pwd`
BUILD_DIR="$ROOT_DIR/build/static"
BINARY="$BUILD_DIR/bin/Jondra"
DIST_DIR="$ROOT_DIR/dist/Linux32-static"

# Radeji hned hlasime chybejici zakladni staticke knihovny.
for f in \
    "$STATIC_PREFIX/lib/libfltk.a" \
    "$STATIC_PREFIX/lib/libfltk_images.a" \
    "$STATIC_PREFIX/lib/libasound.a"
do
    if [ ! -f "$f" ]; then
        echo "Chybi staticka knihovna: $f" >&2
        exit 1
    fi
done

# Staticky build delame vzdy nacisto, aby se nepouzila stara CMake cache
# nebo stare objektove soubory.
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="-Os -DNDEBUG -ffunction-sections -fdata-sections" \
    -DCMAKE_CXX_FLAGS_RELEASE="-Os -DNDEBUG -ffunction-sections -fdata-sections" \
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-Wl,--gc-sections" \
    -DJONDRA_STATIC=ON \
    -DJONDRA_STATIC_PREFIX="$STATIC_PREFIX" \
    "$ROOT_DIR"

make -j"$JOBS"

# Odstraneni symbolu a debug informaci z vysledne release binarky.
strip --strip-all "$BINARY"

# Stejne jako Windows post-build ulozime hotovou release binarku do dist.
mkdir -p "$DIST_DIR"
cp -f "$BINARY" "$DIST_DIR/Jondra"
chmod 755 "$DIST_DIR/Jondra"

printf '\nHotovo: %s\n' "$BINARY"
printf 'Distribuce: %s\n' "$DIST_DIR/Jondra"
ls -lh "$BINARY" || true
file "$BINARY" || true
ldd "$BINARY" || true
size "$BINARY" || true
