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

/*
 * RDF
 * T$ OPC = 027
 * V$ OPC = 4, FLG = 4
 * V$ OPC = 377, FLG = 0
 * 
 * WRT
 * T$ OPC = 216
 * V$ OPC = 377, FLG = 2
 * 
 * FSR
 * POSITION TAPE, p1 = 80, p2 = 01, p3 = 00
 * 
 * FSF
 * POSITION TAPE, p1 = A8, p2 = 01, p3 = 00
 * 
 * BSR
 * POSITION TAPE, p1 = 84, p2 = 01, p3 = 00
 * 
 * BSF
 * POSITION TAPE, p1 = AC, p2 = 01, p3 = 00
 * 
 * RWE
 * INITIATE REWIND, p1 = 40
 * 
 * UNL
 * INITIATE REWIND, p1 = 80
 * 
 */

#include "pdp11_defs.h"
#include "hsc_kg.h"
#include "dec_disk.h"
#include "sim_tape.h"

/* K.sti control area */

#define KT_BAS          0000
#define KT_IPQ          0000
#define KT_WQH          0004
#define KT_WQT          0006
#define KT_FLA          0010
#define KT_SQP          0012
#define KT_TCT          0014
#define KT_LAH          0016
#define KT_WAI          0020
#define KT_SEM          0022
#define KT_BW           0036
#define KT_TLK          0040
#define KT_S0           0140                            /* drive state area (port 0) */
#define KT_S1           0160                            /* drive state area (port 1) */
#define KT_S2           0200                            /* drive state area (port 2) */
#define KT_S3           0220                            /* drive state area (port 3) */
#define KT_DMP          0340
#define KT_AQH          0442
#define KT_AQT          0444
#define KT_BCN          0446
#define KT_TMO          0450
#define KT_FCN          0452
#define KT_FCT          0454
#define KT_T1           0456
#define KT_T2           0460
#define KT_T3           0462
#define KT_T4           0464

static STDEF kt_stc[] = {
    { "KT$BAS", KT_BAS, 0, ST_INT },
    { "KT$IPQ", KT_IPQ, 4, ST_INT },
    { "KT$WQH", KT_WQH, 2, ST_INT },
    { "KT$WQT", KT_WQT, 2, ST_INT },
    { "KT$FLA", KT_FLA, 2, ST_INT },
    { "KT$SQP", KT_SQP, 2, ST_INT },
    { "KT$TCT", KT_TCT, 2, ST_INT },
    { "KT$LAH", KT_LAH, 2, ST_INT },
    { "KT$WAI", KT_WAI, 2, ST_INT },
    { "KT$SEM", KT_SEM, 12, ST_INT },
    { "KT$BW ", KT_BW, 2, ST_INT },
    { "KT$TLK", KT_TLK, 0, ST_LNK, kg_stc },
    { "KT$S0 ", KT_S0, 16, ST_INT },
    { "KT$S1 ", KT_S1, 16, ST_INT },
    { "KT$S2 ", KT_S2, 16, ST_INT },
    { "KT$S3 ", KT_S3, 16, ST_INT },
    { "UNUSED", 0240, 64, ST_INT },
    { "KT$DMP", KT_DMP, 66, ST_INT },
    { "KT$AQH", KT_AQH, 2, ST_INT },
    { "KT$AQT", KT_AQT, 2, ST_INT },
    { "KT$BCN", KT_BCN, 2, ST_INT },
    { "KT$TMO", KT_TMO, 2, ST_INT },
    { "KT$FCN", KT_FCN, 2, ST_INT },
    { "KT$FCT", KT_FCT, 2, ST_INT },
    { "KT$T1 ", KT_T1, 2, ST_INT },
    { "KT$T2 ", KT_T2, 2, ST_INT },
    { "KT$T3 ", KT_T3, 2, ST_INT },
    { "KT$T4 ", KT_T4, 2, ST_INT },
    { NULL }
    };

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

static STDEF dc_stc[] = {
    { "DC$LNK", DC_LNK, 2, ST_INT },
    { "DC$CMD", DC_CMD, 2, ST_INT },
    { "DC$RSB", DC_RSB, 2, ST_INT },
    { "DC$FCT", DC_FCT, 2, ST_INT },
    { "DC$STA", DC_STA, 2, ST_INT },
    { "DC$TFC", DC_TFC, 2, ST_INT },
    { "DC$0CQ", DC_0CQ, 2, ST_INT },
    { "DC$STC", DC_STC, 0, ST_INT },
    { "DC$1CQ", DC_1CQ, 2, ST_INT },
    { "DC$ICT", DC_ICT, 14, ST_INT },
    { "DC$THR", DC_THR, 0, ST_INT },
    { "DC$RTY", DC_RTY, 1, ST_INT },
    { "DC$RBS", DC_RBS, 1, ST_INT },
    { "DC$IRT", DC_IRT, 44, ST_INT },
    { "DC$SN",  DC_SN,  0, ST_INT },
    { "DC$TSN", DC_TSN, 1, ST_INT },
    { "DC$DSN", DC_DSN, 1, ST_INT },
    { "DC$RCT", DC_RCT, 2, ST_INT },
    { NULL }
    };

/* Realtime formatter status */

#define RTDS_AVL        0002                            /* available */
#define RTDS_ATN        0040                            /* attention */
#define RTDS_FRR        0100                            /* receiver ready */

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

/* Drive flags */

#define DFL_BOT         0040                            /* beginning of tape */
#define DFL_ERR         0100                            /* error */
#define DFL_TMK         0200                            /* tape mark */

/* Fragment request block - K.sti */

#define F_TEMP          0066
#define F_TCNT          0070

static STDEF frb_stc[] = {
    { "F$LINK", F_LINK, 2, ST_INT },
    { "F$PCTR", F_PCTR, 2, ST_INT },
    { "F$RTE ", F_RTE , 2, ST_INT },
    { "UNUSED", 006,    2, ST_INT },
    { "F$CTRL", F_CTRL, 2, ST_INT },
    { "F$OFF ", F_OFF , 4, ST_INT },
    { "UNUSED", 016,    2, ST_INT },
    { "F$RT1 ", F_RT1 , 2, ST_INT },
    { "UNUSED", 022,    8, ST_INT },
    { "F$RSB ", F_RSB , 2, ST_INT },
    { "F$SCNT", F_SCNT, 1, ST_INT },
    { "UNUSED", 036,    2, ST_INT },
    { "F$BFHD", F_BFHD, 2, ST_INT },
    { "F$BFTL", F_BFTL, 2, ST_INT },
    { "UNUSED", 044,   18, ST_INT },
    { "F$TEMP", F_TEMP, 2, ST_INT },
    { "F$TCNT", F_TCNT, 2, ST_INT },
    { "UNUSED", 072,    2, ST_INT },
    { "F$DSTR", F_DSTR, 2, ST_INT },
    { "F$ESUM", F_ESUM, 2, ST_INT },
    { "UNUSED", 0100,  16, ST_INT },
    { NULL }
    };

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
#define T_PRIM          0024                            /* ?? FRB queue */
#define T_SEQN          0030
#define T_TSN           0030
#define T_DSN           0031
#define T_ADR           0032                            /* pointer to ?? */
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

static STDEF t_stc[] = {
    { "T$LINK", T_LINK, 2, ST_INT },
    { "T$PCTR", T_PCTR, 2, ST_INT },
    { "T$SUCC", T_SUCC, 2, ST_INT },
    { "T$FAIL", T_FAIL, 2, ST_INT },
    { "T$FCNT", T_FCNT, 0, ST_INT },
    { "T$FLAG", T_FLAG, 2, ST_INT },
    { "T$LEV1", T_LEV1, 0, ST_INT },
    { "T$UNIT", T_UNIT, 1, ST_INT },
    { "T$OPCD", T_OPCD, 1, ST_INT },
    { "T$REC ", T_REC , 2, ST_INT },
    { "T$ERR ", T_ERR , 2, ST_INT },
    { "T$BCL ", T_BCL , 2, ST_INT },
    { "T$BCH ", T_BCH , 2, ST_INT },
    { "T$PRIM", T_PRIM, 4, ST_INT },
    { "T$SEQN", T_SEQN, 0, ST_INT },
    { "T$TSN ", T_TSN , 1, ST_INT },
    { "T$DSN ", T_DSN , 1, ST_INT },
    { "T$ADR ", T_ADR , 2, ST_INT },
    { "T$FRBQ", T_FRBQ, 0, ST_INT },
    { "T$0FRB", T_0FRB, 4, ST_INT },
    { "T$1FRB", T_1FRB, 4, ST_INT },
    { "T$2FRB", T_2FRB, 4, ST_INT },
    { "T$3FRB", T_3FRB, 4, ST_INT },
    { "T$4FRB", T_4FRB, 4, ST_INT },
    { "T$5FRB", T_5FRB, 4, ST_INT },
    { "T$6FRB", T_6FRB, 4, ST_INT },
    { "T$7FRB", T_7FRB, 4, ST_INT },
    { "T$FRBE", T_FRBE, 0, ST_INT },
    { NULL }
    };

#define KT_MAXDR        4                               /* max # drives */

#define KT_NUMBY        512                             /* bytes per block */
#define KT_MAXFR        (1 << 16)                       /* max xfer */

#define UNIT_V_ONL      (MTUF_V_UF + 0)                 /* online */
#define UNIT_V_WLK      (MTUF_V_UF + 1)                 /* hwre write lock */
#define UNIT_V_ATP      (MTUF_V_UF + 2)                 /* attn pending */
#define UNIT_V_DTYPE    (MTUF_V_UF + 3)                 /* drive type */
#define UNIT_M_DTYPE    0x1F
#define UNIT_V_NOAUTO   (MTUF_V_UF + 8)                 /* noautosize */
#define UNIT_ONL        (1 << UNIT_V_ONL)
#define UNIT_WLK        (1 << UNIT_V_WLK)
#define UNIT_ATP        (1 << UNIT_V_ATP)
#define UNIT_NOAUTO     (1 << UNIT_V_NOAUTO)
#define UNIT_DTYPE      (UNIT_M_DTYPE << UNIT_V_DTYPE)
#define GET_DTYPE(x)    (((x) >> UNIT_V_DTYPE) & UNIT_M_DTYPE)
#define unit_plug       u4                              /* drive unit plug value */
#define density         u5
#define speed           u6
#define dbn             up7
#define ktxb            filebuf                         /* xfer buffer */

#define DENS_800        01                              /* NRZI 800 bpi */
#define DENS_1600       02                              /* PE 1600 bpi */
#define DENS_6250       04                              /* GCR 6250 bpi */
#define DENS_NORM       01                              /* normal */
#define DENS_COMP       04                              /* compacted */

#define TF_9TK          01                              /* 9 track */
#define TF_CTP          02                              /* cartridge */
#define TF_3480         03                              /* 3480 */

/* Drive parameters */

#define PAR_VAR         0040                            /* variable speed */
#define PAR_AUTO        0100                            /* automatic speed */

#define TA78_DTYPE      0                               /* TA78 */
#define TA78_UMOD       1                               /* unit ID */
#define TA78_MED        0x6D68104E                      /* media ID */
#define TA78_FREV       0                               /* formatter revs */
#define TA78_UREV       0                               /* unit revs */
#define TA78_CAP        (180 * (1 << 20))               /* capacity */
#define TA78_PAR        0
#define TA78_DENS       (DENS_1600 | DENS_6250)
#define TA78_SP0        125
#define TA78_SP1        0
#define TA78_SP2        0
#define TA78_SP3        0
#define TA78_FMT        0 //(TF_9TK)

#define TA79_DTYPE      1                               /* TA79 */
#define TA79_UMOD       5                               /* unit ID */
#define TA79_MED        0x6D68104F                      /* media ID */
#define TA79_FREV       0                               /* formatter revs */
#define TA79_UREV       0                               /* unit revs */
#define TA79_CAP        (180 * (1 << 20))               /* capacity */
#define TA79_PAR        0
#define TA79_DENS       (DENS_1600 | DENS_6250)
#define TA79_SP0        125
#define TA79_SP1        0
#define TA79_SP2        0
#define TA79_SP3        0
#define TA79_FMT        0 //(TF_9TK)

#define TA81_DTYPE      2                               /* TA81 */
#define TA81_UMOD       4                               /* unit ID */
#define TA81_MED        0x6D681051                      /* media ID */
#define TA81_FREV       0                               /* formatter revs */
#define TA81_UREV       0                               /* unit revs */
#define TA81_CAP        (180 * (1 << 20))               /* capacity */
#define TA81_PAR        (PAR_AUTO)
#define TA81_DENS       (DENS_1600 | DENS_6250)
#define TA81_SP0        25
#define TA81_SP1        75
#define TA81_SP2        0
#define TA81_SP3        0
#define TA81_FMT        0 //(TF_9TK)

#define TA85_DTYPE      3                               /* TA85 */
#define TA85_UMOD       17                              /* unit ID */
#define TA85_MED        0x6D681055                      /* media ID */
#define TA85_FREV       0                               /* formatter revs */
#define TA85_UREV       0                               /* unit revs */
#define TA85_CAP        (200 * (1 << 20))               /* FIXME: should be 2.6GB capacity */
#define TA85_DENS       (DENS_NORM | DENS_COMP)
#define TA85_PAR        0
#define TA85_SP0        100
#define TA85_SP1        0
#define TA85_SP2        0
#define TA85_SP3        0
#define TA85_FMT        (TF_CTP)

#define TA90_DTYPE      4                               /* TA90 */
#define TA90_UMOD       7                               /* unit ID */
#define TA90_MED        0x6D68105A                      /* media ID */
#define TA90_FREV       0                               /* formatter revs */
#define TA90_UREV       0                               /* unit revs */
#define TA90_CAP        (200 * (1 << 20))               /* capacity */
#define TA90_DENS       (DENS_NORM)
#define TA90_PAR        0
#define TA90_SP0        79
#define TA90_SP1        0
#define TA90_SP2        0
#define TA90_SP3        0
#define TA90_FMT        (TF_3480)

#define TA91_DTYPE      5                               /* TA91 */
#define TA91_UMOD       12                              /* unit ID */
#define TA91_MED        0x6D68105B                      /* media ID */
#define TA91_FREV       0                               /* formatter revs */
#define TA91_UREV       0                               /* unit revs */
#define TA91_CAP        (200 * (1 << 20))               /* capacity */
#define TA91_DENS       (DENS_NORM | DENS_COMP)
#define TA91_PAR        0
#define TA91_SP0        79
#define TA91_SP1        0
#define TA91_SP2        0
#define TA91_SP3        0
#define TA91_FMT        (TF_3480)

struct drvtyp {
    uint16      umod;                                   /* unit model */
    uint32      med;                                    /* MSCP media */
    uint16      fver;
    uint16      uver;
    t_addr      cap;                                    /* capacity */
    uint8       par;                                    /* parameters */
    uint8       dens;                                   /* supported densities */
    uint8       fmt;                                    /* format */
    uint8       sp0;                                    /* speed 0 */
    uint8       sp1;                                    /* speed 1 */
    uint8       sp2;                                    /* speed 2 */
    uint8       sp3;                                    /* speed 3 */
    const char  *name;                                  /* name */
    };

#define KT_DRV(d) \
  { d##_UMOD, d##_MED,  d##_FREV,  d##_UREV, \
    d##_CAP,  d##_PAR,  d##_DENS,  d##_FMT, \
    d##_SP0,  d##_SP1,  d##_SP2,   d##_SP3, \
    #d }
#define KT_SIZE(d)      d##_CAP

static struct drvtyp drv_tab[] = {
    KT_DRV (TA78),
    KT_DRV (TA79),
    KT_DRV (TA81),
    KT_DRV (TA85),
    KT_DRV (TA90),
    KT_DRV (TA91),
    { 0 }
    };


extern int32 tmxr_poll;                                 /* calibrated delay */

t_stat kt_svc (UNIT *uptr);
t_stat kt_reset (DEVICE *dptr);
t_stat kt_init (uint16 req, uint16 act);
t_stat kt_poll (DEVICE *dptr, uint32 addr, int32 interval);
t_stat kt_attach (UNIT *uptr, CONST char *cptr);
t_stat kt_detach (UNIT *uptr);
t_stat kt_set_wlk (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_ctype (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_set_plug (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kt_show_plug (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_wlk (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kt_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);

/* debugging bitmaps */

#define DBG_TRC         KG_DBG_TRC                      /* trace routine calls */
#define DBG_STRUCT      KG_DBG_STRUCT                   /* dump structures */
#define DBG_TAP         0x100000                        /* display sim_tape activities */
#define DBG_STI         0x200000                        /* STI commands */
#define DBG_FRB         0x400000                        /* FRB operations */
#define DBG_TX          0x800000                        /* TX operations */
#define DBG_STR         MTSE_DBG_STR                    /* display tape structure detail */
#define DBG_POS         MTSE_DBG_POS                    /* display position activities */
#define DBG_DAT         MTSE_DBG_DAT                    /* display transfer data */

DEBTAB kt_debug[] = {
    { "TRACE",  DBG_TRC,    "trace routine calls" },
    { "STRUCT", DBG_STRUCT, "dump structures" },
    { "TAPE",   DBG_TAP,    "display sim_tape activities" },
    { "STI",    DBG_STI,    "STI commands" },
    { "FRB",    DBG_FRB,    "FRB operations" },
    { "TX",     DBG_TX,     "TX operations" },
    { "STR",    DBG_STR,    "display tape structure detail" },
    { "POS",    DBG_POS,    "display position activities" },
    { "DATA",   DBG_DAT,    "display transfer data" },
    { 0 }
};

HDIB kt_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    NULL, K_STI, 0, &kt_init, &kt_poll
    };

UNIT kt_unit[KT_MAXDR] = {
    { UDATA (&kt_svc, UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (TA81_DTYPE << UNIT_V_DTYPE), KT_SIZE (TA81)) },
    { UDATA (&kt_svc, UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (TA81_DTYPE << UNIT_V_DTYPE), KT_SIZE (TA81)) },
    { UDATA (&kt_svc, UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (TA81_DTYPE << UNIT_V_DTYPE), KT_SIZE (TA81)) },
    { UDATA (&kt_svc, UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (TA81_DTYPE << UNIT_V_DTYPE), KT_SIZE (TA81)) },
    };

REG kt_reg[] = {
    { NULL }
    };

MTAB kt_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL, "Backplane slot number" },
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", "ADDRESS",
      NULL, &kg_show_addr, NULL, "Control memory address" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "CONTROL", NULL,
      NULL, &kt_show_ctrl, NULL, "Display K.sti control memory area" },
    { UNIT_WLK, 0, NULL, "WRITEENABLED",
      &kt_set_wlk, NULL, NULL, "Write enable tape drive" },
    { UNIT_WLK, UNIT_WLK, NULL, "LOCKED",
      &kt_set_wlk, NULL, NULL, "Write lock tape drive" },
    { MTAB_XTD|MTAB_VUN, 0, "WRITE", NULL,
      NULL, &kt_show_wlk, NULL, "Display drive writelock status" },
    { MTAB_XTD|MTAB_VUN, TA78_DTYPE, NULL, "TA78",
      &kt_set_type, NULL, NULL, "Set TA78 Drive Type" },
    { MTAB_XTD|MTAB_VUN, TA79_DTYPE, NULL, "TA79",
      &kt_set_type, NULL, NULL, "Set TA79 Drive Type" },
    { MTAB_XTD|MTAB_VUN, TA81_DTYPE, NULL, "TA81",
      &kt_set_type, NULL, NULL, "Set TA81 Drive Type" },
    { MTAB_XTD|MTAB_VUN, TA85_DTYPE, NULL, "TA85",
      &kt_set_type, NULL, NULL, "Set TA85 Drive Type" },
    { MTAB_XTD|MTAB_VUN, TA90_DTYPE, NULL, "TA90",
      &kt_set_type, NULL, NULL, "Set TA90 Drive Type" },
    { MTAB_XTD|MTAB_VUN, TA91_DTYPE, NULL, "TA91",
      &kt_set_type, NULL, NULL, "Set TA91 Drive Type" },
    { MTAB_XTD|MTAB_VUN, 0, "TYPE", NULL,
      NULL, &kt_show_type, NULL, "Display device type" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "UNIT", "UNIT=val (0-4095)",
      &kt_set_plug, &kt_show_plug, NULL, "Set/Display Unit plug value" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "FORMAT", "FORMAT",
      &sim_tape_set_fmt, &sim_tape_show_fmt, NULL, "Set/Display tape format (SIMH, E11, TPC, P7B, AWS, TAR)" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "CAPACITY", "CAPACITY",
      &sim_tape_set_capac, &sim_tape_show_capac, NULL, "Set/Display capacity" },
    { MTAB_XTD|MTAB_VDV, K_STI, NULL, "KSTI",
      &kt_set_ctype, NULL, NULL, "Set K.sti Controller Type" },
    { MTAB_XTD|MTAB_VDV, K_SI4_TAPE, NULL, "KSI4",
      &kt_set_ctype, NULL, NULL, "Set K.si 4-port Controller Type" },
    { MTAB_XTD | MTAB_VDV, 0, "TYPE", NULL,
      NULL, &kt_show_ctype, NULL, "Display controller type" },
    { 0 }
    };

DEVICE kt_dev = {
    "KT", kt_unit, kt_reg, kt_mod,
    KT_MAXDR, DEV_RDX, T_ADDR_W, 2, DEV_RDX, 16,
    NULL, NULL, &kt_reset,
    NULL, &kt_attach, &kt_detach,
    &kt_dib, DEV_DEBUG | DEV_HSC | DEV_DISABLE | DEV_TAPE,
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

t_stat kt_poll (DEVICE *dptr, uint32 addr, int32 interval)
{
HDIB *dibp = (HDIB *)dptr->ctxt;
uint32 ksa, tb, i;

for (i = 0; i < dptr->numunits; i++) {
    if (addr == dibp->cma + KT_IPQ) {                   /* addr = IPQ? */
        sim_activate_abs (&dptr->units[i], interval);   /* yes, activate unit */
        break;
        }
    tb = GETP (dibp->cma + KT_IPQ);                     /* get T$ pointer */
    if ((addr > (tb + T_FRBQ)) && (addr < (tb + T_FRBE))) { /* addr within T$? */
        sim_activate_abs (&dptr->units[i], interval);   /* yes, activate unit */
        break;
        }
    ksa = dibp->cma + KT_S0 + KSA_DHD + (i * 16);       /* drive state area */
    if (addr == ksa) {
        sim_activate_abs (&dptr->units[i], interval);   /* yes, activate unit */
        break;
        }
    }
return SCPE_OK;
}

t_stat kt_notify (UNIT *uptr, uint32 kt, uint32 port)
{
uint32 ksa = kt + KT_S0 + (port * 16);                  /* drive state area */
uint32 sq, fl;

//fl = GETB (ksa + KSA_STA);                              /* get drive flags */
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
//    PUTB (ksa + KSA_STA, fl);                           /* now on service queue */
    kg_send (sq, ksa);                                  /* send drive state area to service queue */
//    sim_printf ("Drive state area sent to service queue\n");
//    }
return SCPE_OK;
}

/* Online */

uint16 kt_online (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);

sim_debug_unit (DBG_STI, uptr, "ONLINE, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
uptr->flags = uptr->flags | UNIT_ONL;
return 002;
}

/* Rewind */

uint16 kt_rewind (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 flags = GETB (cmd + 002);

sim_debug_unit (DBG_STI, uptr, "INITIATE REWIND, flags = %02X\n", flags);
if (flags & 0x80) 
    sim_tape_detach (uptr);                             /* rewind & unload */
else if (flags & 0x40)
    sim_tape_wreomrw (uptr);                            /* erase remainder of tape & rewind */
else
    sim_tape_rewind (uptr);                             /* rewind only */
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
return 002;
}

/* Disconnect */

uint16 kt_disc (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_STI, uptr, "DISCONNECT\n");
uptr->flags = uptr->flags & ~UNIT_ONL;
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
return 002;
}

/* Diagnose */

uint16 kt_diag (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 test = GETW (cmd + 002);                          /* drive resident test to run */  /* TODO: check this */
sim_debug_unit (DBG_STI, uptr, "DIAGNOSE, test = %02X\n", test);
PUTW (rsp, 0x3C);                                       /* response opcode */
return 004;
}

/* Get formatter characteristics */

uint16 kt_getchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_STI, uptr, "GET FORMATTER CHARACTERISTICS\n");
PUTW (rsp + 000, 0xB8);                                 /* response opcode */
PUTW (rsp + 002, 0x00);                                 /* MBC */
PUTW (rsp + 004, 0x00);                                 /* short timeout */
PUTW (rsp + 006, 0x00);                                 /* long timeout */
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

sim_debug_unit (DBG_STI, uptr, "GET UNIT CHARACTERISTICS, p1 = %02X\n", p1);
PUTW (rsp + 0000, 0x36);                                /* response opcode */
PUTW (rsp + 0002, uptr->unit_plug & 0xFF);              /* unique drive ID #1 */
PUTW (rsp + 0004, (uptr->unit_plug >> 8) & 0xFF);
PUTW (rsp + 0006, 0x00);                                /* unique drive ID #2 */
PUTW (rsp + 0010, 0x00);
PUTW (rsp + 0012, 0x00);                                /* unique drive ID #3 */
PUTW (rsp + 0014, 0x00);
PUTW (rsp + 0016, 0x01);                                /* microcode version */
PUTW (rsp + 0020, 0x02);                                /* hardware revision */
PUTW (rsp + 0022, dtab->umod);                          /* unit model? */
PUTW (rsp + 0024, dtab->med & 0xFF);                    /* media ID */
PUTW (rsp + 0026, (dtab->med >> 8) & 0xFF);
PUTW (rsp + 0030, (dtab->med >> 16) & 0xFF);
PUTW (rsp + 0032, (dtab->med >> 24) & 0xFF);
PUTW (rsp + 0034, PAR_AUTO);                            /* drive parameters */
PUTW (rsp + 0036, dtab->dens);                          /* density menu */
PUTW (rsp + 0040, dtab->sp0);                           /* speed menu */
PUTW (rsp + 0042, dtab->sp1);
PUTW (rsp + 0044, dtab->sp2);
PUTW (rsp + 0046, dtab->sp3);
PUTW (rsp + 0050, dtab->fmt);                           /* SCH */
PUTW (rsp + 0052, 0x00);
return 026;
}

/* Get status */

uint16 kt_getsts (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_STI, uptr, "GET SUMMARY STATUS\n");
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

uint16 kt_getextsts (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint32 dtyp = GET_DTYPE (uptr->flags);                  /* get drive type */
struct drvtyp *dtab = &drv_tab[dtyp];
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_STI, uptr, "GET EXTENDED DRIVE STATUS, p1 = %02X\n", p1);
PUTW (rsp + 000, 0x3A);                                 /* response opcode - DFL? */
PUTW (rsp + 002, uptr->speed);                          /* SPE - current speed? */
PUTW (rsp + 004, uptr->density);                        /* current density */
PUTW (rsp + 006, uptr->unit_plug & 0xFF);               /* unit number (low) */
PUTW (rsp + 010, (uptr->unit_plug >> 8) & 0xFF);        /* unit number (high) */
PUTW (rsp + 012, 0x01);                                 /* GAP */
PUTW (rsp + 014, 0x00);                                 /* GAP */
PUTW (rsp + 016, 0x00);                                 /* GAP */
PUTW (rsp + 020, 0x00);                                 /* GAP */
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
sim_debug_unit (DBG_STI, uptr, "READ MEMORY, p1 = %02X, p2 = %02X, p3 = %02X, p4 = %02X, p5 = %02X\n", p1, p2, p3, p4, p5);
PUTW (rsp, 0x30);                                       /* response opcode */
return 016;
}

/* Read memory */

uint16 kt_wrmem (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_STI, uptr, "WRITE MEMORY\n");
PUTW (rsp, 0xFE);                                       /* response opcode */
return 002;
}

uint16 kt_chflag (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
sim_debug_unit (DBG_STI, uptr, "CHANGE CONTROLLER FLAGS, p1 = %02X, p2 = %02X\n", p1, p2);
PUTW (rsp, 0xFE);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 kt_topology (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_STI, uptr, "TOPOLOGY\n");
PUTW (rsp, 0x2D);                                       /* TOPOLOGY RESPONSE */
return 016;
}

uint16 kt_clrde (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
sim_debug_unit (DBG_STI, uptr, "CLEAR DRIVE ERRORS, p1 = %02X, p2 = %02X\n", p1, p2);
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_clrfe (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_STI, uptr, "CLEAR FORMATTER ERRORS, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_getflog (UNIT *uptr, uint32 cmd, uint32 rsp)
{
sim_debug_unit (DBG_STI, uptr, "GET FORMATTER ERROR LOG\n");
PUTW (rsp, 0x39);
return 002;
}

uint16 kt_egap (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);

sim_debug_unit (DBG_STI, uptr, "ERASE GAP, p1 = %02X\n", p1);
PUTW (rsp, 0x35);
return 003;
}

uint16 kt_pos (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);                            /* flags */
uint8 p2 = GETB (cmd + 004);                            /* records/files */
uint8 p3 = GETB (cmd + 006);
uint32 files = 0, recs = 0, flags = 0;
uint32 recskipped = 0, fileskipped = 0, objectsskipped = 0;

if (p1 & 0x8)
    files = p2;                                         /* skip files */
else
    recs = p2;                                          /* skip records */
if (p1 & 0x4)
    flags |= MTPOS_M_REV;                               /* reverse direction */
sim_debug_unit (DBG_STI, uptr, "POSITION TAPE, flags = %02X, objs = %02X, p3 = %02X\n", p1, p2, p3);
sim_tape_position (uptr, flags, recs, &recskipped, files, &fileskipped, &objectsskipped);
PUTW (rsp + 000, 0xB2);
PUTW (rsp + 002, 0x01);                                 /* GAP? */
PUTW (rsp + 004, 0x02);                                 /* GAP? */
PUTW (rsp + 006, 0x03);                                 /* GAP? */
PUTW (rsp + 010, 0x04);                                 /* GAP? */
if (fileskipped) {
    PUTW (rsp + 012, DFL_TMK);                          /* drive flags? */
    }
else {
    PUTW (rsp + 012, 0);                                /* drive flags? */
    }
PUTW (rsp + 014, 0x00);
return 007;
}

uint16 kt_setumode (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);

sim_debug_unit (DBG_STI, uptr, "SET UNIT EXECUTION MODE, p1 = %02X, p2 = %02X, p3 = %02X\n", p1, p2, p3);
PUTW (rsp, 0xB1);
return 004;
}

uint16 kt_setcount (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);

sim_debug_unit (DBG_STI, uptr, "SET BYTE COUNT, p1 = %02X, p2 = %02X, record size = %d\n", p1, p2, (p3 * 256));
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_setuchar (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
uint8 p2 = GETB (cmd + 004);
uint8 p3 = GETB (cmd + 006);
sim_debug_unit (DBG_STI, uptr, "SET UNIT CHARACTERISTICS, p1 = %02X, density = %02X, speed = %02X\n", p1, p2, p3);
uptr->density = p2;
uptr->speed = p3;
PUTW (rsp, 0xFE);
return 002;
}

uint16 kt_tmark (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_STI, uptr, "WRITE TAPE MARK, p1 = %02X\n", p1);
sim_tape_wrtmk (uptr);
PUTW (rsp, 0xAC);
return 003;
}

uint16 kt_der (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 p1 = GETB (cmd + 002);
sim_debug_unit (DBG_STI, uptr, "DATA ERROR RECOVERY, p1 = %02X\n", p1);
PUTW (rsp, 0xFE);
return 002;
}

/* STI level 2 command */

uint16 kt_cmd (UNIT *uptr, uint32 cmd, uint32 rsp)
{
uint8 opc = GETB (cmd);                                 /* get STI opcode */
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
        sim_printf ("Unknown STI level 2 command: 0x%02X\n", opc);
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
sim_debug_unit (DBG_STI, uptr, "LINE STATUS\n");
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
    dc = GETP (ksa + KSA_DHD);                          /* get DCB */
    if (dc == 0)
        break;

    cq = GETP (dc + DC_0CQ);                            /* get success queue */
    if (cq == 0)
        break;

    if (Q_LOCKED (cq))                                  /* success queue locked? */
        break;

    dc = kg_receive (ksa + KSA_DHD);                    /* dequeue from drive state area */

    kg_print_struct (NULL, uptr, dc, dc_stc);

    rsp = GETP (dc + DC_RSB);                           /* get response buffer */
    opc = GETW (dc + DC_STA);
    rsz = 0;
    switch (opc & 0177400) {
        case 014400:                                    /* ECHO DATA */
            sim_debug_unit (DBG_STI, uptr, "ECHO DATA\n");
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

void kt_process_frb (UNIT *uptr, uint32 t_opc, uint32 frb)
{
uint32 bd, scnt, dma, rte, opc, rsb;
uint32 kt = kt_dib.cma;
uint16 *xbuf = (uint16 *)uptr->ktxb;
t_mtrlnt tbc;
t_stat r;

scnt = GETB (frb + F_SCNT);                             /* sector count */
rte = GETP (frb + F_RTE);                               /* get route */
rsb = GETW (frb + F_RSB);                               /* record size */
opc = GETB (rte + V_OPCD);                              /* get opcode from route */

kg_print_struct (NULL, uptr, frb, frb_stc);
sim_debug_unit (DBG_FRB, uptr, "%07o, SCNT: %d, OPC: %o T_OPC: %o\n", frb, scnt, opc, t_opc);
if (t_opc == 027) {                                    /* read record forwards */
    for (int i = 0; i < scnt; i++) {
        bd = GETP (kt_dib.cma + KG_FBU);               /* get a free data memory buffer */
        bd = kg_receive (bd);
        dma = GETW (bd + 002);
        dma = (dma << 6) & DMEMMASK;
        sim_printf ("Data buffer at %o\n", dma);

        for (int j = 0; j < 288; j++) {                 /* clear buffer */
            PUTDW (dma + (j << 1), 0);
            }

        r = sim_tape_rdrecf (uptr, (uint8 *)xbuf, &tbc, MT_MAXFR);

        for (int i = 0; i < 256; i++) {
            PUTDW (dma + 040 + (i << 1), xbuf[i]);
            }
        PUTDW (dma + 01040, kt_edc (dma));

        kg_send (frb + F_BFHD, bd);                     /* queue buffer to the FRB */
        }
    }
else if (t_opc == 0216) {                               /* write record */
    bd = GETP (frb + F_BFHD);
    for (int k = 0; k < scnt; k++) {
        if (bd == 0)
            break;
        dma = GETW (bd + 002);
        dma = (dma << 6) & DMEMMASK;
        sim_printf ("Data buffer at %o\n", dma);

        for (int i = 0; i < 256; i++) {
            xbuf[i] = GETDW (dma + 040 + (i << 1));
            }

        r = sim_tape_wrrecf (uptr, (uint8 *)xbuf, 512); /* write rec */

        bd = GETP (bd);
        }
    }
else
    sim_printf ("Unknown route opcode for K.sti: %o\n", opc);

kg_route_frb (uptr, kt, frb, TRUE);
}

void kt_process_ipq (UNIT *uptr, uint32 kt)
{
uint32 tb, cnt, cq, frb, fc, flg, rte, ctr, opc, rsb;

tb = GETP (kt + KT_IPQ);                                /* get T$ */
if (tb == 0)
    return;

kg_print_struct (NULL, uptr, tb, t_stc);
sim_debug_unit (DBG_TX, uptr, "TS: %07o\n", tb);

cnt = GETW (tb + T_FCNT);
ctr = GETP (tb + T_PCTR);
opc = GETB (tb + T_OPCD);
fc = 0;
for (int j = 0; j < 50; j++) {                          /* prevent endless loop */
    frb = kg_receive (tb + T_PRIM);                     /* get next FRB */
    if (frb == 0)
        break;
    rte = GETP (frb + F_RTE);                           /* get route */
    flg = GETB (rte + V_FLGS);                          /* get flags from route */
    kt_process_frb (uptr, opc, frb);
    fc++;
    if (((flg & V_NC) == 0) && (cnt > 0)) {
        kg_dec_ctr (uptr, ctr);
        cnt = cnt - 1;
        }
    }
if (fc > 0)
    sim_debug_unit (DBG_TX, uptr, "%d FRBs processed\n", fc);

fc = 0;
for (int k = 0; k < 8; k++) {                           /* for each drive? */
    for (int j = 0; j < 50; j++) {                      /* prevent endless loop */
        frb = kg_receive (tb + T_FRBQ + (k << 2));      /* get next FRB */
        if (frb == 0)
            break;
        rte = GETP (frb + F_RTE);                       /* get route */
        flg = GETB (rte + V_FLGS);                      /* get flags from route */
        rsb = GETW (frb + F_RSB);
        if (rsb < 01000) {
            PUTW (frb + F_ESUM, 0200);
            kg_route_frb (uptr, kt, frb, FALSE);
            }
        else {
            kg_route_frb (uptr, kt, frb, TRUE);
        fc++;
        if ((flg & V_NC) == 0)
            kg_dec_ctr (uptr, ctr);
        }
        }
    }
if (fc > 0)
    sim_debug_unit (DBG_TX, uptr, "%d FRBs routed\n", fc);

PUTW (tb + T_FCNT, cnt);                                /* update counter */

if (cnt == 0) {                                         /* all operation complete? */
    tb = kg_receive (kt + KT_IPQ);                      /* dequeue T$ */
    cq = GETP (tb + T_SUCC);                            /* get T$ success completion queue */
    kg_send (cq, tb);                                   /* send T$ to completion queue */
    sim_debug_unit (DBG_TX, uptr, "T$ completed, dest = %o\n", cq);
    }
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
    dc = kg_receive (ksa + KSA_DHD);                    /* get DCB from drive state area */
    if (dc) {
        cq = GETP (dc + DC_0CQ);                        /* get completion queue */
        kg_send (cq, dc);                               /* put DCB to completion queue */
        }
    }
else if (!Q_LOCKED (ksa + KSA_IND)) {                   /* drive state area locked? */
//    if ((GETB (ksa + KSA_STA) & FLGS_SVC) == 0)         /* drive not known to system? */
    if ((uptr->flags & UNIT_ATP) == 0) {
        kt_notify (uptr, kt, port);                     /* notify drive present */
        uptr->flags |= UNIT_ATP;
        }
    kt_process_dcb (uptr, kt, port);                    /* process DCB queue */
    kt_process_ipq (uptr, kt);                          /* process transfer queue */
    }
kg_svc (uptr);
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
if (uptr->flags & UNIT_WLK)
    fprintf (st, "write locked");
else fprintf (st, "write enabled");
return SCPE_OK;
}

/* Set unit type */

t_stat kt_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (val < 0)
    return SCPE_ARG;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
uptr->flags = (uptr->flags & ~UNIT_DTYPE) | (val << UNIT_V_DTYPE);
uptr->capac = (t_addr)drv_tab[val].cap;
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
DEVICE *dptr = find_dev_from_unit (uptr);
HDIB *dibp = (HDIB *) dptr->ctxt;
uint32 i;

for (i = 0; i < KT_MAXDR; i++) {
    if (dptr->units[i].flags & UNIT_ATT)
        return sim_messagef (SCPE_ALATT, "Can't change controller type on %s with %s attached to %s.\n", 
                                         dptr->name, sim_uname (&dptr->units[i]), dptr->units[i].filename);
    }
dibp->sts = val;
return SCPE_OK;
}

/* Show controller type */

t_stat kt_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr = find_dev_from_unit (uptr);
HDIB *dibp = (HDIB *) dptr->ctxt;

switch (dibp->sts) {
    case K_STI:
        fprintf (st, "K.sti tape data channel");
        break;
        
    case K_SI4:
    case K_SI4_TAPE:
        fprintf (st, "K.si 4-port data channel");
        break;
    }
return SCPE_OK;
}

/* Show the K.sti control area */

t_stat kt_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
t_stat r;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;
kg_print_struct (st, uptr, dibp->cma, kt_stc);
return SCPE_OK;
}

/* Device attach */

t_stat kt_attach (UNIT *uptr, CONST char *cptr)
{
t_stat r;

r = sim_tape_attach_ex (uptr, cptr, DBG_TAP, 0);
if (r != SCPE_OK)
    return r;
return SCPE_OK;
}

/* Device detach */

t_stat kt_detach (UNIT *uptr)
{
t_stat r;

r = sim_tape_detach (uptr);                             /* detach unit */
if (r != SCPE_OK)
    return r;
return SCPE_OK;
}

/* Device reset */

t_stat kt_reset (DEVICE *dptr)
{
uint32 i;
UNIT *uptr;
static t_bool plugs_inited = FALSE;
uint32 dtyp;

sim_debug (DBG_TRC, dptr, "kt_reset\n");

if (!plugs_inited ) {
    uint32 d, u = 0;

    plugs_inited  = TRUE;
    for (d = 0; d < dptr->numunits; d++) {
        dptr->units[d].unit_plug = u++;                 /* unique unit numbers */
        }
    }
for (i = 0; i < dptr->numunits; i++) {                  /* init units */
    uptr = &dptr->units[i];
    sim_cancel (uptr);                                  /* clr activity */
    sim_tape_reset (uptr);
    uptr->flags = uptr->flags & ~(UNIT_ONL | UNIT_ATP);
    dtyp = GET_DTYPE (uptr->flags);                     /* get drive type */
    for (int j = 2; j >= 0; j--) {                      /* find highest supported density */
        if (drv_tab[dtyp].dens & (1u << j)) {
            uptr->density = (1u << j);                  /* default to highest density */
            break;
            }
        }
    uptr->speed = drv_tab[dtyp].sp0;                    /* default to lowest speed */
    uptr->ktxb = (uint16 *) realloc (uptr->ktxb, (KT_MAXFR >> 1) * sizeof (uint16));
    if (uptr->ktxb == NULL)
        return SCPE_MEM;
    }
return SCPE_OK;
}
