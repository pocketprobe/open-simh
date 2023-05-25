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
#define KSA_FLGS        0002                            /* flags */
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


extern int32 tmxr_poll;                                 /* calibrated delay */

uint8 ks_mode = 0;

t_stat ks_svc (UNIT *uptr);
t_stat ks_reset (DEVICE *dptr);
t_stat ks_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ks_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ks_init (uint16 req, uint16 act);

/* debugging bitmaps */

#define DBG_TRC         KG_DBG_TRC                      /* trace routine calls */

DEBTAB ks_debug[] = {
    { "TRACE",  DBG_TRC,   "trace routine calls" },
    { 0 }
};

HDIB ks_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    K_SDI, 0, &ks_init
    };

UNIT ks_unit = { UDATA (&ks_svc, 0, 0) };

REG ks_reg[] = {
    { NULL }
    };

MTAB ks_mod[] = {
    { MTAB_XTD|MTAB_VDV, 004, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      NULL, &kg_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "CONTROL", "CONTROL",
      NULL, &ks_show_ctrl, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "RUN", "RUN",
      &ks_run, NULL, NULL },
    { 0 }
    };

DEVICE ks_dev = {
    "KS", &ks_unit, ks_reg, ks_mod,
    1, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &ks_reset,
    NULL, NULL, NULL,
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
    sim_clock_coschedule (&ks_unit, tmxr_poll);           /* activate */
    }
else {
    sim_debug (DBG_TRC, &ks_dev, "K.sdi %d halt at %o\n", req, (uint16)inst_pc);
    sim_cancel (&ks_unit);
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

print_word (st, ks, "KS$7SM", -040, 0);
print_word (st, ks, "KS$7SC", -040, 2);
print_word (st, ks, "KS$7FL", -036, 0);
print_word (st, ks, "KS$7BW", -036, 2);
print_word (st, ks, "KS$6SM", -034, 0);
print_word (st, ks, "KS$6SC", -034, 2);
print_word (st, ks, "KS$6FL", -032, 0);
print_word (st, ks, "KS$6BW", -032, 2);
print_word (st, ks, "KS$5SM", -030, 0);
print_word (st, ks, "KS$5SC", -030, 2);
print_word (st, ks, "KS$5FL", -026, 0);
print_word (st, ks, "KS$5BW", -026, 2);
print_word (st, ks, "KS$4SM", -024, 0);
print_word (st, ks, "KS$4SC", -024, 2);
print_word (st, ks, "KS$4FL", -022, 0);
print_word (st, ks, "KS$4BW", -022, 2);
print_word (st, ks, "KS$3SM", -020, 0);
print_word (st, ks, "KS$3SC", -020, 2);
print_word (st, ks, "KS$3FL", -016, 0);
print_word (st, ks, "KS$3BW", -016, 2);
print_word (st, ks, "KS$2SM", -014, 0);
print_word (st, ks, "KS$2SC", -014, 2);
print_word (st, ks, "KS$2FL", -012, 0);
print_word (st, ks, "KS$2BW", -012, 2);
print_word (st, ks, "KS$1SM", -010, 0);
print_word (st, ks, "KS$1SC", -010, 2);
print_word (st, ks, "KS$1FL", -006, 0);
print_word (st, ks, "KS$1BW", -006, 2);
print_word (st, ks, "KS$0SM", -004, 0);
print_word (st, ks, "KS$0SC", -004, 2);                 /* sector count? */
print_word (st, ks, "KS$0FL", -002, 0);
print_word (st, ks, "KS$0BW", -002, 2);
print_word (st, ks, "KS$BAS", 000, 0);
print_word (st, ks, "KS$0HD", 000, 2);
print_word (st, ks, "KS$0TD", 002, 2);
print_word (st, ks, "KS$1HD", 004, 2);
print_word (st, ks, "KS$1TD", 006, 2);
print_word (st, ks, "KS$2HD", 010, 2);
print_word (st, ks, "KS$2TD", 012, 2);
print_word (st, ks, "KS$3HD", 014, 2);
print_word (st, ks, "KS$3TD", 016, 2);
print_word (st, ks, "KS$4HD", 020, 2);
print_word (st, ks, "KS$4TD", 022, 2);
print_word (st, ks, "KS$5HD", 024, 2);
print_word (st, ks, "KS$5TD", 026, 2);
print_word (st, ks, "KS$6HD", 030, 2);
print_word (st, ks, "KS$6TD", 032, 2);
print_word (st, ks, "KS$7HD", 034, 2);
print_word (st, ks, "KS$7TD", 036, 2);
print_word (st, ks, "KS$TLK", 040, 2);

r = kg_show_ctrl (st, uptr, val, desc);
if (r != SCPE_OK)
    return r;

print_word (st, ks, "KS$KSA", 0140, 0);                 /* drive state area */
print_word (st, ks, "KS$S0 ", 0140, 16);
print_word (st, ks, "KS$S1 ", 0160, 16);
print_word (st, ks, "KS$S2 ", 0200, 16);
print_word (st, ks, "KS$S3 ", 0220, 16);
print_word (st, ks, "KS$S4 ", 0240, 16);
print_word (st, ks, "KS$S5 ", 0260, 16);
print_word (st, ks, "KS$S6 ", 0300, 16);
print_word (st, ks, "KS$S7 ", 0320, 16);
print_word (st, ks, "KS$DMP", 0340, 66);
print_word (st, ks, "KS$BW ", 0442, 2);
print_word (st, ks, "KS$TM1", 0444, 2);
print_word (st, ks, "KS$TM2", 0446, 2);
print_word (st, ks, "KS$TM3", 0450, 2);
print_word (st, ks, "KS$0LH", 0452, 2);
print_word (st, ks, "KS$0HH", 0454, 2);
print_word (st, ks, "KS$1LH", 0456, 2);
print_word (st, ks, "KS$1HH", 0460, 2);
print_word (st, ks, "KS$2LH", 0462, 2);
print_word (st, ks, "KS$2HH", 0464, 2);
print_word (st, ks, "KS$3LH", 0466, 2);
print_word (st, ks, "KS$3HH", 0470, 2);
print_word (st, ks, "KS$4LH", 0472, 2);
print_word (st, ks, "KS$4HH", 0474, 2);
print_word (st, ks, "KS$5LH", 0476, 2);
print_word (st, ks, "KS$5HH", 0500, 2);
print_word (st, ks, "KS$6LH", 0502, 2);
print_word (st, ks, "KS$6HH", 0504, 2);
print_word (st, ks, "KS$7LH", 0506, 2);
print_word (st, ks, "KS$7HH", 0510, 2);

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

print_word (st, kt, "KT$BAS", 0000, 0);
print_word (st, kt, "KT$IPQ", 0000, 4);
print_word (st, kt, "KT$WQH", 0004, 2);                 /* work queue head? */
print_word (st, kt, "KT$WQT", 0006, 2);                 /* work queue tail? */
print_word (st, kt, "KT$FLA", 0010, 2);
print_word (st, kt, "KT$SQP", 0012, 2);
print_word (st, kt, "KT$TCT", 0014, 2);
print_word (st, kt, "KT$LAH", 0016, 2);
print_word (st, kt, "KT$WAI", 0020, 2);
print_word (st, kt, "KT$SEM", 0022, 12);
print_word (st, kt, "KT$BW ", 0036, 2);

r = kg_show_ctrl (st, uptr, val, desc);
if (r != SCPE_OK)
    return r;

print_word (st, kt, "KT$S0 ", 0140, 16);                /* drive state area */
print_word (st, kt, "KT$S1 ", 0160, 16);
print_word (st, kt, "KT$S2 ", 0200, 16);
print_word (st, kt, "KT$S3 ", 0220, 16);
print_word (st, kt, "UNUSED", 0240, 64);
print_word (st, kt, "KT$DMP", 0340, 66);
print_word (st, kt, "KT$AQH", 0442, 2);
print_word (st, kt, "KT$AQT", 0444, 2);
print_word (st, kt, "KT$BCN", 0446, 2);
print_word (st, kt, "KT$TMO", 0450, 2);
print_word (st, kt, "KT$FCN", 0452, 2);
print_word (st, kt, "KT$FCT", 0454, 2);
print_word (st, kt, "KT$T1 ", 0456, 2);
print_word (st, kt, "KT$T2 ", 0460, 2);
print_word (st, kt, "KT$T3 ", 0462, 2);
print_word (st, kt, "KT$T4 ", 0464, 2);

return SCPE_OK;
}

void print_drat (uint32 dt)
{
print_word (NULL, dt, "DT$LNK", DT_LNK, 2);
print_word (NULL, dt, "DT$L1C", DT_L1C, 2);
print_word (NULL, dt, "DT$ERQ", DT_ERQ, 2);
print_word (NULL, dt, "DT$OFF", DT_OFF, 2);
print_word (NULL, dt, "DT$STA", DT_STA, 2);
print_word (NULL, dt, "DT$DPS", DT_DPS, 1);
print_word (NULL, dt, "DT$HPS", DT_HPS, 1);
print_word (NULL, dt, "DT$CQ ", DT_CQ , 0);
print_word (NULL, dt, "DT$HST", DT_HST, 8);
print_word (NULL, dt, "DT$CNT", DT_CNT, 0);
print_word (NULL, dt, "DT$MSC", DT_MSC, 8);
print_word (NULL, dt, "DT$SCT", DT_SCT, 2);
print_word (NULL, dt, "DT$XXX", DT_XXX, 2);
print_word (NULL, dt, "DT$HLD", DT_HLD, 4);
print_word (NULL, dt, "DT$CYL", DT_CYL, 0);
print_word (NULL, dt, "DT$BFC", DT_BFC, 2);
print_word (NULL, dt, "DT$LFL", DT_LFL, 2);
print_word (NULL, dt, "DT$TFC", DT_TFC, 2);
print_word (NULL, dt, "DT$DU ", DT_DU , 2);
print_word (NULL, dt, "DT$RCC", DT_RCC, 2);
print_word (NULL, dt, "DT$DTC", DT_DTC, 2);
print_word (NULL, dt, "DT$PBC", DT_PBC, 2);
print_word (NULL, dt, "DT$DCB", DT_DCB, 2);
print_word (NULL, dt, "DT$FLG", DT_FLG, 2);
print_word (NULL, dt, "UNUSED", 066, 2);
}

void print_frb (uint32 frb)
{
print_word (NULL, frb, "F$LINK", F_LINK, 2);
print_word (NULL, frb, "F$PCTR", F_PCTR, 2);
print_word (NULL, frb, "F$RTE ", F_RTE , 2);
print_word (NULL, frb, "F$MOD ", F_MOD , 2);
print_word (NULL, frb, "F$CTRL", F_CTRL, 2);
print_word (NULL, frb, "F$OFF ", F_OFF , 4);
print_word (NULL, frb, "F$LLBN", F_LLBN, 2);
print_word (NULL, frb, "F$RT1 ", F_RT1 , 2);
print_word (NULL, frb, "F$HLBN", F_HLBN, 2);
print_word (NULL, frb, "F$SCT ", F_SCT , 0);
print_word (NULL, frb, "F$RT2 ", F_RT2 , 2);
print_word (NULL, frb, "F$TRK ", F_TRK , 1);
print_word (NULL, frb, "UNUSED", 027, 1);
print_word (NULL, frb, "F$GRP ", F_GRP , 0);
print_word (NULL, frb, "F$RT3 ", F_RT3 , 2);
print_word (NULL, frb, "F$RSB ", F_RSB , 2);
print_word (NULL, frb, "F$SCNT", F_SCNT, 1);
print_word (NULL, frb, "UNUSED", 035, 1);
print_word (NULL, frb, "F$SRTE", F_SRTE, 2);
print_word (NULL, frb, "F$BFHD", F_BFHD, 2);
print_word (NULL, frb, "F$BFTL", F_BFTL, 2);
print_word (NULL, frb, "F$RT4 ", F_RT4 , 2);
print_word (NULL, frb, "F$CYL ", F_CYL , 2);
print_word (NULL, frb, "F$SLLB", F_SLLB, 2);
print_word (NULL, frb, "F$SHLB", F_SHLB, 2);
print_word (NULL, frb, "F$SBFH", F_SBFH, 2);
print_word (NULL, frb, "F$SSCT", F_SSCT, 1);
print_word (NULL, frb, "F$REMN", F_REMN, 1);
print_word (NULL, frb, "F$PRMY", F_PRMY, 2);
print_word (NULL, frb, "F$SLRB", F_SLRB, 2);
print_word (NULL, frb, "F$TGDT", F_TGDT, 2);
print_word (NULL, frb, "F$SNUM", F_SNUM, 2);
print_word (NULL, frb, "F$SCR ", F_SCR , 0);
print_word (NULL, frb, "F$TCNT", F_TCNT, 2);
print_word (NULL, frb, "F$DRAT", F_DRAT, 2);
print_word (NULL, frb, "F$DSTR", F_DSTR, 2);
print_word (NULL, frb, "F$ESUM", F_ESUM, 2);
print_word (NULL, frb, "F$DUCB", F_DUCB, 2);
print_word (NULL, frb, "F$BPTR", F_BPTR, 2);
print_word (NULL, frb, "F$SQL ", F_SQL , 2);
print_word (NULL, frb, "F$FLGS", F_FLGS, 2);
print_word (NULL, frb, "F$CRUN", F_CRUN, 1);
print_word (NULL, frb, "F$CFLG", F_CFLG, 1);
print_word (NULL, frb, "UNUSED", 0112, 6);
}

void print_dcb (uint32 dc)
{
print_word (NULL, dc, "DC$LNK", DC_LNK, 2);
print_word (NULL, dc, "DC$CMD", DC_CMD, 2);
print_word (NULL, dc, "DC$RSB", DC_RSB, 2);
print_word (NULL, dc, "DC$FCT", DC_FCT, 2);
print_word (NULL, dc, "DC$STA", DC_STA, 2);
print_word (NULL, dc, "DC$DUC", DC_DUC, 2);
print_word (NULL, dc, "DC$0CQ", DC_0CQ, 2);
print_word (NULL, dc, "DC$STC", DC_STC, 0);
print_word (NULL, dc, "DC$1CQ", DC_1CQ, 2);
print_word (NULL, dc, "DC$ICT", DC_ICT, 14);
print_word (NULL, dc, "DC$RTY", DC_RTY, 1);
print_word (NULL, dc, "DC$RBS", DC_RBS, 1);
print_word (NULL, dc, "DC$IRT", DC_IRT, 32);
print_word (NULL, dc, "DC$THP", DC_THP, 2);
print_word (NULL, dc, "DC$LRB", DC_LRB, 2);
print_word (NULL, dc, "DC$RSV", DC_RSV, 4);
print_word (NULL, dc, "DC$PAD", DC_PAD, 16);
}

t_bool online = FALSE;

t_stat ks_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
uint32 ks = ks_dib.cma;
uint32 port = KS_S0;
uint32 tq;

if (GETW (ks + port) != 0) {                            /* service queue valid? */
    tq = GETP (ks + port);                              /* get service queue pointer */
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    PUTW (ks + port, 0);                                /* clear service queue pointer */
    kg_send (tq, ks + port);                            /* send drive state area to service queue */
    sim_printf ("Drive state area sent to service queue\n");
    }
return SCPE_OK;
}

/* Online */

uint16 ks_online (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: ONLINE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
online = TRUE;
return 002;
}

/* Disconnect */

uint16 ks_disc (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: DISCONNECT\n");
ks_mode = 0;
online = FALSE;
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Diagnose */

uint16 ks_diag (uint32 cmd, uint32 rsp)
{
uint8 test = GETW (cmd + 002);                          /* drive resident test to run */
sim_printf ("SDI command: DIAGNOSE, test = %02X\n", test);
PUTW (rsp, 0xFC);                                       /* response opcode */
return 004;
}

/* Drive clear */

uint16 ks_clear (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: DRIVE CLEAR\n");
ks_mode = 0;
online = FALSE;
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Get common characteristics */

uint16 ks_getchar (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: GET COMMON CHARACTERISTICS\n");
PUTW (rsp + 000, 0x7100 | 0x78);                        /* response opcode */
PUTW (rsp + 002, 0xD400 | 0x33);                        /* short TO / SDI version */
PUTW (rsp + 004, 0xD400 | 0x61);                        /* transfer rate */
PUTW (rsp + 006, 0xD400 | 0x57);                        /* long TO / retry # */
PUTW (rsp + 010, 0xD400 | 0x80 | 4);                    /* RCT copies / reserved, 0x80 = 576 byte mode allowed */
PUTW (rsp + 012, 0xD400 | 0x00);                        /* error recovery levels */
PUTW (rsp + 014, 0xD400 | 0x02);                        /* ECC error threshold */
PUTW (rsp + 016, 0xD400 | 0x0A);                        /* software revision */
PUTW (rsp + 020, 0xD400 | 0x00);                        /* hardware revision, 0x80 = limited diag support */
PUTW (rsp + 022, 0xD400 | 0x7A);                        /* unique drive ID #1 */
PUTW (rsp + 024, 0xD400 | 0x0A);
PUTW (rsp + 026, 0xD400 | 0x00);                        /* unique drive ID #2 */
PUTW (rsp + 030, 0xD400 | 0x00);
PUTW (rsp + 032, 0xD400 | 0x00);                        /* unique drive ID #3 */
PUTW (rsp + 034, 0xD400 | 0x00);
PUTW (rsp + 036, 0xD400 | 1);                           /* drive type identifier */
PUTW (rsp + 040, 0xD400 | 0x74);                        /* drive revs/s */
PUTW (rsp + 042, 0xD400 | 0x00);
PUTW (rsp + 044, 0xD400 | 0x00);
PUTW (rsp + 046, 0xD400 | 0x00);
PUTW (rsp + 050, 0xD400 | 0x00);
PUTW (rsp + 052, 0xD400 | 0x00);
PUTW (rsp + 054, 0xD400 | 0x00);
PUTW (rsp + 056, 0xB200 | 0x00);
return 030;
}

/* Get subunit characteristics */

uint16 ks_getsubchar (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: GET SUBUNIT CHARACTERISTICS\n");
PUTW (rsp + 0000, 0x7100 | 0x77);                       /* response opcode */
PUTW (rsp + 0002, 0xD400 | 0x13);                       /* low order LBN space in cylinders */
PUTW (rsp + 0004, 0xD400 | 0x01);
PUTW (rsp + 0006, 0xD400 | 0x00);                       /* high order LBN space in cylinders */
PUTW (rsp + 0010, 0xD400 | 0x00);                           /* cyl # */
PUTW (rsp + 0012, 0xD400 | 2);                          /* groups/cylinder */
PUTW (rsp + 0014, 0xD400 | 0x00);                           /* start LBN / start XBN */
PUTW (rsp + 0016, 0xD400 | 14);                         /* tracks/group */
PUTW (rsp + 0020, 0xD400 | 0x00);                           /* start RBN / start DBN */
PUTW (rsp + 0022, 0xD400 | 1);                          /* RBNs/track */
PUTW (rsp + 0024, 0xD400 | 0x00);                           /* reserved */
PUTW (rsp + 0026, 0xD400 | 11);                         /* data preamble size */
PUTW (rsp + 0030, 0xD400 | 4);                          /* header preamble size */
PUTW (rsp + 0032, 0xD400 | 0x50);                       /* media type low */
PUTW (rsp + 0034, 0xD400 | 0x10);
PUTW (rsp + 0036, 0xD400 | 0x64);                       /* media type high */
PUTW (rsp + 0040, 0xD400 | 0x25);
PUTW (rsp + 0042, 0xD400 | 0xE0);                       /* FCT copy size in XBNs */
PUTW (rsp + 0044, 0xD400 | 0x01);
PUTW (rsp + 0046, 0xD400 | 31);                         /* LBNs/track (16-bit) */
PUTW (rsp + 0050, 0xD400 | 16);                         /* group offset (16-bit) */
PUTW (rsp + 0052, 0xD400 | 0x9C);                       /* LBNs in host area low (16-bit) */
PUTW (rsp + 0054, 0xD400 | 0x9E);
PUTW (rsp + 0056, 0xD400 | 0x03);                       /* LBNs in host area high (16-bit) */
PUTW (rsp + 0060, 0xD400 | 0x00);
PUTW (rsp + 0062, 0xD400 | 0xD1);                       /* RCT size in LBNs (16-bit) */
PUTW (rsp + 0064, 0xD400 | 0x01);
PUTW (rsp + 0066, 0xD400 | 0x00);                       /* LBNs/track (18-bit) */
PUTW (rsp + 0070, 0xD400 | 0x00);                       /* group offset (18-bit) */
PUTW (rsp + 0072, 0xD400 | 0x00);                       /* LBNs in host area low (18-bit) */
PUTW (rsp + 0074, 0xD400 | 0x00);
PUTW (rsp + 0076, 0xD400 | 0x00);                       /* LBNs in host area high (18-bit) */
PUTW (rsp + 0100, 0xD400 | 0x00);
PUTW (rsp + 0102, 0xD400 | 0x00);                       /* RCT size in LBNs (18-bit) */
PUTW (rsp + 0104, 0xD400 | 0x00);
PUTW (rsp + 0106, 0xD400 | 0x02);                       /* XBN space in cylinders */
PUTW (rsp + 0110, 0xD400 | 0x00);
PUTW (rsp + 0112, 0xD400 | 0x01);                       /* DBN readonly groups */
PUTW (rsp + 0114, 0xD400 | 0x02);                       /* DBN space in cylinders */
PUTW (rsp + 0116, 0xB200 | 0x00);
return 050;
}

/* Get status */

uint16 ks_getsts (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: GET STATUS\n");
PUTW (rsp + 000, 0x7100 | 0xF6);                        /* response opcode */
PUTW (rsp + 002, 0xD400 | 0x00);                        /* unit number (low) */
PUTW (rsp + 004, 0xD400 | UMSK_U1);                     /* unit number (high) / subunit mask */
PUTW (rsp + 006, 0xD400 | REQ_SR | REQ_PS | REQ_RU);    /* request byte */
PUTW (rsp + 010, 0xD400 | ks_mode);                     /* mode byte */
PUTW (rsp + 012, 0xD400 | 0x00);                        /* error byte */
PUTW (rsp + 014, 0xD400 | 0x00);                        /* controller byte */
PUTW (rsp + 016, 0xD400 | 0x00);                        /* retry-fail code */
PUTW (rsp + 020, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 022, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 024, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 026, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 030, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 032, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 034, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 036, 0xB200 | 0x00);                        /* end of frame */
return 020;
}

/* Read memory */

uint16 ks_rdmem (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: READ MEMORY\n");
PUTW (rsp, 0x72);                                       /* response opcode */
return 030;
}

/* Recalibrate */

uint16 ks_recal (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: RECALIBRATE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Seek */

uint16 ks_seek (uint32 cmd, uint32 rsp)
{
uint32 cyl, grp;
cyl = GETB (cmd + 004);
cyl = cyl << 8;
cyl = cyl | GETB (cmd + 002);
grp = GETB (cmd + 006);
sim_printf ("SDI command: SEEK, cyl = %d, grp = %d\n", cyl, grp);
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 ks_chmode (uint32 cmd, uint32 rsp)
{
uint8 msk, val;
val = GETB (cmd + 002);                                 /* new mode value */
msk = GETB (cmd + 004);                                 /* mask */
ks_mode = (ks_mode & ~msk) | (val & msk);
sim_printf ("SDI command: CHANGE MODE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

uint16 ks_chflag (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI command: CHANGE CONTROLLER FLAGS\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* SDI level 2 command */

uint16 ks_cmd (uint32 cmd, uint32 rsp)
{
uint8 opc = GETB (cmd);                                 /* get SDI opcode */
uint16 rsz = 0;

switch (opc) {
    case 0x81:                                          /* CHANGE MODE */
        rsz = ks_chmode (cmd, rsp);
        break;

    case 0x82:                                          /* CHANGE CONTROLLER FLAGS */
        rsz = ks_chflag (cmd, rsp);
        break;

    case 0x03:                                          /* DIAGNOSE */
        rsz = ks_diag (cmd, rsp);
        break;

    case 0x84:                                          /* DISCONNECT */
        rsz = ks_disc (cmd, rsp);
        break;

    case 0x05:                                          /* DRIVE CLEAR */
        rsz = ks_clear (cmd, rsp);
        break;

    case 0x87:                                          /* GET COMMON CHARACTERISTICS */
        rsz = ks_getchar (cmd, rsp);
        break;

    case 0x88:                                          /* GET SUBUNIT CHARACTERISTICS */
        rsz = ks_getsubchar (cmd, rsp);
        break;

    case 0x09:                                          /* GET STATUS */
        rsz = ks_getsts (cmd, rsp);
        break;

    case 0x0A:                                          /* SEEK */
        rsz = ks_seek (cmd, rsp);
        break;

    case 0x8B:                                          /* ONLINE drive */
        rsz = ks_online (cmd, rsp);
        break;

    case 0x8D:                                          /* READ MEMORY */
        rsz = ks_rdmem (cmd, rsp);
        break;

    case 0x8E:                                          /* RECALIBRATE */
        rsz = ks_recal (cmd, rsp);
        break;

    default:
        sim_printf ("SDI level 2 command: opc = 0x%02X\n", opc);
        PUTW (rsp, 0x7E);                               /* response opcode */
        rsz = 2;
        break;
        }
#if 0
PUTB (rsp + 1, 0x71);                                   /* start of frame */
for (int i = 3; i < rsz; i = i + 2)
    PUTB (rsp + i, 0xD4);                               /* continue frame */
PUTW (rsp + rsz, 0xB200);                               /* end of frame */
#endif
return rsz;
}

/* Get realtime drive status */

void ks_rtds (uint32 rsp)
{
uint16 sts = 0;
sim_printf ("LINE STATUS\n");
if (online)
    sts = RTDS_RWR;                                     /* READ/WRITE READY */
else
    sts = RTDS_AVL | RTDS_ATN | RTDS_RR;                /* AVAILABLE, RECEIVER READY */
PUTW (rsp, sts);
}

void ks_process_dcb (uint32 ksa)
{
uint32 dc, sq, eq, rsp, opc, rsz, tq, cmd, tmp;

for (int j = 0; j < 20; j++) {
    dc = GETP (ksa + KSA_HEAD);                         /* get DCB */
    if (dc == 0)
        break;

    sq = GETP (dc + DC_0CQ);                            /* get success queue */
    if (sq == 0)
        break;

    if (Q_LOCKED (sq))                                  /* success queue locked? */
        break;

    eq = GETP (dc + DC_1CQ);                            /* get error queue */
    if (eq == 0)
        break;

    if (Q_LOCKED (eq))                                  /* error queue locked? */
        break;

    dc = kg_receive (ksa + KSA_HEAD);                   /* dequeue from drive state area */

    print_dcb (dc);
//    for (int i = 0; i < 17; i++)
//        sim_printf ("%07o: %06o\n", dc + (i << 1), GETW (dc + (i << 1)));

    rsp = GETP (dc + DC_RSB);                           /* get response buffer */
    opc = GETW (dc + DC_STA);
    rsz = 0;
    tq = sq;                                            /* target is success queue */
    switch (opc & 0177400) {
        case 014400:                                    /* ECHO DATA */
            sim_printf ("ECHO DATA\n");
            cmd = GETW (dc + DC_CMD);                   /* get immediate */
            PUTW (rsp, cmd);                            /* copy to response */
            break;
        case 010400:                                    /* LINE STATUS */
            ks_rtds (rsp);
            break;
        default:
            cmd = GETP (dc + DC_CMD);                   /* get command buffer */
            if (cmd)
                rsz = ks_cmd (cmd, rsp);
            else {
                sim_printf ("No command pointer\n");
                PUTW (rsp, 0x7E);                       /* write response word */
                }
            break;
        }
    tmp = GETW (dc + DC_FCT);
    tmp = tmp - rsz;
    PUTW (dc + DC_FCT, tmp);                            /* set response length */
    sim_printf ("----\n");

    kg_send (tq, dc);                                   /* send to completion queue */
    }
}

void read_fct (uint32 dma)
{
PUTDW (dma + 0000, FMT_512);                            /* bytes/sector */
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

void read_rct (uint32 dma)
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

const uint16 dbn[3] = {
    0155555,
    0133333,
    0066666
    };

void read_dbn (uint32 dma)
{
uint32 ptn = 0;

PUTDW (dma + 0000, 0177777);
for (int i = 0; i < 256; i++) {
    PUTDW (dma + 0002 + (i << 1), dbn[ptn++]);
    if (ptn == 3)
        ptn = 0;
    }
PUTDW (dma + 01000, 030206);                            /* TODO: See ILDISK:51440 for alogrithm */
}

void read_lbn (uint32 dma)
{
PUTDW (dma + 01000, 0105);                              /* TODO: See ILDISK:51440 for alogrithm */
}

void process_frb (uint32 frb, uint32 cyl, uint32 sct)
{
uint32 bd, scnt, tmp, dma, lbn, trk, rte, opc;
uint32 ks = ks_dib.cma;

lbn = GETB (frb + F_HLBN);                              /* high LBN */
lbn = lbn << 16;
lbn = lbn | GETW (frb + F_LLBN);                        /* low LBN */
trk = GETW (frb + F_TRK);                               /* track */
scnt = GETB (frb + F_SCNT);                             /* sector count */
rte = GETP (frb + F_RTE);                               /* get route */
opc = GETB (rte + V_OPCD);                              /* get opcode from route */

print_frb (frb);
sim_printf ("FRB: %07o, LBN: %d, CYL: %d, TRK: %d, SCT: %d, SCNT: %d\n", frb, lbn, cyl, trk, sct, scnt);
bd = GETP (frb + F_BFHD);
if (bd == 0) {                                         /* no data buffer? */
    for (int i = 0; i < scnt; i++) {
        bd = GETP (ks_dib.cma + KG_FBU);               /* get a free data memory buffer */
        bd = kg_receive (bd);
        dma = GETW (bd + 002);
        dma = (dma << 6) & DMEMMASK;
        sim_printf ("DMEM addr = %o\n", dma);

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

        if ((cyl == 275) && (trk == 0) && (sct == 31)) {  /* FCT */
            read_fct (dma + B_DATA);
            }
        else if ((cyl == 273) && (trk == 8) && (sct == 31)) { /* RCT */
            read_rct (dma + B_DATA);
            }
        else if (cyl == 278) { /* DBN */
            read_dbn (dma + B_DATA);
            }
        else
            read_lbn (dma + B_DATA);

        kg_send (frb + F_BFHD, bd);                     /* queue buffer to the FRB */
        }
    }
else {
    /* This is used for writing */
    for (int k = 0; k < scnt; k++) {
        if (bd == 0)
            break;
        sim_printf ("FBU: %07o (old)\n", bd);
        tmp = GETP (bd);
        if (tmp == bd) {
            sim_printf ("Circular FBU reference at %07o\n", bd);
            break;
            }
        bd = tmp;
        }
    }

if ((cyl == 273) && ((ks_mode & MODE_OB) == 0))
    kg_route_frb (ks, frb, FALSE);
else
    kg_route_frb (ks, frb, TRUE);
}

void ks_process_drat (uint32 ksh, uint32 kfl)
{
uint32 dt, cyl, fq, cnt, cq, frb, fc, flg;

dt = GETP (ksh + Q_HEAD);                               /* get DRAT */
if (dt == 0)
    return;

//for (int i = 0; i < 24; i++)
//    sim_printf ("%07o: %06o\n", dt + (i << 1), GETW (dt + (i << 1)));
//print_drat (dt);
//sim_printf ("DRAT: %07o\n", dt);

cyl = GETW (dt + DT_CYL);                               /* get current cylinder */
fq = dt - (32 * 4);                                     /* 32 sectors/track, 4 bytes for queue head/tail */
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
    sim_printf ("%d FRBs pre-processed\n", fc);

fc = 0;
for (int k = 0; k < 32; k++) {                          /* for each sector */
    for (int j = 0; j < 50; j++) {                      /* prevent endless loop */
        frb = kg_receive (fq + (k << 2));               /* get next FRB */
        if (frb == 0)
            break;
        PUTP (frb + F_DRAT, dt);
        process_frb (frb, cyl, k);
        fc++;
        if (cnt > 0)
            cnt = cnt - 1;
        sim_printf ("DRAT count %d -> %d\n", (cnt + 1), cnt);
        }
    }
if (fc > 0)
    sim_printf ("%d FRBs processed\n", fc);
//if (cnt < GETW (dt + DT_CNT)) {
//    for (int i = 0; i < 24; i++)
//        sim_printf ("%07o: %06o\n", dt + (i << 1), GETW (dt + (i << 1)));
//    sim_printf ("DRAT: %07o\n", dt);
//    }

PUTW (dt + DT_CNT, cnt);                                /* update counter */

if (cnt == 0) {                                         /* all operation complete? */
    dt = kg_receive (ksh + Q_HEAD);                     /* dequeue DRAT */
    cq = GETP (dt + DT_CQ);                             /* get DRAT completion queue */
    kg_send (cq, dt);                                   /* send DRAT to completion queue */
    flg = GETW (kfl);
    flg = flg | 0100000;                                /* FIXME: This is expected by ILDISK */
    PUTW (kfl, flg);
    sim_printf ("DRAT completed\n");
    }
}

t_stat ks_process (uint32 port)
{
uint32 ksa, ksh, kfl;

ksa = ks_dib.cma + KS_S0 + (port * 16);                 /* drive state area */
ks_process_dcb (ksa);

ksh = ks_dib.cma + KS_0HD + (port * 4);                 /* DRAT queue */
kfl = ks_dib.cma - KS_0FL - (port * 4);                 /* port flags */
ks_process_drat (ksh, kfl);

return SCPE_OK;
}

t_stat ks_svc (UNIT *uptr)
{
uint32 ks = ks_dib.cma;
uint32 port = 0;
uint32 ksa, tq, dc;

ksa = ks_dib.cma + KS_S0 + (port * 16);                 /* drive state area */
if (GETW (ks + KG_RSL) == 0) {                          /* test started? */
    PUTW (ks + KG_RSL, 1);                              /* KG$RSL - result of self test */
    dc = kg_receive (ksa + KSA_HEAD);                   /* get DCB from drive state area */
    tq = GETP (dc + 014);                               /* get destination queue */
    kg_send (tq, dc);                                   /* put DCB to destination */
    }
else if (!Q_LOCKED (ksa + KSA_LOCK)) {                  /* drive state area locked? */
    if ((GETW (ksa + KSA_FLGS) & 0100000) == 0)         /* drive not known to system? */
        ks_run (uptr, 0, NULL, NULL);                   /* notify drive present */
    ks_process (port);
    }
sim_clock_coschedule (uptr, tmxr_poll);                 /* reactivate */
return SCPE_OK;
}

t_stat ks_reset (DEVICE *dptr)
{
return SCPE_OK;
}
