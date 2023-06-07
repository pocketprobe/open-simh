/* hsc_ks.c: HSC K.sdi simulation

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
#include "hsc_kg.h"
#include "dec_disk.h"
#include "sim_disk.h"

/* K.sdi control area */

#define KS_0FL          0002                            /* flags (port 0) */
#define KS_0HD          0000                            /* DRAT queue (port 0) */
#define KS_KSA          0140                            /* drive state area */
#define KS_S0           0140                            /* drive state area (port 0) */
#define KS_S1           0160                            /* drive state area (port 1) */
#define KS_S2           0200                            /* drive state area (port 2) */
#define KS_S3           0220                            /* drive state area (port 3) */

/* Drive state area */

#define KSA_SRVC        0000                            /* service queue pointer */
#define KSA_FLGS        0003                            /* flags */
#define KSA_HEAD        0004                            /* work queue head */
#define KSA_TAIL        0006                            /* work queue tail */
#define KSA_CTX         0012                            /* software context */
#define KSA_LOCK        0016                            /* drive state area lock */
#define KSA_LEN         0020                            /* size of drive state area */

/* device control block */

#define DC_LNK          0000                            /* link to next */
#define DC_CMD          0002                            /* command buffer pointer */
#define DC_RSB          0004                            /* response buffer pointer */
#define DC_FCT          0006                            /* function? */
#define DC_STA          0010
#define DC_DUC          0012                            /* pointer to DU$ structure */
#define DC_0CQ          0014                            /* success completion queue */
#define DC_STC          0016
#define DC_1CQ          0016                            /* error completion queue */
#define DC_ICT          0020                            /* inline command buffer */
#define DC_RTY          0036
#define DC_RBS          0037
#define DC_IRT          0040                            /* inline response buffer */
#define DC_THP          0100
#define DC_LRB          0102
#define DC_RSV          0104
#define DC_PAD          0110

/* Realtime drive status */

#define RTDS_RWR        0001                            /* read/write ready */
#define RTDS_RR         0002                            /* receiver ready */
#define RTDS_ATN        0010                            /* attention */
#define RTDS_AVL        0020                            /* available */

/* Drive status - Unit mask */

#define UMSK_U1         0020                            /* subunit 1 present */
#define UMSK_U2         0040                            /* subunit 2 present */
#define UMSK_U3         0100                            /* subunit 3 present */
#define UMSK_U4         0200                            /* subunit 4 present */

/* Drive status - Request byte */

#define REQ_RU          0001                            /* run/stop switch in */
#define REQ_PS          0002                            /* port switch in */
#define REQ_EL          0010                            /* send log packet */
#define REQ_SR          0020                            /* spindle at speed */
#define REQ_DR          0040                            /* diagnostic load required */
#define REQ_RR          0100                            /* readjustment required */
#define REQ_OA          0200                            /* online to other port */

/* Drive status - Mode byte */

#define MODE_S7         0001                            /* 0 = 512 bytes/sector, 1 = 576 bytes/sector */
#define MODE_OB         0002                            /* diag cylinder access */
#define MODE_FO         0004                            /* formatting enabled */
#define MODE_DD         0010                            /* drive disabled for diagnostics */
#define MODE_W1         0020                            /* write protect subunit 1 */
#define MODE_W2         0040                            /* write protect subunit 2 */
#define MODE_W3         0100                            /* write protect subunit 3 */
#define MODE_W4         0200                            /* write protect subunit 4 */

/* Drive status - Error byte */

#define ERR_WE          0010                            /* write lock error */
#define ERR_DF          0020                            /* init diag failure */
#define ERR_PE          0040                            /* protocol error */
#define ERR_RE          0100                            /* retryable error */
#define ERR_DE          0200                            /* drive error */

/* Format code for FCT */

#define FMT_512         0126736                         /* 512 bytes/sector */
#define FMT_576         074161                          /* 576 bytes/sector */

/* Disk Rotational Access Table */

#define DT_LNK          0000
#define DT_L1C          0002
#define DT_ERQ          0004
#define DT_OFF          0006                            /* offset to transfer queues */
#define DT_STA          0010                            /* low byte is SDI port number? */
#define DT_DPS          0012                            /* data preamble size */
#define DT_HPS          0013                            /* header preamble size */
#define DT_CQ           0014                            /* completion queue */
#define DT_HST          0014
#define DT_CNT          0024                            /* remaining operation count */
#define DT_MSC          0024                            /* MSCP? */
#define DT_SCT          0034
#define DT_XXX          0036
#define DT_HLD          0040
#define DT_CYL          0044                            /* cylinder */
#define DT_BFC          0044
#define DT_LFL          0046
#define DT_TFC          0050
#define DT_DU           0052                            /* pointer to DU$ structure in program memory? */
#define DT_RCC          0054
#define DT_DTC          0056
#define DT_PBC          0060
#define DT_DCB          0062
#define DT_FLG          0064

/* Fragment request block - K.sdi */

#define F_CYL           0046                            /* cylinder */
#define F_SBFH          0054
#define F_SSCT          0056
#define F_REMN          0057
#define F_PRMY          0060
#define F_SLRB          0062
#define F_TGDT          0064
#define F_SNUM          0066
#define F_SCR           0070
#define F_TCNT          0070
#define F_DRAT          0072
#define F_DUCB          0100
#define F_BPTR          0102
#define F_SQL           0104
#define F_FLGS          0106

#define KS_MAXDR        4                               /* max # drives */

#define KS_NUMBY        512                             /* bytes per block */
#define KS_MAXFR        (1 << 16)                       /* max xfer */

#define OT_LBN          0000                            /* LBN offset type */
#define OT_RBN          0001                            /* RBN offset type */
#define OT_XBN          0002                            /* XBN offset type */
#define OT_DBN          0003                            /* DBN offset type */

#define UNIT_V_ONL      (DKUF_V_UF + 0)                 /* online */
#define UNIT_V_WLK      (DKUF_V_UF + 1)                 /* hwre write lock */
#define UNIT_V_ATP      (DKUF_V_UF + 2)                 /* attn pending */
#define UNIT_V_DTYPE    (DKUF_V_UF + 3)                 /* drive type */
#define UNIT_M_DTYPE    0x1F
#define UNIT_V_NOAUTO   (DKUF_V_UF + 8)                 /* noautosize */
#define UNIT_V_FMT      (DKUF_V_UF + 9)                 /* drive format */
#define UNIT_ONL        (1 << UNIT_V_ONL)
#define UNIT_WLK        (1 << UNIT_V_WLK)
#define UNIT_ATP        (1 << UNIT_V_ATP)
#define UNIT_NOAUTO     (1 << UNIT_V_NOAUTO)
#define UNIT_FMT        (1 << UNIT_V_FMT)
#define UNIT_DTYPE      (UNIT_M_DTYPE << UNIT_V_DTYPE)
#define GET_DTYPE(x)    (((x) >> UNIT_V_DTYPE) & UNIT_M_DTYPE)
#define GET_FMT(x)      (((x) >> UNIT_V_FMT) & 0x1)
#define unit_plug       u4                              /* drive unit plug value */
#define dbn_size        u5
#define dbn             up7
#define ksxb            filebuf                         /* xfer buffer */

#define FMT_16          0                               /* 16-bit format */
#define FMT_18          1                               /* 18-bit format */

#define RA60_DTYPE      0
#define RA70_DTYPE      1
#define RA80_DTYPE      2
#define RA81_DTYPE      3
#define RA82_DTYPE      4

struct drvfmt {
    uint16      sect;                                   /* sectors (16-bit) */
    uint16      gof;                                    /* group offset (16-bit) */
    uint32      lbn;                                    /* LBN size (16-bit) */
    uint16      rcts;                                   /* RCT size (16-bit) */
    };

struct drvtyp {
    uint8       sdiv;                                   /* SDI version */
    uint8       sto;                                    /* short timeout */
    uint8       lto;                                    /* long timeout */
    uint8       retry;                                  /* retry # */
    uint16      xfrt;                                   /* xfer rate */
    uint16      erl;                                    /* error recovery levels */
    uint16      eet;                                    /* ECC error threshold */
    uint16      swr;                                    /* software revision */
    uint16      hwr;                                    /* hardware revision */
    uint16      hps;                                    /* header pre-amble size */
    uint16      dps;                                    /* data pre-amble size */
    int32       surf;                                   /* surfaces */
    uint32      lcyl;                                   /* LBN cylinders */
    uint32      xcyl;                                   /* XBN cylinders */
    uint32      dcyl;                                   /* DBN cylinders */
    uint16      tpg;                                    /* trk/grp */
    uint16      gpc;                                    /* grp/cyl */
    uint16      drg;                                    /* DBN readonly groups */
    uint16      fcts;                                   /* FCT copy size */
    uint8       rctc;                                   /* RCT copies */
    uint8       rbn;                                    /* RBNs/track */
    uint16      revs;                                   /* drive revs/second */
    uint16      mod;                                    /* MSCP model */
    uint32      media;                                  /* MSCP media */
    uint32      flgs;                                   /* flags */
    struct drvfmt fmt[2];
    const char  *name;                                  /* name */
    };

#define KS_DRV(d) \
  { d##_SDIV, d##_STO,  d##_LTO,  d##_RETRY, \
    d##_XFRT, d##_ERL,  d##_EET,  d##_SWR, \
    d##_HWR,  d##_HPS,  d##_DPS,  d##_SURF, \
    d##_LCYL, d##_XCYL, d##_DCYL, d##_TPG, \
    d##_GPC,  d##_DRG,  d##_FCTS, d##_RCTC, \
    d##_RBN,  d##_REVS, d##_MOD,  d##_MED, \
    d##_FLGS, \
    { { d##_SECT_16, d##_GOF_16, d##_LBN_16, d##_RCTS_16 }, \
      { d##_SECT_18, d##_GOF_18, d##_LBN_18, d##_RCTS_18 } }, \
    #d }
#define KS_SIZE(d)      d##_LBN_16                      /* FIXME: support 18-bit mode */

static struct drvtyp drv_tab[] = {
    KS_DRV (RA60),
    KS_DRV (RA70),
    KS_DRV (RA80),
    KS_DRV (RA81),
    KS_DRV (RA82),
    { 0 }
    };


extern int32 tmxr_poll;                                 /* calibrated delay */

uint8 ks_mode = 0;

t_stat ks_svc (UNIT *uptr);
t_stat ks_reset (DEVICE *dptr);
t_stat ks_init (uint16 req, uint16 act);
t_stat ks_attach (UNIT *uptr, CONST char *cptr);
t_stat ks_detach (UNIT *uptr);
t_stat ks_set_wlk (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ks_set_fmt (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ks_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ks_set_ctype (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ks_set_plug (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ks_show_plug (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ks_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ks_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ks_show_wlk (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ks_show_fmt (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ks_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);

/* debugging bitmaps */

#define DBG_TRC         KG_DBG_TRC                      /* trace routine calls */
#define DBG_DSK         0x0010                          /* display sim_disk activities */
#define DBG_SDI         0x0020                          /* SDI commands */
#define DBG_FRB         0x0040                          /* FRB operations */
#define DBG_DRAT        0x0080                          /* DRAT operations */

DEBTAB ks_debug[] = {
    { "TRACE", DBG_TRC,  "trace routine calls" },
    { "SDI",   DBG_SDI,  "SDI commands" },
    { "FRB",   DBG_FRB,  "FRB operations" },
    { "DRAT",  DBG_DRAT, "DRAT operations" },
    { 0 }
};

HDIB ks_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    K_SDI, 0, &ks_init
    };

UNIT ks_unit[KS_MAXDR] = {
    { UDATA (&ks_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KS_SIZE (RA80)) },
    { UDATA (&ks_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KS_SIZE (RA80)) },
    { UDATA (&ks_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KS_SIZE (RA80)) },
    { UDATA (&ks_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KS_SIZE (RA80)) },
    };

REG ks_reg[] = {
    { NULL }
    };

MTAB ks_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", "ADDRESS",
      NULL, &kg_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "CONTROL", "CONTROL",
      NULL, &ks_show_ctrl, NULL },
    { UNIT_WLK, 0, NULL, "WRITEENABLED",
      &ks_set_wlk, NULL, NULL, "Write enable disk drive" },
    { UNIT_WLK, UNIT_WLK, NULL, "LOCKED",
      &ks_set_wlk, NULL, NULL, "Write lock disk drive" },
    { MTAB_XTD|MTAB_VUN, 0, "WRITE", NULL,
      NULL, &ks_show_wlk, NULL, "Display drive writelock status" },
    { MTAB_XTD|MTAB_VUN, FMT_16, NULL, "16BIT",
      &ks_set_fmt, NULL, NULL, "Set drive format to 16-bit" },
    { MTAB_XTD|MTAB_VUN, FMT_18, NULL, "18BIT",
      &ks_set_fmt, NULL, NULL, "Set drive format to 18-bit" },
    { MTAB_XTD|MTAB_VUN, 0, "FORMAT", NULL,
      NULL, &ks_show_fmt, NULL, "Display drive format" },
    { MTAB_XTD|MTAB_VUN, RA60_DTYPE, NULL, "RA60",
      &ks_set_type, NULL, NULL, "Set RA60 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA70_DTYPE, NULL, "RA70",
      &ks_set_type, NULL, NULL, "Set RA70 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA80_DTYPE, NULL, "RA80",
      &ks_set_type, NULL, NULL, "Set RA80 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA81_DTYPE, NULL, "RA81",
      &ks_set_type, NULL, NULL, "Set RA81 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA82_DTYPE, NULL, "RA82",
      &ks_set_type, NULL, NULL, "Set RA82 Disk Type" },
    { MTAB_XTD|MTAB_VUN, 0, "TYPE", NULL,
      NULL, &ks_show_type, NULL, "Display device type" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "UNIT", "UNIT=val (0-4095)",
      &ks_set_plug, &ks_show_plug, NULL, "Set/Display Unit plug value" },
    { MTAB_XTD|MTAB_VDV, K_SDI, NULL, "KSDI",
      &ks_set_ctype, NULL, NULL, "Set K.sdi Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SI4, NULL, "KSI4",
      &ks_set_ctype, NULL, NULL, "Set K.si 4-port Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SI8, NULL, "KSI8",
      &ks_set_ctype, NULL, NULL, "Set K.si 8-port Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SCSI, NULL, "KSCSI",
      &ks_set_ctype, NULL, NULL, "Set K.scsi Controller Type" },
    { MTAB_XTD | MTAB_VDV, 0, "TYPE", NULL,
      NULL, &ks_show_ctype, NULL, "Display controller type" },
    { 0 }
    };

DEVICE ks_dev = {
    "KS", ks_unit, ks_reg, ks_mod,
    KS_MAXDR, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &ks_reset,
    NULL, &ks_attach, &ks_detach,
    &ks_dib, DEV_DEBUG | DEV_HSC | DEV_DISABLE,
    0, ks_debug
    };


/* The version must be at least DS = 3, MC = 00 */

t_stat ks_init (uint16 req, uint16 act)
{
t_bool run;
if (act == K_RUN) {
    ks_dib.cma = kg_init (&run);
    sim_debug (DBG_TRC, &ks_dev, "K.sdi %d startup: KH = %06o at %o\n", req, ks_dib.cma, (uint16)inst_pc);
    PUTW (ks_dib.cma + KG_RSL, 1);                        /* KG$RSL - result of self test */
    PUTW (ks_dib.cma + KG_VRS, 01452);                    /* KG$VRS - version (DS = 3, MC = 42) */
    if (!run)
        return SCPE_OK;
    sim_clock_coschedule (&ks_unit[0], tmxr_poll);        /* activate */
    sim_clock_coschedule (&ks_unit[1], tmxr_poll);        /* activate */
    sim_clock_coschedule (&ks_unit[2], tmxr_poll);        /* activate */
    sim_clock_coschedule (&ks_unit[3], tmxr_poll);        /* activate */
    }
else {
    sim_debug (DBG_TRC, &ks_dev, "K.sdi %d halt at %o\n", req, (uint16)inst_pc);
    sim_cancel (&ks_unit[0]);
    sim_cancel (&ks_unit[1]);
    sim_cancel (&ks_unit[2]);
    sim_cancel (&ks_unit[3]);
    ks_dib.cma = 0;
    }
return SCPE_OK;
}

t_stat ks_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint32 ks;
t_stat r;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;
ks = dibp->cma;

kg_print_word (st, ks, "KS$7SM", -040, 0);
kg_print_word (st, ks, "KS$7SC", -040, 2);
kg_print_word (st, ks, "KS$7FL", -036, 0);
kg_print_word (st, ks, "KS$7BW", -036, 2);
kg_print_word (st, ks, "KS$6SM", -034, 0);
kg_print_word (st, ks, "KS$6SC", -034, 2);
kg_print_word (st, ks, "KS$6FL", -032, 0);
kg_print_word (st, ks, "KS$6BW", -032, 2);
kg_print_word (st, ks, "KS$5SM", -030, 0);
kg_print_word (st, ks, "KS$5SC", -030, 2);
kg_print_word (st, ks, "KS$5FL", -026, 0);
kg_print_word (st, ks, "KS$5BW", -026, 2);
kg_print_word (st, ks, "KS$4SM", -024, 0);
kg_print_word (st, ks, "KS$4SC", -024, 2);
kg_print_word (st, ks, "KS$4FL", -022, 0);
kg_print_word (st, ks, "KS$4BW", -022, 2);
kg_print_word (st, ks, "KS$3SM", -020, 0);
kg_print_word (st, ks, "KS$3SC", -020, 2);
kg_print_word (st, ks, "KS$3FL", -016, 0);
kg_print_word (st, ks, "KS$3BW", -016, 2);
kg_print_word (st, ks, "KS$2SM", -014, 0);
kg_print_word (st, ks, "KS$2SC", -014, 2);
kg_print_word (st, ks, "KS$2FL", -012, 0);
kg_print_word (st, ks, "KS$2BW", -012, 2);
kg_print_word (st, ks, "KS$1SM", -010, 0);
kg_print_word (st, ks, "KS$1SC", -010, 2);
kg_print_word (st, ks, "KS$1FL", -006, 0);
kg_print_word (st, ks, "KS$1BW", -006, 2);
kg_print_word (st, ks, "KS$0SM", -004, 0);
kg_print_word (st, ks, "KS$0SC", -004, 2);              /* sector count? */
kg_print_word (st, ks, "KS$0FL", -002, 0);
kg_print_word (st, ks, "KS$0BW", -002, 2);
kg_print_word (st, ks, "KS$BAS", 000, 0);
kg_print_word (st, ks, "KS$0HD", 000, 2);
kg_print_word (st, ks, "KS$0TD", 002, 2);
kg_print_word (st, ks, "KS$1HD", 004, 2);
kg_print_word (st, ks, "KS$1TD", 006, 2);
kg_print_word (st, ks, "KS$2HD", 010, 2);
kg_print_word (st, ks, "KS$2TD", 012, 2);
kg_print_word (st, ks, "KS$3HD", 014, 2);
kg_print_word (st, ks, "KS$3TD", 016, 2);
kg_print_word (st, ks, "KS$4HD", 020, 2);
kg_print_word (st, ks, "KS$4TD", 022, 2);
kg_print_word (st, ks, "KS$5HD", 024, 2);
kg_print_word (st, ks, "KS$5TD", 026, 2);
kg_print_word (st, ks, "KS$6HD", 030, 2);
kg_print_word (st, ks, "KS$6TD", 032, 2);
kg_print_word (st, ks, "KS$7HD", 034, 2);
kg_print_word (st, ks, "KS$7TD", 036, 2);
kg_print_word (st, ks, "KS$TLK", 040, 2);

r = kg_show_ctrl (st, uptr, val, desc);
if (r != SCPE_OK)
    return r;

kg_print_word (st, ks, "KS$KSA", 0140, 0);              /* drive state area */
kg_print_word (st, ks, "KS$S0 ", 0140, 16);
kg_print_word (st, ks, "KS$S1 ", 0160, 16);
kg_print_word (st, ks, "KS$S2 ", 0200, 16);
kg_print_word (st, ks, "KS$S3 ", 0220, 16);
kg_print_word (st, ks, "KS$S4 ", 0240, 16);
kg_print_word (st, ks, "KS$S5 ", 0260, 16);
kg_print_word (st, ks, "KS$S6 ", 0300, 16);
kg_print_word (st, ks, "KS$S7 ", 0320, 16);
kg_print_word (st, ks, "KS$DMP", 0340, 66);
kg_print_word (st, ks, "KS$BW ", 0442, 2);
kg_print_word (st, ks, "KS$TM1", 0444, 2);
kg_print_word (st, ks, "KS$TM2", 0446, 2);
kg_print_word (st, ks, "KS$TM3", 0450, 2);
kg_print_word (st, ks, "KS$0LH", 0452, 2);
kg_print_word (st, ks, "KS$0HH", 0454, 2);
kg_print_word (st, ks, "KS$1LH", 0456, 2);
kg_print_word (st, ks, "KS$1HH", 0460, 2);
kg_print_word (st, ks, "KS$2LH", 0462, 2);
kg_print_word (st, ks, "KS$2HH", 0464, 2);
kg_print_word (st, ks, "KS$3LH", 0466, 2);
kg_print_word (st, ks, "KS$3HH", 0470, 2);
kg_print_word (st, ks, "KS$4LH", 0472, 2);
kg_print_word (st, ks, "KS$4HH", 0474, 2);
kg_print_word (st, ks, "KS$5LH", 0476, 2);
kg_print_word (st, ks, "KS$5HH", 0500, 2);
kg_print_word (st, ks, "KS$6LH", 0502, 2);
kg_print_word (st, ks, "KS$6HH", 0504, 2);
kg_print_word (st, ks, "KS$7LH", 0506, 2);
kg_print_word (st, ks, "KS$7HH", 0510, 2);

return SCPE_OK;
}

t_stat ksti_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint32 kt;
t_stat r;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;
kt = dibp->cma;

kg_print_word (st, kt, "KT$BAS", 0000, 0);
kg_print_word (st, kt, "KT$IPQ", 0000, 4);
kg_print_word (st, kt, "KT$WQH", 0004, 2);              /* work queue head? */
kg_print_word (st, kt, "KT$WQT", 0006, 2);              /* work queue tail? */
kg_print_word (st, kt, "KT$FLA", 0010, 2);
kg_print_word (st, kt, "KT$SQP", 0012, 2);
kg_print_word (st, kt, "KT$TCT", 0014, 2);
kg_print_word (st, kt, "KT$LAH", 0016, 2);
kg_print_word (st, kt, "KT$WAI", 0020, 2);
kg_print_word (st, kt, "KT$SEM", 0022, 12);
kg_print_word (st, kt, "KT$BW ", 0036, 2);

r = kg_show_ctrl (st, uptr, val, desc);
if (r != SCPE_OK)
    return r;

kg_print_word (st, kt, "KT$S0 ", 0140, 16);             /* drive state area */
kg_print_word (st, kt, "KT$S1 ", 0160, 16);
kg_print_word (st, kt, "KT$S2 ", 0200, 16);
kg_print_word (st, kt, "KT$S3 ", 0220, 16);
kg_print_word (st, kt, "UNUSED", 0240, 64);
kg_print_word (st, kt, "KT$DMP", 0340, 66);
kg_print_word (st, kt, "KT$AQH", 0442, 2);
kg_print_word (st, kt, "KT$AQT", 0444, 2);
kg_print_word (st, kt, "KT$BCN", 0446, 2);
kg_print_word (st, kt, "KT$TMO", 0450, 2);
kg_print_word (st, kt, "KT$FCN", 0452, 2);
kg_print_word (st, kt, "KT$FCT", 0454, 2);
kg_print_word (st, kt, "KT$T1 ", 0456, 2);
kg_print_word (st, kt, "KT$T2 ", 0460, 2);
kg_print_word (st, kt, "KT$T3 ", 0462, 2);
kg_print_word (st, kt, "KT$T4 ", 0464, 2);

return SCPE_OK;
}

void ks_print_drat (uint32 dt)
{
kg_print_word (NULL, dt, "DT$LNK", DT_LNK, 2);
kg_print_word (NULL, dt, "DT$L1C", DT_L1C, 2);
kg_print_word (NULL, dt, "DT$ERQ", DT_ERQ, 2);
kg_print_word (NULL, dt, "DT$OFF", DT_OFF, 2);
kg_print_word (NULL, dt, "DT$STA", DT_STA, 2);
kg_print_word (NULL, dt, "DT$DPS", DT_DPS, 1);
kg_print_word (NULL, dt, "DT$HPS", DT_HPS, 1);
kg_print_word (NULL, dt, "DT$CQ ", DT_CQ , 0);
kg_print_word (NULL, dt, "DT$HST", DT_HST, 8);
kg_print_word (NULL, dt, "DT$CNT", DT_CNT, 0);
kg_print_word (NULL, dt, "DT$MSC", DT_MSC, 8);
kg_print_word (NULL, dt, "DT$SCT", DT_SCT, 2);
kg_print_word (NULL, dt, "DT$XXX", DT_XXX, 2);
kg_print_word (NULL, dt, "DT$HLD", DT_HLD, 4);
kg_print_word (NULL, dt, "DT$CYL", DT_CYL, 0);
kg_print_word (NULL, dt, "DT$BFC", DT_BFC, 2);
kg_print_word (NULL, dt, "DT$LFL", DT_LFL, 2);
kg_print_word (NULL, dt, "DT$TFC", DT_TFC, 2);
kg_print_word (NULL, dt, "DT$DU ", DT_DU , 2);
kg_print_word (NULL, dt, "DT$RCC", DT_RCC, 2);
kg_print_word (NULL, dt, "DT$DTC", DT_DTC, 2);
kg_print_word (NULL, dt, "DT$PBC", DT_PBC, 2);
kg_print_word (NULL, dt, "DT$DCB", DT_DCB, 2);
kg_print_word (NULL, dt, "DT$FLG", DT_FLG, 2);
kg_print_word (NULL, dt, "UNUSED", 066, 2);
}

void ks_print_frb (uint32 frb)
{
kg_print_word (NULL, frb, "F$LINK", F_LINK, 2);
kg_print_word (NULL, frb, "F$PCTR", F_PCTR, 2);
kg_print_word (NULL, frb, "F$RTE ", F_RTE , 2);
kg_print_word (NULL, frb, "F$MOD ", F_MOD , 2);
kg_print_word (NULL, frb, "F$CTRL", F_CTRL, 2);
kg_print_word (NULL, frb, "F$OFF ", F_OFF , 4);
kg_print_word (NULL, frb, "F$LLBN", F_LLBN, 2);
kg_print_word (NULL, frb, "F$RT1 ", F_RT1 , 2);
kg_print_word (NULL, frb, "F$HLBN", F_HLBN, 2);
kg_print_word (NULL, frb, "F$SCT ", F_SCT , 0);
kg_print_word (NULL, frb, "F$RT2 ", F_RT2 , 2);
kg_print_word (NULL, frb, "F$TRK ", F_TRK , 1);
kg_print_word (NULL, frb, "UNUSED", 027, 1);
kg_print_word (NULL, frb, "F$GRP ", F_GRP , 0);
kg_print_word (NULL, frb, "F$RT3 ", F_RT3 , 2);
kg_print_word (NULL, frb, "F$RSB ", F_RSB , 2);
kg_print_word (NULL, frb, "F$SCNT", F_SCNT, 1);
kg_print_word (NULL, frb, "UNUSED", 035, 1);
kg_print_word (NULL, frb, "F$SRTE", F_SRTE, 2);
kg_print_word (NULL, frb, "F$BFHD", F_BFHD, 2);
kg_print_word (NULL, frb, "F$BFTL", F_BFTL, 2);
kg_print_word (NULL, frb, "F$RT4 ", F_RT4 , 2);
kg_print_word (NULL, frb, "F$CYL ", F_CYL , 2);
kg_print_word (NULL, frb, "F$SLLB", F_SLLB, 2);
kg_print_word (NULL, frb, "F$SHLB", F_SHLB, 2);
kg_print_word (NULL, frb, "F$SBFH", F_SBFH, 2);
kg_print_word (NULL, frb, "F$SSCT", F_SSCT, 1);
kg_print_word (NULL, frb, "F$REMN", F_REMN, 1);
kg_print_word (NULL, frb, "F$PRMY", F_PRMY, 2);
kg_print_word (NULL, frb, "F$SLRB", F_SLRB, 2);
kg_print_word (NULL, frb, "F$TGDT", F_TGDT, 2);
kg_print_word (NULL, frb, "F$SNUM", F_SNUM, 2);
kg_print_word (NULL, frb, "F$SCR ", F_SCR , 0);
kg_print_word (NULL, frb, "F$TCNT", F_TCNT, 2);
kg_print_word (NULL, frb, "F$DRAT", F_DRAT, 2);
kg_print_word (NULL, frb, "F$DSTR", F_DSTR, 2);
kg_print_word (NULL, frb, "F$ESUM", F_ESUM, 2);
kg_print_word (NULL, frb, "F$DUCB", F_DUCB, 2);
kg_print_word (NULL, frb, "F$BPTR", F_BPTR, 2);
kg_print_word (NULL, frb, "F$SQL ", F_SQL , 2);
kg_print_word (NULL, frb, "F$FLGS", F_FLGS, 2);
kg_print_word (NULL, frb, "F$CRUN", F_CRUN, 1);
kg_print_word (NULL, frb, "F$CFLG", F_CFLG, 1);
kg_print_word (NULL, frb, "UNUSED", 0112, 6);
}

void ks_print_dcb (uint32 dc)
{
kg_print_word (NULL, dc, "DC$LNK", DC_LNK, 2);
kg_print_word (NULL, dc, "DC$CMD", DC_CMD, 2);
kg_print_word (NULL, dc, "DC$RSB", DC_RSB, 2);
kg_print_word (NULL, dc, "DC$FCT", DC_FCT, 2);
kg_print_word (NULL, dc, "DC$STA", DC_STA, 2);
kg_print_word (NULL, dc, "DC$DUC", DC_DUC, 2);
kg_print_word (NULL, dc, "DC$0CQ", DC_0CQ, 2);
kg_print_word (NULL, dc, "DC$STC", DC_STC, 0);
kg_print_word (NULL, dc, "DC$1CQ", DC_1CQ, 2);
kg_print_word (NULL, dc, "DC$ICT", DC_ICT, 14);
kg_print_word (NULL, dc, "DC$RTY", DC_RTY, 1);
kg_print_word (NULL, dc, "DC$RBS", DC_RBS, 1);
kg_print_word (NULL, dc, "DC$IRT", DC_IRT, 32);
kg_print_word (NULL, dc, "DC$THP", DC_THP, 2);
kg_print_word (NULL, dc, "DC$LRB", DC_LRB, 2);
kg_print_word (NULL, dc, "DC$RSV", DC_RSV, 4);
kg_print_word (NULL, dc, "DC$PAD", DC_PAD, 16);
}

t_stat ks_notify (UNIT *uptr, uint32 ks, uint32 port)
{
uint32 ksa = ks + KS_S0 + (port * 16);                  /* drive state area */
uint32 sq, fl;

fl = GETB (ksa + KSA_FLGS);                             /* get drive flags */
if ((fl & FLGS_SVC) == 0) {                             /* drive not known to system? */
    sq = GETP (ksa);                                    /* get service queue pointer */
    if (sq == 0) {
//        sim_printf ("Drive service queue is NULL!\n");
        return SCPE_OK;
        }
    if (Q_LOCKED (sq)) {
        sim_printf ("Service queue currently locked\n");
        return SCPE_OK;
        }
    PUTW (ksa, 0);                                      /* clear service queue pointer */
    fl |= FLGS_SVC;
    PUTB (ksa + KSA_FLGS, fl);                          /* now on service queue */
    kg_send (sq, ksa);                                  /* send drive state area to service queue */
//    sim_printf ("Drive state area sent to service queue\n");
    }
return SCPE_OK;
}

/* Online */

uint16 ks_online (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "ONLINE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
uptr->flags = uptr->flags | UNIT_ONL;
return 002;
}

/* Disconnect */

uint16 ks_disc (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "DISCONNECT\n");
ks_mode = 0;
uptr->flags = uptr->flags & ~UNIT_ONL;
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Diagnose */

uint16 ks_diag (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 test = GETW (cmd + 002);                          /* drive resident test to run */
sim_debug_unit (DBG_SDI, uptr, "DIAGNOSE, test = %02X\n", test);
PUTW (rsp, 0xFC);                                       /* response opcode */
return 004;
}

/* Drive clear */

uint16 ks_clear (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "DRIVE CLEAR\n");
ks_mode = 0;
uptr->flags = uptr->flags & ~UNIT_ONL;
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Get common characteristics */

uint16 ks_getchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];

sim_debug_unit (DBG_SDI, uptr, "GET COMMON CHARACTERISTICS\n");
PUTW (rsp + 000, 0x78);                                 /* response opcode */
PUTW (rsp + 002, (dtab->sdiv << 4) | dtab->sto);        /* short TO / SDI version */
PUTW (rsp + 004, dtab->xfrt);                           /* transfer rate */
PUTW (rsp + 006, (dtab->retry << 4) | dtab->lto);       /* long TO / retry # */
if (fmt == FMT_18)
    PUTW (rsp + 010, 0x80 | dtab->rctc);                /* RCT copies / reserved, 0x80 = 576 byte mode allowed */
else
    PUTW (rsp + 010, dtab->rctc);                       /* RCT copies / reserved */
PUTW (rsp + 012, dtab->erl);                            /* error recovery levels */
PUTW (rsp + 014, dtab->eet);                            /* ECC error threshold */
PUTW (rsp + 016, dtab->swr);                            /* software revision */
PUTW (rsp + 020, dtab->hwr);                            /* hardware revision, 0x80 = limited diag support */
PUTW (rsp + 022, 0x7A);                                 /* unique drive ID #1 */
PUTW (rsp + 024, 0x0A);
PUTW (rsp + 026, 0x00);                                 /* unique drive ID #2 */
PUTW (rsp + 030, 0x00);
PUTW (rsp + 032, 0x00);                                 /* unique drive ID #3 */
PUTW (rsp + 034, 0x00);
PUTW (rsp + 036, dtab->mod);                            /* drive type identifier */
PUTW (rsp + 040, dtab->revs);                           /* drive revs/s */
PUTW (rsp + 042, 0x00);
PUTW (rsp + 044, 0x00);
PUTW (rsp + 046, 0x00);
PUTW (rsp + 050, 0x00);
PUTW (rsp + 052, 0x00);
PUTW (rsp + 054, 0x00);
PUTW (rsp + 056, 0x00);
return 030;
}

/* Get subunit characteristics */

uint16 ks_getsubchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
struct drvtyp *dtab = &drv_tab[dtyp];

sim_debug_unit (DBG_SDI, uptr, "GET SUBUNIT CHARACTERISTICS\n");
PUTW (rsp + 0000, 0x77);                                /* response opcode */
PUTW (rsp + 0002, dtab->lcyl & 0xFF);                   /* low order LBN space in cylinders */
PUTW (rsp + 0004, (dtab->lcyl >> 8) & 0xFF);
PUTW (rsp + 0006, (dtab->lcyl >> 16) & 0xFF);           /* high order LBN space in cylinders */
PUTW (rsp + 0010, (dtab->lcyl >> 24) & 0xFF);
PUTW (rsp + 0012, dtab->gpc);                           /* groups/cylinder */
PUTW (rsp + 0014, 0x00);                                /* start LBN / start XBN? */
PUTW (rsp + 0016, dtab->tpg);                           /* tracks/group */
PUTW (rsp + 0020, 0x00);                                /* start RBN / start DBN? */
PUTW (rsp + 0022, dtab->rbn);                           /* RBNs/track */
PUTW (rsp + 0024, 0x00);                                /* reserved */
PUTW (rsp + 0026, dtab->dps);                           /* data preamble size */
PUTW (rsp + 0030, dtab->hps);                           /* header preamble size */
PUTW (rsp + 0032, dtab->media & 0xFF);                  /* media type low */
PUTW (rsp + 0034, (dtab->media >> 8) & 0xFF);
PUTW (rsp + 0036, (dtab->media >> 16) & 0xFF);          /* media type high */
PUTW (rsp + 0040, (dtab->media >> 24) & 0xFF);
PUTW (rsp + 0042, dtab->fcts & 0xFF);                   /* FCT copy size in XBNs */
PUTW (rsp + 0044, (dtab->fcts >> 8) & 0xFF);
PUTW (rsp + 0046, dtab->fmt[FMT_16].sect);              /* LBNs/track (16-bit) */
PUTW (rsp + 0050, dtab->fmt[FMT_16].gof);               /* group offset (16-bit) */
PUTW (rsp + 0052, dtab->fmt[FMT_16].lbn & 0xFF);        /* LBNs in host area low (16-bit) */
PUTW (rsp + 0054, (dtab->fmt[FMT_16].lbn >> 8) & 0xFF);
PUTW (rsp + 0056, (dtab->fmt[FMT_16].lbn >> 16) & 0xFF); /* LBNs in host area high (16-bit) */
PUTW (rsp + 0060, (dtab->fmt[FMT_16].lbn >> 24) & 0xFF);
PUTW (rsp + 0062, dtab->fmt[FMT_16].rcts & 0xFF);       /* RCT size in LBNs (16-bit) */
PUTW (rsp + 0064, (dtab->fmt[FMT_16].rcts >> 8) & 0xFF);
PUTW (rsp + 0066, dtab->fmt[FMT_18].sect);              /* LBNs/track (18-bit) */
PUTW (rsp + 0070, dtab->fmt[FMT_18].gof);               /* group offset (18-bit) */
PUTW (rsp + 0072, dtab->fmt[FMT_18].lbn & 0xFF);        /* LBNs in host area low (18-bit) */
PUTW (rsp + 0074, (dtab->fmt[FMT_18].lbn >> 8) & 0xFF);
PUTW (rsp + 0076, (dtab->fmt[FMT_18].lbn >> 16) & 0xFF); /* LBNs in host area high (18-bit) */
PUTW (rsp + 0100, (dtab->fmt[FMT_18].lbn >> 24) & 0xFF);
PUTW (rsp + 0102, dtab->fmt[FMT_18].rcts & 0xFF);       /* RCT size in LBNs (18-bit) */
PUTW (rsp + 0104, (dtab->fmt[FMT_18].rcts >> 8) & 0xFF);
PUTW (rsp + 0106, dtab->xcyl & 0xFF);                   /* XBN space in cylinders */
PUTW (rsp + 0110, (dtab->xcyl >> 8) & 0xFF);
PUTW (rsp + 0112, dtab->drg);                           /* DBN readonly groups */
PUTW (rsp + 0114, dtab->dcyl & 0xFF);                   /* DBN space in cylinders */
PUTW (rsp + 0116, (dtab->dcyl >> 8) & 0xFF);
return 050;
}

/* Get status */

uint16 ks_getsts (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "GET STATUS\n");
PUTW (rsp + 000, 0xF6);                                 /* response opcode */
PUTW (rsp + 002, uptr->unit_plug);                      /* unit number (low) */
PUTW (rsp + 004, UMSK_U1 | ((uptr->unit_plug >> 8) & 0xF)); /* unit number (high) / subunit mask */
if (uptr->flags & UNIT_ATT)                             /* attached? */
    PUTW (rsp + 006, REQ_SR | REQ_PS | REQ_RU);         /* request byte - drive ready */
else
    PUTW (rsp + 006, REQ_PS);                           /* request byte - drive not ready */
PUTW (rsp + 010, ks_mode);                              /* mode byte */
PUTW (rsp + 012, 0x00);                                 /* error byte */
PUTW (rsp + 014, 0x00);                                 /* controller byte */
PUTW (rsp + 016, 0x00);                                 /* retry-fail code */
PUTW (rsp + 020, 0x00);                                 /* extended drive status */
PUTW (rsp + 022, 0x00);                                 /* extended drive status */
PUTW (rsp + 024, 0x00);                                 /* extended drive status */
PUTW (rsp + 026, 0x00);                                 /* extended drive status */
PUTW (rsp + 030, 0x00);                                 /* extended drive status */
PUTW (rsp + 032, 0x00);                                 /* extended drive status */
PUTW (rsp + 034, 0x00);                                 /* extended drive status */
PUTW (rsp + 036, 0x00);                                 /* end of frame */
return 020;
}

/* Read memory */

uint16 ks_rdmem (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "READ MEMORY\n");
PUTW (rsp, 0x72);                                       /* response opcode */
return 030;
}

/* Recalibrate */

uint16 ks_recal (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "RECALIBRATE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Seek */

uint16 ks_seek (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint32 cyl, grp;
cyl = GETB (cmd + 004);
cyl = cyl << 8;
cyl = cyl | GETB (cmd + 002);
grp = GETB (cmd + 006);
sim_debug_unit (DBG_SDI, uptr, "SEEK, cyl = %d, grp = %d\n", cyl, grp);
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 ks_chmode (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 msk, val;
val = GETB (cmd + 002);                                 /* new mode value */
msk = GETB (cmd + 004);                                 /* mask */
ks_mode = (ks_mode & ~msk) | (val & msk);
sim_debug_unit (DBG_SDI, uptr, "CHANGE MODE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 ks_chflag (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "CHANGE CONTROLLER FLAGS\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 ks_topology (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "TOPOLOGY\n");
PUTW (rsp, 0x6F);                                       /* TOPOLOGY RESPONSE */
return 020;
}

/* SDI level 2 command */

uint16 ks_cmd (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 opc = GETB (cmd);                                 /* get SDI opcode */
uint16 rsz = 0;

switch (opc) {
    case 0x81:                                          /* CHANGE MODE */
        rsz = ks_chmode (uptr, cmd, rsp);
        break;

    case 0x82:                                          /* CHANGE CONTROLLER FLAGS */
        rsz = ks_chflag (uptr, cmd, rsp);
        break;

    case 0x03:                                          /* DIAGNOSE */
        rsz = ks_diag (uptr, cmd, rsp);
        break;

    case 0x84:                                          /* DISCONNECT */
        rsz = ks_disc (uptr, cmd, rsp);
        break;

    case 0x05:                                          /* DRIVE CLEAR */
        rsz = ks_clear (uptr, cmd, rsp);
        break;

    case 0x87:                                          /* GET COMMON CHARACTERISTICS */
        rsz = ks_getchar (uptr, cmd, rsp);
        break;

    case 0x88:                                          /* GET SUBUNIT CHARACTERISTICS */
        rsz = ks_getsubchar (uptr, cmd, rsp);
        break;

    case 0x09:                                          /* GET STATUS */
        rsz = ks_getsts (uptr, cmd, rsp);
        break;

    case 0x0A:                                          /* SEEK */
        rsz = ks_seek (uptr, cmd, rsp);
        break;

    case 0x8B:                                          /* ONLINE drive */
        rsz = ks_online (uptr, cmd, rsp);
        break;

    case 0x8D:                                          /* READ MEMORY */
        rsz = ks_rdmem (uptr, cmd, rsp);
        break;

    case 0x8E:                                          /* RECALIBRATE */
        rsz = ks_recal (uptr, cmd, rsp);
        break;

    case 0x90:                                          /* TOPOLOGY */
        rsz = ks_topology (uptr, cmd, rsp);
        break;

    default:
        sim_printf ("Unknown SDI level 2 command: 0x%02X\n", opc);
        PUTW (rsp, 0x7E);                               /* response opcode */
        rsz = 2;
        break;
        }

PUTB (rsp + 1, 0x71);                                   /* start of frame */
for (int i = 3; i < (rsz << 1); i = i + 2)
    PUTB (rsp + i, 0xD4);                               /* continue frame */
PUTW (rsp + ((rsz - 1) << 1), 0xB200);                  /* end of frame */
return rsz;
}

/* Get realtime drive status */

void ks_rtds (UNIT *uptr, uint32 rsp)
{
uint16 sts = 0;
sim_debug_unit (DBG_SDI, uptr, "LINE STATUS\n");
if (uptr->flags & UNIT_ONL)
    sts = RTDS_RWR;                                     /* READ/WRITE READY */
else
    sts = RTDS_AVL | RTDS_ATN | RTDS_RR;                /* AVAILABLE, RECEIVER READY */
PUTW (rsp, sts);
}

void ks_process_dcb (UNIT *uptr, uint32 ks, uint32 port)
{
uint32 ksa = ks + KS_S0 + (port * 16);                  /* drive state area */
uint32 dc, cq, rsp, opc, rsz, cmd, fct;

for (int j = 0; j < 20; j++) {
    dc = GETP (ksa + KSA_HEAD);                         /* get DCB */
    if (dc == 0)
        break;

    cq = GETP (dc + DC_0CQ);                            /* get success queue */
    if (cq == 0)
        break;

    if (Q_LOCKED (cq))                                  /* success queue locked? */
        break;

    dc = kg_receive (ksa + KSA_HEAD);                   /* dequeue from drive state area */

//    ks_print_dcb (dc);

    rsp = GETP (dc + DC_RSB);                           /* get response buffer */
    opc = GETW (dc + DC_STA);
    rsz = 0;
    switch (opc & 0177400) {
        case 014400:                                    /* ECHO DATA */
            sim_debug_unit (DBG_SDI, uptr, "ECHO DATA\n");
            cmd = GETW (dc + DC_CMD);                   /* get immediate */
            PUTW (rsp, cmd);                            /* copy to response */
            break;
        case 010400:                                    /* LINE STATUS */
            ks_rtds (uptr, rsp);
            break;
        default:
            cmd = GETP (dc + DC_CMD);                   /* get command buffer */
            if (cmd)
                rsz = ks_cmd (uptr, cmd, rsp);
            else {
                sim_printf ("No command pointer\n");
                PUTW (rsp, 0x7E);                       /* write response word */
                }
            break;
        }
    fct = GETW (dc + DC_FCT);
    fct = fct - rsz;
    PUTW (dc + DC_FCT, fct);                            /* set response length */
    kg_send (cq, dc);                                   /* send to completion queue */
    }
}

void ks_read_fct (UNIT *uptr, uint32 dma)
{
uint32 fmt = GET_FMT (uptr->flags);

if (fmt == FMT_18)
    PUTDW (dma + 0000, FMT_576);                        /* bytes/sector */
else
    PUTDW (dma + 0000, FMT_512);                        /* bytes/sector */
PUTDW (dma + 0002, 6);                                  /* format instance */
PUTDW (dma + 0004, 0xCD15);                             /* volume serial (low) */
PUTDW (dma + 0006, 0x075B);                             /* volume serial (high) */
PUTDW (dma + 0010, 0x00);
PUTDW (dma + 0012, 0x00);
PUTDW (dma + 0014, 0x00);                               /* first formatted */
PUTDW (dma + 0016, 0x00);
PUTDW (dma + 0020, 0xFF);
PUTDW (dma + 0022, 0xFF);
PUTDW (dma + 0024, 0x00);                               /* date formatted */
PUTDW (dma + 0026, 0x00);
PUTDW (dma + 0030, 0xFF);
PUTDW (dma + 0032, 0xFF);
PUTDW (dma + 0034, 0x00);                               /* bad PBNs in FCT (512 - low) */
PUTDW (dma + 0036, 0x00);                               /* bad PBNs in FCT (512 - high) */
PUTDW (dma + 0040, 0x00);                               /* bad PBNs in FCT (576 - low) */
PUTDW (dma + 0042, 0x00);                               /* bad PBNs in FCT (576 - high) */
PUTDW (dma + 0044, 63);                                 /* scratch area offset */
PUTDW (dma + 0046, 417);                                /* size (not last) */
PUTDW (dma + 0050, 289);                                /* size (last) */
PUTDW (dma + 0052, 0);                                  /* flags */
PUTDW (dma + 0054, 0);                                  /* format version */
}

void ks_read_rct (uint32 dma)
{
PUTDW (dma + 0000, 0xCD15);                             /* serial number (low) */
PUTDW (dma + 0002, 0x075B);                             /* serial number (high) */
PUTDW (dma + 0004, 0);
PUTDW (dma + 0006, 0);
PUTDW (dma + 0010, 0);                                  /* flags */
PUTDW (dma + 0012, 0);
PUTDW (dma + 0014, 1000);                               /* LBN being replaced (low) */
PUTDW (dma + 0016, 0);
PUTDW (dma + 0020, 0);                                  /* LBN being replaced (high) */
PUTDW (dma + 0022, 33);                                 /* replacement RBN (low) */
PUTDW (dma + 0024, 0);                                  /* replacement RBN (high) */
PUTDW (dma + 0026, 32);                                 /* bad RBN (low) */
PUTDW (dma + 0030, 0);                                  /* bad RBN (high) */
PUTDW (dma + 0032, 0);                                  /* cache ID */
PUTDW (dma + 0034, 0);
PUTDW (dma + 0036, 0);
PUTDW (dma + 0040, 0);                                  /* cache incarnation (low) */
PUTDW (dma + 0042, 0);                                  /* cache incarnation (high) */
PUTDW (dma + 0044, 0);                                  /* incarnation date */
PUTDW (dma + 0046, 0);
PUTDW (dma + 0050, 0);
PUTDW (dma + 0052, 0);
}

/* Diagnostic block pattern */

const uint16 ks_dbn[3] = {
    0155555,
    0133333,
    0066666
    };

/* See ILDISK:51440 for alogrithm */

uint16 ks_edc (uint32 dma)
{
uint16 edc = 0105;
for (int i = 0; i < 256; i++) {
    edc = edc ^ GETDW (dma + (i << 1));
    if (edc & 0100000) {
        edc = edc << 1;
        edc++;
        }
    else
        edc = edc << 1;
    }
return edc;
}

t_stat ks_read_lbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
t_seccnt sects, sectsread;
uint16 *xbuf = (uint16 *)uptr->ksxb;
t_stat r;

if (lbn >= ftab->lbn)                                   /* outside host area? */
    ks_read_rct (dma);                                  /* read RCT */
else {
    sects = 1;
    r = sim_disk_rdsect (uptr, lbn, (uint8 *)xbuf, &sectsread, sects);
    if (r != SCPE_OK)
        return r;
    for (int i = 0; i < 256; i++) {
        PUTDW (dma + (i << 1), xbuf[i]);
        }
    }
PUTDW (dma + 01000, ks_edc (dma));
return SCPE_OK;
}

t_stat ks_write_lbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
t_seccnt sects, sectswritten;
uint16 *xbuf = (uint16 *)uptr->ksxb;
t_stat r;

if (lbn >= ftab->lbn) { }                               /* outside host area? */
else {
    for (int i = 0; i < 256; i++) {
        xbuf[i] = GETDW (dma + (i << 1));
        }
    sects = 1;
    r = sim_disk_wrsect (uptr, lbn, (uint8 *)xbuf, &sectswritten, sects);
    if (r != SCPE_OK)
        return r;
    }
return SCPE_OK;
}

void ks_read_rbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
}

void ks_read_xbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
if (lbn == 0)
    ks_read_fct (uptr, dma);                            /* read FCT */
}

void ks_read_dbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint16 *dbn = (uint16 *) uptr->dbn;
uint32 ptn = 0;

if (lbn > uptr->dbn_size) {                             /* read only group? */
    PUTDW (dma + 0000, 0177777);
    for (int i = 0; i < 256; i++) {
        PUTDW (dma + 0002 + (i << 1), ks_dbn[ptn++]);
        if (ptn == 3)
            ptn = 0;
        }
    }
else {
    dbn = &dbn[lbn * 256];
    for (int i = 0; i < 256; i++) {
        PUTDW (dma + (i << 1), dbn[i]);
        }
    }
PUTDW (dma + 01000, ks_edc (dma));
}

void ks_write_dbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint16 *dbn = (uint16 *) uptr->dbn;
if (lbn > uptr->dbn_size)                               /* read only group? */
    return;                                             /* yes, ignore write */
dbn = &dbn[lbn * 256];
for (int i = 0; i < 256; i++) {
    dbn[i] = GETDW (dma + (i << 1));
    }
}

const char *ot_names[] = {
    "LBN",
    "RBN",
    "XBN",
    "DBN"
    };

void ks_process_frb (UNIT *uptr, uint32 frb, uint32 cyl, uint32 sct)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
uint32 bd, scnt, dma, lbn, trk, rte, opc, ot;
uint32 ks = ks_dib.cma;

lbn = GETB (frb + F_HLBN);                              /* high LBN */
lbn = lbn << 16;
lbn = lbn | GETW (frb + F_LLBN);                        /* low LBN */
ot = GETB (frb + F_HLBN + 1) >> 6;                      /* offset type */
trk = GETW (frb + F_TRK);                               /* track */
scnt = GETB (frb + F_SCNT);                             /* sector count */
rte = GETP (frb + F_RTE);                               /* get route */
opc = GETB (rte + V_OPCD);                              /* get opcode from route */

//ks_print_frb (frb);
sim_debug_unit (DBG_FRB, uptr, "%07o, %s: %d, CYL: %d, TRK: %d, SCT: %d, SCNT: %d, OPC: %o\n", frb, ot_names[ot], lbn, cyl, trk, sct, scnt, opc);
if (opc == V_DR) {                                     /* disk read? */
    for (int i = 0; i < scnt; i++) {
        bd = GETP (ks_dib.cma + KG_FBU);               /* get a free data memory buffer */
        bd = kg_receive (bd);
        dma = GETW (bd + 002);
        dma = (dma << 6) & DMEMMASK;

        for (int j = 0; j < 288; j++) {                 /* clear buffer */
            PUTDW (dma + (j << 1), 0);
            }

        PUTDW (dma + B_HDRS + 000, lbn & 0177777);      /* header */
        PUTDW (dma + B_HDRS + 002, lbn >> 16);
        PUTDW (dma + B_HDRS + 004, lbn & 0177777);
        PUTDW (dma + B_HDRS + 006, lbn >> 16);
        PUTDW (dma + B_HDRS + 010, lbn & 0177777);
        PUTDW (dma + B_HDRS + 012, lbn >> 16);
        PUTDW (dma + B_HDRS + 014, lbn & 0177777);
        PUTDW (dma + B_HDRS + 016, lbn >> 16);

        switch (ot) {
            case OT_LBN:
                ks_read_lbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_RBN:
                ks_read_rbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_XBN:
                ks_read_xbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_DBN:
                ks_read_dbn (uptr, dma + B_DATA, lbn);
                break;
            }

        kg_send (frb + F_BFHD, bd);                     /* queue buffer to the FRB */
        sct++;
        lbn++;
        }
    }
else if (opc == V_DW) {                                 /* disk write? */
    bd = GETP (frb + F_BFHD);
    for (int k = 0; k < scnt; k++) {
        if (bd == 0)
            break;
        dma = GETW (bd + 002);
        dma = (dma << 6) & DMEMMASK;

        switch (ot) {
            case OT_LBN:
                ks_write_lbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_DBN:
                ks_write_dbn (uptr, dma + B_DATA, lbn);
                break;
            }

        bd = GETP (bd);
        sct++;
        lbn++;
        }
    }
else
    sim_printf ("Unknown route opcode for K.sdi: %o\n", opc);

if ((ot == OT_LBN) && (lbn >= ftab->lbn) && ((ks_mode & MODE_OB) == 0)) /* outside host area? */
    PUTW (frb + F_ESUM, 1);
//    kg_route_frb (ks, frb, FALSE);
//else
    kg_route_frb (ks, frb, TRUE);
}

void ks_process_drat (UNIT *uptr, uint32 ks, uint32 port)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
uint32 ksh = ks + KS_0HD + (port * 4);                  /* DRAT queue */
uint32 kfl = ks - KS_0FL - (port * 4);                  /* port flags */
uint32 dt, cyl, fq, cnt, cq, frb, fc, flg, rte;
uint32 sect = ftab->sect + dtab->rbn;

dt = GETP (ksh + Q_HEAD);                               /* get DRAT */
if (dt == 0)
    return;

//print_drat (dt);
//sim_printf ("DRAT: %07o\n", dt);

cyl = GETW (dt + DT_CYL);                               /* get current cylinder */
fq = dt - (sect * 4);                                   /* 4 bytes for queue head/tail */
cnt = GETW (dt + DT_CNT);

fc = 0;
for (int j = 0; j < 50; j++) {                          /* prevent endless loop */
    frb = kg_receive (dt + DT_HST + 004);               /* get next FRB */
    if (frb == 0)
        break;
    fc++;
    cq = GETP (dt + DT_HST + 002);
    kg_send (cq, frb);
    }
if (fc > 0)
    sim_debug_unit (DBG_DRAT, uptr, "%d FRBs pre-processed\n", fc);

fc = 0;
for (int k = 0; k < sect; k++) {                        /* for each sector */
    for (int j = 0; j < 50; j++) {                      /* prevent endless loop */
        frb = kg_receive (fq + (k << 2));               /* get next FRB */
        if (frb == 0)
            break;
        PUTP (frb + F_DRAT, dt);
        rte = GETP (frb + F_RTE);                       /* get route */
        flg = GETB (rte + V_FLGS);                      /* get flags from route */
        ks_process_frb (uptr, frb, cyl, k);
        fc++;
        if (((flg & V_NC) == 0) && (cnt > 0))
            cnt = cnt - 1;
        }
    }
if (fc > 0)
    sim_debug_unit (DBG_DRAT, uptr, "%d FRBs processed\n", fc);

PUTW (dt + DT_CNT, cnt);                                /* update counter */

if (cnt == 0) {                                         /* all operation complete? */
    dt = kg_receive (ksh + Q_HEAD);                     /* dequeue DRAT */
    cq = GETP (dt + DT_CQ);                             /* get DRAT completion queue */
    kg_send (cq, dt);                                   /* send DRAT to completion queue */
    flg = GETW (kfl);
    flg = flg | 0100000;                                /* FIXME: This is expected by ILDISK */
    PUTW (kfl, flg);
    sim_debug_unit (DBG_DRAT, uptr, "DRAT completed\n");
    }
}

t_stat ks_svc (UNIT *uptr)
{
DEVICE *dptr = find_dev_from_unit (uptr);
uint32 port = (uint32) (uptr - dptr->units);
HDIB *dib = (HDIB *) dptr->ctxt;
uint32 ks = dib->cma;                                   /* K.sdi control area */
uint32 ksa, cq, dc;

ksa = ks + KS_S0 + (port * 16);                         /* drive state area */
if (GETW (ks + KG_RSL) == 0) {                          /* test started? */
    PUTW (ks + KG_RSL, 1);                              /* KG$RSL - result of self test */
    dc = kg_receive (ksa + KSA_HEAD);                   /* get DCB from drive state area */
    if (dc) {
        cq = GETP (dc + DC_0CQ);                        /* get completion queue */
        kg_send (cq, dc);                               /* put DCB to completion queue */
        }
    }
else if (!Q_LOCKED (ksa + KSA_LOCK)) {                  /* drive state area locked? */
    if ((GETB (ksa + KSA_FLGS) & FLGS_SVC) == 0)        /* drive not known to system? */
        ks_notify (uptr, ks, port);                     /* notify drive present */
    ks_process_dcb (uptr, ks, port);                    /* process DCB queue */
    ks_process_drat (uptr, ks, port);                   /* process DRAT queue */
    }
sim_clock_coschedule (uptr, tmxr_poll);                 /* reactivate */
return SCPE_OK;
}

/* Set/clear hardware write lock */

t_stat ks_set_wlk (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return SCPE_OK;
}

/* Show write lock status */

t_stat ks_show_wlk (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */

if (uptr->flags & UNIT_WLK)
    fprintf (st, "write locked");
else fprintf (st, "write enabled");
return SCPE_OK;
}

/* Set drive format */

t_stat ks_set_fmt (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
struct drvtyp *dtab = &drv_tab[dtyp];

if (dtab->fmt[FMT_18].lbn == 0)                         /* 18-bit not supported? */
    return SCPE_NOFNC;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
if (val == FMT_18)
    uptr->flags = uptr->flags | UNIT_FMT;
else
    uptr->flags = uptr->flags & ~UNIT_FMT;
uptr->capac = (t_addr)dtab->fmt[val].lbn;
return SCPE_OK;
}

/* Show drive format */

t_stat ks_show_fmt (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);

if (fmt == FMT_18)
    fprintf (st, "18-bit format");
else fprintf (st, "16-bit format");
return SCPE_OK;
}

/* Set unit type */

t_stat ks_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */

if (val < 0)
    return SCPE_ARG;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
uptr->flags = (uptr->flags & ~UNIT_DTYPE) | (val << UNIT_V_DTYPE);
uptr->flags = uptr->flags & ~UNIT_FMT;
uptr->capac = (t_addr)drv_tab[val].fmt[fmt].lbn;
return SCPE_OK;
}

/* Show unit type */

t_stat ks_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "%s", drv_tab[GET_DTYPE (uptr->flags)].name);
return SCPE_OK;
}

/* Set unit plug */

t_stat ks_set_plug (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 plug;
uint32 i;
t_stat r;
DEVICE *dptr = find_dev_from_unit (uptr);

if (cptr == NULL)
    return sim_messagef (SCPE_ARG, "Must specify UNIT=value\n");
plug = (int32) get_uint (cptr, 10, 0xFFFFFFFF, &r);
if ((r != SCPE_OK) || (plug > 4095))
    return sim_messagef (SCPE_ARG, "Invalid Unit Plug Number: %s\n", cptr);
if (uptr->unit_plug == plug)
    return SCPE_OK;
for (i = 0; i < dptr->numunits; i++)
    if (dptr->units[i].unit_plug == plug)
        return sim_messagef (SCPE_ARG, "Unit Plug %d Already In Use on %s\n", plug, sim_uname (&dptr->units[i]));
uptr->unit_plug = plug;
return SCPE_OK;
}

/* Show unit plug */

t_stat ks_show_plug (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "UNIT=%d", uptr->unit_plug);
return SCPE_OK;
}

/* Set controller type */

t_stat ks_set_ctype (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return SCPE_OK;
}

/* Show controller type */

t_stat ks_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
return SCPE_OK;
}

/* Device attach */

t_stat ks_attach (UNIT *uptr, CONST char *cptr)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
struct drvtyp *dtab = &drv_tab[dtyp];
t_stat r;

r = sim_disk_attach (uptr, cptr, KS_NUMBY, sizeof (uint16), (uptr->flags & UNIT_NOAUTO), DBG_DSK, drv_tab[GET_DTYPE (uptr->flags)].name, 0, 0);
if (r != SCPE_OK)
    return r;
uptr->dbn_size = ((dtab->dcyl * dtab->gpc) - dtab->drg) *
    dtab->tpg * (dtab->fmt[FMT_16].sect + dtab->rbn);
uptr->dbn = (uint16 *) calloc (uptr->dbn_size * 256, sizeof (uint16));
return SCPE_OK;
}

/* Device detach */

t_stat ks_detach (UNIT *uptr)
{
t_stat r;

r = sim_disk_detach (uptr);                             /* detach unit */
if (r != SCPE_OK)
    return r;
free (uptr->dbn);
return SCPE_OK;
}

/* Device reset */

t_stat ks_reset (DEVICE *dptr)
{
uint32 i;
//int32 cidx;
UNIT *uptr;
//MSC *cp;
static t_bool plugs_inited = FALSE;
//DIB *dibp = (DIB *) dptr->ctxt;

sim_debug (DBG_TRC, dptr, "ks_reset\n");

if (!plugs_inited ) {
    uint32 d, u = 0;

    plugs_inited  = TRUE;
    for (d = 0; d < dptr->numunits; d++) {
        dptr->units[d].unit_plug = u++;                 /* unique unit numbers */
        }
    }
//cp->comm = 0;                                           /* no comm region */
for (i = 0; i < dptr->numunits; i++) {                  /* init units */
    uptr = &dptr->units[i];
    sim_cancel (uptr);                                  /* clr activity */
    sim_disk_reset (uptr);
//    uptr->cnum = cidx;                                  /* set ctrl index */
    uptr->flags = uptr->flags & ~(UNIT_ONL | UNIT_ATP);
//    uptr->uf = 0;                                       /* clr unit flags */
    uptr->ksxb = (uint16 *) realloc (uptr->ksxb, (KS_MAXFR >> 1) * sizeof (uint16));
    if (uptr->ksxb == NULL)
        return SCPE_MEM;
    }
return SCPE_OK;
}
