#!/bin/bash
# Builds GNU ld and objcopy for arm-none-eabi as WebAssembly, for the customizer website
# (customize/ld): the website links a NumPlay of your own in the browser with them.
# binutils is unmodified; emscripten and binutils are downloaded to DIR (default ~/.numplay/ldwasm).
#
# Usage: tools/ldwasm.sh [DIR]
set -e
OUT="$(cd "$(dirname "$0")/.." && pwd)/customize/ld"
DIR="${1:-$HOME/.numplay/ldwasm}"
EMSDK=3.1.74 BINUTILS=2.43
mkdir -p "$DIR" && cd "$DIR"
[ -d emsdk ] || git clone --depth 1 https://github.com/emscripten-core/emsdk.git
[ -f emsdk/.done-$EMSDK ] || (cd emsdk && ./emsdk install $EMSDK && ./emsdk activate $EMSDK && touch .done-$EMSDK)
source emsdk/emsdk_env.sh
[ -f binutils-$BINUTILS.tar.xz ] || curl -fsSLO https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS.tar.xz
[ -d binutils-$BINUTILS ] || tar xf binutils-$BINUTILS.tar.xz
rm -rf out && mkdir out && cd out
SFLAGS="-Os -sENVIRONMENT=web,worker,node -sALLOW_MEMORY_GROWTH -sMODULARIZE -sEXPORTED_RUNTIME_METHODS=['FS','callMain'] -sINVOKE_RUN=0 -sEXIT_RUNTIME=0"
# (libiberty's own psignal and strsignal clash with emscripten's)
export ac_cv_func_psignal=yes ac_cv_func_strsignal=yes
emconfigure ../binutils-$BINUTILS/configure --host=wasm32 --target=arm-none-eabi \
  --disable-nls --disable-largefile --disable-gdb --disable-gdbserver --disable-sim --disable-gprof \
  --disable-gprofng --disable-libctf --disable-plugins --disable-werror --enable-compressed-debug-sections=none \
  --without-zstd --without-system-zlib CFLAGS="$SFLAGS" CXXFLAGS="$SFLAGS" LDFLAGS="$SFLAGS"
emmake make -j8 MAKEINFO=true all-ld all-binutils
cp ld/ld-new "$OUT/ld.js" && cp ld/ld-new.wasm "$OUT/ld.wasm"
cp binutils/objcopy "$OUT/objcopy.js" && cp binutils/objcopy.wasm "$OUT/objcopy.wasm"
echo "ld and objcopy are in $OUT"
