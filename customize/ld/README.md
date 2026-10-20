# GNU ld and objcopy, in the browser

`ld.wasm` and `objcopy.wasm` (with their `.js` loaders) are GNU ld and objcopy from
[binutils 2.43](https://ftp.gnu.org/gnu/binutils/binutils-2.43.tar.xz), unmodified, built for
`arm-none-eabi` as WebAssembly with emscripten 3.1.74 by [tools/ldwasm.sh](../../tools/ldwasm.sh).
The customizer website runs them to link a NumPlay of your own from the kit, the same way the
Makefile links NumPlay.nwa.

binutils is free software under the GNU General Public License, version 3 or later
([LICENSE](../../LICENSE), the same as NumPlay's); its source is at the link above.
