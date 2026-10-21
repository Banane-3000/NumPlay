#!/usr/bin/env python3
"""Compare every packed sprite with pre-optimisation RGB565 pixel digests."""
import hashlib
import io
import json
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
def main():
    elf=ELFFile(io.BytesIO((ROOT/'output/stickmanhook.nwa').read_bytes()))
    symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
    def data(name):
        s=symbols[name];section=elf.get_section(s['st_shndx']);off=s['st_value']
        return section.data()[off:off+s['st_size']]
    desc=data('sh_sprites');pixels=data('sh_pixels');palette=data('sh_palettes');digests=[]
    for i in range(0,len(desc),12):
        off,p,w,h,x,y=struct.unpack_from('<IHBBbb',desc,i)
        image=bytearray(struct.pack('<BBbb',w,h,x,y))
        for j in range(w*h):
            index=(pixels[off+j//2]>>(0 if j&1 else 4))&15
            image+=bytes([bool(index)])+(palette[(p+index)*2:(p+index+1)*2] if index else b'\0\0')
        digests.append(hashlib.sha256(image).hexdigest())
    assert digests==json.loads((ROOT/'tests/artwork-digests.json').read_text()),'Packed artwork changed'
    print(f'{len(digests)} sprite images remain pixel-identical after storage optimisation')
if __name__=='__main__':main()
