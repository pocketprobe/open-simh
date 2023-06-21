/* hsc_kh.c: HSC K.ci simulation

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
#include "dec_ci.h"

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
#define CB_SRS          0030
#define CB_DRS          0032
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
    { "REG",    DBG_REG },
    { "WARN",   DBG_WRN },
    { "REQID",  DBG_REQID },
    { "SCSDG",  DBG_SCSDG },
    { "SCSMSG", DBG_SCSMSG },
    { "PPDDG",  DBG_PPDDG },
    { "BLKTF",  DBG_BLKTF },
    { "LCMD",   DBG_LCMD },
    { "CONN",   DBG_CONN },
    { "TRACE",  DBG_TRC,       "trace routine calls" },
    { "PKT",    DBG_PKT },
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
    sim_debug (DBG_TRC, &kh_dev, "K.ci startup: KH = %06o at %o\n", kh_dib.cma, (uint16)inst_pc);
    PUTW (kh_dib.cma + KH_NAD, node);                   /* KH$NAD - CI node address */
    PUTW (kh_dib.cma + KG_VRS, (3 << 8) | 73);          /* KG$VRS - version (DS = 3, MC = 73) */
    if (!run)
        return SCPE_OK;
    ci_set_state (&kh_unit, PORT_INIT);
    ci_set_state (&kh_unit, PORT_ENABLED);
    }
else {
    sim_debug (DBG_TRC, &kh_dev, "K.ci halt at %o\n", (uint16)inst_pc);
    ci_set_state (&kh_unit, PORT_UNINIT);
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
pkt->data[PPD_STATUS] = 0;                              /* status: OK */
pkt->data[PPD_OPC] = OPC_RETID;                         /* set opcode */
if (ci_check_vc (&kh_unit, pkt->data[PPD_PORT])) {      /* don't send ID once VC is open? */
    CI_PUT32 (pkt->data, 0x10, 1);                      /* transaction ID low (MBZ to trigger START) */
    }
else {
    CI_PUT32 (pkt->data, 0x10, 0);                      /* transaction ID low (MBZ to trigger START) */
    }
CI_PUT32 (pkt->data, 0x14, 0);                          /* transaction ID high (MBZ to trigger START) */
CI_PUT32 (pkt->data, 0x18, (CI_DUALPATH | CI_HSC));     /* remote port type */
CI_PUT32 (pkt->data, 0x1C, 0x03060D22);                 /* code revision */
CI_PUT32 (pkt->data, 0x20, 0xFFFF0D00);                 /* function mask */
pkt->data[0x24] = kh_unit.ci_node;                      /* resetting port */
// TODO: should also respond when disabled?
pkt->data[0x25] = 0x4;                                  /* port state (maint = 0, state = enabled) */
pkt->length = 0x26;  // TODO: check length, add #define
return ci_send_ppd (&kh_unit, pkt);
}

/* Block data received */

t_stat kh_receive_data (CI_PKT *pkt)
{
uint32 bct, len, bd, dma, frb;
uint32 kh = kh_dib.cma;

frb = CI_GET32 (pkt->data, PPD_RBNAM);
bct = CI_GET32 (pkt->data, PPD_XFRSZ);
len = (bct > CI_MAXDAT) ? CI_MAXDAT : bct;

sim_debug (DBG_TRC, &kh_dev, "FRB: %07o, receive data\n", frb);

bd = GETP (kh + KG_FBU);                                /* get a free data memory buffer */
bd = kg_receive (bd);
dma = GETW (bd + 002);
dma = (dma << 6) & DMEMMASK;                            /* data memory address */
dma = dma + B_DATA;                                     /* point to sector data */
//sim_printf ("DMEM addr = %o\n", dma);
memcpy (&DM[dma >> 1], &pkt->data[CI_DATHDR], len);     /* copy to packet buffer */
kg_send (frb + F_BFHD, bd);

if (pkt->data[PPD_FLAGS] & PPD_LP)                      /* last packet? */
    kg_route_frb (&kh_unit, kh, frb, TRUE);
return SCPE_OK;
}

/* Confirmation received */

t_stat kh_cnfrec (CI_PKT *pkt)
{
uint32 frb;
uint32 kh = kh_dib.cma;

frb = CI_GET32 (pkt->data, PPD_SBNAM);
kg_route_frb (&kh_unit, kh, frb, TRUE);
return SCPE_OK;
}

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
uint32 node = pkt->data[PPD_PORT];
uint32 kh = kh_dib.cma;
uint32 sb, cb, cq;
uint32 hm, dma, opc, lof, len, rcc;

opc = pkt->data[PPD_OPC] & ~OPC_M_RECV;

sb = GETP (kh - (32 * 2) + (node * 2));                 /* get system block for source node */
if (sb == 0)
    return SCPE_OK;
cb = GETP (sb + SB_NCB) - CB_TLK;                       /* get connection block */
for (;;) {
    if (cb == 0)                                        /* end of list? */
        return SCPE_OK;                                 /* no valid connection */
    srccon = GETW (cb + CB_CID);
    srccon = srccon | (GETW (cb + CB_CID + 002) << 16);
    dstcon = GETW (cb + CB_VTI) | (GETW (cb + CB_INC) << 16);
    if ((srccon == CI_GET32 (pkt->data, SCS_SRCCON)) &&
        (dstcon == CI_GET32 (pkt->data, SCS_DSTCON)))
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
    sim_printf ("No buffer for incoming packet\n");
    return SCPE_OK;
    }

rcc = GETW (cb + CB_RCC);
rcc = rcc + CI_GET16 (pkt->data, SCS_CREDIT);
PUTW (cb + CB_RCC, rcc);
PUTW (hm + HM_CTX, GETW (cb + CB_CTX));
PUTP (hm + HM_CB, cb);                                 /* save connection block */
PUTP (hm + HM_CQ, 0);
PUTW (hm + HM_LOF, lof);

memcpy (&CM[(hm + HM_MSG) >> 1], &pkt->data[SCS_APPL], len);
kg_print_struct (NULL, &kh_unit, hm, hmm_stc);

kg_send (cq, hm);

return SCPE_OK;
}

/* New incoming packet received (internal version) */

t_stat kh_receive_int (CI_PKT *pkt)
{
uint32 kh = kh_dib.cma;
uint32 hm, dma, opc, lof, len, sb, node, cq;
uint16 dg_type = CI_GET16 (pkt->data, PPD_MTYPE) & DG_MTYPE;
uint16 scs_type = CI_GET16 (pkt->data, PPD_STYPE);

opc = pkt->data[PPD_OPC] & ~OPC_M_RECV;
node = pkt->data[PPD_PORT];

if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
    len = pkt->length - PPD_HDR;
else
    len = pkt->length - PPD_HDR + 2;

if (opc == OPC_SNDMSG) {
    if ((dg_type == DG_SCSMSG) && (scs_type == SCS_APPMSG))
        return kh_appmsg (pkt);
    if ((kh_req[node]) && (dg_type == DG_SCSMSG) && (scs_type & 0x1)) {  /* SCS response packet? */
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
    hm = kg_receive (kh + KH_DRR);                     /* datagram */
    lof = 0044000 | 077;
    }

if (hm == 0) {
    sim_printf ("No buffer for incoming packet\n");
    return SCPE_OK;
    }

PUTW (hm + HM_CQ, 0);
PUTW (hm + HM_LOF, lof);
PUTB (hm + HM_CB + 00, node);                          /* source */
PUTB (hm + HM_CB + 01, ~node);                         /* complement */

if (((lof & 077) != 077) && (opc == OPC_SNDMSG)) {
//    sim_printf ("Receive packet to control memory\n");
    PUTB (hm + HM_MSG + 000, opc);
    PUTB (hm + HM_MSG + 001, pkt->data[PPD_FLAGS]);
    if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
        memcpy (&CM[(hm + HM_MSG + 002) >> 1], &pkt->data[PPD_MTYPE], (pkt->length - PPD_MTYPE));
    else
        memcpy (&CM[(hm + HM_MSG + 002) >> 1], &pkt->data[PPD_LENGTH], (pkt->length - PPD_LENGTH));
    kg_print_struct (NULL, &kh_unit, hm, hmm_stc);
    }
else {
    PUTW (hm + HM_BC, len);
    dma = GETW (hm + HM_BMB);                          /* data mem addr */
    dma = (dma << 6);

    if (!ADDR_IS_DMEM (dma)) {
        sim_printf ("Invalid data memory address %o in packet %o\n", dma, hm);
        return SCPE_IERR;
        }
    dma = dma & DMEMMASK;
//    sim_printf ("Receive packet to data memory: %o\n", dma);

    PUTDB (dma + 000, opc);
    PUTDB (dma + 001, pkt->data[PPD_FLAGS]);
    if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
        memcpy (&DM[(dma + 002) >> 1], &pkt->data[PPD_MTYPE], (pkt->length - PPD_MTYPE));
    else
        memcpy (&DM[(dma + 002) >> 1], &pkt->data[PPD_LENGTH], (pkt->length - PPD_LENGTH));
    kg_print_struct (NULL, &kh_unit, hm, hmd_stc);
    }

if ((kh_req[node]) && (dg_type == DG_SCSMSG) && (scs_type & 0x1)) {  /* pending request? */
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
if (pkt->data[PPD_PORT] == ci_get_node (uptr))          /* from local node? */
    return SCPE_OK;                                     /* ignore */
switch (pkt->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_IDREC:
        if (ci_check_vc (uptr, pkt->data[PPD_PORT]))    /* VC open? */
            return SCPE_OK;                             /* discard */
        break;

    case OPC_CNFREC:
        return kh_cnfrec (pkt);

    case OPC_DATREC:
        return kh_receive_data (pkt);

    case OPC_REQREC:
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

pkt.data[PPD_PORT] = GETB (hm + HM_CB);

sim_debug (DBG_TRC, &kh_dev, "Host message: %07o\n", hm);

if ((opc == 020000) || (opc == 040000)) {               /* SCS application message */
    cb = GETP (hm + HM_CB);                             /* get connection block */
    pkt.data[PPD_PORT] = GETB (cb + CB_NOD);
    srccon = GETW (cb + CB_CID);
    srccon = srccon | (GETW (cb + CB_CID + 002) << 16);
    dstcon = GETW (cb + CB_VTI) | (GETW (cb + CB_INC) << 16);
    pkt.data[PPD_OPC] = OPC_SNDMSG;
    pkt.data[PPD_FLAGS] = 0;
    CI_PUT16 (pkt.data, PPD_MTYPE, DG_SCSMSG);
    CI_PUT16 (pkt.data, SCS_MTYPE, SCS_APPMSG);
    CI_PUT16 (pkt.data, SCS_CREDIT, 1);
    CI_PUT32 (pkt.data, SCS_DSTCON, srccon);
    CI_PUT32 (pkt.data, SCS_SRCCON, dstcon);
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
    CI_PUT16 (pkt.data, PPD_LENGTH, len + SCS_APPL - PPD_MSGHDR);
    len = len + SCS_APPL;

    rcc = GETW (cb + CB_RCC);                           /* adjust credits */
    PUTW (cb + CB_RCC, rcc - 1);
    }
else {
    if (len == 077) {                                   /* packet in data memory */
        len = GETW (hm + HM_BC);                        /* actual packet length */
        dma = GETW (hm + HM_BMB);
        dma = (dma << 6) & DMEMMASK;                    /* data memory address */
        pkt.data[PPD_OPC] = GETDB (dma + 000);          /* opcode */
        pkt.data[PPD_FLAGS] = GETDB (dma + 001);
        if (pkt.data[PPD_OPC] <= OPC_SNDMSG)            /* datagram or message? */
            off = PPD_MTYPE;
        else
            off = PPD_LENGTH;
        memcpy (&pkt.data[off], &DM[(dma + 002) >> 1], len - 2);
        kg_print_struct (NULL, &kh_unit, hm, hmd_stc);
        }
    else {
        pkt.data[PPD_OPC] = GETB (hm + HM_MSG);         /* opcode */
        pkt.data[PPD_FLAGS] = GETB (hm + HM_MSG + 001);
        if (pkt.data[PPD_OPC] <= OPC_SNDMSG)            /* datagram or message? */
            off = PPD_MTYPE;
        else
            off = PPD_LENGTH;
        memcpy (&pkt.data[off], &CM[(hm + HM_MSG + 002) >> 1], len - 2);
        kg_print_struct (NULL, &kh_unit, hm, hmm_stc);
        }

    if (pkt.data[PPD_OPC] <= OPC_SNDMSG) {              /* datagram or message? */
        CI_PUT16 (pkt.data, PPD_LENGTH, len - 2);
        len = len + PPD_HDR;                            /* length field + header - opcode and flags */
        }
    else
        len = len + PPD_HDR - 2;                        /* header - opcode and flags */
    }

if (opc == 034000)                                      /* send datagram path A */
    pkt.data[PPD_FLAGS] |= (PPD_PS0 << PPD_V_PS);
else if (opc == 054000)                                 /* send datagram path B */
    pkt.data[PPD_FLAGS] |= (PPD_PS1 << PPD_V_PS);
else
    pkt.data[PPD_FLAGS] |= (PPD_PSAUTO << PPD_V_PS);    /* path auto? */
pkt.length = len;
r = ci_send_ppd (&kh_unit, &pkt);

if (opc == 020000)                                      /* FIXME: APPMSG always goes to SRR? */
    *cq = kh + KH_SRR;
if (opc == 040000) {
    dx = GETP (hm + HM_DX);                             /* get deallocation context */
    kg_dec_ctr (&kh_unit, dx + DX_CTR);                 /* another operation complete */
    *cq = kh + KH_SRR;
    }
if (*cq == 0) {
    if (GETW (hm + HM_CTX) == 0) {
        if (pkt.data[PPD_OPC] == OPC_SNDMSG)
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
uint32 hm, cb, con, bnm, bct, roff, soff, len, bd, dma;
uint32 kh = kh_dib.cma;
CI_PKT pkt;
t_stat r;

sim_debug (DBG_TRC, &kh_dev, "FRB: %07o, send data\n", frb);

hm = GETP (frb + F_PCTR);                               /* get host message */
cb = GETP (hm + HM_CB);                                 /* get connection block */

pkt.data[PPD_PORT] = GETB (cb + CB_NOD);                /* dest port */
pkt.data[PPD_STATUS] = 0;                               /* status */
pkt.data[PPD_OPC] = OPC_SNDDAT;                         /* opcode */
// TODO: Need to set flags (multiple)
pkt.data[PPD_FLAGS] = 0;                                /* flags */

con = GETL (hm + HM_CON);
bnm = GETL (hm + HM_BNM);
bct = GETL (hm + HM_BCT);
roff = GETL (hm + HM_OFF);
soff = GETL (frb + F_OFF);

CI_PUT32 (pkt.data, PPD_LCONID, con);                   /* local connection ID */
CI_PUT32 (pkt.data, PPD_RSPID, 1);                      /* local response ID */
CI_PUT32 (pkt.data, PPD_SBNAM, frb);                    /* send buffer name */
CI_PUT32 (pkt.data, PPD_RBNAM, bnm);                    /* receive buffer name */

bd = frb + F_BFHD;                                      /* initial buffer descriptor */
while (bct) {
    len = (bct > CI_MAXDAT) ? CI_MAXDAT : bct;
    pkt.length = len + CI_DATHDR;
    CI_PUT32 (pkt.data, PPD_XFRSZ, bct);                /* total transfer size */
    CI_PUT32 (pkt.data, PPD_SBOFF, soff);               /* send buffer offset */
    CI_PUT32 (pkt.data, PPD_RBOFF, roff);               /* receive buffer offset */
    if (bct <= CI_MAXDAT)
        pkt.data[PPD_FLAGS] |= PPD_LP;                  /* last packet */
    bd = GETP (bd);                                     /* get next buffer descriptor */
    dma = GETW (bd + 002);
    dma = (dma << 6) & DMEMMASK;                        /* data memory address */
    dma = dma + B_DATA;                                 /* point to sector data */
    memcpy (&pkt.data[CI_DATHDR], &DM[dma >> 1], len);  /* copy to packet buffer */
    r = ci_send_ppd (&kh_unit, &pkt);                   /* send to host */
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

pkt.length = CI_DATHDR;
pkt.data[PPD_PORT] = GETB (cb + CB_NOD);                /* dest port */
pkt.data[PPD_STATUS] = 0;                               /* status */
pkt.data[PPD_OPC] = OPC_REQDAT;                         /* opcode */
// TODO: Need to set flags (multiple, last packet)
pkt.data[PPD_FLAGS] = 0;                                /* flags */

con = GETL (hm + HM_CON);
bnm = GETL (hm + HM_BNM);
bct = GETL (hm + HM_BCT);
roff = GETL (hm + HM_OFF);
soff = GETL (frb + F_OFF);

CI_PUT32 (pkt.data, PPD_LCONID, con);                   /* local connection ID */
CI_PUT32 (pkt.data, PPD_RSPID, 1);                      /* local response ID */
CI_PUT32 (pkt.data, PPD_XFRSZ, bct);                    /* total transfer size */
CI_PUT32 (pkt.data, PPD_SBNAM, bnm);                    /* send buffer name */
CI_PUT32 (pkt.data, PPD_SBOFF, soff);                   /* send buffer offset */
CI_PUT32 (pkt.data, PPD_RBNAM, frb);                    /* receive buffer name */
CI_PUT32 (pkt.data, PPD_RBOFF, roff);                   /* receive buffer offset */
r = ci_send_ppd (&kh_unit, &pkt);
if (r != SCPE_OK)
    sim_printf ("K.ci stalled during REQDAT operation\n");
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
else
    sim_printf ("Unknown route opcode for K.ci: %o\n", opc);
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

for (int i = 0; i < 20; i++) {                          /* avoid endless loop */
    r = ci_receive_ppd (uptr, &pkt);                    /* get next packet */
    // TODO: handle errors from ci_receive
    if (r != SCPE_OK)
        break;
    r = kh_receive (uptr, &pkt);                        /* process packet */
    if (r != SCPE_OK)
        return r;
    };

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

while (cb != 0) {
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
for (int i = 0; i < CI_MAX_NODES; i++) {
    kh_req[i] = 0;
    kh_vcf[i] = 0;
    }
return ci_port_reset (dptr);
}
