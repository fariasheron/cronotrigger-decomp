#!/usr/bin/env python3
"""mksave.py <wram.bin> <out.srm> [slot] [poke addr=hexbytes ...]
Build a Chrono Trigger SRAM save (slot 0-2) from a 128 KB WRAM image, the
same way the game's save routine (FF:F9FB) does: the regions listed in the
table at FF:FA7B are packed back to back into the slot, then the slot's
checksum (16-bit add-with-carry over 0x500 words, FF:FBE7) and the E41B
magic are written. Pokes (WRAM addresses like 7E0100 or 7F0000) are applied
to the WRAM image first."""
import sys
REGIONS=[(0x7E2400,0x5B0),(0x7E2C23,0x33),(0x7E0400,0x10),(0x000100,0x10),(0x7F0000,0x200),(0x00027E,2),(0x000290,0x11),(0x7E2C7C,0x1E)]
def woff(a):
    b=a>>16; o=a&0xFFFF
    return (0x10000 if b==0x7F else 0)+o
def build(wram, sram=None, slot=0, pokes=()):
    w=bytearray(wram)
    for a,data in pokes:
        o=woff(a); w[o:o+len(data)]=data
    s=bytearray(sram) if sram else bytearray(0x2000)
    base=slot*0xA00
    blob=b''.join(bytes(w[woff(a):woff(a)+n]) for a,n in REGIONS)
    s[base:base+0xA00]=blob+bytes(s[base+len(blob):base+0xA00])
    acc=0; c=0
    for y in range(0x9FE,-1,-2):
        acc=acc+(s[base+y]|s[base+y+1]<<8)+c; c=acc>>16; acc&=0xFFFF
    s[0x1FF0+2*slot:0x1FF2+2*slot]=acc.to_bytes(2,'little')
    s[0x1FF8+2*slot:0x1FFA+2*slot]=(0xE41B).to_bytes(2,'little')
    s[0x1FE0]=slot
    return bytes(s)
if __name__=='__main__':
    wram=open(sys.argv[1],'rb').read(); out=sys.argv[2]
    slot=int(sys.argv[3]) if len(sys.argv)>3 else 0
    pokes=[]
    for p in sys.argv[4:]:
        a,h=p.split('='); pokes.append((int(a,16),bytes.fromhex(h)))
    open(out,'wb').write(build(wram,None,slot,pokes))
