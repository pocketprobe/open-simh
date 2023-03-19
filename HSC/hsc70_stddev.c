/* hsc70_stddev.c: HSC70 standard devices

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
#include "hsc_hsc70_bin.h"

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

#define FLMA0_V_A0      8                               /* mem addr 0-7 */
#define FLMA0_M_A0      0377
#define FLMA0_A0        (FLMA0_M_A0 << FLMA0_V_A0)
#define FLMA0_CT        0000377                         /* current track */
#define FLMA0_RW        0177400

#define FLMA1_V_A8      8                               /* mem addr 8-15 */
#define FLMA1_M_A8      0377
#define FLMA1_A8        (FLMA1_M_A8 << FLMA1_V_A8)
#define FLMA1_DS        0000377                         /* desired sector */

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
#define UNIT_V_WLK      (UNIT_V_UF)                     /* write locked */
#define UNIT_WLK        (1u << UNIT_V_UF)
#define UNIT_WPRT       (UNIT_WLK | UNIT_RO)            /* write protect */

int32 WADR[128][8];                                     /* window address registers */
int32 WINDR = 0;                                        /* window index register */
int32 WBUSR = 0;                                        /* window bus register */
int32 KINI = 0;                                         /* K init register */
int32 KSTS[10] = { 0000, 0001, 0002, 0377, 0377, 0377, 0377, 0377, 0377, 0377 };
int32 CDATA = 0;                                        /* control bus data */
int32 fl_csr = 0;
int32 fl_madr0 = 0;
int32 fl_madr1 = 0;
int32 fl_madr2 = 0;
int32 LOCADR = 0;                                       /* control mem low address */
int32 SDR = 0;                                          /* switch/display register */
int32 PCSR = 0;                                         /* Pio control/status */

extern DIB dli_dib;
extern DEVICE dli_dev;
extern int32 EADR, DADR;
extern t_addr cpu_memsize;
extern int32 trap_req;
extern int32 ipl;
extern int32 int_req[IPL_HLVL];
extern int32 R[8];
extern FILE *sim_log;
extern int32 autcon_enb;
extern int16 inst_pc;

t_stat ioc_rd (int32 *data, int32 pa, int32 access);
t_stat ioc_wr (int32 data, int32 pa, int32 access);
t_stat cwin_rd (int32 *data, int32 pa, int32 access);
t_stat cwin_wr (int32 data, int32 pa, int32 access);
t_stat rom_rd (int32 *data, int32 pa, int32 access);
t_stat rom_boot (int32 unitno, DEVICE *dptr);
t_stat fl_rd (int32 *data, int32 pa, int32 access);
t_stat fl_wr (int32 data, int32 pa, int32 access);
t_stat fl_svc (UNIT *uptr);
t_stat fl_reset (DEVICE *dptr);
t_stat fl_boot (int32 unitno, DEVICE *dptr);

extern int32 PReadB (int32 pa);
extern int32 ReadB (int32 va);
extern void PWriteB (int32 data, int32 addr);
extern void WriteB (int32 data, int32 addr);
extern void WriteW (int32 data, int32 addr);
extern int32 ReadW (int32 addr);
extern int32 calc_ints (int32 nipl, int32 trq);

/* Fixed I/O address table entries */

DIB ioc_dib = { IOBA_IOC, IOLN_IOC, &ioc_rd, &ioc_wr, 0 };
DIB cwin_dib = { IOBA_CWIN, IOLN_CWIN, &cwin_rd, &cwin_wr, 0 };

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
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", "VECTOR",
      &set_vec, &show_vec, NULL },
    { 0 }
    };

DEVICE fl_dev = {
    "RX", fl_unit, fl_reg, fl_mod,
    2, DEV_RDX, 20, 1, DEV_RDX, 8,
    NULL, NULL, &fl_reset,
    &fl_boot, NULL, NULL,
    &fl_dib, 0 //DEV_UBUS | DEV_QBUS
    };

int32 parity (int32 val, int32 odd)
{
for ( ; val != 0; val = val >> 1) {
    if (val & 1)
        odd = odd ^ 1;
    }
return odd;
}

UNIT ioc_unit = { UDATA (NULL, 0, 0) };

REG ioc_reg[] = {
    { ORDATA (SDR, SDR, 16) },
    { NULL }
    };

MTAB ioc_mod[] = {
    { 0 }
    };

DEVICE ioc_dev = {
    "IOC", &ioc_unit, ioc_reg, ioc_mod,
    1, 0, 0, 0, 0, 0,
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, 0, 0,
    NULL, NULL, NULL
    };

t_stat ioc_rd (int32 *data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 037;
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
//        sim_printf ("WADR%d rd %o\n", rg, *data);
        return SCPE_OK;

    case 010:                                           /* WINDR */
        *data = WINDR;
//        sim_printf ("WINDR rd: %o\n", *data);
        return SCPE_OK;

    case 011:                                           /* WBUSR */
        *data = WBUSR;
//        sim_printf ("WBUSR rd: %o\n", *data);
        return SCPE_OK;

    case 012:                                           /* low error */
        *data = EADR & 0177777;
//        sim_printf ("low error rd: %o\n", *data);
        return SCPE_OK;

    case 013:                                           /* high error */
        *data = (EADR >> 16) & 077;
//        sim_printf ("high error rd: %o\n", *data);
        return SCPE_OK;

    case 020:                                           /* Pio CSR */
        *data = PCSR | PCSR_ENA;
//        sim_printf ("Pio CSR rd: %o\n", *data);
        return SCPE_OK;

    case 021:                                           /* switch/display reg */
        *data = SDR & 07777;
//        sim_printf ("SDR rd: %o\n", *data);
        return SCPE_OK;

    case 022:                                           /* K init reg - NI */
        *data = KINI;
//        sim_printf ("K init rd\n", *data);
        return SCPE_OK;

    case 023:                                           /* K status reg - NI */
        *data = KSTS[(KINI & 017)];
        *data = *data | (parity (*data, 1) << 7);       /* odd parity bit */
//        sim_printf ("K status rd\n", *data);
        return SCPE_OK;

    case 024:                                           /* data mem low addr */
        *data = DADR & 0177777;
//        sim_printf ("data mem low rd: %o\n", *data);
        return SCPE_OK;

    case 025:                                           /* data mem high addr */
        *data = (DADR >> 16) & 07;
//        sim_printf ("data mem high rd: %o\n", *data);
        return SCPE_OK;

    case 026:                                           /* ctrl mem low addr - NI */
        *data = LOCADR;
//        sim_printf ("ctrl mem low rd: %o\n", *data);
        return SCPE_OK;

    case 027:                                           /* serial number reg - NI */
//        sim_printf ("serial num rd\n");
        *data = 0;
        return SCPE_OK;
        }
        
*data = 0;
return SCPE_NXM;
}

/* Search control memory to find the KI structure, or
 * at least that's what it's assumed to be. This starts
 * with an alternating pattern of 1s and 0s. How much
 * of control memory should be searched to find it?
 *
 * The offset to the KH (another assumption) structure
 * can then be found. The following location should
 * be the offset - 1. The software then expects the
 * requestor to negate the offset as some sort of
 * indication that it has been read.
 */

uint16 k_init_common ()
{
uint16 *CM = &M[016000000 >> 1];                        /* control memory */
uint16 ki, kh = 0;

for (ki = 0; ki < 20; ki++) {                           /* search for KI structure */
    if ((CM[ki] == 0125252) && (CM[ki + 1] == 052525)) {
        kh = CM[ki + 2];                                /* get offset to KH structure */
        if (CM[ki + 3] != (kh - 1))                     /* verify offset */
            break;                                      /* failed */
        CM[ki + 2] = -CM[ki + 2];                       /* indicate offset read */
        kh = (kh << 1);                                 /* convert to word offset */
        }
    }
return kh;
}

/* The value at offset 0164 starts out at 177777 and
 * the software expects the requestor to change this
 * value (to what?)
 *
 * The value at offset 044 is assumed to be a version
 * number for the requestor microcode. Must be at
 * least 2.44?
 */

void kci_init ()
{
uint16 *CM = &M[016000000 >> 1];                        /* control memory */
uint16 kh = k_init_common ();
CM[kh + (0164 >> 1)] = 0;                               /* KH$NAD? */
CM[kh + (044 >> 1)] = 01400;                            /* version? major = 3, minor = 0 */
}

/* The software expects the requestor to set location
 * 074 to a value of 1 or 2
 *
 * The microcode version must be at least 3.00
 */

void ksdi_init ()
{
uint16 *CM = &M[016000000 >> 1];                        /* control memory */
uint16 kh = k_init_common ();
CM[kh + (074 >> 1)] = 1;
CM[kh + (044 >> 1)] = 01400;                            /* version? major = 3, minor = 0 */
}

t_stat ioc_wr (int32 data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 037;
switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* WADR0 */
    case 001:                                           /* WADR1 */
    case 002:                                           /* WADR2 */
    case 003:                                           /* WADR3 */
    case 004:                                           /* WADR4 */
    case 005:                                           /* WADR5 */
    case 006:                                           /* WADR6 */
    case 007:                                           /* WADR7 */
//        sim_printf ("WADR%d wr: %o\n", rg, data);
        WADR[WINDR][rg] = data;
        return SCPE_OK;

    case 010:                                           /* WINDR */
//        sim_printf ("WINDR wr: %o\n", data);
        WINDR = data & 0177;
        return SCPE_OK;

    case 020:                                           /* Pio CSR */
//        sim_printf ("Pio CSR wr: %o\n", data);
        PCSR = data;
        return SCPE_OK;

    case 021:                                           /* switch/display reg */
//        sim_printf ("SDR wr: %o\n", data);
        SDR = data & 07777;
        return SCPE_OK;

    case 022:                                           /* K init reg - NI */
//        sim_printf ("K init wr: %o\n", data);
        if (~KINI & data & 01000) {                     /* K.ci run */
            sim_printf ("K.ci startup\n");
            kci_init ();
            }
        else if (~KINI & data & 02000) {                /* K.sdi run */
            sim_printf ("K.sdi startup\n");
            ksdi_init ();
            }
        KINI = data;
        return SCPE_OK;

    case 023:                                           /* K status reg - NI */
//        sim_printf ("K status wr: %o\n", data);
        KSTS[0] = data;                                 /* can only write to req 0 */
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
WBUSR = (pa >> 2);                                      /* high order address history */
LOCADR = (pa >> 1) & 1;                                 /* low order address history */
pa = pa | (016 << 18);                                  /* FIXME: add offset to control memory */
if (PCSR & PCSR_NMA) {                                  /* non-memory access cycle */
    LOCADR = LOCADR | 016;
    *data = CDATA;
    }
else {
    if (ADDR_IS_MEM (pa))                               /* memory address? */
        *data = (M[pa >> 1]);
    else {
        sim_printf ("Error: %o is not mem addr\n", pa);
        return SCPE_NXM;
        }
    if (PCSR & PCSR_CIE) {                              /* interrupt cycle */
        LOCADR = (LOCADR & ~016) | 04;
        if (*data & 010)
            SET_INT (CB7);
        if (*data & 04)
            SET_INT (CB6);
        if (*data & 02)
            SET_INT (CB5);
        if (*data & 01)
            SET_INT (CB4);
        trap_req = calc_ints (ipl, trap_req);           /* recalc int req */
        }
    else if (PCSR & PCSR_LCK) {                         /* lock cycle */
        LOCADR = (LOCADR & ~016) | 010;
        M[pa >> 1] = 0100000;
        }
    else LOCADR = (LOCADR & ~016) | 02;                 /* read/write cycle */
    CDATA = *data;                                      /* data history */
    }
if (sim_log)
    fprintf (sim_log, "CWIN rd: %06o = %06o (%o) at %o\n", (pa & ~(016 << 18)), *data, va, (uint16)inst_pc);
PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);        /* reset special cycle */
return SCPE_OK;
}

t_stat cwin_wr (int32 data, int32 va, int32 access)
{
int32 pa;
int32 rg;

rg = (va >> 9) & 07;                                    /* translate virtual address */
pa = (WADR[WINDR][rg] & 0177600) << 2;
pa = pa | (va & 0777);
WBUSR = (pa >> 2);                                      /* high order address history */
LOCADR = (pa >> 1) & 1;                                 /* low order address history */
if (sim_log)
    fprintf (sim_log, "CWIN wr: %06o = %06o (%o) at %o\n", pa, data, va, (uint16)inst_pc);
pa = pa | (016 << 18);                                  /* FIXME: add offset to control memory */
CDATA = data;                                           /* data history */
if (PCSR & PCSR_NMA) {                                  /* non-memory access cycle */
    LOCADR = LOCADR | 016;
    return SCPE_OK;
    }
if (PCSR & PCSR_CIE) {                                  /* interrupt cycle */
    LOCADR = (LOCADR & ~016) | 04;
    if (data & 010)
        SET_INT (CB7);
    if (data & 04)
        SET_INT (CB6);
    if (data & 02)
        SET_INT (CB5);
    if (data & 01)
        SET_INT (CB4);
    trap_req = calc_ints (ipl, trap_req);               /* recalc int req */
    }
else if (PCSR & PCSR_LCK) {                             /* lock cycle */
    LOCADR = (LOCADR & ~016) | 010;
    M[pa >> 1] = 0100000;
    }
else LOCADR = (LOCADR & ~016) | 02;                     /* read/write cycle */
PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);        /* reset special cycle */
if (ADDR_IS_MEM (pa)) {                                 /* memory address? */
    if (access == WRITEB) {
        if (pa & 1)
            M[pa >> 1] = (M[pa >> 1] & 0377) | (data << 8);
        else M[pa >> 1] = (M[pa >> 1] & ~0377) | data;
        }
    else
        M[pa >> 1] = data;
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

//sim_printf ("Boot device write, %o -> %X, %d\n", ma, da, sz);
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

//sim_printf ("Boot device read, %X, %d -> %o\n", da, sz, ma);
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
int32 unitno = (fl_csr >> FLCSR_V_SEL) & 1;

if ((fl_csr & FLCSR_ME) == 0) {                         /* motor not enabled? */
    *data = 0;                                          /* controller held in reset state */
    return SCPE_OK;
    }
switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* CSR */
        *data = fl_csr;
//        sim_printf ("fl_csr rd: %o at %o\n", *data, PC);
        return SCPE_OK;

    case 001:                                           /* MADR0 */
        *data = fl_madr0 | fl_unit[unitno].TRK;
//        sim_printf ("fl_madr0 rd: %o\n", *data);
        return SCPE_OK;

    case 002:                                           /* MADR1 */
        *data = fl_madr1;
//        sim_printf ("fl_madr1 rd: %o\n", *data);
        return SCPE_OK;

    case 003:                                           /* MADR2 */
        *data = fl_madr2;
//        sim_printf ("fl_madr2 rd: %o\n", *data);
        return SCPE_OK;
        }
return SCPE_NXM;
}

t_stat fl_wr (int32 data, int32 pa, int32 access)
{
int32 cmd;
int32 rg = (pa >> 1) & 03;
int32 unitno;
switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* CSR */
//        sim_printf ("fl_csr wr: %o at %o\n", data, PC);
        fl_csr = data & FLCSR_RW;
        if (fl_csr & FLCSR_ME) {                        /* motor enabled */
            cmd = data & FLCSR_CS;
            unitno = (data >> FLCSR_V_SEL) & 1;         /* get drive select */
            if ((cmd >> 4) == 015) {                    /* force interrupt? */
                sim_cancel (&fl_unit[unitno]);
                fl_csr = fl_csr & ~1;                   /* set not busy */
                if (cmd & 05) {
                    fl_unit[unitno].CMD = cmd;
                    fl_csr = fl_csr | 1;                /* set busy */
                    sim_activate (&fl_unit[unitno], FL_WAIT);
                    }
                }
            else if ((fl_csr & 1) == 0) {               /* not busy? */
                fl_unit[unitno].CMD = cmd;
                fl_csr = fl_csr | 1;                    /* set busy */
                sim_activate (&fl_unit[unitno], FL_WAIT);
                }
            }
        return SCPE_OK;

    case 001:                                           /* MADR0 */
//        sim_printf ("fl_madr0 wr: %o\n", data);
        fl_madr0 = data & FLMA0_RW;
        return SCPE_OK;

    case 002:                                           /* MADR1 */
//        sim_printf ("fl_madr1 wr: %o\n", data);
        fl_madr1 = data;
        return SCPE_OK;

    case 003:                                           /* MADR2 */
//        sim_printf ("fl_madr2 wr: %o\n", data);
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
    fl_csr = fl_csr | 0200;                             /* not ready */
    fl_setint ();
    return SCPE_OK;
    }
switch (uptr->CMD >> 5) {
    case 0:
        if (uptr->CMD & 020) {                          /* seek */
            uptr->TRK = fl_madr2 & FLMA2_DT;
//            sim_printf ("fl: seek track %d\n", uptr->TRK);
            fl_csr = fl_csr & ~1;                       /* set not busy */
            fl_csr = fl_csr | 04;
            fl_setint ();
            }
        else {                                          /* restore */
            uptr->TRK = 0;
//            sim_printf ("fl: restore\n");
            fl_csr = fl_csr & ~1;                       /* set not busy */
            fl_csr = fl_csr | 04;
            fl_setint ();
            }
        break;

    case 1:                                             /* step */
        if (uptr->DIR) {
            uptr->TRK++;
//            sim_printf ("fl: step in\n");
            }
        else {
            uptr->TRK--;
//            sim_printf ("fl: step out\n");
            }
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 2:                                             /* step in */
        uptr->DIR = 1;
        uptr->TRK++;
//        sim_printf ("fl: step in\n");
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 3:                                             /* step out */
        uptr->DIR = 0;
        uptr->TRK--;
//        sim_printf ("fl: step out\n");
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 4:                                             /* read sector */
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        sec = fl_madr1 & FLMA1_DS;
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), sec);
        if (uptr->CMD & 020) {
//            sim_printf ("fl: read multi sector\n");
            fl_read (uptr, da, (FL_NUMSC - sec), ma, FALSE);
            ma = ma + ((FL_NUMSC - sec + 1) * 512);
            //fl_madr1 = (fl_madr1 & ~FLMA1_DS) | 020;
            }
        else {
//            sim_printf ("fl: read single sector\n");
            fl_read (uptr, da, 1, ma, FALSE);
            ma = ma + 512;
            //fl_madr1 = (fl_madr1 & ~FLMA1_DS) | (sec + 1);
            }
        fl_madr0 = fl_madr0 & ~FLMA0_A0;
        fl_madr0 = fl_madr0 | ((ma << 8) & FLMA0_A0);
        fl_madr1 = fl_madr1 & ~FLMA1_A8;
        fl_madr1 = fl_madr1 | (ma & FLMA1_A8);
        fl_madr2 = fl_madr2 & ~FLMA2_A16;
        fl_madr2 = fl_madr2 | ((ma >> 8) & FLMA2_A16);
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 5:                                             /* write sector */
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        sec = fl_madr1 & FLMA1_DS;
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), sec);
        if ((da * 512) > uptr->hwmark)                  /* update hwmark */
            uptr->hwmark = (da * 512);
        if (uptr->CMD & 020) {
//            sim_printf ("fl: write multi sector\n");
            fl_write (uptr, da, (FL_NUMSC - sec), ma, FALSE);
            ma = ma + ((FL_NUMSC - sec + 1) * 512);
            //fl_madr1 = (fl_madr1 & ~FLMA1_DS) | 020;
            }
        else {
//            sim_printf ("fl: write single sector\n");
            fl_write (uptr, da, 1, ma, FALSE);
            ma = ma + 512;
            //fl_madr1 = (fl_madr1 & ~FLMA1_DS) | (sec + 1);
            }
        fl_madr0 = fl_madr0 & ~FLMA0_A0;
        fl_madr0 = fl_madr0 | ((ma << 8) & FLMA0_A0);
        fl_madr1 = fl_madr1 & ~FLMA1_A8;
        fl_madr1 = fl_madr1 | (ma & FLMA1_A8);
        fl_madr2 = fl_madr2 & ~FLMA2_A16;
        fl_madr2 = fl_madr2 | ((ma >> 8) & FLMA2_A16);
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 6:
        if (uptr->CMD & 020) {                          /* force interrupt */
//            sim_printf ("fl: force interrupt\n");
            fl_csr = fl_csr & ~1;                       /* set not busy */
            if (uptr->CMD & 017)
                fl_setint ();
            }
        else {                                          /* read address */
//            sim_printf ("fl: read address\n");
            fl_setint ();
            }
        break;

    case 7:
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), 1);
        if (uptr->CMD & 020) {                          /* write track */
//            sim_printf ("fl: write track\n");
            if ((da * 512) > uptr->hwmark)              /* update hwmark */
                uptr->hwmark = (da * 512);
            fl_write (uptr, da, FL_NUMSC, ma, FALSE);
            }
        else {                                          /* read track */
//            sim_printf ("fl: read track\n");
            fl_read (uptr, da, FL_NUMSC, ma, FALSE);
            }
        ma = ma + (FL_NUMSC * 512);
        fl_madr0 = fl_madr0 & ~FLMA0_A0;
        fl_madr0 = fl_madr0 | ((ma << 8) & FLMA0_A0);
        fl_madr1 = fl_madr1 & ~FLMA1_A8;
        fl_madr1 = fl_madr1 | (ma & FLMA1_A8);
        fl_madr2 = fl_madr2 & ~FLMA2_A16;
        fl_madr2 = fl_madr2 | ((ma >> 8) & FLMA2_A16);
        fl_csr = fl_csr & ~1;                           /* set not busy */
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
fl_madr1 = 0;
fl_madr2 = 0;
return SCPE_OK;
}

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
//uint32 rg = ((pa >> 1) & 01777);                        /* decode PA<1> */
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

/* Build I/O space entries for IOC */

t_stat ioc_build_dib (void)
{
t_stat r;

autcon_enb = 0;                                         /* turn off autoconfigration */
fl_dev.flags |= (DEV_UBUS | DEV_QBUS);
dli_dev.flags &= ~(DEV_DISABLE | DEV_DIS);
dli_dib.ba = IOBA_DL;
dli_dib.vec = VEC_DL;
if (r = build_ubus_tab (&ioc_dev, &ioc_dib))            /* add to dispatch tab */
     return r;
if (r = build_ubus_tab (&ioc_dev, &cwin_dib))           /* add to dispatch tab */
     return r;
return SCPE_OK;
}
