#!/bin/bash
HOME=/cc/test_gcc/gcc12_m/gcc

mkdir -p $HOME/gcc12_release_m  $HOME/build_release_m


cd $HOME/build_release_m
env CFLAGS="-O2" CXXFLAGS="-O2" ../configure --prefix=$HOME/gcc12_release_m --enable-shared --enable-threads=posix --enable-checking=release --with-system-zlib --enable-__cxa_atexit --disable-libunwind-exceptions --enable-gnu-unique-object --enable-linker-build-id --with-linker-hash-style=gnu --enable-languages=c,c++,lto --enable-plugin --enable-initfini-array --disable-libgcj --without-cloog --enable-gnu-indirect-function --build=aarch64-linux-gnu --with-stage1-ldflags=' -Wl,-z,relro,-z,now' --with-boot-ldflags=' -Wl,-z,relro,-z,now' --disable-bootstrap --with-multilib-list=lp64 --with-pkgversion=2022-3-21-kunpeng --disable-libsanitizer --disable-docs

make -j 8 2>&1 | tee log.log
make install
