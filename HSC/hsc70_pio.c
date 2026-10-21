/* hsc70_pio.c: HSC70 P.ioj simulation

   Copyright (c) 2023, Matt Burke

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.
*/

#include "pdp11_defs.h"
#include "pdp11_cpumod.h"
#include "hsc_hsc70_bin.h"
#include "hsc_kg.h"

/* switch/display register */

#define SDR_LMP0        0000001                         /* lamp 0 */
#define SDR_LMP1        0000002                         /* lamp 0 */
#define SDR_LMP2        0000004                         /* lamp 0 */
#define SDR_LMP3        0000010                         /* lamp 0 */
#define SDR_LMP4        0000020                         /* lamp 0 */
#define SDR_INHP        0000040                         /* inhibit parity traps */
#define SDR_NXM         0000100                         /* force NXM on control/data memory access */
#define SDR_DIA         0000200                         /* diagnostics passed/failed LED */
#define SDR_SW0         0000400                         /* switch 0 */
#define SDR_SW1         0001000                         /* switch 1 */
#define SDR_SW2         0002000                         /* online switch */
#define SDR_SW3         0004000                         /* fault switch */

#define UNIT_V_INIT     (UNIT_V_UF)                     /* media init (hold fault switch) */
#define UNIT_V_ONL      (UNIT_V_UF + 1)                 /* online */
#define UNIT_INIT       (1u << UNIT_V_INIT)
#define UNIT_ONL        (1u << UNIT_V_ONL)

/* floppy control/status register */

#define FLCSR_PE        0100000                         /* parity err */
#define FLCSR_NXM       0040000                         /* non-existant mem err */
#define FLCSR_IE        0020000                         /* interrupt enable */
#define FLCSR_DIS       0010000                         /* disable request */
#define FLCSR_PT        0004000                         /* parity test */
#define FLCSR_BSY       0002000                         /* busy LED */
#define FLCSR_ME        0001000                         /* motor enable */
#define FLCSR_V_SEL     8                               /* drive select */
#define FLCSR_SEL       0000400
#define FLCSR_CS        0000377                         /* command/status */
#define FLCSR_RW        0037400
#define FLSTS_NRDY      0000200                         /* not ready */
#define FLSTS_TRK0      0000004                         /* track 0 */
#define FLSTS_BSY       0000001                         /* busy */

/* floppy memory address register 0 */

#define FLMA0_V_A0      8                               /* mem addr 0-7 */
#define FLMA0_M_A0      0377
#define FLMA0_A0        (FLMA0_M_A0 << FLMA0_V_A0)
#define FLMA0_CT        0000377                         /* current track */
#define FLMA0_RW        0177400

/* floppy memory address register 1 */

#define FLMA1_V_A8      8                               /* mem addr 8-15 */
#define FLMA1_M_A8      0377
#define FLMA1_A8        (FLMA1_M_A8 << FLMA1_V_A8)
#define FLMA1_DS        0000377                         /* desired sector */

/* floppy memory address register 2 */

#define FLMA2_GRN       0100000                         /* green LED */
#define FLMA2_DMA       0040000                         /* DMA test */
#define FLMA2_V_A16     8                               /* mem addr 16-21 */
#define FLMA2_M_A16     077
#define FLMA2_A16       (FLMA2_M_A16 << FLMA2_V_A16)
#define FLMA2_DT        0000377                         /* desired track */

#define TRK             u3
#define CMD             u4
#define DIR             u5

#define FL_WAIT         50

#define FL_NUMTR        80                              /* tracks/disk */
#define FL_NUMSC        15                              /* sectors/track */
#define FL_NUMHD        2                               /* heads */
#define FL_NUMBY        512                             /* bytes/sector */
#define FL_SIZE         (FL_NUMTR * FL_NUMHD * FL_NUMSC * FL_NUMBY) /* bytes/disk */
#define GET_DA(t,h,s)   ((t * FL_NUMHD + h) * FL_NUMSC + (s - 1))

int32 SDR = 0;                                          /* switch/display register */
int32 PCSR = 0;                                         /* Pio control/status */
int32 WADR[128][8];                                     /* window address registers */
int32 WINDR = 0;                                        /* window index register */
int32 KINI = 0;                                         /* K init register */
HDIB *KSTS[10];
int32 fl_csr = 0;
int32 fl_madr0 = 0;
int32 fl_madr1 = 0;
int32 fl_madr2 = 0;

extern DIB dli_dib;
extern DEVICE dli_dev;
extern int32 CADR, CDATA, CCYCLE;
extern int32 EADR, DADR;
extern int32 autcon_enb;
extern int16 inst_pc;

t_stat pio_rd (int32 *data, int32 pa, int32 access);
t_stat pio_wr (int32 data, int32 pa, int32 access);
t_stat pio_set_init (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat pio_show_init (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat pio_set_onl (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat pio_show_onl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat pio_reset (DEVICE *dptr);
t_stat cwin_rd (int32 *data, int32 va, int32 access);
t_stat cwin_wr (int32 data, int32 va, int32 access);
t_stat rom_rd (int32 *data, int32 pa, int32 access);
t_stat rom_boot (int32 unitno, DEVICE *dptr);
t_stat fl_rd (int32 *data, int32 pa, int32 access);
t_stat fl_wr (int32 data, int32 pa, int32 access);
t_stat fl_svc (UNIT *uptr);
t_stat fl_reset (DEVICE *dptr);
t_stat fl_boot (int32 unitno, DEVICE *dptr);
void fl_setint ();

extern int32 PReadB (int32 pa);
extern int32 ReadB (int32 va);
extern void PWriteB (int32 data, int32 addr);
extern void WriteB (int32 data, int32 addr);

/* Fixed I/O address table entries */

DIB cwin_dib = { IOBA_CWIN, IOLN_CWIN, &cwin_rd, &cwin_wr, 0 };

/* debugging bitmaps */

#define DBG_REG         0x0001                          /* display register read/writes */

DEBTAB pio_debug[] = {
    { "REG",  DBG_REG,   "register access" },
    { 0 }
};

HDIB pio_dib = {
    { IOBA_PIO, IOLN_PIO, &pio_rd, &pio_wr,
      0, 0, 0, { NULL } },
    NULL, K_PIO, 0, NULL, NULL
    };

UNIT pio_unit = { UDATA (NULL, UNIT_ONL, 0) };

REG pio_reg[] = {
    { ORDATA (SDR, SDR, 16) },
    { ORDATA (KINI, KINI, 16) },
    { NULL }
    };

MTAB pio_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL, "Backplane slot number" },
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL, "Bus address" },
    { UNIT_ONL, 0, NULL, "OFFLINE",
      &pio_set_onl, NULL, NULL, "Set HSC state to offline" },
    { UNIT_ONL, UNIT_ONL, NULL, "ONLINE",
      &pio_set_onl, NULL, NULL, "Set HSC state to online" },
    { MTAB_XTD|MTAB_VDV, 0, "STATUS", NULL,
      NULL, &pio_show_onl, NULL, "Display HSC online status" },
    { UNIT_INIT, 0, NULL, "NOINIT",
      &pio_set_init, NULL, NULL, "Don't reset media to factory defaults" },
    { UNIT_INIT, UNIT_INIT, NULL, "INIT",
      &pio_set_init, NULL, NULL, "Reset media to factory defaults (hold fault switch)" },
    { MTAB_XTD|MTAB_VDV, 0, "INIT", NULL,
      NULL, &pio_show_init, NULL, "Display media reset status" },
    { 0 }
    };

DEVICE pio_dev = {
    "PIO", &pio_unit, pio_reg, pio_mod,
    1, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &pio_reset,
    NULL, NULL, NULL,
    &pio_dib, DEV_DEBUG | DEV_UBUS | DEV_HSC,
    0, pio_debug
    };

/* ROM device */

DIB rom_dib = {
    IOBA_ROM, IOLN_ROM, &rom_rd, NULL,
    0, 0, 0, { NULL }
    };

UNIT rom_unit = { UDATA (NULL, 0, 0) };

REG rom_reg[] = {
    { NULL }
    };

MTAB rom_mod[] = {
    { 0 }
    };

DEVICE rom_dev = {
    "ROM", &rom_unit, rom_reg, rom_mod,
    1, DEV_RDX, 12, 1, DEV_RDX, 16,
    NULL, NULL, NULL,
    &rom_boot, NULL, NULL,
    &rom_dib, DEV_UBUS | DEV_QBUS
    };

/* Floppy disk device */

DEBTAB fl_debug[] = {
    { "REG",  DBG_REG,   "register access" },
    { 0 }
};

DIB fl_dib = {
    IOBA_FL, IOLN_FL, &fl_rd, &fl_wr,
    1, IVCL (FL), VEC_FL, { NULL }
    };

UNIT fl_unit[] = {
    { UDATA (&fl_svc,
             UNIT_FIX+UNIT_ATTABLE+UNIT_BUFABLE+UNIT_MUSTBUF, FL_SIZE) },
    { UDATA (&fl_svc,
             UNIT_FIX+UNIT_ATTABLE+UNIT_BUFABLE+UNIT_MUSTBUF, FL_SIZE) }
    };

REG fl_reg[] = {
    { NULL }
    };

MTAB fl_mod[] = {
    { UNIT_WLK, 0, "write enabled", "WRITEENABLED", NULL },
    { UNIT_WLK, UNIT_WLK, "write locked", "LOCKED", NULL },
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", "VECTOR",
      &set_vec, &show_vec, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", "VECTOR",
      &set_vec, &show_vec, NULL },
    { 0 }
    };

DEVICE fl_dev = {
    "RX", fl_unit, fl_reg, fl_mod,
    2, DEV_RDX, 20, 1, DEV_RDX, 8,
    NULL, NULL, &fl_reset,
    &fl_boot, NULL, NULL,
    &fl_dib, DEV_DEBUG,
    0, fl_debug
    };

int32 parity (int32 val, int32 odd)
{
for ( ; val != 0; val = val >> 1) {
    if (val & 1)
        odd = odd ^ 1;
    }
return odd;
}

t_stat pio_rd (int32 *data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 037;
HDIB *dibp;

switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* WADR0 */
    case 001:                                           /* WADR1 */
    case 002:                                           /* WADR2 */
    case 003:                                           /* WADR3 */
    case 004:                                           /* WADR4 */
    case 005:                                           /* WADR5 */
    case 006:                                           /* WADR6 */
    case 007:                                           /* WADR7 */
        *data = WADR[WINDR][rg];
        *data = (*data & 0177) << 2;
        *data = *data | (rg << 9) | (016 << 12);
        sim_debug (DBG_REG, &pio_dev, "WADR%d rd %o\n", rg, *data);
        return SCPE_OK;

    case 010:                                           /* WINDR */
        *data = WINDR;
        sim_debug (DBG_REG, &pio_dev, "WINDR rd: %o\n", *data);
        return SCPE_OK;

    case 011:                                           /* WBUSR */
        *data = (CADR >> 1);
        sim_debug (DBG_REG, &pio_dev, "WBUSR rd: %o\n", *data);
        return SCPE_OK;

    case 012:                                           /* low error */
        *data = EADR & 0177777;
        sim_debug (DBG_REG, &pio_dev, "low error rd: %o\n", *data);
        return SCPE_OK;

    case 013:                                           /* high error */
        *data = (EADR >> 16) & 077;
        sim_debug (DBG_REG, &pio_dev, "high error rd: %o\n", *data);
        return SCPE_OK;

    case 020:                                           /* Pio CSR */
        *data = PCSR | PCSR_ENA;
        if (CPUT (CPUT_HSC40))                          /* set HSC type bits */
            *data = *data | PCSR_HSC40;
        if (CPUT (CPUT_HSC60))
            *data = *data | PCSR_HSC60;
        if (CPUT (CPUT_HSC90))
            *data = *data | PCSR_HSC90;
        sim_debug (DBG_REG, &pio_dev, "Pio CSR rd: %o\n", *data);
        return SCPE_OK;

    case 021:                                           /* switch/display reg */
        *data = SDR & 07777;
        if (pio_unit.flags & UNIT_INIT)
            *data = *data | SDR_SW3;                    /* fault switch pressed */
        if (pio_unit.flags & UNIT_ONL)                  /* HSC online? */
            *data = *data | SDR_SW2;                    /* online switch pressed */
        sim_debug (DBG_REG, &pio_dev, "SDR rd: %o\n", *data);
        return SCPE_OK;

    case 022:                                           /* K init reg - NI */
        *data = KINI;
        sim_debug (DBG_REG, &pio_dev, "K init rd\n", *data);
        return SCPE_OK;

    case 023:                                           /* K status reg - NI */
        *data = K_NONE;                                 /* assume no requestor */
        dibp = KSTS[(KINI & 017)];
        if (dibp) {
            *data = dibp->sts;
            *data = *data | (parity (*data, 1) << 7);   /* odd parity bit */
            }
        sim_debug (DBG_REG, &pio_dev, "K status rd\n", *data);
        return SCPE_OK;

    case 024:                                           /* data mem low addr */
        *data = DADR & 0177777;
        sim_debug (DBG_REG, &pio_dev, "data mem low rd: %o\n", *data);
        return SCPE_OK;

    case 025:                                           /* data mem high addr */
        *data = (DADR >> 16) & 07;
        sim_debug (DBG_REG, &pio_dev, "data mem high rd: %o\n", *data);
        return SCPE_OK;

    case 026:                                           /* ctrl mem low addr */
        *data = (CCYCLE << 1) | (CADR & 1);
        sim_debug (DBG_REG, &pio_dev, "ctrl mem low rd: %o\n", *data);
        return SCPE_OK;

    case 027:                                           /* serial number reg - NI */
        sim_debug (DBG_REG, &pio_dev, "serial num rd\n");
        *data = 0;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat pio_wr (int32 data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 037;
HDIB *dibp = NULL;
uint32 slot = 0;
uint16 action, mask;

switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* WADR0 */
    case 001:                                           /* WADR1 */
    case 002:                                           /* WADR2 */
    case 003:                                           /* WADR3 */
    case 004:                                           /* WADR4 */
    case 005:                                           /* WADR5 */
    case 006:                                           /* WADR6 */
    case 007:                                           /* WADR7 */
        sim_debug (DBG_REG, &pio_dev, "WADR%d wr: %o\n", rg, data);
        WADR[WINDR][rg] = data;
        return SCPE_OK;

    case 010:                                           /* WINDR */
        sim_debug (DBG_REG, &pio_dev, "WINDR wr: %o\n", data);
        WINDR = data & 0177;
        return SCPE_OK;

    case 020:                                           /* Pio CSR */
        sim_debug (DBG_REG, &pio_dev, "Pio CSR wr: %o\n", data);
        PCSR = data;
        return SCPE_OK;

    case 021:                                           /* switch/display reg */
        sim_debug (DBG_REG, &pio_dev, "SDR wr: %o\n", data);
        pio_unit.flags &= ~UNIT_INIT;                   /* clear init flag */
        SDR = (SDR & ~0377) | (data & 0377);
        return SCPE_OK;

    case 022:                                           /* K init reg - NI */
        sim_debug (DBG_REG, &pio_dev, "K init wr: %o\n", data);
        for (int i = 6; i < 16; i++) {                  /* loop over K init bits */
            if (i < 8)                                  /* bits 6..7 = slots 8..9 */
                slot = i + 2;
            else if (i > 8)                             /* bits 9..16 = slots 1..7 */
                slot = i - 8;
            else                                        /* bit 8 = slot 0 */
                continue;                               /* no init for K.pio */
            mask = (1u << i);                           /* create a mask for this bit */
            action = K_NOACTION;                        /* assume no action */
            if (~KINI & data & mask)                    /* bit being set */
                action = K_RUN;                         /* action is run */
            else if (~data & KINI & mask)               /* bit being cleared */
                action = K_HALT;                        /* action is halt */
            if (action) {                               /* action required? */
                dibp = KSTS[slot];                      /* get DIB for this slot */
                if (dibp)                               /* any requestor present? */
                    dibp->init (slot, action);          /* send action */
                }
            }
        KINI = data;                                    /* update KINI register */
        return SCPE_OK;

    case 023:                                           /* K status reg - NI */
        sim_debug (DBG_REG, &pio_dev, "K status wr: %o\n", data);
        dibp = KSTS[0];
        dibp->sts = data;                               /* can only write to req 0 */
        return SCPE_OK;
        }

return SCPE_NXM;
}

t_stat cwin_rd (int32 *data, int32 va, int32 access)
{
int32 pa;
int32 rg;

rg = (va >> 9) & 07;                                    /* get window reg for this addr */
pa = (WADR[WINDR][rg] & 0177600) << 2;
pa = pa | (va & 0777);
pa = pa + CMEMBASE;                                     /* add offset to control memory */
if (ADDR_IS_CMEM (pa))                                  /* memory address? */
    *data = hsc_RdMemW (pa);
else {
    sim_printf ("Error: %o is not mem addr\n", pa);
    return SCPE_NXM;
    }
return SCPE_OK;
}

t_stat cwin_wr (int32 data, int32 va, int32 access)
{
int32 pa;
int32 rg;

rg = (va >> 9) & 07;                                    /* translate virtual address */
pa = (WADR[WINDR][rg] & 0177600) << 2;
pa = pa | (va & 0777);
pa = pa + CMEMBASE;                                     /* add offset to control memory */
if (ADDR_IS_CMEM (pa)) {                                /* memory address? */
    if (access == WRITEB)
        hsc_WrMemB (pa, data);
    else
        hsc_WrMemW (pa, data);
    }
else {
    sim_printf ("Error: %o is not mem addr\n", pa);
    return SCPE_NXM;
    }
return SCPE_OK;
}

void fl_write (UNIT *uptr, int32 blk, int32 sz, int32 ma, t_bool mapen)
{
int32 da = blk * 512;
int32 ln = sz * 512;
int32 i;
uint8 *fbuf = uptr->filebuf;

if (fl_csr & FLCSR_DIS)                                 /* disable DMA request? */
    return;
if ((da < 0) || da >= FL_SIZE) {  /* no such sector (e.g. SREG=0 DMA test) */
    sim_debug (DBG_REG, &fl_dev, "fl: invalid write da %d\n", da);
    return;
    }
sim_debug (DBG_REG, &fl_dev, "Boot device write, %o -> %X, %d\n", ma, da, sz);
if (mapen) {                                            /* use reloc? */
    for (i = 0; i < ln; i++)
        fbuf[(da + i)] = ReadB ((ma + i));
    }
else {
    for (i = 0; i < ln; i++)
        fbuf[(da + i)] = PReadB ((ma + i));
    }
}

void fl_read (UNIT *uptr, int32 blk, int32 sz, int32 ma, t_bool mapen)
{
int32 da = blk * 512;
int32 ln = sz * 512;
int32 i;
uint8 *fbuf = uptr->filebuf;

if (fl_csr & FLCSR_DIS)                                 /* disable DMA request? */
    return;
if ((da < 0) || da >= FL_SIZE) {  /* no such sector (e.g. SREG=0 DMA test) */
    sim_debug (DBG_REG, &fl_dev, "fl: invalid read da %d\n", da);
    return;
    }
sim_debug (DBG_REG, &fl_dev, "Boot device read, %X, %d -> %o\n", da, sz, ma);
if (mapen) {                                            /* use reloc? */
    for (i = 0; i < ln; i++)
        WriteB (fbuf[(da + i)], (ma + i));
    }
else {
    for (i = 0; i < ln; i++)
        PWriteB (fbuf[(da + i)], (ma + i));
    }
}

t_stat fl_rd (int32 *data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 03;
int32 unitno = (fl_csr >> FLCSR_V_SEL) & 1;             /* get drive select */

if ((fl_csr & FLCSR_ME) == 0) {                         /* motor not enabled? */
    *data = 0;                                          /* controller held in reset state */
    if (rg == 003) {                                    /* MADR2 */
        if (CPUT (CPUT_HSC60|CPUT_HSC90))               /* HSC60 or HSC90? */
            *data = 2;                                  /* M.std4 present */
        }
    sim_debug (DBG_REG, &fl_dev, "fl_madr2 rd: %o\n", *data);
    return SCPE_OK;
    }
switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* CSR */
        *data = fl_csr;
        sim_debug (DBG_REG, &fl_dev, "fl_csr rd: %o at %o\n", *data, (uint16)inst_pc);
        return SCPE_OK;

    case 001:                                           /* MADR0 */
        *data = fl_madr0 | fl_unit[unitno].TRK;
        sim_debug (DBG_REG, &fl_dev, "fl_madr0 rd: %o\n", *data);
        return SCPE_OK;

    case 002:                                           /* MADR1 */
        *data = fl_madr1;
        sim_debug (DBG_REG, &fl_dev, "fl_madr1 rd: %o\n", *data);
        return SCPE_OK;

    case 003:                                           /* MADR2 */
        *data = fl_madr2;
        sim_debug (DBG_REG, &fl_dev, "fl_madr2 rd: %o\n", *data);
        return SCPE_OK;
        }
return SCPE_NXM;
}

t_stat fl_wr (int32 data, int32 pa, int32 access)
{
int32 cmd;
int32 rg = (pa >> 1) & 03;
int32 unitno = (fl_csr >> FLCSR_V_SEL) & 1;             /* get drive select */
switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* CSR */
        sim_debug (DBG_REG, &fl_dev, "fl_csr wr: %o at %o\n", data, (uint16)inst_pc);
        if ((data & FLCSR_ME) && ((fl_csr & FLCSR_ME) == 0))
            fl_madr1 = (fl_madr1 & ~0377) | 0001;       /* ME 0->1 edge: SREG init 001 (TM 11.7.1.3.2) */
        fl_csr = data & FLCSR_RW;
        if (fl_csr & FLCSR_ME) {                        /* motor enabled */
            cmd = data & FLCSR_CS;
            if ((cmd >> 4) == 015) {                    /* force interrupt? */
                sim_cancel (&fl_unit[unitno]);
                fl_csr = fl_csr & ~FLSTS_BSY;           /* set not busy */
                if (fl_unit[unitno].flags & UNIT_ATT)   /* force int leaves type I status in SR */
                    fl_csr = fl_csr | FLSTS_NRDY;
                if (cmd & 05) {                         /* I2 or I0 */
                    fl_unit[unitno].CMD = cmd;
                    fl_csr = fl_csr | FLSTS_BSY;        /* set busy */
                    sim_activate (&fl_unit[unitno], FL_WAIT);
                    }
                if (cmd & 010)                          /* I3 */
                    fl_setint ();                       /* immediate interrupt */
                }
            else if ((fl_csr & FLSTS_BSY) == 0) {       /* not busy? */
                fl_unit[unitno].CMD = cmd;
                fl_csr = fl_csr | FLSTS_BSY;            /* set busy */
                sim_activate (&fl_unit[unitno], FL_WAIT);
                }
            }
        return SCPE_OK;

    case 001:                                           /* MADR0 */
        sim_debug (DBG_REG, &fl_dev, "fl_madr0 wr: %o\n", data);
        fl_madr0 = data & FLMA0_RW;
        fl_unit[unitno].TRK = data & FLMA0_CT;
        return SCPE_OK;

    case 002:                                           /* MADR1 */
        sim_debug (DBG_REG, &fl_dev, "fl_madr1 wr: %o\n", data);
        fl_madr1 = data;
        return SCPE_OK;

    case 003:                                           /* MADR2 */
        sim_debug (DBG_REG, &fl_dev, "fl_madr2 wr: %o\n", data);
        fl_madr2 = data;
        return SCPE_OK;
        }
return SCPE_NXM;
}

void fl_setint ()
{
if (fl_csr & FLCSR_IE)
    SET_INT (FL);
}

t_stat fl_svc (UNIT *uptr)
{
int32 ma, da, sec;

if ((uptr->flags & UNIT_ATT) == 0) {
    fl_csr = fl_csr & ~FLSTS_NRDY;                      /* 2797 type I S7 = inverted pin 34: not ready -> clear */
    fl_setint ();
    return SCPE_OK;
    }
switch (uptr->CMD >> 5) {
    case 0:
        if (uptr->CMD & 020) {                          /* seek */
            uptr->TRK = fl_madr2 & FLMA2_DT;
            sim_debug (DBG_REG, &fl_dev, "fl: seek track %d\n", uptr->TRK);
            }
        else {                                          /* restore */
            uptr->TRK = 0;
            sim_debug (DBG_REG, &fl_dev, "fl: restore\n");
            }
        fl_csr = fl_csr & ~FLSTS_TRK0;
        if (uptr->TRK == 0)                             /* now on track 0? */
            fl_csr = fl_csr | FLSTS_TRK0;               /* set flag */
        fl_csr = fl_csr & ~FLSTS_NRDY;                  /* type I done: drive ready -> S7 clear (TM Table 11-10) */
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;

    case 1:                                             /* step */
        if (uptr->DIR) {
            uptr->TRK++;
            sim_debug (DBG_REG, &fl_dev, "fl: step in\n");
            }
        else {
            uptr->TRK--;
            sim_debug (DBG_REG, &fl_dev, "fl: step out\n");
            }
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;

    case 2:                                             /* step in */
        uptr->DIR = 1;
        uptr->TRK++;
        sim_debug (DBG_REG, &fl_dev, "fl: step in\n");
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;

    case 3:                                             /* step out */
        uptr->DIR = 0;
        uptr->TRK--;
        sim_debug (DBG_REG, &fl_dev, "fl: step out\n");
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;

    case 4:                                             /* read sector */
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        sec = fl_madr1 & FLMA1_DS;
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), sec);
        if (fl_madr2 & FLMA2_DMA) {                     /* DMA test: no media; SREG byte doubled to memory */
            sim_debug (DBG_REG, &fl_dev, "fl: DMA test read, %o -> %o\n", sec, ma);
            if ((ma >= 0) && (ma < (FL_NUMTR * FL_NUMHD * FL_NUMSC * FL_NUMBY))) {
                PWriteB (sec, ma);
                PWriteB (sec, (ma + 1));
                }
            ma = ma + 2;                                /* one word transferred */
            }
        else if (uptr->CMD & 020) {
            sim_debug (DBG_REG, &fl_dev, "fl: read multi sector\n");
            fl_read (uptr, da, (FL_NUMSC - sec), ma, FALSE);
            ma = ma + ((FL_NUMSC - sec + 1) * 512);
            }
        else {
            sim_debug (DBG_REG, &fl_dev, "fl: read single sector\n");
            fl_read (uptr, da, 1, ma, FALSE);
            ma = ma + 512;
            }
        fl_madr0 = fl_madr0 & ~FLMA0_A0;
        fl_madr0 = fl_madr0 | ((ma << 8) & FLMA0_A0);
        fl_madr1 = fl_madr1 & ~FLMA1_A8;
        fl_madr1 = fl_madr1 | (ma & FLMA1_A8);
        fl_madr2 = fl_madr2 & ~FLMA2_A16;
        fl_madr2 = fl_madr2 | ((ma >> 8) & FLMA2_A16);
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;

    case 5:                                             /* write sector */
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        sec = fl_madr1 & FLMA1_DS;
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), sec);
        if (fl_madr2 & FLMA2_DMA) {                     /* DMA test: no media, no memory data; one word */
            sim_debug (DBG_REG, &fl_dev, "fl: DMA test write, %o <- word\n", ma);
            ma = ma + 2;                                /* one word transferred */
            }
        else if (uptr->CMD & 020) {
            sim_debug (DBG_REG, &fl_dev, "fl: write multi sector\n");
            fl_write (uptr, da, (FL_NUMSC - sec), ma, FALSE);
            ma = ma + ((FL_NUMSC - sec + 1) * 512);
            }
        else {
            sim_debug (DBG_REG, &fl_dev, "fl: write single sector\n");
            fl_write (uptr, da, 1, ma, FALSE);
            if ((da * 512) > uptr->hwmark)              /* update hwmark */
                uptr->hwmark = (da * 512);
            ma = ma + 512;
            }
        fl_madr0 = fl_madr0 & ~FLMA0_A0;
        fl_madr0 = fl_madr0 | ((ma << 8) & FLMA0_A0);
        fl_madr1 = fl_madr1 & ~FLMA1_A8;
        fl_madr1 = fl_madr1 | (ma & FLMA1_A8);
        fl_madr2 = fl_madr2 & ~FLMA2_A16;
        fl_madr2 = fl_madr2 | ((ma >> 8) & FLMA2_A16);
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;

    case 6:
        if (uptr->CMD & 020) {                          /* force interrupt */
            sim_debug (DBG_REG, &fl_dev, "fl: force interrupt\n");
            fl_csr = fl_csr & ~FLSTS_BSY;               /* set not busy */
            if (uptr->CMD & 017)
                fl_setint ();
            }
        else {                                          /* read address */
            sim_debug (DBG_REG, &fl_dev, "fl: read address\n");
            fl_setint ();
            }
        break;

    case 7:
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), 1);
        if (uptr->CMD & 020) {                          /* write track */
            sim_debug (DBG_REG, &fl_dev, "fl: write track\n");
            if ((da * 512) > uptr->hwmark)              /* update hwmark */
                uptr->hwmark = (da * 512);
            fl_write (uptr, da, FL_NUMSC, ma, FALSE);
            }
        else {                                          /* read track */
            sim_debug (DBG_REG, &fl_dev, "fl: read track\n");
            fl_read (uptr, da, FL_NUMSC, ma, FALSE);
            }
        ma = ma + (FL_NUMSC * 512);
        fl_madr0 = fl_madr0 & ~FLMA0_A0;
        fl_madr0 = fl_madr0 | ((ma << 8) & FLMA0_A0);
        fl_madr1 = fl_madr1 & ~FLMA1_A8;
        fl_madr1 = fl_madr1 | (ma & FLMA1_A8);
        fl_madr2 = fl_madr2 & ~FLMA2_A16;
        fl_madr2 = fl_madr2 | ((ma >> 8) & FLMA2_A16);
        fl_csr = fl_csr & ~FLSTS_BSY;                   /* set not busy */
        fl_setint ();
        break;
        }
return SCPE_OK;
}

t_stat fl_reset (DEVICE *dptr)
{
fl_unit[0].TRK = 0;
fl_unit[0].CMD = 0;
fl_unit[0].DIR = 1;
fl_unit[1].TRK = 0;
fl_unit[1].CMD = 0;
fl_unit[1].DIR = 1;
fl_csr = 0;
fl_madr0 = 0;
fl_madr1 = 0001;                                        /* SREG init 001 on power up/BINIT (TM 11.7.1.3.2) */
fl_madr2 = 0;
return SCPE_OK;
}

/* Direct boot from floppy device (bypass ROM) */

t_stat fl_boot (int32 unitno, DEVICE *dptr)
{
extern int32 REGFILE[6][2];
extern int32 rs;
extern int32 saved_PC;

if (unitno != 0) {
    sim_printf ("Boot only supported from unit 0\n");
    return SCPE_NOFNC;
    }
fl_read (&fl_unit[unitno], 0, 1, 06000, FALSE);         /* read first 8 blocks */
fl_read (&fl_unit[unitno], 2, 7, 07000, FALSE);
REGFILE[1][rs] = 0177400;                               /* boot device CSR */
REGFILE[2][rs] = 06000;                                 /* boot base */
saved_PC = 06000;
return SCPE_OK;
}

t_stat rom_rd (int32 *data, int32 pa, int32 access)
{
uint16 *rom = (uint16*) hsc_hsc70_bin;
uint32 rg = ((pa - IOPAGEBASE - 013000) >> 1);
if (PCSR & PCSR_PG2)                                    /* second page selected? */
    rg = rg + 02000;                                    /* yes, add offset */
*data = sim_rom_read_with_delay (rom[rg]);
return SCPE_OK;
}

t_stat rom_boot (int32 unitno, DEVICE *dptr)
{
extern int32 saved_PC;

saved_PC = IOBA_ROM;
return SCPE_OK;
}

/* Set/clear media reset */

t_stat pio_set_init (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return SCPE_OK;
}

/* Show media reset status */

t_stat pio_show_init (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
if (uptr->flags & UNIT_INIT)
    fprintf (st, "media init on next boot (hold fault switch)");
return SCPE_OK;
}

/* Set HSC online/offline */

t_stat pio_set_onl (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return SCPE_OK;
}

/* Show HSC online status */

t_stat pio_show_onl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
if (uptr->flags & UNIT_ONL)
    fprintf (st, "online");
else fprintf (st, "offline");
return SCPE_OK;
}

/* Build I/O space entries for P.ioj */

t_stat pio_build_dib (void)
{
t_stat r;

autcon_enb = 0;                                         /* turn off autoconfigration */
fl_dev.flags |= (DEV_UBUS | DEV_QBUS);
dli_dev.flags &= ~(DEV_DISABLE | DEV_DIS);
dli_dib.ba = IOBA_DL;
dli_dib.vec = VEC_DL;
if (r = build_ubus_tab (&pio_dev, &cwin_dib))           /* add to dispatch tab */
     return r;
if (CM == NULL) {                                       /* First time init */
    CM = (uint16 *) calloc (CMEMSIZE, sizeof (uint16)); /* 2 banks */
    if (CM == NULL)
        return SCPE_MEM;
    }
if (DM == NULL) {                                       /* First time init */
    DM = (uint16 *) calloc (DMEMSIZE >> 1, sizeof (uint16));
    if (DM == NULL)
        return SCPE_MEM;
    }
return SCPE_OK;
}

t_stat pio_reset (DEVICE *dptr)
{
if (dptr->units[0].flags & UNIT_INIT)
    SDR = SDR | SDR_SW3;                                /* hold fault switch */
return SCPE_OK;
}
