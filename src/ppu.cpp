#include "cpu.hpp"
#include "gui.hpp"
#include "ppu.hpp"

namespace PPU {
#include "palette.inc"

struct cartridge_mmc *MMC;

Mirroring mirroring;       // Mirroring mode.
u8 ciRam[0x800];           // VRAM for nametables.
u8 cgRam[0x20];            // VRAM for palettes.
u8 oamMem[0x100];          // VRAM for sprite properties.
Sprite oam[8], secOam[8];  // Sprite buffers.

Addr vAddr, tAddr;  // Loopy V, T.
u8 fX;              // Fine X.
u8 oamAddr;         // OAM address.

Ctrl ctrl;      // PPUCTRL   ($2000) register.
Mask mask;      // PPUMASK   ($2001) register.
Status status;  // PPUSTATUS ($2002) register.

static inline bool rendering() { return mask.bg || mask.spr; }
static inline int spr_height() { return ctrl.sprSz ? 16 : 8; }

/* Get CIRAM address according to mirroring */
u16 nt_mirror(u16 addr)
{
    switch (mirroring)
    {
        case VERTICAL:    return addr % 0x800;
        case HORIZONTAL:  return ((addr / 2) & 0x400) + (addr % 0x400);
        default:          return addr - 0x2000;
    }
}
void set_mirroring(Mirroring mode) { mirroring = mode; }

/* Access PPU memory */
u8 rd(u16 addr)
{
    switch (addr)
    {
        case 0x0000 ... 0x1FFF:  
            return MMC->chr_map[addr / CARTRIDGE_CHR_BANK_SIZE][addr % CARTRIDGE_CHR_BANK_SIZE];  // CHR-ROM/RAM.
        case 0x2000 ... 0x3EFF:  return ciRam[nt_mirror(addr)];          // Nametables.
        case 0x3F00 ... 0x3FFF:  // Palettes:
            if ((addr & 0x13) == 0x10) addr &= ~0x10;
            return cgRam[addr & 0x1F] & (mask.gray ? 0x30 : 0xFF);
        default: return 0;
    }
}
void wr(u16 addr, u8 v)
{
    switch (addr)
    {
        case 0x0000 ... 0x1FFF:  MMC->mapper.chr_write(addr, v); break;  // CHR-ROM/RAM.
        case 0x2000 ... 0x3EFF:  ciRam[nt_mirror(addr)] = v; break;         // Nametables.
        case 0x3F00 ... 0x3FFF:  // Palettes:
            if ((addr & 0x13) == 0x10) addr &= ~0x10;
            cgRam[addr & 0x1F] = v; break;
    }
}

/* Access PPU through registers. */
template <bool write> u8 access(u16 index, u8 v)
{
    static u8 res;      // Result of the operation.
    static u8 buffer;   // VRAM read buffer.
    static bool latch;  // Detect second reading.

    /* Write into register */
    if (write)
    {
        res = v;

        switch (index)
        {
            case 0:  ctrl.r = v; tAddr.nt = ctrl.nt; break;       // PPUCTRL   ($2000).
            case 1:  mask.r = v; break;                           // PPUMASK   ($2001).
            case 3:  oamAddr = v; break;                          // OAMADDR   ($2003).
            case 4:  oamMem[oamAddr++] = v; break;                // OAMDATA   ($2004).
            case 5:                                               // PPUSCROLL ($2005).
                if (!latch) { fX = v & 7; tAddr.cX = v >> 3; }      // First write.
                else  { tAddr.fY = v & 7; tAddr.cY = v >> 3; }      // Second write.
                latch = !latch; break;
            case 6:                                               // PPUADDR   ($2006).
                if (!latch) { tAddr.h = v & 0x3F; }                 // First write.
                else        { tAddr.l = v; vAddr.r = tAddr.r; }     // Second write.
                latch = !latch; break;
            case 7:  wr(vAddr.addr, v); vAddr.addr += ctrl.incr ? 32 : 1;  // PPUDATA ($2007).
        }
    }
    /* Read from register */
    else
        switch (index)
        {
            // PPUSTATUS ($2002):
            case 2:  res = (res & 0x1F) | status.r; status.vBlank = 0; latch = 0; break;
            case 4:  res = oamMem[oamAddr]; break;  // OAMDATA ($2004).
            case 7:                                 // PPUDATA ($2007).
                if (vAddr.addr <= 0x3EFF)
                {
                    res = buffer;
                    buffer = rd(vAddr.addr);
                }
                else
                    res = buffer = rd(vAddr.addr);
                vAddr.addr += ctrl.incr ? 32 : 1;
        }
    return res;
}

template u8 access<0>(u16, u8); 
template u8 access<1>(u16, u8);

/* Calculate graphics addresses */
static inline u16 nt_addr() { return 0x2000 | (vAddr.r & 0xFFF); }
static inline u16 at_addr() { return 0x23C0 | (vAddr.nt << 10) | ((vAddr.cY / 4) << 3) | (vAddr.cX / 4); }
static inline u16 bg_addr(u8 nt) { return (ctrl.bgTbl * 0x1000) + (nt * 16) + vAddr.fY; }

/* Increment the scroll by one pixel */
static inline void h_scroll() 
{
    if (vAddr.cX == 31) 
        vAddr.r ^= 0x41F; 
    else 
        vAddr.cX++; 
}

static inline void v_scroll()
{
    if (vAddr.fY < 7) 
        vAddr.fY++;
    else
    {
        vAddr.fY = 0;
        if      (vAddr.cY == 31)   vAddr.cY = 0;
        else if (vAddr.cY == 29) { vAddr.cY = 0; vAddr.nt ^= 0b10; }
        else                       vAddr.cY++;
    }
}

/* Copy scrolling data from loopy T to loopy V */
static inline void h_update() 
{ 
    vAddr.r = (vAddr.r & ~0x041F) | (tAddr.r & 0x041F); 
}
static inline void v_update() 
{ 
    vAddr.r = (vAddr.r & ~0x7BE0) | (tAddr.r & 0x7BE0); 
}

/* Clear secondary OAM */
void clear_oam()
{
    for (int i = 0; i < 8; i++)
    {
        secOam[i].id    = 64;
        secOam[i].y     = 0xFF;
        secOam[i].tile  = 0xFF;
        secOam[i].attr  = 0xFF;
        secOam[i].x     = 0xFF;
        secOam[i].dataL = 0;
        secOam[i].dataH = 0;
    }
}

/* Fill secondary OAM with the sprite infos for the next scanline */
void eval_sprites(int scanline)
{
    int n = 0;
    for (int i = 0; i < 64; i++)
    {
        int line = (scanline == 261 ? -1 : scanline) - oamMem[i*4 + 0];
        // If the sprite is in the scanline, copy its properties into secondary OAM:
        if (line >= 0 and line < spr_height())
        {
            secOam[n].id   = i;
            secOam[n].y    = oamMem[i*4 + 0];
            secOam[n].tile = oamMem[i*4 + 1];
            secOam[n].attr = oamMem[i*4 + 2];
            secOam[n].x    = oamMem[i*4 + 3];
            if (++n >= 8)
            {
                status.sprOvf = true;
                break;
            }
        }
    }
}

/* Load the sprite info into primary OAM and fetch their tile data. */
void load_sprites(int line)
{
    u16 addr;
    for (int i = 0; i < 8; i++)
    {
        oam[i] = secOam[i];  // Copy secondary OAM into primary.

        // Different address modes depending on the sprite height:
        if (spr_height() == 16)
            addr = ((oam[i].tile & 1) * 0x1000) + ((oam[i].tile & ~1) * 16);
        else
            addr = ( ctrl.sprTbl      * 0x1000) + ( oam[i].tile       * 16);

        unsigned sprY = (line - oam[i].y) % spr_height();  // Line inside the sprite.
        if (oam[i].attr & 0x80) sprY ^= spr_height() - 1;      // Vertical flip.
        addr += sprY + (sprY & 8);  // Select the second tile if on 8x16.

        oam[i].dataL = rd(addr + 0);
        oam[i].dataH = rd(addr + 8);
    }
}

u8 rowdata[8 + 256 + 8];

static inline void cacheOAM(int line)
{
    clear_oam();
    eval_sprites(line);
    load_sprites(line);
}

void draw_bgtile(u8* buffer, u8 bgL, u8 bgH, u8 at)
{
    u32 pattern = ((bgH & 0xaa) << 8) | ((bgH & 0x55) << 1)
                    | ((bgL & 0xaa) << 7) | (bgL & 0x55);
    buffer[0] = at + ((pattern >> 14) & 3);
    buffer[1] = at + ((pattern >> 6) & 3);
    buffer[2] = at + ((pattern >> 12) & 3);
    buffer[3] = at + ((pattern >> 4) & 3);
    buffer[4] = at + ((pattern >> 10) & 3);
    buffer[5] = at + ((pattern >> 2) & 3);
    buffer[6] = at + ((pattern >> 8) & 3);
    buffer[7] = at + (pattern & 3);
}

void renderBGLine(u8* row)
{
    u16 addr;
    u8 nt, at, bgL, bgH;
    u8 *buffer = row;
    if (!mask.bg) {
        memset(buffer + 8, 0, 256);
        return;
    }
    buffer += (8 - fX);
    int tile_num = 0;
    while(tile_num < 33) {
        addr = nt_addr();
        nt = rd(addr);
        addr = at_addr();
        at = rd(addr);  
        if (vAddr.cY & 2) {
            at >>= 4;
        } 
        if (vAddr.cX & 2) {
            at >>= 2; 
        }
        addr = bg_addr(nt);
        bgL = rd(addr);
        bgH = rd(addr + 8);
        draw_bgtile(buffer, bgL, bgH, (at & 3) << 2);
        h_scroll();
        buffer += 8;
        tile_num += 1;
    }
    if (!mask.bgLeft) {
        u32 *bf = (u32*)(row + 8);
        bf[0] = 0;
        bf[1] = 0;
    }
}

void renderOAMLine(u8* row)
{
    if (!mask.spr) {
        return;
    }
    for(int i = 7; i >= 0; i--)
    {
        if (oam[i].id == 64) {
            continue;  // Void entry.
        }
        u8 pat1 = oam[i].dataH;
        u8 pat0 = oam[i].dataL;
        u32 pattern = ((pat1 & 0xaa) << 8) | ((pat1 & 0x55) << 1)
                    | ((pat0 & 0xaa) << 7) | (pat0 & 0x55);
        u8 pixels[8];
        if ((oam[i].attr & 0x40) == 0) {
            pixels[0] = (pattern >> 14) & 3;
            pixels[1] = (pattern >> 6) & 3;
            pixels[2] = (pattern >> 12) & 3;
            pixels[3] = (pattern >> 4) & 3;
            pixels[4] = (pattern >> 10) & 3;
            pixels[5] = (pattern >> 2) & 3;
            pixels[6] = (pattern >> 8) & 3;
            pixels[7] = pattern & 3;
        } else {
            pixels[7] = (pattern >> 14) & 3;
            pixels[6] = (pattern >> 6) & 3;
            pixels[5] = (pattern >> 12) & 3;
            pixels[4] = (pattern >> 4) & 3;
            pixels[3] = (pattern >> 10) & 3;
            pixels[2] = (pattern >> 2) & 3;
            pixels[1] = (pattern >> 8) & 3;
            pixels[0] = pattern & 3;
        }
        u8 at = ((oam[i].attr & 3) << 2) + 16;
        int x = oam[i].x;
        bool objPriority = oam[i].attr & 0x20;
        for(int j = 0; j < 8; ++j)
        {
            if (!mask.sprLeft && (x + j) < 8) {
                continue;
            }
            if (pixels[j] != 0 && (row[x + j] == 0 || objPriority == 0))
            {
                row[x + j] = at + pixels[j];
            }
        }
    }
    
}

void renderScanline(u32* buffer)
{
    renderBGLine(rowdata);
    renderOAMLine(rowdata + 8);
    for(int i = 0; i < 256; i++)
    {
    	buffer[i] = nesRgb[rd(0x3F00 + (rendering() ? rowdata[i + 8] : 0))];
    }
}

void scanline_visible(int line, u32* buffer)
{
	renderScanline(buffer);
	if (rendering())
	{
		v_scroll();
        h_update();
	}
    cacheOAM(line);
}

void scanline_other(int line)
{
	if (line == 241)
	{
		status.vBlank = true;
	}
    // avoid race condition:
    // http://wiki.nesdev.com/w/index.php/NMI#Race_condition
    else if (line == 242)
    {   
        if (ctrl.nmi)
	    {
	    	CPU::set_nmi();
	    }
    }
	else if (line == 261)
	{
		status.vBlank = false;
        cacheOAM(line);
        if (rendering()) {
		    v_update();
	    }
	}
}

void init(struct cartridge_mmc *mmc)
{
    MMC = mmc;
    ctrl.r = mask.r = status.r = 0;
    memset(ciRam,  0xFF, sizeof(ciRam));
    memset(oamMem, 0x00, sizeof(oamMem));
}

}
