/* hsc_kt.c: HSC K.sti simulation

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

/*
 * References:
 * 
 *    EK-OTA79-TM-001 - PDF page 120, 322
 */

#include "pdp11_defs.h"
#include "hsc_kg.h"
#include "dec_disk.h"
#include "sim_disk.h"

/* K.sti control area */

#define KT_IPQ          0000
#define KT_S0           0140                            /* drive state area (port 0) */
#define KT_S1           0160                            /* drive state area (port 1) */
#define KT_S2           0200                            /* drive state area (port 2) */
#define KT_S3           0220                            /* drive state area (port 3) */

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
#define DC_TFC          0012                            /* pointer to DU$ structure */
#define DC_0CQ          0014                            /* success completion queue */
#define DC_STC          0016
#define DC_1CQ          0016                            /* error completion queue */
#define DC_ICT          0020                            /* inline command buffer */
#define DC_THR          0036
#define DC_RTY          0036
#define DC_RBS          0037
#define DC_IRT          0040                            /* inline response buffer */
#define DC_SN           0114
#define DC_TSN          0114
#define DC_DSN          0115
#define DC_RCT          0116

/* Realtime formatter status */

#define RTDS_AVL        0002                            /* available */
#define RTDS_ATN        0040                            /* attention */
#define RTDS_FRR        0100                            /* receiver ready */

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

/* Fragment request block - K.sti */

#define F_TEMP          0066
#define F_TCNT          0070

/* Transfer block? */

#define T_LINK          0000                            /* link to next */
#define T_PCTR          0002                            /* pointer to counter */
#define T_SUCC          0004                            /* success completion queue */
#define T_FAIL          0006                            /* failure completion queue */
#define T_FCNT          0010                            /* FRB count? */
#define T_FLAG          0010
#define T_LEV1          0012
#define T_UNIT          0012                            /* unit number */
#define T_OPCD          0013                            /* opcode */
#define T_REC           0014
#define T_ERR           0016
#define T_BCL           0020                            /* byte count low */
#define T_BCH           0022                            /* byte count high */
#define T_PRIM          0024                            /* ?? queue */
#define T_SEQN          0030
#define T_TSN           0030
#define T_DSN           0031
#define T_ADR           0032
#define T_FRBQ          0034
#define T_0FRB          0034                            /* ?? FRB queue */
#define T_1FRB          0040                            /* ?? FRB queue */
#define T_2FRB          0044                            /* ?? FRB queue */
#define T_3FRB          0050                            /* ?? FRB queue */
#define T_4FRB          0054                            /* ?? FRB queue */
#define T_5FRB          0060                            /* ?? FRB queue */
#define T_6FRB          0064                            /* ?? FRB queue */
#define T_7FRB          0070                            /* ?? FRB queue */
#define T_FRBE          0074

#define KT_MAXDR        4                               /* max # drives */

#define KT_NUMBY        512                             /* bytes per block */
#define KT_MAXFR        (1 << 16)                       /* max xfer */

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
#define ktxb            filebuf                         /* xfer buffer */

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

#define KT_DRV(d) \
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
#define KT_SIZE(d)      d##_LBN_16                      /* FIXME: support 18-bit mode */

static struct drvtyp drv_tab[] = {
    KT_DRV (RA60),
    KT_DRV (RA70),
    KT_DRV (RA80),
    KT_DRV (RA81),
    KT_DRV (RA82),
    { 0 }
    };


extern int32 tmxr_poll;                                 /* calibrated delay */

uint8 kt_mode = 0;

t_stat kt_svc (UNIT *uptr);
t_stat kt_reset (DEVICE *dptr);
t_stat kt_init (uint16 req, uint16 act);
t_stat kt_attach (UNIT *uptr, CONST char *cptr);
t_stat kt_detach (UNIT *uptr);
t_stat kt_set_wlk (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_fmt (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_ctype (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_plug (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_show_plug (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_wlk (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_fmt (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);

/* debugging bitmaps */

#define DBG_TRC         KG_DBG_TRC                      /* trace routine calls */
#define DBG_DSK         0x0010                          /* display sim_disk activities */
#define DBG_SDI         0x0020                          /* SDI commands */
#define DBG_FRB         0x0040                          /* FRB operations */
#define DBG_DRAT        0x0080                          /* DRAT operations */

DEBTAB kt_debug[] = {
    { "TRACE", DBG_TRC,  "trace routine calls" },
    { "SDI",   DBG_SDI,  "SDI commands" },
    { "FRB",   DBG_FRB,  "FRB operations" },
    { "DRAT",  DBG_DRAT, "DRAT operations" },
    { 0 }
};

HDIB kt_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    K_STI, 0, &kt_init
    };

UNIT kt_unit[KT_MAXDR] = {
    { UDATA (&kt_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KT_SIZE (RA80)) },
    { UDATA (&kt_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KT_SIZE (RA80)) },
    { UDATA (&kt_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KT_SIZE (RA80)) },
    { UDATA (&kt_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RA80_DTYPE << UNIT_V_DTYPE), KT_SIZE (RA80)) },
    };

REG kt_reg[] = {
    { NULL }
    };

MTAB kt_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", "ADDRESS",
      NULL, &kg_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "CONTROL", "CONTROL",
      NULL, &kt_show_ctrl, NULL },
    { UNIT_WLK, 0, NULL, "WRITEENABLED",
      &kt_set_wlk, NULL, NULL, "Write enable disk drive" },
    { UNIT_WLK, UNIT_WLK, NULL, "LOCKED",
      &kt_set_wlk, NULL, NULL, "Write lock disk drive" },
    { MTAB_XTD|MTAB_VUN, 0, "WRITE", NULL,
      NULL, &kt_show_wlk, NULL, "Display drive writelock status" },
    { MTAB_XTD|MTAB_VUN, FMT_16, NULL, "16BIT",
      &kt_set_fmt, NULL, NULL, "Set drive format to 16-bit" },
    { MTAB_XTD|MTAB_VUN, FMT_18, NULL, "18BIT",
      &kt_set_fmt, NULL, NULL, "Set drive format to 18-bit" },
    { MTAB_XTD|MTAB_VUN, 0, "FORMAT", NULL,
      NULL, &kt_show_fmt, NULL, "Display drive format" },
    { MTAB_XTD|MTAB_VUN, RA60_DTYPE, NULL, "RA60",
      &kt_set_type, NULL, NULL, "Set RA60 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA70_DTYPE, NULL, "RA70",
      &kt_set_type, NULL, NULL, "Set RA70 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA80_DTYPE, NULL, "RA80",
      &kt_set_type, NULL, NULL, "Set RA80 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA81_DTYPE, NULL, "RA81",
      &kt_set_type, NULL, NULL, "Set RA81 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA82_DTYPE, NULL, "RA82",
      &kt_set_type, NULL, NULL, "Set RA82 Disk Type" },
    { MTAB_XTD|MTAB_VUN, 0, "TYPE", NULL,
      NULL, &kt_show_type, NULL, "Display device type" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "UNIT", "UNIT=val (0-4095)",
      &kt_set_plug, &kt_show_plug, NULL, "Set/Display Unit plug value" },
    { MTAB_XTD|MTAB_VDV, K_SDI, NULL, "KSDI",
      &kt_set_ctype, NULL, NULL, "Set K.sti Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SI4, NULL, "KSI4",
      &kt_set_ctype, NULL, NULL, "Set K.si 4-port Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SI8, NULL, "KSI8",
      &kt_set_ctype, NULL, NULL, "Set K.si 8-port Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SCSI, NULL, "KSCSI",
      &kt_set_ctype, NULL, NULL, "Set K.scsi Controller Type" },
    { MTAB_XTD | MTAB_VDV, 0, "TYPE", NULL,
      NULL, &kt_show_ctype, NULL, "Display controller type" },
    { 0 }
    };

DEVICE kt_dev = {
    "KT", kt_unit, kt_reg, kt_mod,
    KT_MAXDR, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &kt_reset,
    NULL, &kt_attach, &kt_detach,
    &kt_dib, DEV_DEBUG | DEV_HSC | DEV_DISABLE,
    0, kt_debug
    };


/* The version must be at least DS = 3, MC = 00 */

t_stat kt_init (uint16 req, uint16 act)
{
t_bool run;
if (act == K_RUN) {
    kt_dib.cma = kg_init (&run);
    sim_debug (DBG_TRC, &kt_dev, "K.sti %d startup: KH = %06o at %o\n", req, kt_dib.cma, (uint16)inst_pc);
    PUTW (kt_dib.cma + KG_RSL, 1);                        /* KG$RSL - result of self test */
    PUTW (kt_dib.cma + KG_VRS, 01452);                    /* KG$VRS - version (DS = 3, MC = 42) */
    if (!run)
        return SCPE_OK;
    sim_clock_coschedule (&kt_unit[0], tmxr_poll);        /* activate */
//    sim_clock_coschedule (&kt_unit[1], tmxr_poll);        /* activate */
//    sim_clock_coschedule (&kt_unit[2], tmxr_poll);        /* activate */
//    sim_clock_coschedule (&kt_unit[3], tmxr_poll);        /* activate */
    }
else {
    sim_debug (DBG_TRC, &kt_dev, "K.sti %d halt at %o\n", req, (uint16)inst_pc);
    sim_cancel (&kt_unit[0]);
//    sim_cancel (&kt_unit[1]);
//    sim_cancel (&kt_unit[2]);
//    sim_cancel (&kt_unit[3]);
    kt_dib.cma = 0;
    }
return SCPE_OK;
}

t_stat kt_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
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

void kt_print_frb (uint32 frb)
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

void kt_print_dcb (uint32 dc)
{
kg_print_word (NULL, dc, "DC$LNK", DC_LNK, 2);
kg_print_word (NULL, dc, "DC$CMD", DC_CMD, 2);
kg_print_word (NULL, dc, "DC$RSB", DC_RSB, 2);
kg_print_word (NULL, dc, "DC$FCT", DC_FCT, 2);
kg_print_word (NULL, dc, "DC$STA", DC_STA, 2);
kg_print_word (NULL, dc, "DC$TFC", DC_TFC, 2);
kg_print_word (NULL, dc, "DC$0CQ", DC_0CQ, 2);
kg_print_word (NULL, dc, "DC$STC", DC_STC, 0);
kg_print_word (NULL, dc, "DC$1CQ", DC_1CQ, 2);
kg_print_word (NULL, dc, "DC$ICT", DC_ICT, 14);
kg_print_word (NULL, dc, "DC$THR", DC_THR, 0);
kg_print_word (NULL, dc, "DC$RTY", DC_RTY, 1);
kg_print_word (NULL, dc, "DC$RBS", DC_RBS, 1);
kg_print_word (NULL, dc, "DC$IRT", DC_IRT, 44);
kg_print_word (NULL, dc, "DC$SN",  DC_SN,  0);
kg_print_word (NULL, dc, "DC$TSN", DC_TSN, 1);
kg_print_word (NULL, dc, "DC$DSN", DC_DSN, 1);
kg_print_word (NULL, dc, "DC$RCT", DC_RCT, 2);
}

t_stat kt_notify (UNIT *uptr, uint32 kt, uint32 port)
{
uint32 ksa = kt + KT_S0 + (port * 16);                  /* drive state area */
uint32 sq, fl;

//fl = GETB (ksa + KSA_FLGS);                             /* get drive flags */
//if ((fl & FLGS_SVC) == 0) {                             /* drive not known to system? */
    sq = GETP (ksa);                                    /* get service queue pointer */
    if (sq == 0) {
        sim_printf ("Drive service queue is NULL!\n");
        return SCPE_OK;
        }
    if (Q_LOCKED (sq)) {
        sim_printf ("Service queue currently locked\n");
        return SCPE_OK;
        }
    PUTW (ksa, 0);                                      /* clear service queue pointer */
//    fl |= FLGS_SVC;
//    PUTB (ksa + KSA_FLGS, fl);                          /* now on service queue */
    kg_send (sq, ksa);                                  /* send drive state area to service queue */
    sim_printf ("Drive state area sent to service queue\n");
//    }
return SCPE_OK;
}

/* Online */

uint16 kt_online (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);

sim_debug_unit (DBG_SDI, uptr, "ONLINE, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
uptr->flags = uptr->flags | UNIT_ONL;
return 002;
}

/* Rewind */

uint16 kt_rewind (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);

sim_debug_unit (DBG_SDI, uptr, "INITIATE REWIND, p1 = %02X\n", p1);
sim_printf ("INITIATE REWIND, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
return 002;
}

/* Disconnect */

uint16 kt_disc (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "DISCONNECT\n");
kt_mode = 0;
uptr->flags = uptr->flags & ~UNIT_ONL;
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
return 002;
}

/* Diagnose */

uint16 kt_diag (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 test = GETW (cmd + 002);                          /* drive resident test to run */  /* TODO: check this */
sim_debug_unit (DBG_SDI, uptr, "DIAGNOSE, test = %02X\n", test);
PUTW (rsp, 0x3C);                                       /* response opcode */
return 004;
}

/* Get formatter characteristics */

uint16 kt_getchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];

sim_debug_unit (DBG_SDI, uptr, "GET FORMATTER CHARACTERISTICS\n");
PUTW (rsp + 000, 0xB8);                                 /* response opcode */
PUTW (rsp + 002, 0x00);                                 /* MBC */
PUTW (rsp + 004, 0x03);                                 /* short timeout */
PUTW (rsp + 006, 0x07);                                 /* long timeout */
PUTW (rsp + 010, 0x00);                                 /* DTO */
PUTW (rsp + 012, 0x00);                                 /* MWB low */
PUTW (rsp + 014, 0x00);                                 /* MWB high */
PUTW (rsp + 016, 0x01);                                 /* hardware revision */
PUTW (rsp + 020, 0x02);                                 /* software revision */
PUTW (rsp + 022, 0x00);                                 /* TID - tape ID? */
PUTW (rsp + 024, 0x00);                                 /* FID - formatter ID? */
PUTW (rsp + 026, 0x00);                                 /* unused */
PUTW (rsp + 030, 0x00);                                 /* unused */
PUTW (rsp + 032, 0x00);                                 /* unused */
PUTW (rsp + 034, 0x00);                                 /* unused */
PUTW (rsp + 036, 0x00);                                 /* ERT */
PUTW (rsp + 040, 0x00);                                 /* unused */
PUTW (rsp + 042, 0x00);                                 /* unused */
PUTW (rsp + 044, 0x00);                                 /* RWT */
PUTW (rsp + 046, 0x00);                                 /* unused */
PUTW (rsp + 050, 0x00);                                 /* unused */
PUTW (rsp + 052, 0x00);
return 026;
}

/* Get unit characteristics */

uint16 kt_getuchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
struct drvtyp *dtab = &drv_tab[dtyp];
uint8 p1 = GETB (cmd + 002);

sim_debug_unit (DBG_SDI, uptr, "GET UNIT CHARACTERISTICS, p1 = %02X\n", p1);
PUTW (rsp + 0000, 0x36);                                /* response opcode */
PUTW (rsp + 0002, 0x12);                                /* unique drive ID #1 */
PUTW (rsp + 0004, 0x00);
PUTW (rsp + 0006, 0x00);                                /* unique drive ID #2 */
PUTW (rsp + 0010, 0x00);
PUTW (rsp + 0012, 0x00);                                /* unique drive ID #3 */
PUTW (rsp + 0014, 0x00);
PUTW (rsp + 0016, 0x01);                                /* microcode version */
PUTW (rsp + 0020, 0x02);                                /* hardware revision */
PUTW (rsp + 0022, 0x00);                                /* TID - unit model? */
PUTW (rsp + 0024, 0x51);                                /* media ID */
PUTW (rsp + 0026, 0x10);
PUTW (rsp + 0030, 0x68);
PUTW (rsp + 0032, 0x6D);
PUTW (rsp + 0034, 0x11);                                /* DMU */
PUTW (rsp + 0036, 0x22);                                /* SMU */
PUTW (rsp + 0040, 0x00);                                /* unused */
PUTW (rsp + 0042, 0x00);                                /* unused */
PUTW (rsp + 0044, 0x00);                                /* unused */
PUTW (rsp + 0046, 0xBB);                                /* SCH */
PUTW (rsp + 0050, 0x00);                                /* unused */
PUTW (rsp + 0052, 0x00);
return 026;
}

/* Get status */

uint16 kt_getsts (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "GET SUMMARY STATUS\n");
PUTW (rsp + 000, 0xB7);                                 /* response opcode */
PUTW (rsp + 002, 0x02);                                 /* summary mode byte */     /* see EK-OTA79-TM-001 page B-33 */
PUTW (rsp + 004, 0x00);                                 /* summary error byte */
PUTW (rsp + 006, 0x00);                                 /* summary mode byte 2? */
PUTW (rsp + 010, 0x00);                                 /* controller flag byte? */
PUTW (rsp + 012, 0x0C);                                 /* drive 0 mode byte */     /* see TFUNCT.SYM:002510 */
PUTW (rsp + 014, 0x00);                                 /* drive 0 error byte */
PUTW (rsp + 016, 0x00);                                 /* drive 1 mode byte */
PUTW (rsp + 020, 0x00);                                 /* drive 1 error byte */
PUTW (rsp + 022, 0x00);                                 /* drive 2 mode byte */
PUTW (rsp + 024, 0x00);                                 /* drive 2 error byte */
PUTW (rsp + 026, 0x00);                                 /* drive 3 mode byte */
PUTW (rsp + 030, 0x00);                                 /* drive 3 error byte */
PUTW (rsp + 032, 0x00);
return 016;
}

/* Get extended drive status */

/* Density */
/* 1 = NRZI 800 */
/* 2 = PE 1600 */
/* 4 = GCR 6250 */

uint16 kt_getextsts (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_SDI, uptr, "GET EXTENDED DRIVE STATUS, p1 = %02X\n", p1);
sim_printf ("GET EXTENDED DRIVE STATUS, p1 = %02X\n", p1);
PUTW (rsp + 000, 0x3A);                                 /* response opcode - DFL? */
PUTW (rsp + 002, 0x55);                                 /* SPE */
PUTW (rsp + 004, 0x04);                                 /* density */
PUTW (rsp + 006, uptr->unit_plug);                      /* unit number (low) */
PUTW (rsp + 010, (uptr->unit_plug >> 8));               /* unit number (high) */
PUTW (rsp + 012, 0x55);                                 /* GAP */
PUTW (rsp + 014, 0xAA);                                 /* GAP */
PUTW (rsp + 016, 0x55);                                 /* GAP */
PUTW (rsp + 020, 0xAA);                                 /* GAP */
PUTW (rsp + 022, 0x00);
return 012;
}

/* Read memory */

uint16 kt_rdmem (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);
uint8 p4 = GETB (cmd + 010);
uint8 p5 = GETB (cmd + 012);
sim_debug_unit (DBG_SDI, uptr, "READ MEMORY, p1 = %02X, p2 = %02X, p3 = %02X, p4 = %02X, p5 = %02X\n", p1, p2, p3, p4, p5);
PUTW (rsp, 0x30);                                       /* response opcode */
return 016;
}

/* Read memory */

uint16 kt_wrmem (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "WRITE MEMORY\n");
PUTW (rsp, 0xFE);                                       /* response opcode */
return 002;
}

uint16 kt_chflag (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
sim_debug_unit (DBG_SDI, uptr, "CHANGE CONTROLLER FLAGS, p1 = %02X, p2 = %02X\n", p1, p2);
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 kt_topology (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "TOPOLOGY\n");
PUTW (rsp, 0x2D);                                       /* TOPOLOGY RESPONSE */
return 016;
}

uint16 kt_clrde (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
sim_debug_unit (DBG_SDI, uptr, "CLEAR DRIVE ERRORS, p1 = %02X, p2 = %02X\n", p1, p2);
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_clrfe (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_SDI, uptr, "CLEAR FORMATTER ERRORS, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_getflog (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_SDI, uptr, "GET FORMATTER ERROR LOG\n");
PUTW (rsp, 0x39);
return 002;
}

uint16 kt_egap (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);

sim_debug_unit (DBG_SDI, uptr, "ERASE GAP, p1 = %02X\n", p1);
PUTW (rsp, 0x35);
return 003;
}

uint16 kt_pos (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);

sim_debug_unit (DBG_SDI, uptr, "POSITION TAPE, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
sim_printf ("POSITION TAPE, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
PUTW (rsp, 0xB2);
return 007;
}

uint16 kt_setumode (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);

sim_debug_unit (DBG_SDI, uptr, "SET UNIT EXECUTION MODE, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
sim_printf ("SET UNIT EXECUTION MODE, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
PUTW (rsp, 0xB1);
return 004;
}

uint16 kt_setcount (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);
//if (!sim_log)
//    sim_set_logon (0, "hsc_sti.log");

sim_debug_unit (DBG_SDI, uptr, "SET BYTE COUNT, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_setuchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);
sim_debug_unit (DBG_SDI, uptr, "SET UNIT CHARACTERISTICS, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_tmark (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_SDI, uptr, "WRITE TAPE MARK, p1 = %02X\n", p1);
sim_printf ("WRITE TAPE MARK, p1 = %02X\n", p1);
PUTW (rsp, 0xAC);
return 003;
}

uint16 kt_der (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_SDI, uptr, "DATA ERROR RECOVERY, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);
return 002;
}

/* SDI level 2 command */

uint16 kt_cmd (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 opc = GETB (cmd);                                 /* get SDI opcode */
uint16 rsz = 0;

switch (opc) {
    case 0x41:                                          /* CLEAR DRIVE ERRORS */
        rsz = kt_clrde (uptr, cmd, rsp);
        break;

    case 0x42:                                          /* CLEAR FORMATTER ERRORS */
        rsz = kt_clrfe (uptr, cmd, rsp);
        break;

    case 0x44:                                          /* DISCONNECT */
        rsz = kt_disc (uptr, cmd, rsp);
        break;

    case 0x47:                                          /* GET FORMATTER CHARACTERISTICS */
        rsz = kt_getchar (uptr, cmd, rsp);
        break;

    case 0x48:                                          /* GET SUMMARY STATUS */
        rsz = kt_getsts (uptr, cmd, rsp);
        break;

    case 0x4B:                                          /* INITIATE REWIND */
        rsz = kt_rewind (uptr, cmd, rsp);
        break;

    case 0x4D:                                          /* POSITION TAPE */
        rsz = kt_pos (uptr, cmd, rsp);
        break;

    case 0x4E:                                          /* SET UNIT EXECUTION MODE */
        rsz = kt_setumode (uptr, cmd, rsp);
        break;

    case 0x50:                                          /* SET BYTE COUNT */
        rsz = kt_setcount (uptr, cmd, rsp);
        break;

    case 0x53:                                          /* WRITE TAPE MARK */
        rsz = kt_tmark (uptr, cmd, rsp);
        break;

    case 0x55:                                          /* DATA ERROR RECOVERY */
        rsz = kt_der (uptr, cmd, rsp);
        break;

    case 0xC0:                                          /* CHANGE CONTROLLER FLAGS */
        rsz = kt_chflag (uptr, cmd, rsp);
        break;

    case 0xC3:                                          /* DIAGNOSE */
        rsz = kt_diag (uptr, cmd, rsp);
        break;

    case 0xC5:                                          /* GET EXTENDED DRIVE STATUS */
        rsz = kt_getextsts (uptr, cmd, rsp);
        break;

    case 0xC6:                                          /* GET FORMATTER ERROR LOG */
        rsz = kt_getflog (uptr, cmd, rsp);
        break;

    case 0xC9:                                          /* GET UNIT CHARACTERISTICS */
        rsz = kt_getuchar (uptr, cmd, rsp);
        break;

    case 0xCA:                                          /* ERASE GAP */
        rsz = kt_egap (uptr, cmd, rsp);
        break;

    case 0xCC:                                          /* ONLINE drive */
        rsz = kt_online (uptr, cmd, rsp);
        break;

    case 0xCF:                                          /* READ MEMORY */
        rsz = kt_rdmem (uptr, cmd, rsp);
        break;

    case 0xD1:                                          /* SET UNIT CHARACTERISTICS */
        rsz = kt_setuchar (uptr, cmd, rsp);
        break;

    case 0xD2:                                          /* TOPOLOGY */
        rsz = kt_topology (uptr, cmd, rsp);
        break;

    case 0xD4:                                          /* WRITE MEMORY */
        rsz = kt_wrmem (uptr, cmd, rsp);
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

void kt_rtds (UNIT *uptr, uint32 rsp)
{
uint16 sts = 0;
sim_debug_unit (DBG_SDI, uptr, "LINE STATUS\n");
if (uptr->flags & UNIT_ONL)
    sts = 0;
else
    sts = 0267;
//    sts = RTDS_AVL | RTDS_FRR;                          /* AVAILABLE, RECEIVER READY */
PUTW (rsp, sts);
}

void kt_process_dcb (UNIT *uptr, uint32 kt, uint32 port)
{
uint32 ksa = kt + KT_S0 + (port * 16);                  /* drive state area */
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

    kt_print_dcb (dc);

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
            kt_rtds (uptr, rsp);
            break;
        default:
            cmd = GETP (dc + DC_CMD);                   /* get command buffer */
            if (cmd)
                rsz = kt_cmd (uptr, cmd, rsp);
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

void kt_read_fct (UNIT *uptr, uint32 dma)
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

void kt_read_rct (uint32 dma)
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

const uint16 kt_dbn[3] = {
    0155555,
    0133333,
    0066666
    };

/* See ILDISK:51440 for alogrithm */

uint16 kt_edc (uint32 dma)
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

t_stat kt_read_lbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
t_seccnt sects, sectsread;
uint16 *xbuf = (uint16 *)uptr->ktxb;
t_stat r;

if (lbn >= ftab->lbn)                                   /* outside host area? */
    kt_read_rct (dma);                                  /* read RCT */
else {
    sects = 1;
    r = sim_disk_rdsect (uptr, lbn, (uint8 *)xbuf, &sectsread, sects);
    if (r != SCPE_OK)
        return r;
    for (int i = 0; i < 256; i++) {
        PUTDW (dma + (i << 1), xbuf[i]);
        }
    }
PUTDW (dma + 01000, kt_edc (dma));
return SCPE_OK;
}

t_stat kt_write_lbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
t_seccnt sects, sectswritten;
uint16 *xbuf = (uint16 *)uptr->ktxb;
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

void kt_read_rbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
}

void kt_read_xbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
if (lbn == 0)
    kt_read_fct (uptr, dma);                            /* read FCT */
}

void kt_read_dbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint16 *dbn = (uint16 *) uptr->dbn;
uint32 ptn = 0;

if (lbn > uptr->dbn_size) {                             /* read only group? */
    PUTDW (dma + 0000, 0177777);
    for (int i = 0; i < 256; i++) {
        PUTDW (dma + 0002 + (i << 1), kt_dbn[ptn++]);
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
PUTDW (dma + 01000, kt_edc (dma));
}

void kt_write_dbn (UNIT *uptr, uint32 dma, uint32 lbn)
{
uint16 *dbn = (uint16 *) uptr->dbn;
if (lbn > uptr->dbn_size)                               /* read only group? */
    return;                                             /* yes, ignore write */
dbn = &dbn[lbn * 256];
for (int i = 0; i < 256; i++) {
    dbn[i] = GETDW (dma + (i << 1));
    }
}

void kt_process_frb (UNIT *uptr, uint32 frb, uint32 cyl, uint32 sct)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);                     /* get drive format */
struct drvtyp *dtab = &drv_tab[dtyp];
struct drvfmt *ftab = &dtab->fmt[fmt];
uint32 bd, scnt, dma, lbn, trk, rte, opc, ot;
uint32 kt = kt_dib.cma;

lbn = GETB (frb + F_HLBN);                              /* high LBN */
lbn = lbn << 16;
lbn = lbn | GETW (frb + F_LLBN);                        /* low LBN */
ot = GETB (frb + F_HLBN + 1) >> 6;                      /* offset type */
trk = GETW (frb + F_TRK);                               /* track */
scnt = GETB (frb + F_SCNT);                             /* sector count */
rte = GETP (frb + F_RTE);                               /* get route */
opc = GETB (rte + V_OPCD);                              /* get opcode from route */

//kt_print_frb (frb);
//sim_debug_unit (DBG_FRB, uptr, "%07o, %s: %d, CYL: %d, TRK: %d, SCT: %d, SCNT: %d, OPC: %o\n", frb, ot_names[ot], lbn, cyl, trk, sct, scnt, opc);
if (opc == V_DR) {                                     /* disk read? */
    for (int i = 0; i < scnt; i++) {
        bd = GETP (kt_dib.cma + KG_FBU);               /* get a free data memory buffer */
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
                kt_read_lbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_RBN:
                kt_read_rbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_XBN:
                kt_read_xbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_DBN:
                kt_read_dbn (uptr, dma + B_DATA, lbn);
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
                kt_write_lbn (uptr, dma + B_DATA, lbn);
                break;
            case OT_DBN:
                kt_write_dbn (uptr, dma + B_DATA, lbn);
                break;
            }

        bd = GETP (bd);
        sct++;
        lbn++;
        }
    }
else
    sim_printf ("Unknown route opcode for K.sti: %o\n", opc);

if ((ot == OT_LBN) && (lbn >= ftab->lbn) && ((kt_mode & MODE_OB) == 0)) /* outside host area? */
    PUTW (frb + F_ESUM, 1);
//    kg_route_frb (kt, frb, FALSE);
//else
    kg_route_frb (kt, frb, TRUE);
}

t_stat kt_svc (UNIT *uptr)
{
DEVICE *dptr = find_dev_from_unit (uptr);
uint32 port = (uint32) (uptr - dptr->units);
HDIB *dib = (HDIB *) dptr->ctxt;
uint32 kt = dib->cma;                                   /* K.sti control area */
uint32 ksa, cq, dc;

ksa = kt + KT_S0 + (port * 16);                         /* drive state area */
if (GETW (kt + KG_RSL) == 0) {                          /* test started? */
    PUTW (kt + KG_RSL, 1);                              /* KG$RSL - result of self test */
    dc = kg_receive (ksa + KSA_HEAD);                   /* get DCB from drive state area */
    if (dc) {
        cq = GETP (dc + DC_0CQ);                        /* get completion queue */
        kg_send (cq, dc);                               /* put DCB to completion queue */
        }
    }
else if (!Q_LOCKED (ksa + KSA_LOCK)) {                  /* drive state area locked? */
//    if ((GETB (ksa + KSA_FLGS) & FLGS_SVC) == 0)        /* drive not known to system? */
    if ((uptr->flags & UNIT_ATP) == 0) {
        kt_notify (uptr, kt, port);                     /* notify drive present */
        uptr->flags |= UNIT_ATP;
        }
    kt_process_dcb (uptr, kt, port);                    /* process DCB queue */
//    kt_process_drat (uptr, kt, port);                   /* process DRAT queue */
    }
sim_clock_coschedule (uptr, tmxr_poll);                 /* reactivate */
return SCPE_OK;
}

/* Set/clear hardware write lock */

t_stat kt_set_wlk (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return SCPE_OK;
}

/* Show write lock status */

t_stat kt_show_wlk (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */

if (uptr->flags & UNIT_WLK)
    fprintf (st, "write locked");
else fprintf (st, "write enabled");
return SCPE_OK;
}

/* Set drive format */

t_stat kt_set_fmt (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
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

t_stat kt_show_fmt (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
uint32 fmt = GET_FMT (uptr->flags);

if (fmt == FMT_18)
    fprintf (st, "18-bit format");
else fprintf (st, "16-bit format");
return SCPE_OK;
}

/* Set unit type */

t_stat kt_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
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

t_stat kt_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "%s", drv_tab[GET_DTYPE (uptr->flags)].name);
return SCPE_OK;
}

/* Set unit plug */

t_stat kt_set_plug (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
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

t_stat kt_show_plug (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "UNIT=%d", uptr->unit_plug);
return SCPE_OK;
}

/* Set controller type */

t_stat kt_set_ctype (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return SCPE_OK;
}

/* Show controller type */

t_stat kt_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
return SCPE_OK;
}

/* Device attach */

t_stat kt_attach (UNIT *uptr, CONST char *cptr)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
struct drvtyp *dtab = &drv_tab[dtyp];
t_stat r;

r = sim_disk_attach (uptr, cptr, KT_NUMBY, sizeof (uint16), (uptr->flags & UNIT_NOAUTO), DBG_DSK, drv_tab[GET_DTYPE (uptr->flags)].name, 0, 0);
if (r != SCPE_OK)
    return r;
uptr->dbn_size = ((dtab->dcyl * dtab->gpc) - dtab->drg) *
    dtab->tpg * (dtab->fmt[FMT_16].sect + dtab->rbn);
uptr->dbn = (uint16 *) calloc (uptr->dbn_size * 256, sizeof (uint16));
return SCPE_OK;
}

/* Device detach */

t_stat kt_detach (UNIT *uptr)
{
t_stat r;

r = sim_disk_detach (uptr);                             /* detach unit */
if (r != SCPE_OK)
    return r;
free (uptr->dbn);
return SCPE_OK;
}

/* Device reset */

t_stat kt_reset (DEVICE *dptr)
{
uint32 i;
//int32 cidx;
UNIT *uptr;
//MSC *cp;
static t_bool plugs_inited = FALSE;
//DIB *dibp = (DIB *) dptr->ctxt;

sim_debug (DBG_TRC, dptr, "kt_reset\n");

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
    uptr->ktxb = (uint16 *) realloc (uptr->ktxb, (KT_MAXFR >> 1) * sizeof (uint16));
    if (uptr->ktxb == NULL)
        return SCPE_MEM;
    }
return SCPE_OK;
}
