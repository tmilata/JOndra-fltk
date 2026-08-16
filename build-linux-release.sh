#!/bin/sh
set -eu

# Pocet paralelnich uloh lze zmenit napr.:
#   JOBS=1 ./build-linux-release.sh
JOBS=${JOBS:-2}

# Vzdy buildime vzhledem k adresari, kde lezi tento skript,
# takze ho lze spustit i z jineho aktualniho adresare.
SCRIPT_DIR=`dirname "$0"`
cd "$SCRIPT_DIR"
ROOT_DIR=`pwd`
BUILD_DIR="$ROOT_DIR/build/release"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release "$ROOT_DIR"
make -j"$JOBS"

printf '\nHotovo: %s\n' "$BUILD_DIR/bin/Jondra"
