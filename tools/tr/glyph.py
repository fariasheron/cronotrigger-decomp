# CT dialogue font: 12x12 2bpp. main table FF:2060+c*24 (8px), nibble table FF:3860+(c>>1)*24 (4px; even=hi nibble, odd=lo)
R=None
def load(rom):
    global R; R=rom
def offs(c):
    return 0x3F0000+((0x2060+c*24)&0xFFFF), 0x3F0000+((0x3860+(c>>1)*24)&0xFFFF)
def get(c,rom=None):
    r=rom or R; m,n=offs(c); g=[[0]*12 for _ in range(12)]
    for y in range(12):
        for p in range(2):
            mb=r[m+2*y+p]; nb=r[n+2*y+p]; nb=(nb<<4)&0xF0 if c&1 else nb&0xF0
            for x in range(8): g[y][x]|=((mb>>(7-x))&1)<<p
            for x in range(4): g[y][8+x]|=((nb>>(7-x))&1)<<p
    return g
def put(c,g,rom):
    m,n=offs(c)
    for y in range(12):
        for p in range(2):
            mb=0; nb=0
            for x in range(8): mb|=((g[y][x]>>p)&1)<<(7-x)
            for x in range(4): nb|=((g[y][8+x]>>p)&1)<<(7-x)
            rom[m+2*y+p]=mb
            if c&1: rom[n+2*y+p]=(rom[n+2*y+p]&0xF0)|(nb>>4)
            else: rom[n+2*y+p]=(rom[n+2*y+p]&0x0F)|nb
