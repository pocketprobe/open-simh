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
int32 KINI = 0;                                         /* K init register */
int32 KSTS[10] = { 0000, 0001, 0002, 0377, 0377, 0377, 0377, 0377, 0377, 0377 };
int32 fl_csr = 0;
int32 fl_madr0 = 0;
int32 fl_madr1 = 0;
int32 fl_madr2 = 0;
int32 SDR = 0;                                          /* switch/display register */
int32 PCSR = 0;                                         /* Pio control/status */
uint16 kci_kh;

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
extern int32 CADR, CDATA, CCYCLE;

t_stat ioc_rd (int32 *data, int32 pa, int32 access);
t_stat ioc_wr (int32 data, int32 pa, int32 access);
t_stat cwin_rd (int32 *data, int32 va, int32 access);
t_stat cwin_wr (int32 data, int32 va, int32 access);
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

t_stat kci_show (int32 flag, CONST char *ptr);
t_stat kci_process (int32 flag, CONST char *ptr);

/* Special boot command, overrides regular boot */

CTAB hsc_cmd[] = {
    { "KCI", &kci_show, 0,
      "kci                   dump K.ci structures\n", NULL, NULL },
    { "KPR", &kci_process, 0,
      "kpr                   process K.ci queue\n", NULL, NULL },
    { NULL }
    };

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
        *data = (CADR >> 1);
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

    case 026:                                           /* ctrl mem low addr */
        *data = (CCYCLE << 1) | (CADR & 1);
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

#define GETW(off)       RdxMemW (CM, off)
#define GETB(off)       RdxMemB (CM, off)
#define PUTW(off, val)  WrxMemW (CM, off, val)
#define PUTB(off, val)  WrxMemB (CM, off, val)
#define GETP(off)       ((GETW (off) & 0077776) ? (GETW (off) << 2) : GETW (off))
#define PUTP(off, val)  PUTW (off, (val >> 2))

#define Q_LOCKED(q)     (GETW (q) & 0100000)

/* transmission queue */

#define Q_SRVC          0004                            /* service queue pointer */
#define Q_PROC          0002
#define Q_FLGS          0001
#define Q_HEAD          0000                            /* Resource queue head */
#define Q_TAIL          0002                            /* Resource queue tail */
#define Q_WTHD          0004                            /* Waiter queue head */
#define Q_WTTL          0006                            /* Waiter queue tail */

#define FLGS_IQ         0100                            /* interrupt queue */
#define FLGS_INT        0200                            /* interrupt pending */

#define KI_C1           0000                            /* check value 1 - 125252 */
#define KI_C2           0002                            /* check value 2 - 052525 */
#define KI_KIA          0004                            /* K init address */
#define KI_CHK          0006                            /* check value */

#define KG_VRS          0044                            /* microcode version */
#define KG_RSL          0074

#define KH_LNK          0000
#define KH_2WK          0010
#define KH_NAD          0164                            /* node address */

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
uint16 ki, kh = 0, chk;

for (ki = 0; ki < 20; ki = ki + 2) {                     /* search for KI structure */
    if ((GETW (ki) == 0125252) && (GETW (ki + 2) == 052525)) {
        kh = GETW (ki + KI_KIA);                        /* get offset to KH structure */
        if (GETW (ki + KI_CHK) != (kh - 1))             /* verify offset */
            break;                                      /* failed */
        PUTW (ki + KI_KIA, -kh);
        kh = kh << 2;                                   /* convert to byte offset */
        }
    }
return kh;
}

/* The microcode version must be at least 2.44 */

void kci_init ()
{
uint16 kh = k_init_common ();
kci_kh = kh;
sim_printf ("K.ci startup: KH = %06o at %o\n", kh, (uint16)inst_pc);
PUTW (kh + KH_NAD, 3);                                  /* KH$NAD - CI node address */
PUTW (kh + KG_VRS, 01400);                              /* KG$VRS - version (major = 3, minor = 0) */
}

/* The software expects the requestor to set location
 * 074 to a value of 1 or 2
 *
 * The microcode version must be at least 3.00
 */

void ksdi_init (uint32 req)
{
uint16 kh = k_init_common ();
sim_printf ("K.sdi %d startup: KH = %06o at %o\n", req, kh, (uint16)inst_pc);
PUTW (kh + KG_RSL, 1);                                  /* KG$RSL? */
PUTW (kh + KG_VRS, 01400);                              /* KG$VRS - version (major = 3, minor = 0) */
}

void print_word (uint16 kh, char *name, uint16 off, uint16 len)
{
if (len == 0)
    sim_printf ("%s %06o\n", name, kh + off);
else if (len == 1) {
    sim_printf ("%s %06o\\  %03o\n", name, kh + off, GETB (kh + off));
    }
else {
    sim_printf ("%s %06o/", name, kh + off);
    for (int i = 1; i <= (len >> 1); i++) {
        sim_printf ("  %06o", GETW (kh + off + (i << 1)));
        if (i % 8 == 0)
            sim_printf ("\n              ");
        }
    sim_printf ("\n");
    }
}

t_stat kci_show (int32 flag, CONST char *ptr)
{
uint16 kh = kci_kh;
uint16 wk2h, wk2t;

print_word (kh, "KH$LNK", KH_LNK, 2);

wk2h = kh + KH_LNK;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   LNK", 0000, 2);
    print_word (wk2h, "   LNK", 0002, 2);
    print_word (wk2h, "   LNK", 0004, 2);
    print_word (wk2h, "   LNK", 0006, 2);
    print_word (wk2h, "   LNK", 0010, 2);
    print_word (wk2h, "   LNK", 0012, 2);
    print_word (wk2h, "   LNK", 0014, 2);
    print_word (wk2h, "   LNK", 0016, 2);
    print_word (wk2h, "   LNK", 0020, 2);
}

print_word (kh, "KH$FLG", 0002, 2);
print_word (kh, "KH$3WK", 0004, 4);
print_word (kh, "KH$2WK", 0010, 4);
wk2h = kh + KH_2WK;
for (;;) {
    wk2h = GETP (wk2h);
    if ((wk2h == 0) || wk2h == 0100000)
        break;
    print_word (wk2h, "   2WK", 0000, 2);
    print_word (wk2h, "   2WK", 0002, 2);
    print_word (wk2h, "   2WK", 0004, 2);
    print_word (wk2h, "   2WK", 0006, 2);
    print_word (wk2h, "   2WK", 0010, 2);
    print_word (wk2h, "   2WK", 0012, 2);
    print_word (wk2h, "   2WK", 0014, 2);
    print_word (wk2h, "   2WK", 0016, 2);
    print_word (wk2h, "   2WK", 0020, 2);
    sim_printf ("   ---\n");
    }
print_word (kh, "KH$1WK", 0014, 4);
print_word (kh, "KH$0WK", 0020, 4);
print_word (kh, "KH$EQ ", 0024, 4);
print_word (kh, "KH$SRR", 0030, 4);
print_word (kh, "KH$DRR", 0034, 4);
print_word (kh, "KG$TLK", 0040, 2);
print_word (kh, "KG$SLT", 0042, 1);
print_word (kh, "KG$TYP", 0043, 1);
print_word (kh, "KG$VRS", 0044, 2);
print_word (kh, "KG$FCB", 0046, 2);
print_word (kh, "KG$FBU", 0050, 2);
print_word (kh, "KG$BWS", 0052, 2);
print_word (kh, "KG$TMR", 0054, 2);
print_word (kh, "KG$DMP", 0056, 2);

wk2h = kh + 0056;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   DMP", 0000, 2);
    print_word (wk2h, "   DMP", 0002, 2);
    print_word (wk2h, "   DMP", 0004, 2);
    print_word (wk2h, "   DMP", 0006, 2);
    print_word (wk2h, "   DMP", 0010, 2);
    print_word (wk2h, "   DMP", 0012, 2);
    print_word (wk2h, "   DMP", 0014, 2);
    print_word (wk2h, "   DMP", 0016, 2);
    print_word (wk2h, "   DMP", 0020, 2);
}

print_word (kh, "KG$DCA", 0060, 2);
print_word (kh, "KG$TST", 0062, 2);
print_word (kh, "KG$FRS", 0064, 2);
print_word (kh, "KG$LAS", 0066, 2);
print_word (kh, "KG$MDL", 0070, 0);
print_word (kh, "KG$MTY", 0070, 2);
print_word (kh, "KG$PFL", 0072, 2);
print_word (kh, "KG$RSL", 0074, 2);
print_word (kh, "KG$LTR", 0076, 2);
print_word (kh, "KG$PSA", 0100, 2);
print_word (kh, "KG$PSB", 0102, 2);
print_word (kh, "KG$DSA", 0104, 20);
print_word (kh, "KG$MC0", 0130, 8);
print_word (kh, "KG$KSA", 0140, 0);
print_word (kh, "KH$D2T", 0140, 2);
print_word (kh, "KH$CIM", 0142, 2);

wk2h = kh + 0142;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   CIM", 0000, 2);
    print_word (wk2h, "   CIM", 0002, 2);
    print_word (wk2h, "   CIM", 0004, 2);
    print_word (wk2h, "   CIM", 0006, 2);
    print_word (wk2h, "   CIM", 0010, 2);
    print_word (wk2h, "   CIM", 0012, 2);
    print_word (wk2h, "   CIM", 0014, 2);
    print_word (wk2h, "   CIM", 0016, 2);
    print_word (wk2h, "   CIM", 0020, 2);
}

print_word (kh, "KH$D1T", 0144, 2);
print_word (kh, "KH$VA ", 0146, 2);
print_word (kh, "KH$D0T", 0150, 2);
print_word (kh, "KH$CVS", 0152, 2);
print_word (kh, "KH$TMR", 0154, 4);
print_word (kh, "KH$BFR", 0160, 4);
print_word (kh, "KH$NAD", 0164, 2);
print_word (kh, "KH$PDS", 0166, 1);
print_word (kh, "KH$KDS", 0167, 1);
print_word (kh, "KH$MBA", 0170, 1);
print_word (kh, "KH$MXD", 0171, 1);
print_word (kh, "KH$BSV", 0172, 2);
print_word (kh, "KH$ABQ", 0174, 2);

wk2h = kh + 0174;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   ABQ", 0000, 2);
    print_word (wk2h, "   ABQ", 0002, 2);
    print_word (wk2h, "   ABQ", 0004, 2);
    print_word (wk2h, "   ABQ", 0006, 2);
    print_word (wk2h, "   ABQ", 0010, 2);
    print_word (wk2h, "   ABQ", 0012, 2);
    print_word (wk2h, "   ABQ", 0014, 2);
    print_word (wk2h, "   ABQ", 0016, 2);
    print_word (wk2h, "   ABQ", 0020, 2);
}

return SCPE_OK;
}

uint16 get_from_queue (uint16 q)
{
uint16 qh;

qh = GETP (q + Q_HEAD);
if ((qh != 0) && (qh != 0100000)) {                     /* not empty or locked? */
    PUTP (q + Q_HEAD, GETP (qh));                       /* point queue head to next item */
    if (GETP (qh) == 0)                                 /* no next item? */
        PUTP (q + Q_TAIL, 0);                           /* queue now empty */
    PUTP (qh, 0);                                       /* remove next link from removed item */
    sim_printf ("Item %06o removed from queue at address %06o\n", qh, q);
    }
return qh;
}

void put_to_queue (uint16 q, uint16 itm)
{
uint16 qh, qt, qf, tq, sq, mask;

qh = GETP (q + Q_HEAD);
if (qh == 0100000) {                                    /* should not happen */
    sim_printf ("put_to_queue: queue at address %06o is locked\n", q);
    return;
    }
if (qh == 1) {    /* software process waiting on queue */
    qh = 0;
    qf = GETB (q - Q_FLGS);                             /* get queue flags */
    if (qf & FLGS_IQ) {                                 /* interrupt queue */
        if ((qf & FLGS_INT) == 0) {                     /* no interrupt pending */
            mask = GETW (q - Q_SRVC);                   /* get interrupt mask */
            if (mask & 017)
                qf |= FLGS_INT;
            if (mask & 010)
                SET_INT (CB7);
            if (mask & 04)
                SET_INT (CB6);
            if (mask & 02)
                SET_INT (CB5);
            if (mask & 01)
                SET_INT (CB4);
            trap_req = calc_ints (ipl, trap_req);       /* recalc int req */
            PUTB (q - Q_FLGS, qf);                      /* interrupt now pending */
            sim_printf ("Interrupt for queue %06o\n", q);
            }
        }
    else {                                              /* transmission queue */
        tq = q - Q_SRVC;                                /* get base of transmission queue */
        sq = GETP (tq);                                 /* get service queue pointer */
        if (sq) {
            PUTP (tq, 0);                               /* clear service queue pointer */
            sim_printf ("Sending transmission queue %06o to service queue %06o\n", tq, sq);
            put_to_queue (sq, tq);                      /* send transmission queue to service queue */
            }
        }
    }
if (qh == 0) {                                          /* queue empty */
    PUTP (q + Q_HEAD, itm);                             /* set head */
    PUTP (q + Q_TAIL, itm);                             /* set tail */
    }
else {                                                  /* queue is not empty */
    qt = GETP (q + Q_TAIL);                             /* get the tail pointer */
    if (GETW (qt) != 0)                                 /* should not happen */
        sim_printf ("Queue tail item has non-zero pointer: %06o\n", GETW (qt));
    else {                                              /* add to end of queue */
        PUTP (qt, itm);                                 /* add link to item */
        PUTP (q + Q_TAIL, itm);                         /* update queue tail pointer */
        }
    }
sim_printf ("Item %06o added to queue at address %06o\n", itm, q);
}

t_stat kci_process (int32 flag, CONST char *ptr)
{
uint16 kh = kci_kh;
uint16 wq, tq, rsc;

wq = kh + KH_2WK;
for (;;) {
    rsc = GETP (wq + Q_HEAD);                           /* get the resource pointer */
    if (rsc == 0)                                       /* queue is empty */
        break;
    if (rsc == 0100000) {                               /* queue is locked */
        sim_printf ("Source queue currently locked\n");
        break;
        }
    tq = GETP (rsc + 0004);                             /* get destination queue pointer */
    if (tq == 0) {                                      /* should not happen */
        sim_printf ("Resource has no desitnation queue\n");
        break;
        }
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        break;
        }

    rsc = get_from_queue (wq);                          /* get resource from work queue */
                                                        /* TODO: process resource */
    put_to_queue (tq, rsc);                             /* put resource to destination queue */
    }

return SCPE_OK;
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
        if (~KINI & data & 01000)                       /* K.ci run */
            kci_init ();
        else if (~KINI & data & 0100)                   /* K.sdi run */
            ksdi_init (8);
        else if (~KINI & data & 0200)                   /* K.sdi run */
            ksdi_init (9);
        else {
            for (int i = 10; i < 16; i++) {             /* K.sdi run */
                if (~KINI & data & (1u << i)) {
                    ksdi_init (i - 10);
                    break;
                    }
                }
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
pa = pa + CMEMBASE;                                     /* FIXME: add offset to control memory */
if (ADDR_IS_CMEM (pa))                                  /* memory address? */
    *data = hsc_RdMemW (pa);
else {
    sim_printf ("Error: %o is not mem addr\n", pa);
    return SCPE_NXM;
    }
//if (sim_log)
//    fprintf (sim_log, "CWIN rd: %06o = %06o (%o) at %o\n", (pa & CMEMMASK), *data, va, (uint16)inst_pc);
return SCPE_OK;
}

t_stat cwin_wr (int32 data, int32 va, int32 access)
{
int32 pa;
int32 rg;

rg = (va >> 9) & 07;                                    /* translate virtual address */
pa = (WADR[WINDR][rg] & 0177600) << 2;
pa = pa | (va & 0777);
//if (sim_log)
//    fprintf (sim_log, "CWIN wr: %06o = %06o (%o) at %o\n", (pa & CMEMMASK), data, va, (uint16)inst_pc);
pa = pa + CMEMBASE;                                     /* FIXME: add offset to control memory */
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
sim_vm_cmd = hsc_cmd;
return SCPE_OK;
}
