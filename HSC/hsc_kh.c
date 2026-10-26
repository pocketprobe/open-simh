/* hsc_kh.c: HSC K.ci simulation

   Copyright (c) 2023-2024, Matt Burke

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
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"

/* K.ci control area */

#define KH_LNK          0000
#define KH_FLG          0002
#define KH_3WK          0004                            /* work queues */
#define KH_2WK          0010
#define KH_1WK          0014
#define KH_0WK          0020
#define KH_EQ           0024
#define KH_SRR          0030                            /* sequence receive buffers */
#define KH_DRR          0034                            /* datagram receive buffers */
#define KH_TLK          0040
#define KH_D2T          0140
#define KH_CIM          0142                            /* CI manager queue pointer */
#define KH_D1T          0144
#define KH_VA           0146
#define KH_D0T          0150
#define KH_CVS          0152
#define KH_TMR          0154                            /* timer */
#define KH_BFR          0160
#define KH_NAD          0164                            /* node address */
#define KH_PDS          0166
#define KH_KDS          0167
#define KH_MBA          0170
#define KH_MXD          0171
#define KH_BSV          0172
#define KH_ABQ          0174

static STDEF kh_stc[] = {
    { "KH$LNK", KH_LNK, 2, ST_INT },
    { "KH$FLG", KH_FLG, 2, ST_INT },
    { "KH$3WK", KH_3WK, 4, ST_INT },
    { "KH$2WK", KH_2WK, 4, ST_INT },
    { "KH$1WK", KH_1WK, 4, ST_INT },
    { "KH$0WK", KH_0WK, 4, ST_INT },
    { "KH$EQ ", KH_EQ,  4, ST_INT },
    { "KH$SRR", KH_SRR, 4, ST_INT },
    { "KH$DRR", KH_DRR, 4, ST_INT },
    { "KH$TLK", KH_TLK, 0, ST_LNK, kg_stc },
    { "KH$D2T", KH_D2T, 2, ST_INT },
    { "KH$CIM", KH_CIM, 2, ST_INT },
    { "KH$D1T", KH_D1T, 2, ST_INT },
    { "KH$VA ", KH_VA,  2, ST_INT },
    { "KH$D0T", KH_D0T, 2, ST_INT },
    { "KH$CVS", KH_CVS, 2, ST_INT },
    { "KH$TMR", KH_TMR, 4, ST_INT },
    { "KH$BFR", KH_BFR, 4, ST_INT },
    { "KH$NAD", KH_NAD, 2, ST_INT },
    { "KH$PDS", KH_PDS, 1, ST_INT },
    { "KH$KDS", KH_KDS, 1, ST_INT },
    { "KH$MBA", KH_MBA, 1, ST_INT },
    { "KH$MXD", KH_MXD, 1, ST_INT },
    { "KH$BSV", KH_BSV, 2, ST_INT },
    { "KH$ABQ", KH_ABQ, 2, ST_INT },
    { NULL }
    };

/* host message block */

#define HM_LNK          0000                            /* link to next */
#define HM_DUO          0002
#define HM_CTX          0002                            /* context? */
#define HM_CQ           0004                            /* completion queue */
#define HM_DX           0004                            /* pointer to DX$ structure */
#define HM_ERR          0006
#define HM_LOF          0010                            /* length, opcode, flags */
#define HM_CB           0012                            /* connection block */
#define HM_MSG          0014                            /* CI message */

static STDEF hm_stc[] = {
    { "HM$LNK", HM_LNK, 2, ST_INT },
    { "HM$DUO", HM_DUO, 0, ST_INT },
    { "HM$CTX", HM_CTX, 2, ST_INT },
    { "HM$CQ ", HM_CQ,  0, ST_INT },
    { "HM$DX ", HM_DX,  2, ST_INT },
    { "HM$ERR", HM_ERR, 2, ST_INT },
    { "HM$LOF", HM_LOF, 2, ST_INT },
    { "HM$CB ", HM_CB,  2, ST_INT },
    { NULL }
    };

static STDEF hmm_stc[] = {
    { "HM$LNK", HM_LNK, 0, ST_LNK, hm_stc },
    { "HM$MSG", HM_MSG, 12, ST_INT },
    { NULL }
    };

/* host message block - data */

#define HM_BC           0014                            /* byte count */
#define HM_CN           0016
#define HM_BMB          0020                            /* block message buffer? */

static STDEF hmd_stc[] = {
    { "HM$LNK", HM_LNK, 0, ST_LNK, hm_stc },
    { "HM$BC ", HM_BC,  2, ST_INT },
    { "HM$CN ", HM_CN,  2, ST_INT },
    { "HM$BMB", HM_BMB, 12, ST_INT },
    { NULL }
    };

/* host message block - virtual circuit */

#define HM_CF           0014                            /* function */
#define HM_CN           0016                            /* node */
#define HM_VCF          0020                            /* virtual circuit flags */
#define HM_MK           0022                            /* mask */
#define HM_TB           0024

static STDEF hmvc_stc[] = {
    { "HM$LNK", HM_LNK, 0, ST_LNK, hm_stc },
    { "HM$CF ", HM_CF,  2, ST_INT },
    { "HM$VCF", HM_VCF, 2, ST_INT },
    { "HM$MK ", HM_MK,  2, ST_INT },
    { "HM$TB ", HM_TB,  2, ST_INT },
    { NULL }
    };

/* host message block - counters */

#define HM_AKA          0020                            /* ACKs path A */
#define HM_NKA          0024                            /* NAKs path A */
#define HM_NRA          0030                            /* no response path A */
#define HM_AKB          0034                            /* ACKs path A */
#define HM_NKB          0040                            /* NAKs path A */
#define HM_NRB          0044                            /* no response path A */
#define HM_DGD          0050                            /* discarded datagrams */
#define HM_RDR          0054                            /* discarded RTNDATs */

static STDEF hmct_stc[] = {
    { "HM$LNK", HM_LNK, 0, ST_LNK, hm_stc },
    { "HM$AKA", HM_AKA, 4, ST_INT },
    { "HM$NKA", HM_NKA, 4, ST_INT },
    { "HM$NRA", HM_NRA, 4, ST_INT },
    { "HM$AKB", HM_AKB, 4, ST_INT },
    { "HM$NKB", HM_NKB, 4, ST_INT },
    { "HM$NRB", HM_NRB, 4, ST_INT },
    { "HM$DGD", HM_DGD, 4, ST_INT },
    { "HM$RDR", HM_RDR, 4, ST_INT },
    { NULL }
    };

/* host message block - transfers */

#define HM_CR           0000                            /* counter queue */
#define HM_BCT          0030                            /* byte count */
#define HM_OFF          0034                            /* buffer offset */
#define HM_BNM          0040                            /* buffer name */
#define HM_CON          0044                            /* connection ID */
#define HM_BLK          0050
#define HM_SCT          0060
#define HM_SLB          0064
#define HM_RSB          0064
#define HM_RPL          0066
#define HM_ISC          0070
#define HM_RF           0070
#define HM_SCA          0072
#define HM_OMD          0074
#define HM_OBC          0076
#define HM_CST          0102
#define HM_VUP          0102
#define HM_DCH          0104
#define HM_SCQ          0104
#define HM_CCX          0104
#define HM_XTN          0106
#define HM_FLG          0106
#define HM_CFL          0107
#define HM_XFL          0110
#define HM_DUB          0112
#define HM_SCX          0114
#define HM_RTN          0116

static STDEF hmxf_stc[] = {
    { "HM$CR ", HM_CR,  4, ST_INT },
    { "HM$DX ", HM_DX,  2, ST_INT },
    { "UNUSED", 0006,   2, ST_INT },
    { "HM$LOF", HM_LOF, 2, ST_INT },
    { "HM$CB ", HM_CB,  2, ST_INT },
    { "UNUSED", 0014,  12, ST_INT },
    { "HM$BCT", HM_BCT, 4, ST_INT },
    { "HM$OFF", HM_OFF, 4, ST_INT },
    { "HM$BNM", HM_BNM, 4, ST_INT },
    { "HM$CON", HM_CON, 4, ST_INT },
    { "HM$BLK", HM_BLK, 4, ST_INT },
    { "UNUSED", 0054,   4, ST_INT },
    { "HM$SCT", HM_SCT, 4, ST_INT },
    { "HM$SLB", HM_SLB, 0, ST_INT },
    { "HM$RSB", HM_RSB, 2, ST_INT },
    { "HM$RPL", HM_RPL, 2, ST_INT },
    { "HM$ISC", HM_ISC, 0, ST_INT },
    { "HM$RF ", HM_RF,  2, ST_INT },
    { "HM$SCA", HM_SCA, 2, ST_INT },
    { "HM$OMD", HM_OMD, 2, ST_INT },
    { "HM$OBC", HM_OBC, 4, ST_INT },
    { "HM$CST", HM_CST, 0, ST_INT },
    { "HM$VUP", HM_VUP, 2, ST_INT },
    { "HM$DCH", HM_DCH, 0, ST_INT },
    { "HM$SCQ", HM_SCQ, 0, ST_INT },
    { "HM$CCX", HM_CCX, 2, ST_INT },
    { "HM$XTN", HM_XTN, 0, ST_INT },
    { "HM$FLG", HM_FLG, 1, ST_INT },
    { "HM$CFL", HM_CFL, 1, ST_INT },
    { "HM$XFL", HM_XFL, 2, ST_INT },
    { "HM$DUB", HM_DUB, 2, ST_INT },
    { "HM$SCX", HM_SCX, 2, ST_INT },
    { "HM$RTN", HM_RTN, 2, ST_INT },
    { NULL }
    };

/* system block */

#define SB_LNK          0000                            /* link to next */
#define SB_CBN          0002
#define SB_CBQ          0004                            /* connection block queue? */
#define SB_NCB          0010                            /* connection block pointer */
#define SB_CNT          0012                            /* connection count */
#define SB_FL2          0013
#define SB_SCS          0014                            /* SCS request message buffer? */
#define SB_BMB          0016                            /* block message buffer? */
#define SB_TIM          0022
#define SB_PAS          0023
#define SB_VCS          0024                            /* virtual circuit state */
#define SB_FLG          0026
#define SB_NOD          0030                            /* node */
#define SB_NDC          0031                            /* node complement */
#define SB_MDG          0032                            /* max datagram size? */
#define SB_MDM          0034                            /* max message size? */
#define SB_DID          0036                            /* SCS system ID */
#define SB_MID          0044
#define SB_NAM          0050                            /* node name */
#define SB_IOC          0060
#define SB_PRO          0062

static STDEF sb_stc[] = {
    { "SB$LNK", SB_LNK, 2, ST_INT },
    { "SB$CBN", SB_CBN, 2, ST_INT },
    { "SB$CBQ", SB_CBQ, 4, ST_INT },
    { "SB$NCB", SB_NCB, 2, ST_INT },
    { "SB$CNT", SB_CNT, 1, ST_INT },
    { "SB$FL2", SB_FL2, 1, ST_INT },
    { "SB$SCS", SB_SCS, 2, ST_INT },
    { "SB$BMB", SB_BMB, 4, ST_INT },
    { "SB$TIM", SB_TIM, 1, ST_INT },
    { "SB$PAS", SB_PAS, 1, ST_INT },
    { "SB$VCS", SB_VCS, 2, ST_INT },
    { "SB$FLG", SB_FLG, 2, ST_INT },
    { "SB$NOD", SB_NOD, 1, ST_INT },
    { "SB$NDC", SB_NDC, 1, ST_INT },
    { "SB$MDG", SB_MDG, 2, ST_INT },
    { "SB$MDM", SB_MDM, 2, ST_INT },
    { "SB$DID", SB_DID, 6, ST_INT },
    { "SB$MID", SB_MID, 4, ST_INT },
    { "SB$NAM", SB_NAM, 8, ST_STR },
    { "SB$IOC", SB_IOC, 2, ST_INT },
    { "SB$PRO", SB_PRO, 1, ST_INT },
    { NULL }
    };

/* connection block */

#define CB_LNK          0000                            /* link to next */
#define CB_REF          0002
#define CB_SND          0004
#define CB_STA          0006                            /* connection state */
#define CB_BLK          0007                            /* connection block state */
#define CB_ACT          0010
#define CB_TYP          0011
#define CB_CMX          0012
#define CB_TLK          0014                            /* link to next connection */
#define CB_DIR          0016
#define CB_CID          0020                            /* source connection ref */
#define CB_VTI          0024                            /* destination connection ref (low) */
#define CB_INC          0026                            /* destination connection ref (high) */
#define CB_SRS          0030                            /* source reason (for reject or disconnect) */
#define CB_DRS          0032                            /* destination reason (for reject or disconnect) */
#define CB_MSC          0034                            /* minimum send credits */
#define CB_RCC          0036                            /* receive credit count */
#define CB_MRC          0040                            /* minimum receive credits */
#define CB_PC           0042                            /* pending receive credits */
#define CB_RQC          0044                            /* requested credits */
#define CB_RTC          0046                            /* returned credits */
#define CB_RN           0050                            /* cancel credit requests */
#define CB_SB           0052                            /* back pointer to system block */
#define CB_ED           0054
#define CB_EM           0056
#define CB_CTX          0060
#define CB_NOD          0062                            /* node */
#define CB_SVR          0064                            /* server queue pointer? */
#define CB_SIU          0066
#define CB_PRN          0070                            /* remote process name */
#define CB_REQ          0110
#define CB_ICT          0114
#define CB_DDL          0120
#define CB_DDF          0122
#define CB_CRN          0124
#define CB_CMI          0126
#define CB_CMA          0127
#define CB_CDF          0130
#define CB_CCG          0131
#define CB_SMX          0132
#define CB_STO          0134
#define CB_VCT          0136
#define CB_DBG          0137

static STDEF cb_stc[] = {
    { "CB$LNK", CB_LNK, 2, ST_INT },
    { "CB$REF", CB_REF, 2, ST_INT },
    { "CB$SND", CB_SND, 2, ST_INT },
    { "CB$STA", CB_STA, 1, ST_INT },
    { "CB$BLK", CB_BLK, 1, ST_INT },
    { "CB$ACT", CB_ACT, 1, ST_INT },
    { "CB$TYP", CB_TYP, 1, ST_INT },
    { "CB$CMX", CB_CMX, 2, ST_INT },
    { "CB$TLK", CB_TLK, 2, ST_INT },
    { "CB$DIR", CB_DIR, 2, ST_INT },
    { "CB$CID", CB_CID, 4, ST_INT },
    { "CB$VTI", CB_VTI, 2, ST_INT },
    { "CB$INC", CB_INC, 2, ST_INT },
    { "CB$SRS", CB_SRS, 2, ST_INT },
    { "CB$DRS", CB_DRS, 2, ST_INT },
    { "CB$MSC", CB_MSC, 2, ST_INT },
    { "CB$RCC", CB_RCC, 2, ST_INT },
    { "CB$MRC", CB_MRC, 2, ST_INT },
    { "CB$PC ", CB_PC,  2, ST_INT },
    { "CB$RQC", CB_RQC, 2, ST_INT },
    { "CB$RTC", CB_RTC, 2, ST_INT },
    { "CB$RN ", CB_RN,  2, ST_INT },
    { "CB$SB ", CB_SB,  2, ST_INT },
    { "CB$ED ", CB_ED,  2, ST_INT },
    { "CB$EM ", CB_EM,  2, ST_INT },
    { "CB$CTX", CB_CTX, 2, ST_INT },
    { "CB$NOD", CB_NOD, 2, ST_INT },
    { "CB$SVR", CB_SVR, 2, ST_INT },
    { "CB$SIU", CB_SIU, 2, ST_INT },
    { "CB$PRN", CB_PRN, 16, ST_STR },
    { "CB$REQ", CB_REQ, 4, ST_INT },
    { "CB$ICT", CB_ICT, 4, ST_INT },
    { "CB$DDL", CB_DDL, 2, ST_INT },
    { "CB$DDF", CB_DDF, 2, ST_INT },
    { "CB$CRN", CB_CRN, 2, ST_INT },
    { "CB$CMI", CB_CMI, 1, ST_INT },
    { "CB$CMA", CB_CMA, 1, ST_INT },
    { "CB$CDF", CB_CDF, 1, ST_INT },
    { "CB$CCG", CB_CCG, 1, ST_INT },
    { "CB$SMX", CB_SMX, 2, ST_INT },
    { "CB$STO", CB_STO, 2, ST_INT },
    { "CB$VCT", CB_VCT, 1, ST_INT },
    { "CB$DBG", CB_DBG, 1, ST_INT },
    { NULL }
    };

/* Fragment request block - K.ci */

#define F_SFLG          0054
#define F_SBFH          0054
#define F_CHO           0056
#define F_SSCT          0056
#define F_QSLT          0060
#define F_CB            0064
#define F_NODE          0066
#define F_NBFR          0070
#define F_SCR           0070
#define F_BFCT          0072

static STDEF frb_stc[] = {
    { "F$LINK", F_LINK, 2, ST_INT },
    { "F$PCTR", F_PCTR, 2, ST_INT },
    { "F$RTE ", F_RTE , 2, ST_INT },
    { "F$MOD ", F_MOD , 2, ST_INT },
    { "F$CTRL", F_CTRL, 2, ST_INT },
    { "F$OFF ", F_OFF , 4, ST_INT },
    { "F$LLBN", F_LLBN, 2, ST_INT },
    { "F$RT1 ", F_RT1 , 2, ST_INT },
    { "F$HLBN", F_HLBN, 2, ST_INT },
    { "F$SCT ", F_SCT , 0, ST_INT },
    { "F$RT2 ", F_RT2 , 2, ST_INT },
    { "F$TRK ", F_TRK , 1, ST_INT },
    { "UNUSED", 027,    1, ST_INT },
    { "F$GRP ", F_GRP , 0, ST_INT },
    { "F$RT3 ", F_RT3 , 2, ST_INT },
    { "F$RSB ", F_RSB , 2, ST_INT },
    { "F$SCNT", F_SCNT, 1, ST_INT },
    { "UNUSED", 035,    1, ST_INT },
    { "F$SRTE", F_SRTE, 2, ST_INT },
    { "F$BFHD", F_BFHD, 2, ST_INT },
    { "F$BFTL", F_BFTL, 2, ST_INT },
    { "UNUSED", 044,    8, ST_INT },
    { "F$SBFH", F_SBFH, 0, ST_INT },
    { "F$SFLG", F_SFLG, 2, ST_INT },
    { "F$SSCT", F_SSCT, 0, ST_INT },
    { "F$CHO ", F_CHO , 2, ST_INT },
    { "F$QSLT", F_QSLT, 2, ST_INT },
    { "UNUSED", 062,    2, ST_INT },
    { "F$CB  ", F_CB  , 2, ST_INT },
    { "F$NODE", F_NODE, 2, ST_INT },
    { "F$SCR ", F_SCR , 0, ST_INT },
    { "F$NBFR", F_NBFR, 2, ST_INT },
    { "F$BFCT", F_BFCT, 2, ST_INT },
    { "F$DSTR", F_DSTR, 2, ST_INT },
    { "F$ESUM", F_ESUM, 2, ST_INT },
    { "UNUSED", 0100,   8, ST_INT },
    { "F$CRUN", F_CRUN, 1, ST_INT },
    { "F$CFLG", F_CFLG, 1, ST_INT },
    { "UNUSED", 0112,   6, ST_INT },
    { NULL }
    };


extern int32 tmxr_poll;                                 /* calibrated delay */

uint32 kh_req[CI_MAX_NODES];                            /* pending request buffers */
uint32 kh_vcf[CI_MAX_NODES];                            /* virtual circuit flags */
uint8  kh_rstnode;
static CI_PKT kh_hold_pkt;                             /* held incoming SCS message */
static t_bool kh_hold_valid;                           /* a message is parked for re-delivery */
static uint32 kh_n_donated;                            /* LCMD buffer posts accepted */
static uint32 kh_n_held;                               /* packets parked for re-delivery */
static uint32 kh_n_svc;                                /* kh_svc activations */

t_stat kh_svc (UNIT *uptr);
t_stat kh_reset (DEVICE *dptr);
t_stat kh_init (uint16 req, uint16 act);
t_stat kh_poll (DEVICE *dptr, uint32 addr, int32 interval);
t_stat kh_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kh_show_sb (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kh_show_cb (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kh_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kh_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kh_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc);

DEBTAB kh_debug[] = {
    { "REG",   DBG_REG },
    { "WARN",  DBG_WRN },
    { "WRN",   DBG_WRN },                               /* alias - dec_ci.c spells it WRN */
    { "DIAG",  DBG_DIAG },
    { "CONF",  DBG_CONF },
    { "PPD",   DBG_PPD },
    { "SCS",   DBG_SCS },
    { "BLK",   DBG_BLK },
    { "LCMD",  DBG_LCMD },
    { "CONN",  DBG_CONN },
    { "TRACE", (DBG_TRC | KG_DBG_TRC), "trace routine calls" },
    { "PKT",   DBG_PKT },
    { "STRUCT", KG_DBG_STRUCT, "dump structures" },
    { 0 }
    };

HDIB kh_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    NULL, K_CI, 0, &kh_init, &kh_poll
    };

UNIT kh_unit = { UDATA (&kh_svc, 0, 0) };

REG kh_reg[] = {
    { NULL }
    };

MTAB kh_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL, "Backplane slot number" },
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", "ADDRESS",
      NULL, &kg_show_addr, NULL, "Control memory address" },
    { MTAB_XTD | MTAB_VDV, 0, "TYPE", NULL,
      NULL, &kh_show_ctype, NULL, "Display controller type" },
    { MTAB_XTD|MTAB_VDV, 0, "NODE", "NODE",
      &ci_set_node, &ci_show_node, NULL, "Display CI node number" },
    { MTAB_XTD|MTAB_VDV, 0, "GROUP", "GROUP",
      &ci_set_group, &ci_show_group, NULL, "Display CI group" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "LINKS", NULL,
      NULL, &ci_show_links, NULL, "Display established CI links" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "CONTROL", NULL,
      NULL, &kh_show_ctrl, NULL, "Display K.ci control memory area" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "SB", NULL,
      NULL, &kh_show_sb, NULL, "Display system block for a specific node" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "CB", NULL,
      NULL, &kh_show_cb, NULL, "Display connection blocks for a specific node" },
    { 0 }
    };

DEVICE kh_dev = {
    "KH", &kh_unit, kh_reg, kh_mod,
    1, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &kh_reset,
    NULL, &ci_attach, &ci_detach,
    &kh_dib, DEV_DEBUG | DEV_HSC | DEV_CI | DEV_DISABLE,
    0, kh_debug
    };


/* The version must be at least DS = 2, MC = 44 */

t_stat kh_init (uint16 req, uint16 act)
{
uint8 node = ci_get_node (&kh_unit);
t_bool run;
if (act == K_RUN) {
    kh_dib.cma = kg_init (&run);
    /* A K_RUN = the FW's K.ci driver restarted: every outstanding
       request lease from the previous era is dead (its peer is gone
       or the driver state was torn down).  kh_req[] is the kh's OWN
       bookkeeping, so re-initializing it here is port-side state
       hygiene, not firmware doctoring.  Witnessed: a VAX reboot
       mid-dress left leases parked against dead connections - the
       SRR drained to 0 (8009 packets held, none re-delivered) and
       the FW 007020-family died grabbing a degraded buffer. */
    for (int i = 0; i < CI_MAX_NODES; i++)
        kh_req[i] = 0;
    kh_hold_valid = FALSE;
    sim_debug (DBG_TRC, &kh_dev, "K.ci startup: KH = %06o at %o\n", kh_dib.cma, (uint16)inst_pc);
    PUTW (kh_dib.cma + KH_NAD, node);                   /* KH$NAD - CI node address */
    PUTW (kh_dib.cma + KG_VRS, (3 << 8) | 73);          /* KG$VRS - version (DS = 3, MC = 73) */
    ci_rx_enable (&kh_unit, (CI_PATH_A | CI_PATH_B));
    }
else {
    sim_debug (DBG_TRC, &kh_dev, "K.ci halt at %o\n", (uint16)inst_pc);
    /* Keep the receiver enabled: the physical K.ci's CI listener is
       always on, independent of the FW driver's state.  Disabling
       here deadlocks the cluster after any FW reboot: the FW's
       crash handler halts K.ci (witnessed: halt 14700 on the
       chaos-boot re-dress), INIPIO's re-run takes the diagnostic
       path so kg_init never re-enables, and the kh stops reading
       TCP/multicast - the VAX's port fills the socket buffer with
       queued packets and its START/IDREQ handshake wedges forever
       (the "HSC fell over" of the second-boot era). */
    ci_rx_enable (&kh_unit, (CI_PATH_A | CI_PATH_B));
    kh_dib.cma = 0;
    }
return SCPE_OK;
}

t_stat kh_poll (DEVICE *dptr, uint32 addr, int32 interval)
{
if (addr == kh_dib.cma + KH_3WK)
    sim_activate_abs (&kh_unit, interval);
else if (addr == kh_dib.cma + KH_2WK)
    sim_activate_abs (&kh_unit, interval);
else if (addr == kh_dib.cma + KH_1WK)
    sim_activate_abs (&kh_unit, interval);
else if (addr == kh_dib.cma + KH_0WK)
    sim_activate_abs (&kh_unit, interval);
return SCPE_OK;
}

/* Request ID received */

t_stat kh_request_id (CI_PKT *pkt)
{
CI_PUTB (pkt->data, CI_OPC, OPC_ID);                    /* set opcode */
if (pkt->path == CI_PATH_A)
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_0);           /* same as received path */
else
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_1);
CI_PUTL (pkt->data, CI_MAINT_ID, (ID_DUAL | ID_HSC));   /* remote port type */
CI_PUTL (pkt->data, CI_CODE_REV, 0x03060D22);           /* code revision */
CI_PUTL (pkt->data, CI_PORT_FCN, 0xFFFF0D00);           /* function mask */
CI_PUTB (pkt->data, CI_RST_PORT, kh_rstnode);           /* resetting port */
CI_PUTB (pkt->data, CI_SYS_STATE, CI_PTST_ENA);         /* port state */
pkt->length = LEN_ID;

return ci_send (&kh_unit, pkt);
}

/* Block data received */

t_stat kh_receive_data (CI_PKT *pkt)
{
uint32 bct, len, bd, dma, frb;
uint32 kh = kh_dib.cma;

frb = CI_GETL (pkt->data, CI_RBNAM);
len = pkt->length - CI_DATA;

sim_debug (DBG_TRC, &kh_dev, "FRB: %07o, receive data\n", frb);

bd = GETP (kh + KG_FBU);                                /* get a free data memory buffer */
bd = kg_receive (bd);
dma = GETW (bd + 002);
dma = (dma << 6) & DMEMMASK;                            /* data memory address */
dma = dma + B_DATA;                                     /* point to sector data */
//sim_printf ("DMEM addr = %o\n", dma);
memcpy (&DM[dma >> 1], &pkt->data[CI_DATA], len);       /* copy to data memory */
kg_send (frb + F_BFHD, bd);

if (pkt->data[CI_FLAGS] & CI_LP)                        /* last packet? */
    kg_route_frb (&kh_unit, kh, frb, TRUE);
return SCPE_OK;
}

/* Confirmation received */

t_stat kh_cnfrec (CI_PKT *pkt)
{
uint32 frb;
uint32 kh = kh_dib.cma;

frb = CI_GETL (pkt->data, CI_XCTID + 4);                /* local response ID */
kg_route_frb (&kh_unit, kh, frb, TRUE);
return SCPE_OK;
}

/* Last in-band BMB stamp seen per buffer.  The FW loads HM$BMB raw
   into a UDPA register (NXM from M.std, UDPA(4) = 051515, error reg
   151500): only [140000,160000) maps to data memory.  When the kh
   delivers M.DAT on a buffer whose current stamp is stale
   (051515/000011/000007 completion metadata), it substitutes the
   stamp the FW itself posted for that buffer.  Pure kh-side
   bookkeeping - no FW writes. */

#define KH_NSTAMP 8
uint32 kh_stamp_hm[KH_NSTAMP];                          /* buffer CM address */
uint32 kh_stamp_val[KH_NSTAMP];                         /* last in-band stamp */

void kh_stamp_remember (uint32 hm, uint32 bmb)
{
int32 i;
for (i = 0; i < KH_NSTAMP; i++) {
    if (kh_stamp_hm[i] == hm) {
        kh_stamp_val[i] = bmb;
        return;
        }
    }
for (i = 0; i < KH_NSTAMP; i++) {
    if (kh_stamp_hm[i] == 0) {
        kh_stamp_hm[i] = hm;
        kh_stamp_val[i] = bmb;
        return;
        }
    }
kh_stamp_hm[0] = hm;                                    /* evict oldest slot */
kh_stamp_val[0] = bmb;
}

uint32 kh_stamp_lookup (uint32 hm)
{
int32 i;
for (i = 0; i < KH_NSTAMP; i++) {
    if (kh_stamp_hm[i] == hm)
        return kh_stamp_val[i];
    }
return 0;
}

/* Get a free message buffer */

uint32 kh_get_srr (uint32 len)
{
uint32 kh = kh_dib.cma;
uint32 hm, bmb, sz, cn;
int32 n, p;

/* Bound the walk by the current queue depth.  The firmware mutates its
   queues concurrently, so a once-saved tail sentinel can go stale and
   the old pop/repush/compare loop would rotate forever - a guest-
   triggered simh hang.  The depth count itself is hard-capped: a
   corrupted chain with a cycle (a buffer linked on two queues) would
   otherwise spin the count loop endlessly. */

for (n = 0, p = GETP (kh + KH_SRR + Q_HEAD); p != 0 && n < 02000; p = GETP (p))
    n++;
if (p != 0)
    sim_printf ("KH: SRR free chain longer than %d or cyclic - queue corruption?\n", 02000);

for (; n > 0; n--) {
    hm = kg_receive (kh + KH_SRR);
    if (hm == 0)
        return 0;
    sim_debug (DBG_WRN, &kh_dev, "SRR walk: hm %07o LOF %06o CN %06o BMB %06o (need len %o)\n",
        hm, GETW (hm + HM_LOF), GETW (hm + HM_CN), GETW (hm + HM_BMB), len);
    bmb = GETW (hm + HM_BMB);
    if ((bmb >= 0140000) && (bmb < 0160000))            /* an in-band stamp: */
        kh_stamp_remember (hm, bmb);                    /* remember it for M.DAT */
    sz = GETW (hm + HM_LOF) & 077;
    cn = GETW (hm + HM_CN);
    if (((sz == 077) && (len >= 077)) && (cn == 2))
        return hm;
    if ((sz != 077) && (len < 077))
        return hm;
    if (n == 1) {
        kg_send (kh + KH_SRR, hm);
        return 0;
        }
    kg_send (kh + KH_SRR, hm);
    }
return 0;
}

/* Application sequenced message received */

t_stat kh_appmsg (CI_PKT *pkt)
{
uint32 srccon, dstcon;
uint32 kh = kh_dib.cma;
uint32 sb, cb, cq;
uint32 hm, dma, lof, len, rcc;
int32 n;

sb = GETP (kh - (32 * 2) + (pkt->node * 2));            /* get system block for source node */
if (sb == 0)
    return SCPE_OK;
cb = GETP (sb + SB_NCB) - CB_TLK;                       /* get connection block */
for (n = 0; n < 64; n++) {
    if (cb == 0)                                        /* end of list? */
        return SCPE_OK;                                 /* no valid connection */
    if (n == 64) {                                      /* chain never ended: cyclic or corrupt */
        sim_debug (DBG_WRN, &kh_dev, "CB chain exceeds 64 entries or is cyclic - corruption?\n");
        return SCPE_OK;
        }
    srccon = GETW (cb + CB_CID);
    srccon = srccon | (GETW (cb + CB_CID + 002) << 16);
    dstcon = GETW (cb + CB_VTI) | (GETW (cb + CB_INC) << 16);
    if ((srccon == CI_GETL (pkt->data, SCS_SRCCON)) &&
        (dstcon == CI_GETL (pkt->data, SCS_DSTCON)))
        break;                                          /* valid connection found */
    cb = GETP (cb + CB_TLK) - CB_TLK;                   /* try next connection */
    }
cq = GETP (cb + CB_SVR);                                /* get server queue */
if (cq == 0)  
    return SCPE_OK;

len = pkt->length - SCS_APPL;
hm = kh_get_srr (len);
lof = 0 | len;

if (hm == 0) {
    /* CIPA 2.1: when the sequenced free queue is empty the port HOLDS
       the packet and delivers a MSGREC later - dropping an SCS message
       loses a dress/enumerate step forever.  Park it; kh_svc re-
       delivers when a buffer frees, ahead of newer arrivals. */
    kh_hold_pkt = *pkt;
    kh_n_held++;
    kh_hold_valid = TRUE;
    sim_debug (DBG_WRN, &kh_dev, "holding incoming SCS message, SRR empty\n");
    return SCPE_OK;
    }

rcc = GETW (cb + CB_RCC);
rcc = rcc + CI_GETW (pkt->data, SCS_CREDIT);
PUTW (cb + CB_RCC, rcc);
PUTW (hm + HM_CTX, GETW (cb + CB_CTX));
PUTP (hm + HM_CB, cb);                                  /* save connection block */
PUTP (hm + HM_CQ, 0);
/* Preserve the posted LOF's format/flag byte (V8.30 posts flags 020/030,
   capacity in the low byte): zeroing it made the FW parse the delivered
   message bytes as a DATA descriptor (BC/CN/BMB) - "Invalid data memory
   address 5151500 in packet 40120".  The FW's parser follows these bits. */
PUTW (hm + HM_LOF, (GETW (hm + HM_LOF) & 0177400) | (lof & 0377));

memcpy (&CM[(hm + HM_MSG) >> 1], &pkt->data[SCS_APPL], len);
kg_print_struct (NULL, &kh_unit, hm, hmm_stc);

sim_debug (DBG_WRN, &kh_dev, "deliver appmsg hm %07o to server queue %06o (cb %06o)\n", hm, cq, cb);
    kg_send (cq, hm);

return SCPE_OK;
}

/* New incoming packet received (internal version) */

t_stat kh_receive_int (CI_PKT *pkt)
{
uint32 kh = kh_dib.cma;
uint32 hm, dma, opc, lof, len, sb, node, cq, bmb, nb;
int32 i;
uint16 dg_type = CI_GETW (pkt->data, PPD_MTYPE) & PPD_M_MTYPE;
uint16 scs_type = CI_GETW (pkt->data, SCS_MTYPE);

opc = pkt->data[CI_OPC];
node = pkt->node;
len = pkt->length - LEN_HEADER;

if (opc == OPC_MSG) {
    if ((dg_type == PPD_SCSMSG) && (scs_type == SCS_APPMSG))
        return kh_appmsg (pkt);
    if ((kh_req[node]) && (dg_type == PPD_SCSMSG) && (scs_type & 0x1)) {  /* SCS response packet? */
        hm = kh_req[node];
        lof = GETW (hm + HM_LOF);
        lof = lof & ~0074000;
        lof = lof | 0040000;
        }
    else {
        lof = 0040000;
        hm = kh_get_srr (len);
        if (len >= 0100)
            lof = lof | 077;
        else
            lof = lof | len;
        }
    }
else {
    hm = kg_receive (kh + KH_DRR);                      /* datagram */
    if (pkt->path == CI_PATH_A)
        lof = 0044000 | 077;
    else
        lof = 0064000 | 077;
    }

if (hm == 0) {
    uint16 mt = CI_GETW (pkt->data, PPD_MTYPE) & PPD_M_MTYPE;
    if ((opc == OPC_MSG) || (mt == PPD_START) || (mt == PPD_STACK)) {
        kh_hold_pkt = *pkt;                             /* sequenced messages AND the revival
                                                           handshake datagrams (START/STACK):
                                                           park for re-delivery when a buffer
                                                           frees - a dropped STACK loses the
                                                           handshake leg */
        kh_n_held++;
    kh_hold_valid = TRUE;
        sim_debug (DBG_WRN, &kh_dev, "holding incoming packet (opc %o mt %04x), buffer pool empty\n", opc, mt);
        return SCPE_OK;
        }
    sim_printf ("No buffer for incoming packet\n");    /* other datagrams: discard (CIPA 2.1) */
    return SCPE_OK;
    }

PUTW (hm + HM_CQ, 0);
PUTW (hm + HM_LOF, lof);
PUTB (hm + HM_CB + 00, node);                           /* source */
PUTB (hm + HM_CB + 01, ~node);                          /* complement */

if (((lof & 077) != 077) && (opc == OPC_MSG)) {
//    sim_printf ("Receive packet to control memory\n");
    memcpy (&CM[(hm + HM_MSG) >> 1], &pkt->data[CI_OPC], (pkt->length - CI_OPC));
    kg_print_struct (NULL, &kh_unit, hm, hmm_stc);
    }
else {
    PUTW (hm + HM_BC, len);
    bmb = GETW (hm + HM_BMB);                           /* UDPA DM descriptor */
    if ((bmb != 0) && ((bmb < 0140000) || (bmb >= 0160000))) {
        bmb = kh_stamp_lookup (hm);                     /* stale: use the FW's own */
        if (bmb == 0) {                                 /* never posted in-band? */
            PUTW (hm + HM_ERR, 004000);                 /* HME$VC - not deliverable */
            kg_send (kh + KH_SRR, hm);                  /* return the lease */
            return SCPE_OK;
            }
        PUTW (hm + HM_BMB, bmb);                        /* valid UDPA stamp */
        }
    /* HM$CN must equal the blocks the kh actually filled, and the BMB
       stamp ARRAY (HM$BMB + 2*i) must list them in the FW's order:
       descending [s, s-1, ...] (witnessed: the FW's own 2-block post =
       dma 15517700 + p3 155176; delivered HM$BMB = 155177 155176).
       Chunk i of the payload goes at (bmb0 - i) << 6.  A stale CN=3
       with a 2-block payload bugchecks HOST at 020076 (007007); a
       contiguous payload clobbers the FW's walk (NXM at M.std). */
    nb = (len + 077) >> 6;
    PUTW (hm + HM_CN, nb);
    for (i = 0; i < nb; i++) {
        int32 chunk = len - (i << 6);
        if (chunk > 0100)
            chunk = 0100;
        dma = ((bmb - i) << 6) & DMEMMASK;
        memcpy (&DM[dma >> 1], &pkt->data[CI_OPC + (i << 6)], chunk);
        PUTW (hm + HM_BMB + (i << 1), bmb - i);
        }
    kg_print_struct (NULL, &kh_unit, hm, hmd_stc);
    }

if ((kh_req[node]) && (dg_type == PPD_SCSMSG) && (scs_type & 0x1)) {  /* pending request? */
    sim_debug (DBG_TRC, &kh_dev, "Complete pending request for %07o\n", kh_req[node]);
    kh_req[node] = 0;
    }

cq = GETP (kh + KH_CIM);                                /* send to CI manager */
kg_send (cq, hm);

return SCPE_OK;
}

/* New incoming packet received */

t_stat kh_receive (UNIT *uptr, CI_PKT *pkt)
{
uint32 dg_type;

/* LOADER-E K.ci loopback microcode loaded */
if (pkt->node == ci_get_node (uptr))                    /* from local node? */
    return SCPE_OK;                                     /* ignore */
switch (pkt->data[CI_OPC]) {                            /* opcodes handled by port */
    case OPC_CNF:
        return kh_cnfrec (pkt);

    case OPC_RETDAT:
        return kh_receive_data (pkt);

    case OPC_IDREQ:
        return kh_request_id (pkt);
        }
return kh_receive_int (pkt);
}

/* Local command to get CI counters */

void kh_counters (uint32 hm, uint8 node)
{
kg_print_struct (NULL, &kh_unit, hm, hmct_stc);
/* No counter data is written into the hm.  HM_AKA aliases HM_BMB (+0020):
   the FW's resource auditor counts stampless buffers as "Resource lost to
   K.ci" (241 on every green boot), and the upstream zero-fill of
   AKA/NKA/NRA/AKB/NKB/NRB/DGD/RDR (+0020..+0056) wiped the BMB stamp on
   firmware-allocated SRR shells at every counters poll.  Counters stay
   zero - the sim has no real counter source yet, so the readback is
   unchanged. */
}

/* Local command to get/set virtual circuit flags */

void kh_set_vc (uint32 hm)
{
uint32 node, vcf, mask;

kg_print_struct (NULL, &kh_unit, hm, hmvc_stc);
node = GETW (hm + HM_CN);                               /* node */
vcf = GETW (hm + HM_VCF);                               /* new flags */
mask = GETW (hm + HM_MK);                               /* mask */

if (vcf && mask)                                        /* opening VC? */
    ci_open_vc (&kh_unit, node);
else if (mask)                                          /* closing VC? */
    ci_close_vc (&kh_unit, node);
kh_vcf[node] = (kh_vcf[node] & ~mask) | (vcf & mask);   /* update flags */
PUTW (hm + HM_VCF, kh_vcf[node]);                       /* return updated value */
}

/* Local K.ci commands */

void kh_local_cmd (uint32 hm, uint32 *cq)
{
uint32 opc, p1, p2, p3;

opc = GETB (hm + HM_MSG);
p1 = GETW (hm + HM_MSG + 002);
p2 = GETW (hm + HM_MSG + 004);
p3 = GETW (hm + HM_MSG + 006);
switch (opc) {
    case 001:                                           /* get/set virtual circuit flags */
        sim_debug (DBG_LCMD, &kh_dev, "opc = %d, node = %d, p2 = %06o, p3 = %06o, cq = %06o\n", opc, p1, p2, p3, *cq);
        kh_set_vc (hm);
        break;
    case 002:                                           /* get counters */
        kh_counters (hm, p1);
        break;
    case 003:                                           /* enqueue message buffer (SRR) */
    case 004:                                           /* enqueue datagram buffer (DRR) */
        sim_debug (DBG_LCMD, &kh_dev, "opc = %d, type = %06o, dma = %08o, p3 = %06o, cq = %06o\n", opc, p1, (p2 << 6), p3, *cq);
        sim_debug (DBG_LCMD | DBG_WRN, &kh_dev, "  item as posted: LOF %06o CTX %06o CN %06o BMB %06o BC %06o\n",
            GETW (hm + HM_LOF), GETW (hm + HM_CTX), GETW (hm + HM_CN), GETW (hm + HM_BMB), GETW (hm + HM_BC));
        kh_n_donated++;
        PUTW (hm + HM_LOF, 060077);
        break;
    default:
        sim_printf ("unknown local cmd\n");
        break;
    }
}

/* Process a host message block */

void kh_process_hmb (uint32 hm, uint32 *cq)
{
uint32 len, opc, flgs, node, dma, cb, srccon, dstcon, rcc, off, dx, val;
uint32 kh = kh_dib.cma;
CI_PKT pkt;
t_stat r = SCPE_OK;

opc = GETW (hm + HM_LOF) & 074000;
flgs = GETW (hm + HM_LOF) & 03700;
len = GETW (hm + HM_LOF) & 077;
node = GETB (hm + HM_CB);                               /* destination node */

pkt.node = GETB (hm + HM_CB);

sim_debug (DBG_TRC, &kh_dev, "Host message: %07o\n", hm);

if ((opc == 020000) || (opc == 040000)) {               /* SCS application message */
    cb = GETP (hm + HM_CB);                             /* get connection block */
    pkt.node = GETB (cb + CB_NOD);
    srccon = GETW (cb + CB_CID);
    srccon = srccon | (GETW (cb + CB_CID + 002) << 16);
    dstcon = GETW (cb + CB_VTI) | (GETW (cb + CB_INC) << 16);
    pkt.data[CI_OPC] = OPC_MSG;
    pkt.data[CI_FLAGS] = 0;
    CI_PUTW (pkt.data, PPD_MTYPE, PPD_SCSMSG);
    CI_PUTW (pkt.data, SCS_MTYPE, SCS_APPMSG);
    CI_PUTW (pkt.data, SCS_CREDIT, 1);
    CI_PUTL (pkt.data, SCS_DSTCON, srccon);
    CI_PUTL (pkt.data, SCS_SRCCON, dstcon);
    if (len == 077) {                                   /* packet in data memory */
        len = GETW (hm + HM_BC);                        /* actual packet length */
        dma = GETW (hm + HM_BMB);
        dma = (dma << 6) & DMEMMASK;                    /* data memory address */
        memcpy (&pkt.data[SCS_APPL], &DM[dma >> 1], len);
        kg_print_struct (NULL, &kh_unit, hm, hmd_stc);
        }
    else {
        memcpy (&pkt.data[SCS_APPL], &CM[(hm + HM_MSG) >> 1], len);
        kg_print_struct (NULL, &kh_unit, hm, hmm_stc);
        }
    len = len + SCS_APPL;

    rcc = GETW (cb + CB_RCC);                           /* adjust credits */
    PUTW (cb + CB_RCC, rcc - 1);
    }
else {
    if (len == 077) {                                   /* packet in data memory */
        len = GETW (hm + HM_BC);                        /* actual packet length */
        dma = GETW (hm + HM_BMB);
        dma = (dma << 6) & DMEMMASK;                    /* data memory address */
        memcpy (&pkt.data[CI_OPC], &DM[dma >> 1], len);
        kg_print_struct (NULL, &kh_unit, hm, hmd_stc);
        }
    else {
        memcpy (&pkt.data[CI_OPC], &CM[(hm + HM_MSG) >> 1], len);
        kg_print_struct (NULL, &kh_unit, hm, hmm_stc);
        }

    len = len + CI_OPC;                                 /* add length of CI header */
    }

if (opc == 034000)                                      /* send datagram path A */
    pkt.path = CI_PATH_A;
else if (opc == 054000)                                 /* send datagram path B */
    pkt.path = CI_PATH_B;
else
    pkt.path = CI_PATH_AUTO;                            /* path auto? */
pkt.length = len;
r = ci_send (&kh_unit, &pkt);

if (opc == 020000)                                      /* FIXME: APPMSG always goes to SRR? */
    *cq = kh + KH_SRR;
if (opc == 040000) {
    dx = GETP (hm + HM_DX);                             /* get deallocation context */
    kg_dec_ctr (&kh_unit, dx + DX_CTR);                 /* another operation complete */
    *cq = kh + KH_SRR;
    }
if (*cq == 0) {
    if (GETW (hm + HM_CTX) == 0) {
        if (pkt.data[CI_OPC] == OPC_MSG)
            *cq = kh + KH_SRR;
        else
            *cq = kh + KH_DRR;
        }
    else {
        kh_req[node] = hm;
        sim_debug (DBG_TRC, &kh_dev, "Start pending request for %07o\n", hm);
        }
    }
}

/* Send block data */

void kh_send_data (uint32 frb)
{
uint32 hm, cb, con, bnm, bct, roff, soff, len, bd, dma, slb;
uint32 kh = kh_dib.cma;
CI_PKT pkt;
t_stat r;

sim_debug (DBG_TRC, &kh_dev, "FRB: %07o, send data\n", frb);

hm = GETP (frb + F_PCTR);                               /* get host message */
cb = GETP (hm + HM_CB);                                 /* get connection block */

kg_print_struct (NULL, &kh_unit, hm, hmxf_stc);
pkt.node = GETB (cb + CB_NOD);                          /* dest port */
pkt.path = CI_PATH_AUTO;
pkt.data[CI_OPC] = OPC_SNTDAT;                          /* opcode */
pkt.data[CI_FLAGS] = 0;                                 /* flags */

con = GETL (hm + HM_CON);
bnm = GETL (hm + HM_BNM);
bct = GETB (hm + HM_BCT);
slb = GETB (hm + HM_SLB);                               /* same as HM_BCT? */
bct = (GETB (frb + F_SCNT) * 512) - slb;                /* use this or HM_SCT? */
soff = GETL (frb + F_OFF);
roff = GETL (hm + HM_OFF) + soff;

CI_PUTL (pkt.data, CI_XCTID + 0, con);                  /* local connection ID */
CI_PUTL (pkt.data, CI_XCTID + 4, frb);                  /* local response ID */
CI_PUTL (pkt.data, CI_RBNAM, bnm);                      /* receive buffer name */

bd = frb + F_BFHD;                                      /* initial buffer descriptor */
while (bct) {
    len = (bct > CI_MAXDAT) ? CI_MAXDAT : bct;
    pkt.length = len + CI_DATA;
    CI_PUTL (pkt.data, CI_RBOFF, roff);                 /* receive buffer offset */
    if (bct <= CI_MAXDAT)
        pkt.data[CI_FLAGS] |= CI_LP;                    /* last packet */
    bd = GETP (bd);                                     /* get next buffer descriptor */
    dma = GETW (bd + 002);
    dma = (dma << 6) & DMEMMASK;                        /* data memory address */
    dma = dma + B_DATA;                                 /* point to sector data */
    memcpy (&pkt.data[CI_DATA], &DM[dma >> 1], len);    /* copy to packet buffer */
    r = ci_send (&kh_unit, &pkt);                       /* send to host */
    if (r != SCPE_OK) {
        sim_printf ("K.ci stalled during SNDDAT operation\n");
        break;
        }
    soff += len;
    roff += len;
    bct -= len;
    }
}

/* Request block data */

void kh_request_data (uint32 frb)
{
uint32 hm, cb, con, bnm, bct, roff, soff;
uint32 kh = kh_dib.cma;
CI_PKT pkt;
t_stat r;

hm = GETP (frb + F_PCTR);                               /* get host message */
cb = GETP (hm + HM_CB);                                 /* get connection block */

kg_print_struct (NULL, &kh_unit, hm, hmxf_stc);
pkt.length = LEN_DATREQ;
pkt.node = GETB (cb + CB_NOD);                          /* dest port */
pkt.path = CI_PATH_AUTO;
pkt.data[CI_OPC] = OPC_DATREQ0;                         /* opcode */
pkt.data[CI_FLAGS] = 0;                                 /* flags */

con = GETL (hm + HM_CON);
bnm = GETL (hm + HM_BNM);
bct = GETB (frb + F_SCNT) * 512;
roff = GETL (hm + HM_OFF);
soff = GETL (frb + F_OFF) + roff;

CI_PUTL (pkt.data, CI_XCTID + 0, con);                  /* local connection ID */
CI_PUTL (pkt.data, CI_XCTID + 4, frb);                  /* local response ID */
CI_PUTL (pkt.data, CI_REQ_XFRSZ, bct);                  /* total transfer size */
CI_PUTL (pkt.data, CI_REQ_SBNAM, bnm);                  /* send buffer name */
CI_PUTL (pkt.data, CI_REQ_SBOFF, soff);                 /* send buffer offset */
CI_PUTL (pkt.data, CI_REQ_RBNAM, frb);                  /* receive buffer name */
CI_PUTL (pkt.data, CI_REQ_RBOFF, roff);                 /* receive buffer offset */
r = ci_send (&kh_unit, &pkt);
if (r != SCPE_OK)
    sim_printf ("K.ci stalled during DATREQ operation\n");
}

/* Process a fragment request block */

void kh_process_frb (uint32 frb, uint32 *cq)
{
uint32 rte, opc, bd, scnt;
uint32 kh = kh_dib.cma;

*cq = 0;
rte = GETP (frb + F_RTE);                               /* get route */
opc = GETB (rte + V_OPCD);                              /* get opcode */
kg_print_struct (NULL, &kh_unit, frb, frb_stc);
if (opc == V_SD)                                        /* send data */
    kh_send_data (frb);
else if (opc == V_RD)                                   /* request data */
    kh_request_data (frb);
else if (opc == 3)                                      /* send data? */
    kh_send_data (frb);
else if (opc == 4) {                                    /* just allocate buffers */
    scnt = GETB (frb + F_SCNT);                         /* sector count */
    for (int i = 0; i < scnt; i++) {
        bd = GETP (kh + KG_FBU);                        /* get a free data memory buffer */
        bd = kg_receive (bd);
        kg_send (frb + F_BFHD, bd);
        kg_route_frb (&kh_unit, kh, frb, TRUE);
        }
    }
else {
    /* Diagnostic-phase route probes (blank FRBs, opcodes 7/15 seen in
       the HEAD era; the gen-107 census swept 10/11/13/14/15 at ~20
       retries each): the FW's CI self-test sends no-op routes to
       verify the port's router.  Complete them - dropping one stalls
       the diagnostic phase (and spammed the console once per probe
       retry). */
    sim_debug (DBG_WRN, &kh_dev, "K.ci route opcode %o (diagnostic): completing FRB %o\n", opc, frb);
    kg_route_frb (&kh_unit, kh, frb, TRUE);
    }
}

/* Process resources from a work queue */

t_stat kh_process_queue (uint32 kh, uint32 wq)
{
uint32 cq, rsc, opc, i;

for (i = 0; i < 20; i++) {                              /* process maximum of 20 items */
    if (Q_LOCKED (wq + Q_HEAD))                         /* source queue locked */
        break;
    if (Q_EMPTY (wq + Q_HEAD))                          /* source queue empty */
        break;
    rsc = GETP (wq + Q_HEAD);                           /* get the resource pointer */
    cq = GETP (rsc + HM_CQ);                            /* get completion queue pointer */
    if (cq) {
        if (Q_LOCKED (cq))                              /* completion queue locked */
            break;
        }

    rsc = kg_receive (wq);                              /* get resource from work queue */
    opc = GETW (rsc + HM_LOF) & 074000;

    switch (opc) {
        case 060000:                                    /* local command */
            kh_local_cmd (rsc, &cq);
            break;
        case 034000:                                    /* send datagram (path A) */
        case 054000:                                    /* send datagram (path B) */
        case 074000:                                    /* send datagram (path auto?) */
        case 030000:                                    /* send message */
        case 020000:                                    /* send SCS message? */
        case 040000:                                    /* send SCS message? */
            kh_process_hmb (rsc, &cq);
            break;
        case 000000:                                    /* fragment request block */
        case 004000:
            kh_process_frb (rsc, &cq);
            break;
        default:
            sim_printf ("K.ci unknown opcode: %o\n", opc);
            sim_debug (DBG_WRN, &kh_dev, "K.ci unknown opcode %o, wq %07o hm %07o LOF %06o CTX %06o CN %06o, cq %06o%s\n",
                opc, wq, rsc, GETW (rsc + HM_LOF), GETW (rsc + HM_CTX), GETW (rsc + HM_CN), cq,
                cq ? "" : " (no cq: HM leaked)");
            sim_debug (DBG_WRN, &kh_dev, "  hm neighborhood: %06o %06o %06o %06o | %06o %06o %06o %06o\n",
                GETW (rsc - 010), GETW (rsc - 006), GETW (rsc - 004), GETW (rsc - 002),
                GETW (rsc), GETW (rsc + 002), GETW (rsc + 004), GETW (rsc + 006));
            break;
        }

    if (cq) {
        kg_send (cq, rsc);                              /* put resource to completion queue */
        }
else {
        int32 ni;
        for (ni = 0; ni < CI_MAX_NODES; ni++) {
            if (kh_req[ni] == rsc)
                break;      /* the kh's OWN parked lease: it stays
                               parked!  The park path (HM$CTX != 0)
                               does not set *cq, so the just-parked
                               shell lands here - pooling it would
                               hand the lease to the free pool while
                               kh_req still points at it (the
                               007020 crash).  The response re-
                               animates it via the reuse path. */
            }
        if (ni != CI_MAX_NODES) {
            sim_debug (DBG_TRC, &kh_dev, "cq-less parked lease %07o stays parked (node %d)\n", rsc, ni);
            }
else if ((GETW (rsc + HM_MSG + 002) == 0177776) &&
         (GETW (rsc + HM_CN) == 0177776)) {
        /* The NODE-DEATH MARKER (type=-2 AND HM$CN=-2 together) - its
           LOF class is 060000 (local-command), but it must NEVER be
           completed or pooled in any form: the FW's retrieve path
           treats a completed marker as a resource operation = the
           007020 "Failure to retrieve SCS resources" bugcheck
           (witnessed: the first two-class-rule boot).  Vanish it.
           The FW pre-writes the marker's shell off (ABQ ledger). */
        sim_debug (DBG_WRN, &kh_dev, "node death marker (NACK) consumed (shell %07o)\n", rsc);
        }
else if ((opc == 020000) || (opc == 040000) || (opc == 030000) ||
         (opc == 034000) || (opc == 054000) || (opc == 074000)) {
        /* A valid-class shell re-posted cq-less: the FW's normal way
           of handing a TX'd shell back (witnessed: LOF 030277/030224
           CONRSP- and DISCON-family shells after TX).  Return it CLEAN
           to the SRR pool (the FW's fresh-pool format: LOF/CN/BMB = 0)
           - the FW's auditor counts vanished shells as "Resource lost
           to K.ci".  FRB items (000000/004000) complete via
           kg_route_frb, not the shell pool - they stay on the vanish
           path. */
        PUTW (rsc + HM_LOF, 0);
        PUTW (rsc + HM_CN, 0);
        PUTW (rsc + HM_BMB, 0);
        kg_send (kh + KH_SRR, rsc);
        sim_debug (DBG_WRN, &kh_dev, "cq-less shell %07o returned clean to SRR (class %06o)\n", rsc, opc);
        }
else {
        /* Artifacts and FRBs: the FW pre-writes these off (the ABQ
           ledger); vanishing is the green behavior.  Count them. */
        sim_debug (DBG_WRN, &kh_dev, "cq-less item %07o vanished (wq %07o, LOF %06o)\n",
            rsc, wq, GETW (rsc + HM_LOF));
        }
        }
    }
return SCPE_OK;
}

t_stat kh_svc (UNIT *uptr)
{
uint32 kh = kh_dib.cma;
CI_PKT pkt;
t_stat r;

if (kh == 0) {                                          /* FW halted: drain the transport */
    for (int i = 0; i < 20; i++) {                      /* drop inbound; the sender's port */
        if (ci_receive (uptr, &pkt) != SCPE_OK)         /* retries; the next K_RUN resumes */
            break;
        }
    return ci_port_svc (uptr);                          /* keep the transport alive */
    }
r = kh_process_queue (kh, kh + KH_0WK);                 /* process work queues */
if (r != SCPE_OK)
    return r;
r = kh_process_queue (kh, kh + KH_1WK);
if (r != SCPE_OK)
    return r;
r = kh_process_queue (kh, kh + KH_2WK);
if (r != SCPE_OK)
    return r;
r = kh_process_queue (kh, kh + KH_3WK);
if (r != SCPE_OK)
    return r;

for (int i = 0; i < 20; i++) {
    if (kh_hold_valid) {                                /* held SCS message pending? */
        r = kh_receive_int (&kh_hold_pkt);              /* re-deliver it first (order) */
        if (kh_hold_valid)
            break;                                      /* still no buffer: stop consuming */
        continue;                                       /* delivered: pull more */
        }
    r = ci_receive (uptr, &pkt);                        /* get next packet */
    // TODO: handle errors from ci_receive
    if (r != SCPE_OK)
        break;
    r = kh_receive (uptr, &pkt);                        /* process packet */
    if (r != SCPE_OK)
        return r;
    };

if ((++kh_n_svc & 0377) == 0) {                         /* pool health: chain depths */
    int32 ns = 0, nd = 0;                               /* countable, no FW writes */
    uint32 pp;                                          /* the second-boot diagnostic */
    if (kh_dib.cma) {
    for (pp = GETP (kh + KH_SRR + Q_HEAD); (pp != 0) && (ns < 02000); pp = GETP (pp))
        ns++;
    for (pp = GETP (kh + KH_DRR + Q_HEAD); (pp != 0) && (nd < 02000); pp = GETP (pp))
        nd++;
    sim_debug (DBG_WRN, &kh_dev, "pool: SRR free %d DRR free %d donated %u held %u svc %u\n", ns, nd, kh_n_donated, kh_n_held, kh_n_svc);
    }
    }

kg_svc (uptr);
return ci_port_svc (uptr);
}

/* Show the K.ci control area */

t_stat kh_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
kg_print_struct (st, uptr, kh_dib.cma, kh_stc);
return SCPE_OK;
}

/* Show system block for a specific node */

t_stat kh_show_sb (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
const char *cptr = (const char *) desc;
uint32 kh = kh_dib.cma;
uint32 sb, node;
t_stat r;

if (cptr == NULL)
    return SCPE_ARG;

node = (uint32) get_uint (cptr, 10, CI_MAX_NODES, &r);
if (r != SCPE_OK)
    return r;

sb = GETP (kh - (32 * 2) + (node * 2));                 /* get system block for node */

if (sb == 0) {
    fprintf (st, "No system block for node %d\n", node);
    return SCPE_OK;
    }

kg_print_struct (st, uptr, sb, sb_stc);

return SCPE_OK;
}

/* Show connection block for a specific node */

t_stat kh_show_cb (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
int32 n;

const char *cptr = (const char *) desc;
uint32 kh = kh_dib.cma;
uint32 sb, cb, node;
t_stat r;

if (cptr == NULL)
    return SCPE_ARG;

node = (uint32) get_uint (cptr, 10, CI_MAX_NODES, &r);
if (r != SCPE_OK)
    return r;

sb = GETP (kh - (32 * 2) + (node * 2));                 /* get system block for node */

if (sb == 0) {
    fprintf (st, "No system block for node %d\n", node);
    return SCPE_OK;
    }

cb = GETP (sb + SB_NCB);                                /* get connection block */

if (cb == 0) {
    fprintf (st, "No connections from node %d\n", node);
    return SCPE_OK;
    }

for (n = 0; (cb != 0) && (n < 64); n++) {
    cb = cb - CB_TLK;                                   /* offset to start of CB$ */
    kg_print_struct (st, uptr, cb, cb_stc);
    fprintf (st, "---\n");

    cb = GETP (cb + CB_TLK);                            /* get next connection */
    }

return SCPE_OK;
}

/* Show controller type */

t_stat kh_show_ctype (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr = find_dev_from_unit (uptr);
HDIB *dibp = (HDIB *) dptr->ctxt;

switch (dibp->sts) {
    case K_CI:
        fprintf (st, "K.ci host interface");
        break;

    case K_CI2:
    case K_CI2_MICRO:
        fprintf (st, "K.ci2 host interface");
        break;
    }
return SCPE_OK;
}

t_stat kh_reset (DEVICE *dptr)
{
kh_rstnode = 0;
for (int i = 0; i < CI_MAX_NODES; i++) {
    kh_req[i] = 0;
    kh_vcf[i] = 0;
    }
return ci_port_reset (dptr);
}
