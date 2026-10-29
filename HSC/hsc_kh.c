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
t_bool kh_consume_item;                                 /* work item consumed with no completion */
static CI_PKT kh_hold_pkt;                             /* held incoming SCS message */
static t_bool kh_hold_valid;                           /* a message is parked for re-delivery */
static uint32 kh_n_donated;                            /* LCMD buffer posts accepted */
static uint32 kh_n_held;                               /* packets parked for re-delivery */
static uint32 kh_n_svc;                                /* kh_svc activations */
static uint32 kh_n_krun;                               /* K_RUNs witnessed - the kernel-era gate */

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
    kh_n_krun++;                                        /* the kernel-era gate: the port-status
                                                           cell (PMEM 034530) is loader workspace
                                                           until the first K_RUN - witnessed gen 121:
                                                           the ungated heartbeat stomped the loader
                                                           mid-boot and the FW BEL-spammed to death */
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

static uint32 kh_n_srr_borrows;                         /* DRR shells borrowed while SRR was dry */
static uint32 kh_n_srr_floor;                           /* receives deferred at the SRR floor */

#define KH_SRR_FLOOR    2                               /* never spend below this */

/* Get a free message buffer */

uint32 kh_get_srr (uint32 len)
{
uint32 kh = kh_dib.cma;
uint32 hm, last, sz, cn;

last = GETP (kh + KH_SRR + Q_TAIL);                     /* save last buffer address */
for (;;) {
    hm = kg_receive (kh + KH_SRR);                      /* get next buffer */
    if (hm == 0)
        break;                                          /* no buffers available */
    sz = GETW (hm + HM_LOF) & 077;                      /* get buffer size */
    cn = GETW (hm + HM_CN);                             /* FIXME */
    if (((sz == 077) && (len >= 077)) && (cn == 2))
        break;                                          /* found buffer for large packet */
    if ((sz != 077) && (len < 077))
        break;                                          /* found buffer for small packet */
    if (hm == last) {                                   /* reached last buffer? */
        hm = 0;
        break;                                          /* no buffers available */
        }
    kg_send (kh + KH_SRR, hm);                          /* put to back of queue */
    }
return hm;
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
    /* DEC's grammar (hscma_sv, "K.ci exception detected"): KHF$NH =
       "Sequenced message received while reserved-to-receive queue
       was empty" - the K.ci REPORTS the empty-ring condition in
       KH$FLG (KG+2 = the K.ci Control Area's exception word, the
       adapter's own status) instead of swallowing it silently.
       The FW's exception machinery watches this word; the FW's
       K_RUN init writes it (040000 witnessed) = it owns the reset,
       the kh owns the runtime exceptions.  Non-destructive OR. */
    PUTW (kh + KH_FLG, GETW (kh + KH_FLG) | 0100);      /* KHF$NH */
    sim_debug (DBG_WRN, &kh_dev, "holding incoming SCS message, SRR empty (KHF$NH set)\n");
    return SCPE_OK;
    }

rcc = GETW (cb + CB_RCC);
rcc = rcc + CI_GETW (pkt->data, SCS_CREDIT);
PUTW (cb + CB_RCC, rcc);
PUTW (hm + HM_CTX, GETW (cb + CB_CTX));
PUTP (hm + HM_CB, cb);                                  /* save connection block */
PUTP (hm + HM_CQ, 0);
/* Delivery format by shell class.  A native FW donation (LOF class
   060000) carries its buffer-descriptor overlay (BC/CN/BMB) exactly
   where the MSG area lives: clear it and stamp the received-sequence-
   message class (0, HM$L$RM) with the length.  The F1-era "Invalid
   data memory address 5151500 in packet 40120" = the FW reading a
   clobbered descriptor's BMB field (051515 - the message's own first
   bytes) as a DM pointer.  For the 020/030-class posts, preserve the
   posted format byte (F1) and set the length. */
/* THE GREEN'S FULL-WORD LOF (2840f70b, restored A137+1): the
   delivered shell's LOF OPCODE FIELD must read HML$RM = 0 ("received
   sequence message over connection" - the manual's 012003: "The
   Opcode specified in the HM$LOF field... not equal to HML$RM=000000";
   and the MSCP server's 004005: "a non sequenced message from a
   connection").  THE F1-ERA PRESERVE (e0acdf18: "(GETW & 0177400) |
   (lof & 0377)") = THE 004005: a TX'd shell returns from the pool
   with its TX class (LOF 020000-class = "send SCS message"!) still
   in the high bits, our length stamp lands in the low byte, and the
   delivered shell reads as a DATAGRAM-CLASS message at the MSCP
   server = "non sequenced" = 004005 (A137: the offending shell at
   CM 0o60620, LOF = 0o20044 = the TX class 0o20000 + our 0o44 stamp,
   read live at the bugcheck; the green's full-word write = LOF =
   0|len = the class CLEARED = HML$RM = 0 by construction).
   THE STALE-REMNANTS CLEAR (the 92e3ad6d observation): the pooled
   TX-remnant shell carries its own CN/BMB (witnessed: CN=2, BC=2
   riding on the delivered 0o60620) - delivering with the stale
   CN = the "wrong number of BMBs" 007007 (the DUP's check).  The
   kh owns the shell it delivers into: clear the remnant cells, then
   the full-word LOF.  The FW's serializing HMB = never delivered
   into = its CN never touched by this path. */
PUTW (hm + HM_CN, 0);
/* A142: DO NOT CLEAR HM_BMB.  The FW's drain-recycle (VA 024236)
   re-stamps LOF=060000 ONLY - it trusts CN/BMB to round-trip because
   BY THE FW'S OWN BOOKS the BMB cell is FW property ("each side's own
   books").  Zeroing it converted every appmsg-delivered shell into
   buffer-less pool filler: the pooled shells decayed to CN=0/BMB=0
   (witnessed: the t=267238M SRR walk, all four entries 060000/0/0),
   the deliverer's len-test found no usable shell ("need len 44"),
   kh hold-parked a live host message with SRR "empty" (KHF$NH), the
   FW's consumer wedged INTERLOCKED waiting for that exact message
   (the t=267241M DX$-SMQ freeze), and the poller filed 007003 twenty
   mega-ticks later with 153 HMBs stranded.  The 92e3 remnant hazard
   (007007, "wrong number of BMBs") is governed by CN ALONE - with
   CN=0 the FW never enters the BMB-reading branch.  Keep the CN
   clear, keep the FW's BMB. */
PUTW (hm + HM_LOF, lof);

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
    /* THE GREEN GRAMMAR RESTORED (2840f70b = A106, the last enumerating
       era): the datagram delivery = BC = the length + ONE contiguous DM
       write at the SHELL'S OWN HM$BMB (the FW's posted address).  THE
       F2-ERA BLOCK (eea5af2a: the stamp-lookup rescue, CN = nb, the
       descending BMB array, the 64-byte chunking) = WRITES THE GREEN
       NEVER MADE - and the CN stamp = THE 007020: the FW builds its
       VC-serializing HMB ONCE (witnessed live: LOF=030277-class, CN=2,
       BC=104, the contiguous BMB pair 154377/154376), hands the shell
       back through the port, and re-reads its own count at the next
       data command (user VA 030142: CMP #2,16(R0), caught live A135) -
       the kh's CN=nb (nb=1 for a 20-byte datagram!) CLOBBERED the FW's
       count: found=1 required=2 (the A134/A135/A136 crash: the FW's CN
       genealogy ends at its own build, the ONLY unlogged writer = the
       kh's raw PUTW path).  THE GREEN: BC = len, the payload at the
       shell's own BMB (contiguous across the pair), the FW's
       CN/LOF/BMB cells UNTOUCHED.  The M.DAT dialect's multi-block
       stamps (if ever needed) = a separate, DETECTED path - never a
       blanket stamp over the FW's live serializing HMB. */
    PUTW (hm + HM_BC, len);
    dma = GETW (hm + HM_BMB);                           /* data mem addr */
    dma = (dma << 6);

    if (!ADDR_IS_DMEM (dma)) {
        /* A flooded or stale BMB must never down the simulator.  The
           FW's dawn-boot CTBL re-init covers the K.ci control area
           (KH_SRR/KH_DRR heads read as the flood values 146314/031463)
           and an era-persistent datagram can arrive before donations
           re-prime the pools (A140's post-crash reboot: pool head
           031463 -> shell 0146314 with BMB 031463 -> dma 03146300 ->
           SCPE_IERR out of kh_svc -> "unexpected internal error", sim
           dead).  The real card raises its own exception word and
           drops the delivery; the shell is abandoned UNLINKED (not
           sent to the FW's queues with a live-stale BMB).  Never
           SCPE_IERR out of an event handler. */
        sim_debug (DBG_WRN, &kh_dev, "invalid data memory address %o in packet %o - delivery dropped (flooded or stale BMB)\n", dma, hm);
        return SCPE_OK;
        }
    dma = dma & DMEMMASK;

    memcpy (&DM[dma >> 1], &pkt->data[CI_OPC], (pkt->length - CI_OPC));
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
PUTW (hm + HM_AKA + 000, 0);                            /* ACKs path A - low word */
PUTW (hm + HM_AKA + 002, 0);                            /* ACKs path A - high word */
PUTW (hm + HM_NKA + 000, 0);                            /* NAKs path A - low word */
PUTW (hm + HM_NKA + 002, 0);                            /* NAKs path A - high word */
PUTW (hm + HM_NRA + 000, 0);                            /* No response path A - low word */
PUTW (hm + HM_NRA + 002, 0);                            /* No response path A - high word */

PUTW (hm + HM_AKB + 000, 0);                            /* ACKs path B - low word */
PUTW (hm + HM_AKB + 002, 0);                            /* ACKs path B - high word */
PUTW (hm + HM_NKB + 000, 0);                            /* NAKs path B - low word */
PUTW (hm + HM_NKB + 002, 0);                            /* NAKs path B - high word */
PUTW (hm + HM_NRB + 000, 0);                            /* No response path B - low word */
PUTW (hm + HM_NRB + 002, 0);                            /* No response path B - high word */

PUTW (hm + HM_DGD + 000, 0);                            /* Discarded Datagrams - low word */
PUTW (hm + HM_DGD + 002, 0);                            /* Discarded Datagrams - high word */
PUTW (hm + HM_RDR + 000, 0);                            /* Discarded RTNDATs - low word */
PUTW (hm + HM_RDR + 002, 0);                            /* Discarded RTNDATs - high word */
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
uint32 kh = kh_dib.cma;
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
        if ((p1 == 0177776) && (GETW (hm + HM_CN) == 0177776)) {  /* NODE DEATH MARKER (NACK):
                                                                    the marker's signature is type=-2
                                                                    AND HM$CN=-2 together (witnessed:
                                                                    shell CTX=073513 BC=3 CN=177776
                                                                    MSG= 3,-2,3,0).  The marker must
                                                                    NOT be normalized and pooled -
                                                                    the FW's retrieve path treats a
                                                                    completed marker as a resource
                                                                    operation = the 007020 'Failure
                                                                    to retrieve SCS resources'
                                                                    bugcheck (witnessed: gen 108,
                                                                    VMS configuring-devices phase). */
            kh_consume_item = TRUE;
            sim_debug (DBG_LCMD | DBG_WRN, &kh_dev, "node death marker (NACK) consumed (type %06o dma %08o)\n", p1, (p2 << 6));
            break;
            }
        kh_n_donated++;
        /* The shell keeps the FW's posted identity UNTOUCHED.  The old
           LOF=060077 rewrite forged a format onto FW-owned shells that
           re-enter FW-visible queues: the FW's server free-path then
           misfiled the shell and its attached BMB, the BMB window
           (0140000-0160000) drained to zero (the pool-control word at
           0140000 included - bit 01000 CLEAR at the crash), and the
           first 2-BMB serializing allocation bugchecked 007020
           (found=1, required=2 - the ledger row held one entry).
           The donation's native grammar is self-describing: LOF 060000,
           CN=1, BMB pointer, BC=4 (witnessed: the "item as posted" LCMD
           traces).  F1's preservation downstream was faithfully
           preserving THIS corruption. */
        /* THE DONATION ITSELF: the shell IS the buffer the FW just
           handed us (LCMD opc 3 = sequenced/SRR, opc 4 = datagram/DRR).
           Enqueue it into the port's own pool - completing it back to
           the FW instead LOST every donated buffer: the FW's auditor
           counted them as 'Resource lost to K.ci -- 241. HMBs' and the
           config-era retrieve starved on a pool that never grew
           (007020, 'Failure to retrieve SCS resources').  The FW's own
           death-row trail: empty queue sweep -> the 013400 foreign-
           class error item -> the wake -> the disable.
           kh_consume_item = TRUE as well: the handler handled the item
           completely (pooled) - a completion would ALSO queue the same
           shell into the FW's collector, double-linking it; the FW's
           re-post of a multiply-linked shell turned the SRR into a
           ring (witnessed: 'SRR free 1024' = the walk's cap spinning
           through the donation cycle, gen 111). */
        kh_consume_item = TRUE;
        /* THE SHELL'S HOME QUEUE: the donation's HM$CQ = where the FW
           wants the shell back, and the FW SORTS ITS OWN INVENTORY -
           witnessed in the LCMD traces: the CN=1 datagram shells post
           with cq = 0144434 (the DRR) while the CN=2 serializing-
           capable shells post with cq = 0144430 (the SRR).  The old
           opcode-based routing (opc3 -> SRR, opc4 -> DRR) MISFILED
           every CN=2 shell into the DRR: the FW posted 4 CN=2
           donations, the SRR walks ever saw ONE (the crash-moment
           delivery, DBG 50239501), and CIMGR's 2-BMB serializing
           allocation at the first data command (SSSC$DIRECTORY, the
           DMEM trace!) found=1 required=2 = 007020.  The latent bug
           since the pooling era - unmasked by the first data command.
           Honor the posted home; fall back to the opcode routing only
           if the cq points outside the KG region. */
        {
        uint32 home = GETP (hm + HM_CQ);                /* the FW's pointers = x4 units (kg_init's kg<<2!); GETW read the raw 031126-class word and the sanity check fell back EVERY time - the A131 walks stayed 91x CN=1 + 1x CN=2 unchanged */
        if ((GETW (hm + HM_CN) == 2) ||
            ((GETW (hm + HM_BMB) >= 0150000) && (GETW (hm + HM_BMB) < 0160000)))
            /* A151: UNION the CN==2 test with the durable big-arena
               test (the BMB value itself), hoisted over both routing
               branches.  The CIMGR retrieval gate (user VA 0o30614)
               reads LOF's low 9 bits == 0o77 as THE big-capacity
               marker, but the FW's drain-recycle (VA 024236) re-stamps
               plain 060000 - washing the marker off every recycled
               shell, so the pool's marking decays across each ~90s
               maintenance ceremony until the second ceremony's check
               comes up short ("no elements enqueued on .KHSRR" ->
               forced crash -> 007003 at the 4-minute wall, EVERY
               generation since A140).  CN decays past each delivery;
               the FW-owned BMB round-trips intact (fix #5) - let the
               marker follow the BMB, the property that never lies. */
            PUTW (hm + HM_LOF, GETW (hm + HM_LOF) | 077);
        if (((home - kh) & 0177000) == 0) {             /* a KG-region queue */
            /* THE DOUBLE'S MARKER (Dana's push): the FW's serializing
               allocation gate (the CIMGR helper at user VA 0o30614,
               decoded 2026-10-28 from the live simh) checks the LOF's
               low 9 bits == 0o77 - THE BIG-SHELL CAPACITY MARKER -
               and ONLY the 0o77-marked HMBs get their BMBs registered
               with the port (the TRAP-0o10 pushes: (6,0o30) then
               (HM$BMB, 0o70)).  Without the registration the retrieval
               comes up short: found=1 required=2 = 007020 at the
               first data command.  Both grammars honor this marker:
               the kh's own kh_get_srr tests sz == 077 for the big
               class!  The posted donations carry LOF = 0o60000 (no
               marker) - the OLD 060077 normalization KNEW this, but
               stamped EVERY shell and corrupted the FW's free-path
               (the 9e52352a scar).  THE SURGICAL VERSION: stamp 0o77
               ONLY on the CN=2 shells (the doubles!) as they enter
               the port's pool - the marker without the forgery.  The
               FW retrieves the double, the gate reads 0o77, the BMB
               registration runs, and the serializing allocation finds
               its pair. */
            kg_send (home, hm);
            }
        else
            kg_send (kh + ((opc == 3) ? KH_SRR : KH_DRR), hm);
        }
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
    if (dx)                                             /* A140: all 23 completions
                                                           carried HM$DX = 0; dec(0)
                                                           decrements CM word 2 and
                                                           can queue CM cell 0 as a
                                                           phantom counter item. */
        kg_dec_ctr (&kh_unit, dx + DX_CTR);             /* another operation complete */
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
        /* Same contract as the DATREQ path: the FW's route machinery
           needs the FRB's completion to make progress - a hung FRB
           stalls the diagnostic phase. */
        sim_debug (DBG_WRN, &kh_dev, "K.ci SNDDAT send failed - completing FRB %o\n", frb);
        kg_route_frb (&kh_unit, kh, frb, TRUE);
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
if (r != SCPE_OK) {
    /* The send failed (the boot-time route probes run before the
       transport is linked - no listener yet).  The FW's route test
       needs the FRB's COMPLETION, not the data: route it back or the
       probe hangs the diagnostic phase. */
    sim_debug (DBG_WRN, &kh_dev, "K.ci DATREQ send failed - completing FRB %o\n", frb);
    kg_route_frb (&kh_unit, kh, frb, TRUE);
    }
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
            break;
        }

    if (cq) 
        kg_send (cq, rsc);                              /* put resource to completion queue */
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

/* The doorbell-heartbeat family (the port-status writes toward PMEM
   034530 = the kernel-D VA 0150530) is RETIRED.  The cell = the FW's
   OWN process state: the relay (PMEM 0o73104) reads its own mirror,
   and the kh's memory writes poisoned the running kernel - the
   ungated build BEL-stormed the loader (gen 121), the gated builds
   stalled at INIPIO waiting for spawn-ready flags that never came
   (the CM 0127300 pending-init pointer pairs, gens 122/124).  The
   port feeds the FW through the event channels only: the LCMDs, the
   queues, the custody plumbing.  The poller's doorbell grammar
   (status codes 021 = all-clear / 023 = signal, decoded at PMEM
   0o1032xx) = the count-drift thread's territory. */
if ((++kh_n_svc & 0377) == 0) {                         /* pool health: chain depths */
    int32 ns = 0, nd = 0;                               /* countable, no FW writes */
    uint32 pp;                                          /* the second-boot diagnostic */
    if (kh_dib.cma) {
    for (pp = GETP (kh + KH_SRR + Q_HEAD); (pp != 0) && (ns < 02000); pp = GETP (pp))
        ns++;
    for (pp = GETP (kh + KH_DRR + Q_HEAD); (pp != 0) && (nd < 02000); pp = GETP (pp))
        nd++;
    int32 nm = 0;                                       /* the DM free pool (KG$FBU): */
    pp = GETP (kh + KG_FBU);                            /* the config-era 007020's economy - */
    while ((pp != 0) && (nm < 02000)) {                 /* the CIMGR's 2-BMB serializing-HMB */
        nm++;                                           /* allocation comes from HERE */
        pp = GETP (pp);
        }
    if (pp != 0)
        sim_printf ("KH: DM free chain longer than %d or cyclic\n", 02000);
    sim_debug (DBG_WRN, &kh_dev,  "pool: SRR free %d DRR free %d DM free %d donated %u held %u svc %u\n",
        ns, nd, nm, kh_n_donated, kh_n_held, kh_n_svc);
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
