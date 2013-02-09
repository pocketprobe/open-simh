/* pdp11_cpumod.c: PDP-11 CPU model-specific features

   Copyright (c) 2004-2022, Robert M Supnik

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
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   system       PDP-11 model-specific registers

   19-Nov-22    RMS     Fixed byte access errors in PIRQ, STKLIM, CDR (Walter Mueller)
   15-Sep-20    RMS     Fixed problem in KDJ11E programmable clock (Paul Koning)
   04-Mar-16    RMS     Fixed maximum memory sizes to exclude IO page
   14-Mar-16    RMS     Modified to keep cpu_memsize in sync with MEMSIZE
   06-Jun-13    RMS     Fixed change model to set memory size last
   20-May-08    RMS     Added JCSR default for KDJ11B, KDJ11E
   22-Apr-08    RMS     Fixed write behavior of 11/70 MBRK, LOSIZE, HISIZE
                        (Walter Mueller)
   29-Apr-07    RMS     Don't run bus setup routine during RESTORE
   30-Aug-05    RMS     Added additional 11/60 registers
   16-Aug-05    RMS     Fixed C++ declaration and cast problems
   15-Feb-05    RMS     Fixed bug in SHOW MODEL (Sergey Okhapkin)
   19-Jan-05    RMS     Added variable SYSID, MBRK write (Tim Chapman)

   This module includes CPU- and system-specific registers, such as the Unibus
   map and control registers on 22b Unibus systems, the board registers for the
   F11- and J11-based systems, and the system registers for the PDP-11/44,
   PDP-11/45, PDP-11/60, and PDP-11/70.  Most registers are implemented at
   a minimum level: just enough to satisfy the machine identification code
   in the various operating systems.
*/

#include "pdp11_defs.h"
#include "pdp11_cpumod.h"

/* Byte write macros for system registers

   EVN_IGN      byte writes to the even byte are ignored
   ODD_IGN      byte writes to the odd byte are ignored
   ODD_SHF      byte writes to the odd byte zero the even byte
   ODD_MRG      byte writes write appropriate byte
*/

#define EVN_IGN(cur) \
    if ((access == WRITEB) && ((pa & 1) == 0)) \
        return SCPE_OK
#define ODD_IGN(cur) \
    if ((access == WRITEB) && (pa & 1)) \
        return SCPE_OK
#define ODD_SHF(cur) \
    if ((access == WRITEB) && (pa & 1)) \
        cur = cur << 8
#define ODD_MRG(prv,cur) \
    if (access == WRITEB) \
        cur =((pa & 1)? (((prv) & 0377) | ((cur) & 0177400)) : \
                        (((prv) & 0177400) | ((cur) & 0377)))

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

int32 SR = 0;                                           /* switch register */
int32 DR = 0;                                           /* display register */
int32 SDR = 0;                                          /* switch/display register */
int32 PCSR = 0;                                         /* Pio control/status */
int32 MBRK = 0;                                         /* 11/70 microbreak */
int32 SYSID = 0x1234;                                   /* 11/70 system ID */
int32 WCS = 0;                                          /* 11/60 WCS control */
int32 CPUERR = 0;                                       /* CPU error reg */
int32 MEMERR = 0;                                       /* memory error reg */
int32 CCR = 0;                                          /* cache control reg */
int32 HITMISS = 0;                                      /* hit/miss reg */
int32 MAINT = 0;                                        /* maint reg */
int32 JCSR = 0;                                         /* J11 control */
int32 JCSR_dflt = 0;                                    /* J11 boot ctl def */
int32 JPCR = 0;                                         /* J11 page ctrl */
int32 JASR = 0;                                         /* J11 addtl status */
int32 UDCR = 0;                                         /* UBA diag ctrl */
int32 UDDR = 0;                                         /* UBA diag data */
int32 UCSR = 0;                                         /* UBA control */
int32 WADR[128][8];
int32 WIDX = 0;
int32 WBUS = 0;
int32 KINI = 0;                                         /* K init reg */
int32 KSTS[10] = { 0000, 0004, 0002, 0377, 0377, 0377, 0377, 0377, 0377, 0377 };
int32 CDATA = 0;
int32 fl_csr = 0;
int32 fl_madr0 = 0;
int32 fl_madr1 = 0;
int32 fl_madr2 = 0;
int32 LOCADR = 0;
int32 uba_last = 0;                                     /* UBA last mapped */
int32 ub_map[UBM_LNT_LW] = { 0 };                       /* UBA map array */
int32 toy_state = 0;
uint8 toy_data[TOY_LNT] = { 0 };
static int32 clk_tps_map[4] = { 0, 50, 60, 800 };       /* 0 = use BEVENT */
DIB fl_dib;

extern int32 R[8];
extern int32 STKLIM, PIRQ;
extern int32 clk_fie, clk_fnxm, clk_tps, clk_default;
extern DIB dli_dib;
extern int32 EADR, DADR;
extern t_addr cpu_memsize;
extern int32 trap_req;
extern int32 ipl;
extern int32 int_req[IPL_HLVL];
extern FILE *sim_log;

t_stat CPU24_rd (int32 *data, int32 addr, int32 access);
t_stat CPU24_wr (int32 data, int32 addr, int32 access);
t_stat CPU44_rd (int32 *data, int32 addr, int32 access);
t_stat CPU44_wr (int32 data, int32 addr, int32 access);
t_stat CPU45_rd (int32 *data, int32 addr, int32 access);
t_stat CPU45_wr (int32 data, int32 addr, int32 access);
t_stat CPU60_rd (int32 *data, int32 addr, int32 access);
t_stat CPU60_wr (int32 data, int32 addr, int32 access);
t_stat CPU70_rd (int32 *data, int32 addr, int32 access);
t_stat CPU70_wr (int32 data, int32 addr, int32 access);
t_stat CPUJ_rd (int32 *data, int32 addr, int32 access);
t_stat CPUJ_wr (int32 data, int32 addr, int32 access);
t_stat REG_rd (int32 *data, int32 addr, int32 access);
t_stat REG_wr (int32 data, int32 addr, int32 access);
t_stat SR_rd (int32 *data, int32 addr, int32 access);
t_stat DR_wr (int32 data, int32 addr, int32 access);
t_stat CTLFB_rd (int32 *data, int32 addr, int32 access);
t_stat CTLFB_wr (int32 data, int32 addr, int32 access);
t_stat CTLJB_rd (int32 *data, int32 addr, int32 access);
t_stat CTLJB_wr (int32 data, int32 addr, int32 access);
t_stat CTLJD_rd (int32 *data, int32 addr, int32 access);
t_stat CTLJD_wr (int32 data, int32 addr, int32 access);
t_stat CTLJE_rd (int32 *data, int32 addr, int32 access);
t_stat CTLJE_wr (int32 data, int32 addr, int32 access);
t_stat UBA24_rd (int32 *data, int32 addr, int32 access);
t_stat UBA24_wr (int32 data, int32 addr, int32 access);
t_stat UBAJ_rd (int32 *data, int32 addr, int32 access);
t_stat UBAJ_wr (int32 data, int32 addr, int32 access);
t_stat ioctl_rd (int32 *data, int32 pa, int32 access);
t_stat ioctl_wr (int32 data, int32 pa, int32 access);
t_stat ctlwin_rd (int32 *data, int32 pa, int32 access);
t_stat ctlwin_wr (int32 data, int32 pa, int32 access);
t_stat rom_rd (int32 *data, int32 pa, int32 access);
t_stat fl_rd (int32 *data, int32 pa, int32 access);
t_stat fl_wr (int32 data, int32 pa, int32 access);
t_stat fl_svc (UNIT *uptr);
t_stat fl_reset (DEVICE *dptr);
t_stat fl_boot (int32 unitno, DEVICE *dptr);
t_stat sys_reset (DEVICE *dptr);
int32 toy_read (void);
void toy_write (int32 bit);
uint8 toy_set (int32 val);
t_stat sys_set_jclk_dflt (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat sys_show_jclk_dflt (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static t_stat sys_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
static const char *sys_description (DEVICE *dptr);


extern t_stat PSW_rd (int32 *data, int32 addr, int32 access);
extern t_stat PSW_wr (int32 data, int32 addr, int32 access);
extern t_stat APR_rd (int32 *data, int32 addr, int32 access);
extern t_stat APR_wr (int32 data, int32 addr, int32 access);
extern t_stat MMR012_rd (int32 *data, int32 addr, int32 access);
extern t_stat MMR012_wr (int32 data, int32 addr, int32 access);
extern t_stat MMR3_rd (int32 *data, int32 addr, int32 access);
extern t_stat MMR3_wr (int32 data, int32 addr, int32 access);
extern t_stat ubm_rd (int32 *data, int32 addr, int32 access);
extern t_stat ubm_wr (int32 data, int32 addr, int32 access);
extern void put_PIRQ (int32 val);
extern int32 PReadB (int32 pa);
extern int32 ReadB (int32 va);
extern void PWriteB (int32 data, int32 addr);
extern void WriteB (int32 data, int32 addr);
extern void WriteW (int32 data, int32 addr);
extern int32 ReadW (int32 addr);
extern int32 calc_ints (int32 nipl, int32 trq);

/* Fixed I/O address table entries */

DIB psw_dib = { IOBA_PSW, IOLN_PSW, &PSW_rd, &PSW_wr, 0 };
DIB cpuj_dib = { IOBA_CPU, IOLN_CPU, &CPUJ_rd, &CPUJ_wr, 0 };
DIB cpu24_dib = { IOBA_CPU, IOLN_CPU, &CPU24_rd, &CPU24_wr, 0 };
DIB cpu44_dib = { IOBA_CPU, IOLN_CPU, &CPU44_rd, &CPU44_wr, 0 };
DIB cpu45_dib = { IOBA_CPU, IOLN_CPU, &CPU45_rd, &CPU45_wr, 0 };
DIB cpu60_dib = { IOBA_CPU, IOLN_CPU, &CPU60_rd, &CPU60_wr, 0 };
DIB cpu70_dib = { IOBA_CPU, IOLN_CPU, &CPU70_rd, &CPU70_wr, 0 };
DIB reg_dib = { IOBA_GPR, IOLN_GPR, &REG_rd, &REG_wr, 0 };
DIB ctlfb_dib = { IOBA_CTL, IOLN_CTL, &CTLFB_rd, &CTLFB_wr };
DIB ctljb_dib = { IOBA_CTL, IOLN_CTL, &CTLJB_rd, &CTLJB_wr };
DIB ctljd_dib = { IOBA_CTL, IOLN_CTL, &CTLJD_rd, &CTLJD_wr };
DIB ctlje_dib = { IOBA_CTL, IOLN_CTL, &CTLJE_rd, &CTLJE_wr };
DIB uba24_dib = { IOBA_UCTL, IOLN_UCTL, &UBA24_rd, &UBA24_wr };
DIB ubaj_dib = {IOBA_UCTL, IOLN_UCTL, &UBAJ_rd, &UBAJ_wr };
DIB supv_dib = { IOBA_SUP, IOLN_SUP, &APR_rd, &APR_wr, 0 };
DIB kipdr_dib = { IOBA_KIPDR, IOLN_KIPDR, &APR_rd, &APR_wr, 0 };
DIB kdpdr_dib = { IOBA_KDPDR, IOLN_KDPDR, &APR_rd, &APR_wr, 0 };
DIB kipar_dib = { IOBA_KIPAR, IOLN_KIPAR, &APR_rd, &APR_wr, 0 };
DIB kdpar_dib = { IOBA_KDPAR, IOLN_KDPAR, &APR_rd, &APR_wr, 0 };
DIB uipdr_dib = { IOBA_UIPDR, IOLN_UIPDR, &APR_rd, &APR_wr, 0 };
DIB udpdr_dib = { IOBA_UDPDR, IOLN_UDPDR, &APR_rd, &APR_wr, 0 };
DIB uipar_dib = { IOBA_UIPAR, IOLN_UIPAR, &APR_rd, &APR_wr, 0 };
DIB udpar_dib = { IOBA_UDPAR, IOLN_UDPAR, &APR_rd, &APR_wr, 0 };
DIB sr_dib = { IOBA_SR, IOLN_SR, &SR_rd, NULL, 0 };
DIB dr_dib = { IOBA_SR, IOLN_SR, NULL, &DR_wr, 0 };
DIB mmr012_dib = { IOBA_MMR012, IOLN_MMR012, &MMR012_rd, &MMR012_wr, 0 };
DIB mmr3_dib = { IOBA_MMR3, IOLN_MMR3, &MMR3_rd, &MMR3_wr, 0 };
DIB ubm_dib = { IOBA_UBM, IOLN_UBM, &ubm_rd, &ubm_wr, 0 };
DIB ioctl_dib = { (IOPAGEBASE + 010000), 060, &ioctl_rd, &ioctl_wr, 0 };
DIB ctlwin_dib = { IOPAGEBASE, 010000, &ctlwin_rd, &ctlwin_wr, 0 };
DIB rom_dib = { (IOPAGEBASE + 013000), 04000, &rom_rd, NULL, 0 };

CPUTAB cpu_tab[MOD_MAX] = {
    { "11/03", SOP_1103, OPT_1103, MEMSIZE64K, PSW_1103,
       0, 0, 0, 0, 0 },
    { "11/04", SOP_1104, OPT_1104, MEMSIZE64K, PSW_1104,
       0, 0, 0, 0, 0 },
    { "11/05", SOP_1105, OPT_1105, MEMSIZE64K, PSW_1105,
       0, 0, 0, 0, 0 },
    { "11/20", SOP_1120, OPT_1120, MEMSIZE64K, PSW_1120,
       0, 0, 0, 0, 0 },
    { "11/23", SOP_1123, OPT_1123, MAXMEMSIZE, PSW_F,
       MFPT_F, PAR_F, PDR_F, MM0_F, MM3_F },
    { "11/23+", SOP_1123P, OPT_1123P, MAXMEMSIZE, PSW_F,
       MFPT_F, PAR_F, PDR_F, MM0_F, MM3_F },
    { "11/24", SOP_1124, OPT_1124, MAXMEMSIZE, PSW_F,
       MFPT_F, PAR_F, PDR_F, MM0_F, MM3_F },
    { "11/34", SOP_1134, OPT_1134, UNIMEMSIZE, PSW_1134,
       0, PAR_1134, PDR_1134, MM0_1134, 0 },
    { "11/40", SOP_1140, OPT_1140, UNIMEMSIZE, PSW_1140,
       0, PAR_1140, PDR_1140, MM0_1140, 0 },
    { "11/44", SOP_1144, OPT_1144, MAXMEMSIZE, PSW_1144,
       MFPT_44, PAR_1144, PDR_1144, MM0_1144, MM3_1144 },
    { "11/45", SOP_1145, OPT_1145, UNIMEMSIZE, PSW_1145,
       0, PAR_1145, PDR_1145, MM0_1145, MM3_1145 },
    { "11/53", SOP_1153, OPT_1153, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "11/60", SOP_1160, OPT_1160, UNIMEMSIZE, PSW_1160,
       0, PAR_1160, PDR_1160, MM0_1160, 0 },
    { "11/70", SOP_1170, OPT_1170, MAXMEMSIZE, PSW_1170,
       0, PAR_1170, PDR_1170, MM0_1170, MM3_1170 },
    { "11/73", SOP_1173, OPT_1173, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "11/73B", SOP_1173B, OPT_1173B, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "11/83", SOP_1183, OPT_1183, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "11/84", SOP_1184, OPT_1184, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "11/93", SOP_1193, OPT_1193, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "11/94", SOP_1194, OPT_1194, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J },
    { "HSC70", SOP_HSC70, OPT_HSC70, MAXMEMSIZE, PSW_J,
       MFPT_J, PAR_J, PDR_J, MM0_J, MM3_J }
    };

CNFTAB cnf_tab[] = {
    { HAS_PSW,  0, &psw_dib },                          /* PSW */
    { CPUT_J,   0, &cpuj_dib },                         /* CPU control */
    { CPUT_24,  0, &cpu24_dib },
    { CPUT_44,  0, &cpu44_dib },
    { CPUT_45,  0, &cpu45_dib },
    { CPUT_60,  0, &cpu60_dib },
    { CPUT_70,  0, &cpu70_dib },
    { CPUT_HSC70,  0, &ioctl_dib },
    { CPUT_HSC70,  0, &ctlwin_dib },
    { CPUT_HSC70,  0, &rom_dib },
    { CPUT_HSC70,  0, &dli_dib },
    { CPUT_HSC70,  0, &fl_dib },
    { HAS_IOSR, 0, &reg_dib },
    { CPUT_23P, 0, &ctlfb_dib },                        /* board ctls */
    { CPUT_JB,  0, &ctljb_dib },
    { CPUT_53,  0, &ctljd_dib },
    { CPUT_JE,  0, &ctlje_dib },
    { CPUT_24,  0, &uba24_dib },                        /* UBA */
    { CPUT_JU,  0, &ubaj_dib },
    { 0, OPT_MMU,  &kipdr_dib },                        /* MMU */
    { 0, OPT_MMU,  &kipar_dib },
    { 0, OPT_MMU,  &uipdr_dib },
    { 0, OPT_MMU,  &uipar_dib },
    { 0, OPT_MMU,  &mmr012_dib },                       /* MMR0-2 */
    { HAS_MMR3, 0, &mmr3_dib },                         /* MMR3 */
    { 0, OPT_UBM,  &ubm_dib },                          /* Unibus map */
    { HAS_SID,  0, &kdpdr_dib },                        /* supv, I/D */
    { HAS_SID,  0, &kdpar_dib },
    { HAS_SID,  0, &supv_dib },
    { HAS_SID,  0, &udpdr_dib },
    { HAS_SID,  0, &udpar_dib },
    { HAS_SR,   0, &sr_dib },                           /* SR */
    { HAS_DR,   0, &dr_dib },                           /* DR */
    { 0, 0,        NULL }
    };

static const char *opt_name[] = {
    "Unibus", "Qbus", "EIS", "NOEIS", "FIS", "NOFIS",
    "FPP", "NOFPP", "CIS", "NOCIS", "MMU", "NOMMU",
    "RH11", "RH70", "PARITY", "NOPARITY", "Unibus map", "No map",
    "BEVENT enabled", "BEVENT disabled", NULL
    };

static const char *jcsr_val[4] = {
    "LINE", "50HZ", "60HZ", "800HZ"
    };

/* SYSTEM data structures

   sys_dev      SYSTEM device descriptor
   sys_unit     SYSTEM unit descriptor
   sys_reg      SYSTEM register list
*/

UNIT sys_unit = { UDATA (NULL, 0, 0) };

REG sys_reg[] = {
    { ORDATAD (SR, SR, 16, "switch or configuration register ("
                           "11/04, 11/05, 11/20, "
                           "11/23+, 11/34, 11/40, "
                           "11/44, 11/45, 11/60, "
                           "11/70, 11/73B, 11/83, "
                           "11/84, 11/93, 11/94)") },
    { ORDATAD (DR, DR, 16, "display register or board LEDs ("
                           "11/04, 11/05, 11/20, "
                           "1123+, 11/24, 11/34, "
                           "11/70, 11/73B, 11/83, "
                           "11/84, 11/93, 11/94)") },
    { ORDATA (SDR, SDR, 16, "switch/display register ("
                           "HSC)") },
    { ORDATAD (MEMERR, MEMERR, 16, "memory error register ("
                           "11/44, 11/60, 11/70, "
                           "11/53, 11/73, 11/73B, "
                           "11/83, 11/84, 11/93, "
                           "11/94)") },
    { ORDATAD (CCR, CCR, 16, "cache control register ("
                           "11/44, 11/60, 11/70, "
                           "11/53, 11/73, 11/73B, "
                           "11/83, 11/84, 11/93, "
                           "11/94)") },
    { ORDATAD (MAINT, MAINT, 16, "maintenance register ("
                           "11/23+, 11/44, 11/70, "
                           "11/53, 11/73, 11/73B, "
                           "11/83, 11/84, 11/93, "
                           "11/94)") },
    { ORDATAD (HITMISS, HITMISS, 16, "hit/miss register ("
                           "11/44, 11/60, 11/70, "
                           "11/53, 11/73, 11/73B, "
                           "11/83, 11/84, 11/93, "
                           "11/94)") },
    { ORDATAD (CPUERR, CPUERR, 16, "CPU error register ("
                           "11/24, 11/44, 11/70, "
                           "11/53, 11/73, 11/73B, "
                           "11/83, 11/84, 11/93, "
                           "11/94)") },
    { ORDATAD (MBRK, MBRK, 16, "microbreak register ("
                           "11/45, 11/70)") },
    { ORDATAD (WCS, WCS, 16, "WCS control (11/60)") },
    { ORDATAD (SYSID, SYSID, 16, "system ID (11/70 - default = 1234 hex)") },
    { ORDATAD (JCSR, JCSR, 16, "board control/status ("
                           "11/53, 11/73B, 11/83, "
                           "11/84, 11/93, 11/94)") },
    { ORDATA (JCSR_DFLT, JCSR_dflt, 16), REG_HRO },
    { ORDATAD (JPCR, JPCR, 16, "page control register ("
                           "11/23+, 11/53, 11/73B, "
                           "11/83, 11/84, 11/93, "
                           "11/94)") },
    { ORDATAD (JASR, JASR, 16, "additional status ("
                           "11/93, 11/94)") },
    { ORDATAD (UDCR, UDCR, 16, "Unibus map diag control ("
                           "11/84, 11/94)") },
    { ORDATAD (UDDR, UDDR, 16, "Unibus map diag data ("
                           "11/84, 11/94)") },
    { ORDATAD (UCSR, UCSR, 16, "Unibus map control/status ("
                           "11/84, 11/94)") },
    { ORDATAD (ULAST, uba_last, 23, "last Unibus map result ("
                           "11/24)") },
    { BRDATAD (UBMAP, ub_map, 8, 22, UBM_LNT_LW, "UBA map array") },
    { DRDATA (TOY_STATE, toy_state, 6), REG_HRO },
    { BRDATA (TOY_DATA, toy_data, 8, 8, TOY_LNT), REG_HRO },
    { NULL}
    };

MTAB sys_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "JCLK_DFLT", "JCLK_DFLT={LINE|50HZ|60HZ|800HZ}",
      &sys_set_jclk_dflt, &sys_show_jclk_dflt, NULL, "J11 default clock frequency" },
    { 0 }
    };

DEVICE sys_dev = {
    "SYSTEM", &sys_unit, sys_reg, sys_mod,
    1, 0, 0, 0, 0, 0,
    NULL, NULL, &sys_reset,
    NULL, NULL, NULL,
    NULL, 0, 0,
    NULL, NULL, NULL,
    &sys_help, NULL, NULL, &sys_description
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
    &fl_dib, 0
    };

/* Switch and display registers - many */

t_stat SR_rd (int32 *data, int32 pa, int32 access)
{
*data = SR;
return SCPE_OK;
}

t_stat DR_wr (int32 data, int32 pa, int32 access)
{
DR = data;
return SCPE_OK;
}

/* GPR's - 11/04, 11/05 */

t_stat REG_rd (int32 *data, int32 pa, int32 access)
{
*data = R[pa & 07];
return SCPE_OK;
}

t_stat REG_wr (int32 data, int32 pa, int32 access)
{
int32 reg = pa & 07;

if (access == WRITE)
    R[reg] = data;
else if (pa & 1)
    R[reg] = (R[reg] & 0377) | (data << 8);
else R[reg] = (R[reg] & ~0377) | data;
return SCPE_OK;
}

/* CPU control registers - 11/24 */

t_stat CPU24_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 013:                                           /* CPUERR */
        *data = 0;
        return SCPE_OK;
        }                                               /* end switch PA */

*data = 0;
return SCPE_NXM;                                        /* unimplemented */
}

t_stat CPU24_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 013:                                           /* CPUERR */
        return SCPE_OK;
        }                                               /* end switch pa */

return SCPE_NXM;                                        /* unimplemented */
}

/* CPU control registers - 11/44 */

t_stat CPU44_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 002:                                           /* MEMERR */
        *data = MEMERR;
        return SCPE_OK;

    case 003:                                           /* CCR */
        *data = CCR & CCR44_RD;
        return SCPE_OK;

    case 004:                                           /* MAINT */
        *data = MAINT & CMR44_RD;
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        *data = HITMISS;
        return SCPE_OK;

    case 006:                                           /* CDR */
        *data = 0;
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        if (CPUERR & CPUE_YEL)                          /* 11/44 stack err */
            CPUERR = (CPUERR & ~CPUE_YEL) | CPUE_RED;   /* in <2> not <3> */
        if (CPUERR & (CPUE_ODD|CPUE_NXM|CPUE_TMO))      /* additional flag */
            CPUERR = CPUERR | CPUE44_BUSE;
        *data = CPUERR & CPUE_IMP;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        *data = PIRQ;
        return SCPE_OK;
        }                                               /* end switch PA */

*data = 0;
return SCPE_NXM;                                        /* unimplemented */
}

t_stat CPU44_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 002:                                           /* MEMERR */
        MEMERR = 0;
        return SCPE_OK;

    case 003:                                           /* CCR */
        ODD_MRG (CCR, data);
        CCR = data & CCR44_WR;
        return SCPE_OK;

    case 004:                                           /* MAINT */
        ODD_MRG (MAINT, data);
        MAINT = data & CMR44_WR;
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        CPUERR = 0;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        EVN_IGN (data);
        ODD_SHF (data);
        put_PIRQ (data);
        return SCPE_OK;
        }

return SCPE_NXM;                                        /* unimplemented */
}

/* CPU control registers - 11/45 */

t_stat CPU45_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 014:                                           /* MBRK */
        *data = MBRK;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        *data = PIRQ;
        return SCPE_OK;

    case 016:                                           /* STKLIM */
        *data = STKLIM & STKLIM_RW;
        return SCPE_OK;
        }                                               /* end switch PA */

*data = 0;
return SCPE_NXM;                                        /* unimplemented */
}

t_stat CPU45_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 015:                                           /* PIRQ */
        EVN_IGN (data);
        ODD_SHF (data);
        put_PIRQ (data);
        return SCPE_OK;

    case 016:                                           /* STKLIM */
        EVN_IGN (data);
        ODD_SHF (data);
        STKLIM = data & STKLIM_RW;
        return SCPE_OK;
        }                                               /* end switch pa */

return SCPE_NXM;                                        /* unimplemented */
}

/* CPU control registers - 11/60 */

t_stat CPU60_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 000:                                           /* WCS */
        *data = WCS & WCS60_RD;
        return SCPE_OK;

    case 002:                                           /* MEMERR */
        *data = MEMERR & MEME60_RD;
        return SCPE_OK;

    case 003:                                           /* CCR */
        *data = CCR & CCR60_RD;
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        *data = HITMISS;
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        if (CPUERR & CPUE_NXM)                          /* TMO only */
            CPUERR = (CPUERR & ~CPUE_NXM) | CPUE_TMO;
        *data = CPUERR & CPUE60_RD;
        return SCPE_OK;

    case 016:                                           /* STKLIM */
        *data = STKLIM & STKLIM_RW;
        return SCPE_OK;
        }                                               /* end switch PA */

*data = 0;
return SCPE_NXM;                                        /* unimplemented */
}

t_stat CPU60_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 000:                                           /* WCS */
        WCS = data & WCS60_WR;
        return SCPE_OK;

    case 002:                                           /* MEMERR */
        MEMERR = 0;
        return SCPE_OK;

    case 003:                                           /* CCR */
        ODD_IGN (data);
        CCR = data & CCR60_WR;
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        CPUERR = 0;
        return SCPE_OK;

    case 014:                                           /* MBRK */
        MBRK = data & MBRK60_WR;
        return SCPE_OK;

    case 016:                                           /* STKLIM */
        EVN_IGN (data);
        ODD_SHF (data);
        STKLIM = data & STKLIM_RW;
        return SCPE_OK;
        }                                               /* end switch pa */

return SCPE_NXM;                                        /* unimplemented */
}

/* CPU control registers - 11/70 */

t_stat CPU70_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 000:                                           /* low error */
        *data = 0;
        return SCPE_OK;

    case 001:                                           /* high error */
        *data = 0;
        return SCPE_OK;

    case 002:                                           /* MEMERR */
        *data = MEMERR;
        return SCPE_OK;

    case 003:                                           /* CCR */
        *data = CCR;
        return SCPE_OK;

    case 004:                                           /* MAINT */
        *data = 0;
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        *data = HITMISS;
        return SCPE_OK;

    case 010:                                           /* low size */
        *data = (MEMSIZE >> 6) - 1;
        return SCPE_OK;

    case 011:                                           /* high size */
        *data = 0;
        return SCPE_OK;

    case 012:                                           /* system ID */
        *data = SYSID;
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        *data = CPUERR & CPUE_IMP;
        return SCPE_OK;

    case 014:                                           /* MBRK */
        *data = MBRK;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        *data = PIRQ;
        return SCPE_OK;

    case 016:                                           /* STKLIM */
        *data = STKLIM & STKLIM_RW;
        return SCPE_OK;
        }                                               /* end switch PA */

*data = 0;
return SCPE_NXM;                                        /* unimplemented */
}

/* From Walter Mueller: MEMERR is always written using the standard data
   on the bus: the whole word for DATO, the high byte for DATOB odd,
   and the low byte with DATOB even. The simulator always puts a byte
   in the low 8 bits, so for an odd byte reference, it must be shifted
   to the high byte.
*/

t_stat CPU70_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */
    case 000:
    case 001:
        return SCPE_OK;                                 /* error addr */

    case 002:                                           /* MEMERR */
        ODD_SHF (data);
        MEMERR = MEMERR & ~data;
        return SCPE_OK;

    case 003:                                           /* CCR */
        ODD_MRG (CCR, data);
        CCR = data;
        return SCPE_OK;

    case 004:                                           /* MAINT */
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        return SCPE_OK;

    case 010:                                           /* low size */
        return SCPE_OK;

    case 011:                                           /* high size */
        return SCPE_OK;

    case 012:                                           /* system ID */
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        CPUERR = 0;
        return SCPE_OK;

    case 014:                                           /* MBRK */
        ODD_IGN (data);
        MBRK = data & MBRK70_WR;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        EVN_IGN (data);
        ODD_SHF (data);
        put_PIRQ (data);
        return SCPE_OK;

    case 016:                                           /* STKLIM */
        EVN_IGN (data);
        ODD_SHF (data);
        STKLIM = data & STKLIM_RW;
        return SCPE_OK;
        }                                               /* end switch pa */

return SCPE_NXM;                                        /* unimplemented */
}

/* CPU control registers - J11 */

t_stat CPUJ_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 002:                                           /* MEMERR */
        *data = MEMERR;
        return SCPE_OK;

    case 003:                                           /* CCR */
        *data = CCR;
        return SCPE_OK;

    case 004:                                           /* MAINT */
        *data = MAINT | MAINT_NOFPA | MAINT_BPOK | (UNIBUS? MAINT_U: MAINT_Q);
        if (CPUT (CPUT_53))
            *data |= MAINT_KDJD | MAINT_POROM;
        if (CPUT (CPUT_73))
            *data |= MAINT_KDJA | MAINT_POODT;
        if (CPUT (CPUT_73B|CPUT_83|CPUT_84))
            *data |= MAINT_KDJB | MAINT_POROM;
        if (CPUT (CPUT_93|CPUT_94))
            *data |= MAINT_KDJE | MAINT_POROM;
        if (CPUT (CPUT_HSC70))
            *data |= MAINT_KPIO | MAINT_POROM;
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        if (CPUT (CPUT_73B))                            /* must be 0 for 73B */
            *data = 0;
        else *data = HITMISS | 010;                     /* must be nz for 11/8X */
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        *data = CPUERR & CPUE_IMP;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        *data = PIRQ;
        return SCPE_OK;
        }                                               /* end switch PA */

*data = 0;
return SCPE_NXM;                                        /* unimplemented */
}

t_stat CPUJ_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 017) {                              /* decode pa<4:1> */

    case 002:                                           /* MEMERR */
        MEMERR = 0;
        return SCPE_OK;

    case 003:                                           /* CCR */
        ODD_MRG (CCR, data);
        CCR = data;
        return SCPE_OK;

    case 004:                                           /* MAINT */
        return SCPE_OK;

    case 005:                                           /* Hit/miss */
        return SCPE_OK;

    case 013:                                           /* CPUERR */
        CPUERR = 0;
        return SCPE_OK;

    case 015:                                           /* PIRQ */
        EVN_IGN (data);
        ODD_SHF (data);
        put_PIRQ (data);
        return SCPE_OK;
        }                                               /* end switch pa */

return SCPE_NXM;                                        /* unimplemented */
}

/* Board control registers - KDF11B */

t_stat CTLFB_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* PCR */
        *data = JPCR & PCRFB_RW;
        return SCPE_OK;

    case 1:                                             /* MAINT */
        *data = MAINT;
        return SCPE_OK;

    case 2:                                             /* CDR */
        *data = SR & CDRFB_RD;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat CTLFB_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* PCR */
        ODD_MRG (JPCR, data);
        JPCR = data & PCRFB_RW;
        return SCPE_OK;

    case 1:                                             /* MAINT */
        ODD_MRG (MAINT, data);
        MAINT = data;
        return SCPE_OK;

    case 2:                                             /* CDR */
        ODD_IGN (data);
        DR = data & CDRFB_WR;
        return SCPE_OK;
        }

return SCPE_NXM;
}

/* Board control registers - KDJ11B */

t_stat CTLJB_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

   case 0:                                              /* CSR */
    *data = JCSR & CSRJB_RD;
    return SCPE_OK;

    case 1:                                             /* PCR */
        *data = JPCR & PCRJB_RW;
        return SCPE_OK;

    case 2:                                             /* CDR */
        *data = SR & CDRJB_RD;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat CTLJB_wr (int32 data, int32 pa, int32 access)
{
int32 t;

switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* CSR */
        ODD_MRG (JCSR, data);
        JCSR = (JCSR & ~CSRJB_WR) | (data & CSRJB_WR);
        if (JCSR & CSRJ_LTCI)                           /* force LTC int enb? */
            clk_fie = 1;
        else clk_fie = 0;
        if (JCSR & CSRJ_LTCD)                           /* force LTC reg nxm? */
            clk_fnxm = 1;
        else clk_fnxm = 0;
        t = CSRJ_LTCSEL (JCSR);                         /* get freq sel */
        if (t != 0)
            clk_tps = clk_tps_map[t];
        else clk_tps = clk_default;
        return SCPE_OK;

    case 1:                                             /* PCR */
        ODD_MRG (JPCR, data);
        JPCR = data & PCRJB_RW;
        return SCPE_OK;

    case 2:                                             /* CDR */
        ODD_IGN (data);
        DR = data & CDRJB_WR;
        return SCPE_OK;
        }

return SCPE_NXM;
}

/* Board control registers - KDJ11D */

t_stat CTLJD_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* CSR */
        *data = JCSR & CSRJD_RD;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat CTLJD_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* CSR */
        ODD_MRG (JCSR, data);
        JCSR = (JCSR & ~CSRJD_WR) | (data & CSRJD_WR);
        return SCPE_OK;
        }

return SCPE_NXM;
}

/* Board control registers - KDJ11E */

t_stat CTLJE_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* CSR */
        *data = JCSR & CSRJE_RD;
        return SCPE_OK;

    case 1:                                             /* PCR */
        *data = JPCR & PCRJE_RW;
        return SCPE_OK;

    case 2:                                             /* CDR */
        *data = SR & CDRJE_RD;
        return SCPE_OK;

    case 3:                                             /* ASR */
        JASR = (JASR & ~ASRJE_TOY) | (toy_read () << ASRJE_V_TOY);
        *data = JASR & ASRJE_RW;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat CTLJE_wr (int32 data, int32 pa, int32 access)
{
int32 t;

switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* CSR */
        ODD_MRG (JCSR, data);
        JCSR = (JCSR & ~CSRJE_WR) | (data & CSRJE_WR);
        if (JCSR & CSRJ_LTCI)                           /* force LTC int enb? */
            clk_fie = 1;
        else clk_fie = 0;
        if (JCSR & CSRJ_LTCD)                           /* force LTC reg nxm? */
            clk_fnxm = 1;
        else clk_fnxm = 0;
        t = CSRJ_LTCSEL (JCSR);                         /* get freq sel */
        if (t != 0)
            clk_tps = clk_tps_map[t];
        else clk_tps = clk_default;
        return SCPE_OK;

    case 1:                                             /* PCR */
        ODD_MRG (JPCR, data);
        JPCR = data & PCRJE_RW;
        return SCPE_OK;

    case 2:                                             /* CDR */
        ODD_IGN (data);
        DR = data & CDRJE_WR;
        return SCPE_OK;

    case 3:                                             /* ASR */
        ODD_MRG (JASR, data);
        JASR = data & ASRJE_RW;
        toy_write (ASRJE_TOYBIT (JASR));
        return SCPE_OK;
        }

return SCPE_NXM;
}

/* Unibus adapter registers - KT24 */

t_stat UBA24_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 2:                                             /* LMAL */
        *data = uba_last & LMAL_RD;
        return SCPE_OK;
    case 3:                                             /* LMAH */
        *data = uba_last & LMAH_RD;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat UBA24_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 3:                                             /* ASR */
        ODD_IGN (data);
        uba_last = (uba_last & ~LMAH_WR) | ((data & LMAH_WR) << 16);
        return SCPE_OK;
        }

return SCPE_NXM;
}

/* Unibus registers - KTJ11B */

t_stat UBAJ_rd (int32 *data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* DCR */
        *data = UDCR & DCRKTJ_RD;
        return SCPE_OK;

    case 1:                                             /* DDR */
        *data = UDDR & DDRKTJ_RW;
        return SCPE_OK;

    case 2:                                             /* CSR */
        *data = UCSR & MCRKTJ_RD;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat UBAJ_wr (int32 data, int32 pa, int32 access)
{
switch ((pa >> 1) & 03) {                               /* decode pa<2:1> */

    case 0:                                             /* DCR */
        ODD_MRG (UDCR, data);
        UDCR = (UDCR & ~DCRKTJ_WR) | (data & DCRKTJ_WR);
        return SCPE_OK;

    case 1:                                             /* DDR */
        ODD_MRG (UDDR, data);
        UDDR = data & DDRKTJ_RW;;
        return SCPE_OK;

    case 2:                                             /* CSR */
        ODD_MRG (UCSR, data);
        UCSR = (UCSR & ~MCRKTJ_WR) | (data & MCRKTJ_WR);
        return SCPE_OK;
        }

return SCPE_NXM;
}

int32 parity (int32 val, int32 odd)
{
for ( ; val != 0; val = val >> 1) {
    if (val & 1)
        odd = odd ^ 1;
    }
return odd;
}

t_stat ioctl_rd (int32 *data, int32 pa, int32 access)
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
        *data = WADR[WIDX][rg];
        *data = (*data & 0177) << 2;
        *data = *data | (rg << 9) | (016 << 12);
//        printf ("WADR%d rd %o\n", rg, *data);
        return SCPE_OK;

    case 010:                                           /* WIDX */
        *data = WIDX;
//        printf ("WIDX rd: %o\n", *data);
        return SCPE_OK;

    case 011:                                           /* WBUS */
        *data = WBUS;
//        printf ("WBUS rd: %o\n", *data);
        return SCPE_OK;

    case 012:                                           /* low error */
        *data = EADR & 0177777;
//        printf ("low error rd: %o\n", *data);
        return SCPE_OK;

    case 013:                                           /* high error */
        *data = (EADR >> 16) & 077;
//        printf ("high error rd: %o\n", *data);
        return SCPE_OK;

    case 020:                                           /* Pio CSR - NI */
        *data = PCSR | 0100000;
//        printf ("Pio CSR rd: %o\n", *data);
        return SCPE_OK;

    case 021:                                           /* switch/display reg */
        *data = SDR & 07777;
//        printf ("SDR rd: %o\n", *data);
        return SCPE_OK;

    case 022:                                           /* K init reg - NI */
        *data = KINI;
        printf ("K init rd\n", *data);
        if (sim_log)
            fprintf (sim_log, "K init rd\n", *data);
        return SCPE_OK;

    case 023:                                           /* K status reg - NI */
        *data = KSTS[(KINI & 017)];
        printf ("K status rd\n", *data);
        if (sim_log)
            fprintf (sim_log, "K status rd\n", *data);
        return SCPE_OK;

    case 024:                                           /* data mem low addr */
        *data = DADR & 0177777;
//        printf ("data mem low rd: %o\n", *data);
        return SCPE_OK;

    case 025:                                           /* data mem high addr */
        *data = (DADR >> 16) & 07;
//        printf ("data mem high rd: %o\n", *data);
        return SCPE_OK;

    case 026:                                           /* ctrl mem low addr - NI */
        *data = LOCADR;
//        printf ("ctrl mem low rd: %o\n", *data);
        return SCPE_OK;

    case 027:                                           /* serial number reg - NI */
        printf ("serial num rd\n");
        *data = 0;
        return SCPE_OK;
        }

*data = 0;
return SCPE_NXM;
}

t_stat ioctl_wr (int32 data, int32 pa, int32 access)
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
//        printf ("WADR%d wr: %o\n", rg, data);
        WADR[WIDX][rg] = data;
        return SCPE_OK;

    case 010:                                           /* WIDX */
//        printf ("WIDX wr: %o\n", data);
        WIDX = data & 0177;
        return SCPE_OK;

    case 020:                                           /* Pio CSR */
//        printf ("Pio CSR wr: %o\n", data);
        PCSR = data;
        return SCPE_OK;

    case 021:                                           /* switch/display reg */
//        printf ("SDR wr: %o\n", data);
        SDR = data & 07777;
        return SCPE_OK;

    case 022:                                           /* K init reg - NI */
        printf ("K init wr: %o\n", data);
        if (sim_log)
            fprintf (sim_log, "K init wr: %o\n", data);
        KINI = data;
        return SCPE_OK;

    case 023:                                           /* K status reg - NI */
        printf ("K status wr: %o\n", data);
        if (sim_log)
            fprintf (sim_log, "K status wr: %o\n", data);
        KSTS[0] = data | (parity (data, 1) << 7);       /* can only write to req 0 */
        return SCPE_OK;
        }

return SCPE_NXM;
}

t_stat ctlwin_rd (int32 *data, int32 pa, int32 access)
{
int32 va;
int32 rg;

rg = (pa >> 9) & 07;                                    /* get window reg for this addr */
va = (WADR[WIDX][rg] & 0177600) << 2;
va = va | (pa & 0777);
WBUS = (va >> 2);                                       /* save history */
LOCADR = (va >> 1) & 1;
va = va | (016 << 18);
if (PCSR & 04) {                                        /* non-memory access cycle */
    LOCADR = LOCADR | 016;
    *data = CDATA;
    }
else {
    if (ADDR_IS_MEM (va))                               /* memory address? */
        *data = (M[va >> 1]);
    else {
        printf ("Error: %o is not mem addr\n", va);
        return SCPE_NXM;
        }
    if (PCSR & 02) {                                    /* interrupt cycle */
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
    else if (PCSR & 01) {                               /* lock cycle */
        LOCADR = (LOCADR & ~016) | 010;
        M[va >> 1] = 0100000;
        }
    else LOCADR = (LOCADR & ~016) | 02;                 /* read/write cycle */
    CDATA = *data;
    }
//printf ("CMEM rd: %06o = %06o (%o) at %o\n", (va & ~(016 << 18)), *data, pa, PC);
if (sim_log)
    fprintf (sim_log, "CMEM rd: %06o = %06o (%o) at %o\n", (va & ~(016 << 18)), *data, pa, PC);
PCSR = PCSR & ~07;                                      /* reset special cycle */
return SCPE_OK;
}

t_stat ctlwin_wr (int32 data, int32 pa, int32 access)
{
int32 va;
int32 rg;

rg = (pa >> 9) & 07;
va = (WADR[WIDX][rg] & 0177600) << 2;
va = va | (pa & 0777);
WBUS = (va >> 2);
LOCADR = (va >> 1) & 1;
//printf ("CMEM wr: %06o = %06o (%o) at %o\n", va, data, pa, PC);
if (sim_log)
    fprintf (sim_log, "CMEM wr: %06o = %06o (%o) at %o\n", va, data, pa, PC);
va = va | (016 << 18);
CDATA = data;
if (PCSR & 04) {                                        /* non-memory access cycle */
    LOCADR = LOCADR | 016;
    return SCPE_OK;
    }
if (PCSR & 02) {                                        /* interrupt cycle */
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
else if (PCSR & 01) {                                   /* lock cycle */
    LOCADR = (LOCADR & ~016) | 010;
    M[va >> 1] = 0100000;
    }
else LOCADR = (LOCADR & ~016) | 02;                     /* read/write cycle */
PCSR = PCSR & ~07;
if (ADDR_IS_MEM (va)) {                                 /* memory address? */
    if (access == WRITEB) {
        if (va & 1)
            M[va >> 1] = (M[va >> 1] & 0377) | (data << 8);
        else M[va >> 1] = (M[va >> 1] & ~0377) | data;
        }
    else
        M[va >> 1] = data;
    }
else {
    printf ("Error: %o is not mem addr\n", va);
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

printf ("Boot device write, %o -> %X, %d\n", ma, da, sz);
if (sim_log)
    fprintf (sim_log, "Boot device write, %o -> %X, %d\n", ma, da, sz);
if (mapen) {                                            /* use reloc? */
    for (i = 0; i < ln; i++) {
        fbuf[(da + i)] = ReadB ((ma + i));
        if (sim_log)
            fprintf (sim_log, "fbuf[%X] = %o\n", (da + i), fbuf[(da + i)]);
        }
    }
else {
    for (i = 0; i < ln; i++) {
        fbuf[(da + i)] = PReadB ((ma + i));
        if (sim_log)
            fprintf (sim_log, "fbuf[%X] = %o\n", (da + i), fbuf[(da + i)]);
        }
    }
}

void fl_read (UNIT *uptr, int32 blk, int32 sz, int32 ma, t_bool mapen)
{
int32 da = blk * 512;
int32 ln = sz * 512;
int32 i;
uint8 *fbuf = uptr->filebuf;

printf ("Boot device read, %X, %d -> %o\n", da, sz, ma);
if (sim_log)
    fprintf (sim_log, "Boot device read, %X, %d -> %o\n", da, sz, ma);
if (mapen) {                                            /* use reloc? */
    for (i = 0; i < ln; i++) {
        WriteB (fbuf[(da + i)], (ma + i));
        }
    }
else {
    for (i = 0; i < ln; i++) {
        PWriteB (fbuf[(da + i)], (ma + i));
        }
    }
}

t_stat fl_rd (int32 *data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 03;
int32 unitno = (fl_csr >> FLCSR_V_SEL) & 1;
switch (rg) {                                           /* decode pa<2:1> */

    case 000:                                           /* CSR */
        *data = fl_csr;
        printf ("fl_csr rd: %o at %o\n", *data, PC);
        return SCPE_OK;

    case 001:                                           /* MADR0 */
        *data = fl_madr0 | fl_unit[unitno].TRK;
        printf ("fl_madr0 rd: %o\n", *data);
        return SCPE_OK;

    case 002:                                           /* MADR1 */
        *data = fl_madr1;
        printf ("fl_madr1 rd: %o\n", *data);
        return SCPE_OK;

    case 003:                                           /* MADR2 */
        *data = fl_madr2;
        printf ("fl_madr2 rd: %o\n", *data);
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
        printf ("fl_csr wr: %o at %o\n", data, PC);
        unitno = (data >> FLCSR_V_SEL) & 1;            /* get drive select */
        fl_csr = data & FLCSR_RW;
        cmd = data & FLCSR_CS;
        if (fl_csr & FLCSR_ME) {                        /* motor enabled */
            if ((cmd >> 4) == 015) {                    /* force interrupt? */
                sim_cancel (&fl_unit[unitno]);
                fl_csr = fl_csr & ~1;                   /* set not busy */
                if (cmd & 05) {
                    fl_unit[unitno].CMD = cmd;
                    fl_csr = fl_csr | 1;                /* set busy */
                    sim_activate (&fl_unit[unitno], FL_WAIT);
                    }
//                fl_madr2 = fl_madr2 | 0377;             /* indicate no M.cache2 module? */
//                printf ("force_interrupt: fl_madr2 = %o\n", fl_madr2);
                }
            else if ((fl_csr & 1) == 0) {               /* not busy? */
                fl_unit[unitno].CMD = cmd;
                fl_csr = fl_csr | 1;                    /* set busy */
                sim_activate (&fl_unit[unitno], FL_WAIT);
                }
            }
        return SCPE_OK;

    case 001:                                           /* MADR0 */
        printf ("fl_madr0 wr: %o\n", data);
        fl_madr0 = data & FLMA0_RW;
        return SCPE_OK;

    case 002:                                           /* MADR1 */
        printf ("fl_madr1 wr: %o\n", data);
        fl_madr1 = data;
        return SCPE_OK;

    case 003:                                           /* MADR2 */
        printf ("fl_madr2 wr: %o\n", data);
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
            printf ("fl: seek track %d\n", uptr->TRK);
            fl_csr = fl_csr & ~1;                       /* set not busy */
            fl_csr = fl_csr | 04;
            fl_setint ();
            }
        else {                                          /* restore */
            uptr->TRK = 0;
            printf ("fl: restore\n");
            fl_csr = fl_csr & ~1;                       /* set not busy */
            fl_csr = fl_csr | 04;
            fl_setint ();
            }
        break;

    case 1:                                             /* step */
        if (uptr->DIR) {
            uptr->TRK++;
            printf ("fl: step in\n");
            }
        else {
            uptr->TRK--;
            printf ("fl: step out\n");
            }
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 2:                                             /* step in */
        uptr->DIR = 1;
        uptr->TRK++;
        printf ("fl: step in\n");
        fl_csr = fl_csr & ~1;                           /* set not busy */
        fl_setint ();
        break;

    case 3:                                             /* step out */
        uptr->DIR = 0;
        uptr->TRK--;
        printf ("fl: step out\n");
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
            printf ("fl: read multi sector\n");
            fl_read (uptr, da, (FL_NUMSC - sec), ma, FALSE);
            ma = ma + ((FL_NUMSC - sec + 1) * 512);
            //fl_madr1 = (fl_madr1 & ~FLMA1_DS) | 020;
            }
        else {
            printf ("fl: read single sector\n");
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
            printf ("fl: write multi sector\n");
            fl_write (uptr, da, (FL_NUMSC - sec), ma, FALSE);
            ma = ma + ((FL_NUMSC - sec + 1) * 512);
            //fl_madr1 = (fl_madr1 & ~FLMA1_DS) | 020;
            }
        else {
            printf ("fl: write single sector\n");
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
            printf ("fl: force interrupt\n");
            fl_csr = fl_csr & ~1;                       /* set not busy */
            if (uptr->CMD & 017)
                fl_setint ();
            }
        else {                                          /* read address */
            printf ("fl: read address\n");
            fl_setint ();
            }
        break;

    case 7:
        ma = ((fl_madr2 & FLMA2_A16) << 8) | \
            (fl_madr1 & FLMA1_A8) | \
            ((fl_madr0 & FLMA0_A0) >> 8);
        da = GET_DA (uptr->TRK, (uptr->CMD & 02 ? 1 : 0), 1);
        if (uptr->CMD & 020) {                          /* write track */
            printf ("fl: write track\n");
            if ((da * 512) > uptr->hwmark)              /* update hwmark */
                uptr->hwmark = (da * 512);
            fl_write (uptr, da, FL_NUMSC, ma, FALSE);
            }
        else {                                          /* read track */
            printf ("fl: read track\n");
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
    printf ("Boot only supported from unit 0\n");
    return SCPE_NOFNC;
    }
fl_read (&fl_unit[unitno], 0, 1, 06000, FALSE);                           /* read first 8 blocks */
fl_read (&fl_unit[unitno], 2, 7, 07000, FALSE);
REGFILE[1][rs] = 0177400;                               /* boot device CSR */
REGFILE[2][rs] = 06000;                                 /* boot base */
saved_PC = 06000;
return SCPE_OK;
}

t_stat rom_rd (int32 *data, int32 pa, int32 access)
{
int32 rg = (pa >> 1) & 0377;
switch (rg) {                                           /* decode pa<2:1> */

    case 007:                                           /* ptr to unknown subroutine 2 */
        *data = 0173060;
        break;

    case 010:                                           /* ptr to boot device read */
        *data = 0173030;
        break;

    case 012:                                           /* ptr to unknown subroutine 1 */
        *data = 0173050;
        break;

    case 013:                                           /* ptr to console halt */
        *data = 0173040;
        break;

    case 014:                                           /* boot device read */
        fl_read (&fl_unit[0], R[5], R[4], R[2], TRUE);
        *data = 0241;                                   /* CLC */
        break;

    case 015:
        *data = 0207;                                   /* RTS PC */
        break;

    case 020:                                           /* console halt */
        printf ("console halt, status = %o\n", R[0]);
        *data = 0;                                      /* HALT */
        break;

    case 024:                                           /* unknown subroutine 1 */
        *data = 0241;                                   /* CLC */
        break;

    case 025:
        *data = 0207;                                   /* RTS PC */
        break;

    case 030:                                           /* unknown subroutine 2 */
        *data = 0207;                                   /* RTS PC */
        break;

    default:
        printf ("rom_rd: %o\n", pa);
        *data = 0;
        }
return SCPE_OK;
}

/* KDJ11E TOY routines */

int32 toy_read (void)
{
int32 bit;

if (toy_state == 0) {
    struct timespec now;
    time_t curr;
    struct tm *ctm;

    sim_rtcn_get_time (&now, 0);
    curr = (time_t)now.tv_sec;

    if (curr == (time_t) -1)                            /* error? */
        return 0;
    ctm = localtime (&curr);                            /* decompose */
    if (ctm == NULL)                                    /* error? */
        return 0;
    toy_data[TOY_HSEC] = toy_set ((now.tv_nsec + 5000000) / 10000000);
    toy_data[TOY_SEC] = toy_set (ctm->tm_sec);
    toy_data[TOY_MIN] = toy_set (ctm->tm_min);
    toy_data[TOY_HR] = toy_set (ctm->tm_hour);
    toy_data[TOY_DOW] = toy_set (ctm->tm_wday);
    toy_data[TOY_DOM] = toy_set (ctm->tm_mday);
    toy_data[TOY_MON] = toy_set (ctm->tm_mon + 1);
    toy_data[TOY_YR] = toy_set (ctm->tm_year % 100);
    }
bit = toy_data[toy_state >> 3] >> (toy_state & 07);
toy_state = (toy_state + 1) % (TOY_LNT * 8);
return (bit & 1);
}

void toy_write (int32 bit)
{
toy_state = 0;
return;
}

uint8 toy_set (int32 val)
{
uint32 d1, d2;

d1 = val / 10;
d2 = val % 10;
return (uint8) ((d1 << 4) | d2);
}

/* Build I/O space entries for CPU */

t_stat cpu_build_dib (void)
{
int32 i;
t_stat r;

for (i = 0; cnf_tab[i].dib != NULL; i++) {              /* loop thru config tab */
    if (((cnf_tab[i].cpum == 0) || (cpu_type & cnf_tab[i].cpum)) &&
        ((cnf_tab[i].optm == 0) || (cpu_opt & cnf_tab[i].optm))) {
        if ((r = build_ubus_tab (&cpu_dev, cnf_tab[i].dib)))/* add to dispatch tab */
             return r;
        }
    }
return SCPE_OK;
}

/* Set/show CPU model */

t_stat cpu_set_model (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (cptr != NULL)
    return SCPE_ARG;
if (val >= MOD_MAX)
    return SCPE_IERR;
if (val == (int32) cpu_model)
    return SCPE_OK;
cpu_model = val;
cpu_type = 1u << cpu_model;
cpu_opt = cpu_tab[cpu_model].std;
cpu_set_bus (cpu_opt);
if (MEMSIZE > cpu_tab[val].maxm)
    cpu_set_size (uptr, cpu_tab[val].maxm, NULL, NULL);
if (MEMSIZE > cpu_tab[val].maxm)
    return SCPE_INCOMP;
reset_all (0);                                          /* reset world */
return SCPE_OK;
}

t_stat cpu_show_model (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint32 i, all_opt;

fprintf (st, "%s", cpu_tab[cpu_model].name);
all_opt = cpu_tab[cpu_model].opt;
for (i = 0; opt_name[2 * i] != NULL; i++) {
    if ((all_opt >> i) & 1)
        fprintf (st, ", %s",
                ((cpu_opt >> i) & 1)? opt_name[2 * i]: opt_name[(2 * i) + 1]);
    }
return SCPE_OK;
}

/* Set/clear CPU option */

t_stat cpu_set_opt (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (cptr)
    return SCPE_ARG;
if ((val & cpu_tab[cpu_model].opt) == 0) {
    uint32 i;

    for (i = 0; opt_name[2 * i] != NULL; i++) {
        if ((val >> i) & 1)
            break;
        }
    return sim_messagef (SCPE_ARG, "The %s option can't be enabled on a %s CPU\n",
                                ((val >> i) & 1)? opt_name[2 * i] : "unknown",
                                cpu_tab[cpu_model].name);
    }
cpu_opt = cpu_opt | val;
return SCPE_OK;
}

t_stat cpu_clr_opt (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (cptr)
    return SCPE_ARG;
if ((val & cpu_tab[cpu_model].opt) == 0) {
    uint32 i;

    for (i = 0; opt_name[2 * i] != NULL; i++) {
        if ((val >> i) & 1)
            break;
        }
    return sim_messagef (SCPE_ARG, "The %s option can't be disabled on a %s CPU\n",
                                ((val >> i) & 1)? opt_name[2 * i] : "unknown",
                                cpu_tab[cpu_model].name);
    }
cpu_opt = cpu_opt & ~val;
return SCPE_OK;
}

/* Memory allocation */

t_stat cpu_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 mc = 0;
uint32 i, clim;
uint16 *nM;

if ((val <= 0) ||
    (val > ((int32) cpu_tab[cpu_model].maxm)) ||
    ((val & 07777) != 0))
    return SCPE_ARG;
if (val > ((int32) (cpu_tab[cpu_model].maxm - IOPAGESIZE)))
    val = (int32) (cpu_tab[cpu_model].maxm - IOPAGESIZE);
for (i = val; i < MEMSIZE; i = i + 2)
    mc = mc | M[i >> 1];
if ((mc != 0) && !get_yn ("Really truncate memory [N]?", FALSE))
    return SCPE_OK;
nM = (uint16 *) calloc (val >> 1, sizeof (uint16));
if (nM == NULL)
    return SCPE_MEM;
clim = (((t_addr) val) < MEMSIZE)? (uint32)val: MEMSIZE;
for (i = 0; i < clim; i = i + 2)
    nM[i >> 1] = M[i >> 1];
free (M);
M = nM;
MEMSIZE = val;
if (!(sim_switches & SIM_SW_REST))                      /* unless restore, */
    cpu_set_bus (cpu_opt);                              /* alter periph config */
return SCPE_OK;
}

/* Bus configuration, disable Unibus or Qbus devices */

t_stat cpu_set_bus (int32 opt)
{
DEVICE *dptr;
uint32 i, mask;

if (opt & BUS_U)                                        /* Unibus variant? */
    mask = DEV_UBUS;
else if (MEMSIZE <= UNIMEMSIZE)                         /* 18b Qbus devices? */
    mask = DEV_QBUS | DEV_Q18;
else mask = DEV_QBUS;                                   /* must be 22b */
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
    if ((dptr->flags & DEV_DISABLE) &&                  /* disable-able? */
        !(dptr->flags & DEV_DIS) &&                     /* enabled? */
        ((dptr->flags & mask) == 0)) {                  /* not allowed? */
        sim_printf ("Disabling %s\n", sim_dname (dptr));
        dptr->flags = dptr->flags | DEV_DIS;
        }
    }
return SCPE_OK;
}

/* System reset */

t_stat sys_reset (DEVICE *dptr)
{
int32 i;

CCR = 0;
HITMISS = 0;
CPUERR = 0;
MEMERR = 0;
if (!CPUT (CPUT_J))
    MAINT = 0;
MBRK = 0;
WCS = 0;
if (CPUT (CPUT_JB|CPUT_JE))
    JCSR = JCSR_dflt;
else JCSR = 0;
JPCR = 0;
JASR = 0;
UDCR = 0;
UDDR = 0;
UCSR = 0;
uba_last = 0;
DR = 0;
toy_state = 0;
for (i = 0; i < UBM_LNT_LW; i++)
    ub_map[i] = 0;
for (i = 0; i < TOY_LNT; i++)
    toy_data[i] = 0;
return SCPE_OK;
}

/* Set/show JCLK default values */

t_stat sys_set_jclk_dflt (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
uint32 i;

if ((CPUT (CPUT_JB|CPUT_JE)) && cptr) {
    for (i = 0; i < 4; i++) {
        if (strncmp (cptr, jcsr_val[i], strlen (cptr)) == 0) {
            JCSR_dflt = i << CSRJ_V_LTCSEL;
            return SCPE_OK;
            }
        }
    }
return SCPE_ARG;
}

t_stat sys_show_jclk_dflt (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
if (CPUT (CPUT_JB|CPUT_JE))
    fprintf (st, "JCLK default=%s\n", jcsr_val[CSRJ_LTCSEL (JCSR_dflt)]);
else fprintf (st, "Not implemented\n");
return SCPE_OK;
}

const char *sys_description (DEVICE *dptr)
{
return "PDP-11 model options";
}

t_stat sys_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "The %s (%s) device help\n\n", dptr->description (dptr), dptr->name);
fprintf (st, "The SYSTEM device implements registers that vary among CPU types:\n");
fprint_reg_help (st, dptr);
fprintf (st, "\n");
fprintf (st, "For the 11/83, 11/84, 11/93, and 11/94, the user can set the default value\n");
fprintf (st, "of the clock frequency:\n\n");
fprint_set_help (st, dptr);
fprintf (st, "\n");
fprintf (st, "The user can check the default value with:\n");
fprint_show_help (st, dptr);
return SCPE_OK;
}
