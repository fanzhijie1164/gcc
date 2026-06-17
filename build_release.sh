#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${TARGET:-aarch64-linux-gnu}"
BUILD="${BUILD:-$("$SRC_DIR/config.guess")}"
HOST="${HOST:-$BUILD}"
BUILD_DIR="${BUILD_DIR:-$SRC_DIR/build_cross_release_12}"
INSTALL_DIR="${INSTALL_DIR:-$SRC_DIR/gcc12_cross_release}"
TARGET_SYSROOT_DIR="${TARGET_SYSROOT_DIR:-/usr/$TARGET}"
# Ubuntu/Debian cross toolchains use / as the sysroot and store target
# headers/libraries under /usr/$TARGET.  Do not set SYSROOT to
# /usr/$TARGET, because the glibc linker scripts contain absolute paths
# such as /usr/aarch64-linux-gnu/lib/libc.so.6.
SYSROOT="${SYSROOT:-/}"
NATIVE_SYSTEM_HEADER_DIR="${NATIVE_SYSTEM_HEADER_DIR:-/usr/$TARGET/include}"
MAKE_JOBS="${MAKE_JOBS:-$(nproc)}"

export LC_ALL=C
unset LIBRARY_PATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset CPATH

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: missing required command: $1" >&2
    exit 1
  }
}

need_file() {
  test -f "$1" || {
    echo "error: missing required file: $1" >&2
    exit 1
  }
}

need_cmd make
need_cmd gcc
need_cmd g++
need_cmd "${TARGET}-as"
need_cmd "${TARGET}-ld"
need_cmd "${TARGET}-ar"
need_cmd "${TARGET}-ranlib"
need_cmd "${TARGET}-gcc"
need_cmd "${TARGET}-g++"

need_file "$TARGET_SYSROOT_DIR/include/stdio.h"
need_file "$TARGET_SYSROOT_DIR/lib/crti.o"
need_file "$TARGET_SYSROOT_DIR/lib/crtn.o"
need_file "$TARGET_SYSROOT_DIR/lib/libc.so.6"
need_file "$TARGET_SYSROOT_DIR/lib/libc_nonshared.a"
need_file "$TARGET_SYSROOT_DIR/lib/ld-linux-aarch64.so.1"

mkdir -p "$INSTALL_DIR" "$BUILD_DIR"

case "$BUILD_DIR" in
  "$SRC_DIR"/build_cross_release_12|"$SRC_DIR"/build_cross_release_12/*) ;;
  *)
    echo "error: refusing to clean unexpected BUILD_DIR: $BUILD_DIR" >&2
    echo "       set BUILD_DIR under $SRC_DIR/build_cross_release_12 or clean it manually" >&2
    exit 1
    ;;
esac

echo "source:  $SRC_DIR"
echo "build:   $BUILD_DIR"
echo "prefix:  $INSTALL_DIR"
echo "build:   $BUILD"
echo "host:    $HOST"
echo "target:  $TARGET"
echo "sysroot: $SYSROOT"
echo "target sysroot dir: $TARGET_SYSROOT_DIR"
echo "headers: $NATIVE_SYSTEM_HEADER_DIR"
echo "jobs:    $MAKE_JOBS"

rm -rf "$BUILD_DIR"/*
cd "$BUILD_DIR"

env CFLAGS="-O2" CXXFLAGS="-O2" \
  "$SRC_DIR/configure" \
  --prefix="$INSTALL_DIR" \
  --build="$BUILD" \
  --host="$HOST" \
  --target="$TARGET" \
  --with-sysroot="$SYSROOT" \
  --with-build-sysroot="$SYSROOT" \
  --with-native-system-header-dir="$NATIVE_SYSTEM_HEADER_DIR" \
  --with-libs="$TARGET_SYSROOT_DIR/lib" \
  --with-as="/usr/bin/${TARGET}-as" \
  --with-ld="/usr/bin/${TARGET}-ld" \
  --enable-languages=c,c++,lto \
  --disable-bootstrap \
  --disable-multilib \
  --enable-shared \
  --enable-threads=posix \
  --enable-checking=release \
  --with-system-zlib \
  --disable-analyzer \
  --disable-libsanitizer \
  --disable-libquadmath \
  --disable-libquadmath-support \
  --disable-libgomp \
  --disable-libvtv \
  --disable-libssp \
  --disable-docs \
  2>&1 | tee configure.log

make -j"$MAKE_JOBS" 2>&1 | tee build.log
make install 2>&1 | tee install.log

"$INSTALL_DIR/bin/${TARGET}-gcc" -v
