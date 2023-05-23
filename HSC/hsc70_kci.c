/* hsc70_kci.c: HSC70 K.ci simulation

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
#include "hsc70_kreq.h"
#include "dec_ci.h"

/* K.ci control area */

#define KH_LNK          0000
#define KH_3WK          0004                            /* work queues */
#define KH_2WK          0010
#define KH_1WK          0014
#define KH_0WK          0020
#define KH_SRR          0030                            /* sequence receive buffers */
#define KH_DRR          0034                            /* datagram receive buffers */
#define KH_CIM          0142                            /* CI manager queue pointer */
#define KH_NAD          0164                            /* node address */

/* host message block */

#define HM_LNK          0000                            /* link to next */
#define HM_DUO          0002
#define HM_CTX          0002                            /* context? */
#define HM_CQ           0004                            /* completion queue */
#define HM_DX           0004
#define HM_ERR          0006
#define HM_LOF          0010                            /* length, opcode, flags */
#define HM_CB           0012                            /* connection block */
#define HM_MSG          0014                            /* CI message */

/* host message block - data */

#define HM_BC           0014                            /* byte count */
#define HM_CN           0016
#define HM_BMB          0020                            /* block message buffer? */

/* host message block - virtual circuit */

#define HM_CF           0014                            /* function */
#define HM_CN           0016                            /* node */
#define HM_VCF          0020                            /* virtual circuit flags */
#define HM_MK           0022                            /* mask */
#define HM_TB           0024

/* host message block - counters */

#define HM_AKA          0020                            /* ACKs path A */
#define HM_NKA          0024                            /* NAKs path A */
#define HM_NRA          0030                            /* no response path A */
#define HM_AKB          0034                            /* ACKs path A */
#define HM_NKB          0040                            /* NAKs path A */
#define HM_NRB          0044                            /* no response path A */
#define HM_DGD          0050                            /* discarded datagrams */
#define HM_RDR          0054                            /* discarded RTNDATs */

/* host message block - transfers */

#define HM_CR           000000                          /* counter queue */
#define HM_BCT          000030                          /* byte count */
#define HM_OFF          000034                          /* buffer offset */
#define HM_BNM          000040                          /* buffer name */
#define HM_CON          000044                          /* connection ID */

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

/* fragment request block */

#define F_LINK          0000
#define F_PCTR          0002
#define F_RTE           0004
#define F_MOD           0006
#define F_CTRL          0010
#define F_OFF           0012
#define F_LLBN          0016
#define F_RT1           0020
#define F_HLBN          0022
#define F_SCT           0024
#define F_RT2           0024
#define F_TRK           0026
#define F_GRP           0030
#define F_RT3           0030
#define F_RSB           0032
#define F_SCNT          0034
#define F_SRTE          0036
#define F_BFHD          0040
#define F_BFTL          0042
#define F_SBFH          0054
#define F_SFLG          0054
#define F_SSCT          0056
#define F_CHO           0056
#define F_QSLT          0060
#define F_CB            0064
#define F_NODE          0066
#define F_SCR           0070
#define F_NBFR          0070
#define F_BFCT          0072
#define F_DSTR          0074
#define F_ESUM          0076
#define F_CRUN          0110
#define F_CFLG          0111


extern int32 tmxr_poll;                                 /* calibrated delay */

uint32 kci_req[CI_MAX_NODES];                           /* pending request buffers */
uint32 kci_vcf[CI_MAX_NODES];                           /* virtual circuit flags */

t_stat kci_svc (UNIT *uptr);
t_stat kci_reset (DEVICE *dptr);
t_stat kci_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_show_sb (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_show_cb (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kci_init (uint16 req, uint16 act);

DEBTAB kci_debug[] = {
    { "REG",    DBG_REG },
    { "WARN",   DBG_WRN },
    { "REQID",  DBG_REQID },
    { "SCSDG",  DBG_SCSDG },
    { "SCSMSG", DBG_SCSMSG },
    { "PPDDG",  DBG_PPDDG },
    { "BLKTF",  DBG_BLKTF },
    { "LCMD",   DBG_LCMD },
    { "CONN",   DBG_CONN },
    { "TRACE",  DBG_TRC, "trace routine calls" },
    { "PKT",    DBG_PKT },
    { 0 }
    };

HDIB kci_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    K_CI, 0, &kci_init
    };

UNIT kci_unit = { UDATA (&kci_svc, 0, 0) };

REG kci_reg[] = {
    { NULL }
    };

MTAB kci_mod[] = {
    { MTAB_XTD|MTAB_VDV, 004, "SLOT", "SLOT",
      NULL, &kg_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      NULL, &kg_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "NODE", "NODE",
      &ci_set_node, &ci_show_node },
    { MTAB_XTD|MTAB_VDV, 0, "GROUP", "GROUP",
      &ci_set_group, &ci_show_group },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "LINKS", NULL,
      NULL, &ci_show_links, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "CONTROL", NULL,
      NULL, &kci_show_ctrl, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 004, "SB", NULL,
      NULL, &kci_show_sb, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 004, "CB", NULL,
      NULL, &kci_show_cb, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, KH_2WK, "2WK", NULL,
      NULL, &kg_show_queue, NULL, "Display work queue 2" },
    { 0 }
    };

DEVICE kci_dev = {
    "KCI", &kci_unit, kci_reg, kci_mod,
    1, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &kci_reset,
    NULL, &ci_attach, &ci_detach,
    &kci_dib, DEV_DEBUG | DEV_HSC | DEV_CI | DEV_DISABLE,
    0, kci_debug
    };


/* The version must be at least DS = 2, MC = 44 */

t_stat kci_init (uint16 req, uint16 act)
{
uint8 node = ci_get_node (&kci_unit);
t_bool run;
if (act == K_RUN) {
    kci_dib.cma = kg_init (&run);
    sim_debug (DBG_TRC, &kci_dev, "K.ci startup: KH = %06o at %o\n", kci_dib.cma, (uint16)inst_pc);
    PUTW (kci_dib.cma + KH_NAD, node);                      /* KH$NAD - CI node address */
    PUTW (kci_dib.cma + KG_VRS, (3 << 8) | 73);             /* KG$VRS - version (DS = 3, MC = 73) */
    if (!run)
        return SCPE_OK;
    ci_set_state (&kci_unit, PORT_INIT);
    ci_set_state (&kci_unit, PORT_ENABLED);
    sim_clock_coschedule (&kci_unit, tmxr_poll);            /* activate */
    }
else {
    sim_debug (DBG_TRC, &kci_dev, "K.ci halt at %o\n", (uint16)inst_pc);
    ci_set_state (&kci_unit, PORT_UNINIT);
    sim_cancel (&kci_unit);
    kci_dib.cma = 0;
    }
return SCPE_OK;
}

t_stat kci_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint32 kh = kci_dib.cma;
t_stat r;

print_word (st, kh, "KH$LNK", KH_LNK, 2);
print_word (st, kh, "KH$FLG", 0002, 2);
print_word (st, kh, "KH$3WK", KH_3WK, 4);               /* work queue 3 */
print_word (st, kh, "KH$2WK", KH_2WK, 4);               /* work queue 2 */
print_word (st, kh, "KH$1WK", KH_1WK, 4);               /* work queue 1 */
print_word (st, kh, "KH$0WK", KH_0WK, 4);               /* work queue 0 */
print_word (st, kh, "KH$EQ ", 0024, 4);                 /* error queue? */
print_word (st, kh, "KH$SRR", KH_SRR, 4);
print_word (st, kh, "KH$DRR", KH_DRR, 4);

r = kg_show_ctrl (st, uptr, val, desc);
if (r != SCPE_OK)
    return r;

print_word (st, kh, "KH$D2T", 0140, 2);
print_word (st, kh, "KH$CIM", KH_CIM, 2);               /* CI manager queue pointer? */
print_word (st, kh, "KH$D1T", 0144, 2);
print_word (st, kh, "KH$VA ", 0146, 2);
print_word (st, kh, "KH$D0T", 0150, 2);
print_word (st, kh, "KH$CVS", 0152, 2);
print_word (st, kh, "KH$TMR", 0154, 4);                 /* timer */
print_word (st, kh, "KH$BFR", 0160, 4);
print_word (st, kh, "KH$NAD", KH_NAD, 2);               /* node address */
print_word (st, kh, "KH$PDS", 0166, 1);
print_word (st, kh, "KH$KDS", 0167, 1);
print_word (st, kh, "KH$MBA", 0170, 1);
print_word (st, kh, "KH$MXD", 0171, 1);
print_word (st, kh, "KH$BSV", 0172, 2);
print_word (st, kh, "KH$ABQ", 0174, 2);                 /* abort queue? */

return SCPE_OK;
}

t_stat kci_show_sb (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
const char *cptr = (const char *) desc;
uint32 kh = kci_dib.cma;
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
print_word (st, sb, "SB$LNK", SB_LNK, 2);
print_word (st, sb, "SB$CBN", SB_CBN, 2);
print_word (st, sb, "SB$CBQ", SB_CBQ, 4);
print_word (st, sb, "SB$NCB", SB_NCB, 2);
print_word (st, sb, "SB$CNT", SB_CNT, 1);
print_word (st, sb, "SB$FL2", SB_FL2, 1);
print_word (st, sb, "SB$SCS", SB_SCS, 2);
print_word (st, sb, "SB$BMB", SB_BMB, 4);
print_word (st, sb, "SB$TIM", SB_TIM, 1);
print_word (st, sb, "SB$PAS", SB_PAS, 1);
print_word (st, sb, "SB$VCS", SB_VCS, 2);
print_word (st, sb, "SB$FLG", SB_FLG, 2);
print_word (st, sb, "SB$NOD", SB_NOD, 1);
print_word (st, sb, "SB$NDC", SB_NDC, 1);
print_word (st, sb, "SB$MDG", SB_MDG, 2);
print_word (st, sb, "SB$MDM", SB_MDM, 2);
print_word (st, sb, "SB$DID", SB_DID, 6);
print_word (st, sb, "SB$MID", SB_MID, 4);
print_string (st, sb, "SB$NAM", SB_NAM, 8);
print_word (st, sb, "SB$IOC", SB_IOC, 2);
print_word (st, sb, "SB$PRO", SB_PRO, 1);

return SCPE_OK;
}

t_stat kci_show_cb (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
const char *cptr = (const char *) desc;
uint32 kh = kci_dib.cma;
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

cb = GETP (sb + SB_NCB) - CB_TLK;                       /* get connection block */

if (cb == 0) {
    fprintf (st, "No connections from node %d\n", node);
    return SCPE_OK;
    }

while (cb != 0) {
    print_word (st, cb, "CB$LNK", CB_LNK, 2);
    print_word (st, cb, "CB$REF", CB_REF, 2);
    print_word (st, cb, "CB$SND", CB_SND, 2);
    print_word (st, cb, "CB$STA", CB_STA, 1);
    print_word (st, cb, "CB$BLK", CB_BLK, 1);
    print_word (st, cb, "CB$ACT", CB_ACT, 1);
    print_word (st, cb, "CB$TYP", CB_TYP, 1);
    print_word (st, cb, "CB$CMX", CB_CMX, 2);
    print_word (st, cb, "CB$TLK", CB_TLK, 2);
    print_word (st, cb, "CB$DIR", CB_DIR, 2);
    print_word (st, cb, "CB$CID", CB_CID, 4);
    print_word (st, cb, "CB$VTI", CB_VTI, 2);
    print_word (st, cb, "CB$INC", CB_INC, 2);
    print_word (st, cb, "CB$SRS", CB_SRS, 2);
    print_word (st, cb, "CB$DRS", CB_DRS, 2);
    print_word (st, cb, "CB$MSC", CB_MSC, 2);
    print_word (st, cb, "CB$RCC", CB_RCC, 2);
    print_word (st, cb, "CB$MRC", CB_MRC, 2);
    print_word (st, cb, "CB$PC ", CB_PC, 2);
    print_word (st, cb, "CB$RQC", CB_RQC, 2);
    print_word (st, cb, "CB$RTC", CB_RTC, 2);
    print_word (st, cb, "CB$RN ", CB_RN, 2);
    print_word (st, cb, "CB$SB ", CB_SB, 2);
    print_word (st, cb, "CB$ED ", CB_ED, 2);
    print_word (st, cb, "CB$EM ", CB_EM, 2);
    print_word (st, cb, "CB$CTX", CB_CTX, 2);
    print_word (st, cb, "CB$NOD", CB_NOD, 2);
    print_word (st, cb, "CB$SVR", CB_SVR, 2);
    print_word (st, cb, "CB$SIU", CB_SIU, 2);
    print_string (st, cb, "CB$PRN", CB_PRN, 16);
    print_word (st, cb, "CB$REQ", CB_REQ, 4);
    print_word (st, cb, "CB$ICT", CB_ICT, 4);
    print_word (st, cb, "CB$DDL", CB_DDL, 2);
    print_word (st, cb, "CB$DDF", CB_DDF, 2);
    print_word (st, cb, "CB$CRN", CB_CRN, 2);
    print_word (st, cb, "CB$CMI", CB_CMI, 1);
    print_word (st, cb, "CB$CMA", CB_CMA, 1);
    print_word (st, cb, "CB$CDF", CB_CDF, 1);
    print_word (st, cb, "CB$CCG", CB_CCG, 1);
    print_word (st, cb, "CB$SMX", CB_SMX, 2);
    print_word (st, cb, "CB$STO", CB_STO, 2);
    print_word (st, cb, "CB$VCT", CB_VCT, 1);
    print_word (st, cb, "CB$DBG", CB_DBG, 1);
    fprintf (st, "---\n");

    cb = GETP (cb + CB_TLK);                            /* get next connection */
    }

return SCPE_OK;
}

void kci_print_frb (uint32 frb)
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
print_word (NULL, frb, "UNUSED", 044, 8);
print_word (NULL, frb, "F$SBFH", F_SBFH, 0);
print_word (NULL, frb, "F$SFLG", F_SFLG, 2);
print_word (NULL, frb, "F$SSCT", F_SSCT, 0);
print_word (NULL, frb, "F$CHO ", F_CHO , 2);
print_word (NULL, frb, "F$QSLT", F_QSLT, 2);
print_word (NULL, frb, "UNUSED", 062, 2);
print_word (NULL, frb, "F$CB  ", F_CB  , 2);
print_word (NULL, frb, "F$NODE", F_NODE, 2);
print_word (NULL, frb, "F$SCR ", F_SCR , 0);
print_word (NULL, frb, "F$NBFR", F_NBFR, 2);
print_word (NULL, frb, "F$BFCT", F_BFCT, 2);
print_word (NULL, frb, "F$DSTR", F_DSTR, 2);
print_word (NULL, frb, "F$ESUM", F_ESUM, 2);
print_word (NULL, frb, "UNUSED", 0100, 8);
print_word (NULL, frb, "F$CRUN", F_CRUN, 1);
print_word (NULL, frb, "F$CFLG", F_CFLG, 1);
print_word (NULL, frb, "UNUSED", 0112, 6);
}

t_stat ci_request_id (UNIT *uptr, CI_PKT *pkt)
{
pkt->data[PPD_STATUS] = 0;                              /* status: OK */
pkt->data[PPD_OPC] = OPC_RETID;                         /* set opcode */
if (ci_check_vc (&kci_unit, pkt->data[PPD_PORT])) {     /* don't send ID once VC is open? */
    CI_PUT32 (pkt->data, 0x10, 1);                      /* transaction ID low (MBZ to trigger START) */
    }
else {
    CI_PUT32 (pkt->data, 0x10, 0);                      /* transaction ID low (MBZ to trigger START) */
    }
CI_PUT32 (pkt->data, 0x14, 0);                          /* transaction ID high (MBZ to trigger START) */
CI_PUT32 (pkt->data, 0x18, (CI_DUALPATH | CI_HSC));     /* remote port type */
CI_PUT32 (pkt->data, 0x1C, 0x03060D22);                 /* code revision */
CI_PUT32 (pkt->data, 0x20, 0xFFFF0D00);                 /* function mask */
pkt->data[0x24] = kci_unit.ci_node;                     /* resetting port */
// TODO: should also respond when disabled?
pkt->data[0x25] = 0x4;                                  /* port state (maint = 0, state = enabled) */
pkt->length = 0x26;  // TODO: check length, add #define
return ci_send_ppd (&kci_unit, pkt);
}

t_stat ci_send_data (UNIT *uptr, CI_PKT *pkt)
{
return SCPE_OK;
}

t_stat ci_receive_data (UNIT *uptr, CI_PKT *pkt)
{
return SCPE_OK;
}

uint32 kci_get_srr (uint32 len)
{
uint32 kh = kci_dib.cma;
uint32 hm, last, sz;

last = GETP (kh + KH_SRR + Q_TAIL);                     /* save last buffer address */
for (;;) {
    hm = get_from_queue (kh + KH_SRR);                  /* get next buffer */
    if (hm == 0)
        break;                                          /* no buffers available */
    sz = GETW (hm + HM_LOF) & 077;                      /* get buffer size */
    if ((sz == 077) && (len >= 077))
        break;                                          /* found buffer for large packet */
    if ((sz != 077) && (len < 077))
        break;                                          /* found buffer for small packet */
    if (hm == last) {                                   /* reached last buffer? */
        hm = 0;
        break;                                          /* no buffers available */
        }
    put_to_queue (kh + KH_SRR, hm);                     /* put to back of queue */
    }
return hm;
}

/* New incoming packet received (internal version) */

t_stat kci_receive_int (UNIT *uptr, CI_PKT *pkt, uint32 cq)
{
uint32 kh = kci_dib.cma;
uint32 rsc, dma, opc, lof, len, sb, node;
    uint16 dg_type = CI_GET16 (pkt->data, PPD_MTYPE) & DG_MTYPE;
    uint16 scs_type = CI_GET16 (pkt->data, PPD_STYPE);

opc = pkt->data[PPD_OPC] & ~OPC_M_RECV;
node = pkt->data[PPD_PORT];

if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
    len = pkt->length - PPD_HDR;
else
    len = pkt->length - PPD_HDR + 2;

if (opc == OPC_SNDMSG) {
    if ((kci_req[node]) && (dg_type == DG_SCSMSG) && (scs_type & 0x1)) {  /* SCS response packet? */
        rsc = kci_req[node];
        lof = GETW (rsc + HM_LOF);
        lof = lof & ~0074000;
        lof = lof | 0040000;
        }
    else {
        lof = 0040000;
        rsc = kci_get_srr (len);
        if (len >= 0100)
            lof = lof | 077;
        else
            lof = lof | len;
        }
    }
else {
    rsc = get_from_queue (kh + KH_DRR);                 /* datagram */
    lof = 0044000 | 077;
    }

if (rsc == 0) {
    sim_printf ("No buffer for incoming packet\n");
    return SCPE_OK;
    }

PUTW (rsc + HM_CQ, 0);
PUTW (rsc + HM_LOF, lof);
PUTB (rsc + HM_CB + 00, node);                          /* source */
PUTB (rsc + HM_CB + 01, ~node);                         /* complement */

if (((lof & 077) != 077) && (opc == OPC_SNDMSG)) {
    sim_printf ("Receive packet to control memory\n");
    PUTB (rsc + HM_MSG + 000, opc);
    PUTB (rsc + HM_MSG + 001, pkt->data[PPD_FLAGS]);
    if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
        memcpy (&CM[(rsc + HM_MSG + 002) >> 1], &pkt->data[PPD_MTYPE], (pkt->length - PPD_MTYPE));
    else
        memcpy (&CM[(rsc + HM_MSG + 002) >> 1], &pkt->data[PPD_LENGTH], (pkt->length - PPD_LENGTH));
    }
else {
    PUTW (rsc + HM_BC, len);
    dma = GETW (rsc + HM_BMB);                          /* data mem addr */
    dma = (dma << 6) & DMEMMASK;
    sim_printf ("Receive packet to data memory: %o\n", dma);

    if (dma == 0) {
        sim_printf ("No data memory address in packet!\n");
        return SCPE_IERR;
        }
    PUTDB (dma + 000, opc);
    PUTDB (dma + 001, pkt->data[PPD_FLAGS]);
    if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
        memcpy (&DM[(dma + 002) >> 1], &pkt->data[PPD_MTYPE], (pkt->length - PPD_MTYPE));
    else
        memcpy (&DM[(dma + 002) >> 1], &pkt->data[PPD_LENGTH], (pkt->length - PPD_LENGTH));
    }

if ((kci_req[node]) && (dg_type == DG_SCSMSG) && (scs_type & 0x1)) {  /* pending request? */
    sim_printf ("K.ci complete pending request for %07o\n", kci_req);
    kci_req[node] = 0;
    }

if (cq == 0)
    cq = GETP (kh + KH_CIM);
for (int i = 0; i < 12; i++)
    sim_printf ("R %07o : %06o\n", rsc + (i << 1), GETW (rsc + (i << 1)));
put_to_queue (cq, rsc);

return SCPE_OK;
}

t_stat kci_appmsg (UNIT *uptr, CI_PKT *pkt)
{
uint32 srccon, dstcon;
uint32 node = pkt->data[PPD_PORT];
uint32 kh = kci_dib.cma;
uint32 sb, cb, cq;
uint32 rsc, dma, opc, lof, len, rcc;

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
    cb = GETP (cb + CB_TLK);                            /* try next connection */
    }
cq = GETP (cb + CB_SVR);                                /* get server queue */
if (cq == 0)
    return SCPE_OK;

len = pkt->length - SCS_APPL;
rsc = kci_get_srr (len);
lof = 0 | len;

if (rsc == 0) {
    sim_printf ("No buffer for incoming packet\n");
    return SCPE_OK;
    }

rcc = GETW (cb + CB_RCC);
rcc = rcc + CI_GET16 (pkt->data, SCS_CREDIT);
PUTW (cb + CB_RCC, rcc);
PUTW (rsc + HM_CTX, GETW (cb + CB_CTX));
PUTP (rsc + HM_CB, cb);                                 /* save connection block */
PUTP (rsc + HM_CQ, 0);
PUTW (rsc + HM_LOF, lof);

memcpy (&CM[(rsc + HM_MSG) >> 1], &pkt->data[SCS_APPL], len);

for (int i = 0; i < 12; i++)
    sim_printf ("R %07o : %06o\n", rsc + (i << 1), GETW (rsc + (i << 1)));
put_to_queue (cq, rsc);

return SCPE_OK;
}

t_stat kci_cnfrec (UNIT *uptr, CI_PKT *pkt)
{
uint32 frb, ctr, rte, val, cq, flg;

frb = CI_GET32 (pkt->data, PPD_SBNAM);
ctr = GETP (frb + F_PCTR);
rte = GETP (frb + F_RTE);                               /* get route */

flg = GETW (rte);                                       /* get opcode/flags from route */
sim_printf ("OPC: %o, FLG: %o\n", (flg >> 8), (flg & 0377));

val = GETW (ctr + C_CNT);                               /* get counter value */
val = val - 1;                                          /* another operation completed */
PUTW (ctr + C_CNT, val);                                /* update counter value */
if (val == 0) {                                         /* all operations complete? */
    cq = GETP (ctr);
    sim_printf ("CTR dest = %07o\n", cq);
    PUTP (ctr, 0);                                      /* clear queue pointer */
    put_to_queue (cq, ctr);                             /* send counter to completion queue */
    }
rte = rte + 010;                                        /* advance route pointer */
PUTP (frb + F_RTE, rte);
cq = GETW (rte + V_SUCC);                               /* get success destination */
if (cq & 0177400)
    cq = cq << 2;                                       /* direct address */
else {
    cq = frb + (cq << 2);                               /* offset into FRB */
    cq = GETP (cq);
    }
put_to_queue (cq, frb);                                 /* send FRB to completion queue */
return SCPE_OK;
}

t_stat kci_scsrec (UNIT *uptr, CI_PKT *pkt)
{
uint32 msg_type = CI_GET16 (pkt->data, PPD_STYPE);

switch (msg_type) {
    
    case SCS_APPMSG:                                    /* application message */
    case SCS_APPDG:                                     /* application datagram */
        return kci_appmsg (uptr, pkt);

    default:
        break;
        }
return kci_receive_int (uptr, pkt, 0);
}

t_stat kci_msgrec (UNIT *uptr, CI_PKT *pkt)
{
uint32 dg_type = CI_GET16 (pkt->data, PPD_MTYPE);

switch (dg_type) {

    case DG_SCSMSG:
        return kci_scsrec (uptr, pkt);

    default:
        break;
        }
return kci_receive_int (uptr, pkt, 0);
}

/* New incoming packet received */

t_stat kci_receive (CI_PKT *pkt)
{
UNIT *uptr = &kci_unit;
uint32 dg_type;

/* LOADER-E K.ci loopback microcode loaded */
if (pkt->data[PPD_PORT] == ci_get_node (uptr))          /* from local node? */
    return SCPE_OK;                                     /* ignore */
switch (pkt->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_MSGREC:
        return kci_msgrec (uptr, pkt);

    case OPC_IDREC:
        if (ci_check_vc (uptr, pkt->data[PPD_PORT]))    /* VC open? */
            return SCPE_OK;                             /* discard */
        break;

    case OPC_CNFREC:
        return kci_cnfrec (uptr, pkt);

    case OPC_REQDATREC:
        return ci_send_data (uptr, pkt);

    case OPC_SNDDATREC:
    case OPC_DATREC:
        return ci_receive_data (uptr, pkt);

    case OPC_REQREC:
        return ci_request_id (uptr, pkt);
        }
return kci_receive_int (uptr, pkt, 0);
}

void kci_counters (uint32 rsc, uint8 node)
{
PUTW (rsc + HM_AKA + 000, 0);                           /* ACKs path A - low word */
PUTW (rsc + HM_AKA + 002, 0);                           /* ACKs path A - high word */
PUTW (rsc + HM_NKA + 000, 0);                           /* NAKs path A - low word */
PUTW (rsc + HM_NKA + 002, 0);                           /* NAKs path A - high word */
PUTW (rsc + HM_NRA + 000, 0);                           /* No response path A - low word */
PUTW (rsc + HM_NRA + 002, 0);                           /* No response path A - high word */

PUTW (rsc + HM_AKB + 000, 0);                           /* ACKs path B - low word */
PUTW (rsc + HM_AKB + 002, 0);                           /* ACKs path B - high word */
PUTW (rsc + HM_NKB + 000, 0);                           /* NAKs path B - low word */
PUTW (rsc + HM_NKB + 002, 0);                           /* NAKs path B - high word */
PUTW (rsc + HM_NRB + 000, 0);                           /* No response path B - low word */
PUTW (rsc + HM_NRB + 002, 0);                           /* No response path B - high word */

PUTW (rsc + HM_DGD + 000, 0);                           /* Discarded Datagrams - low word */
PUTW (rsc + HM_DGD + 002, 0);                           /* Discarded Datagrams - high word */
PUTW (rsc + HM_RDR + 000, 0);                           /* Discarded RTNDATs - low word */
PUTW (rsc + HM_RDR + 002, 0);                           /* Discarded RTNDATs - high word */
}

/* get/set virtual circuit flags */

void kci_set_vc (uint32 rsc)
{
uint32 node, vcf, mask;

node = GETW (rsc + HM_CN);                              /* node */
vcf = GETW (rsc + HM_VCF);                              /* new flags */
mask = GETW (rsc + HM_MK);                              /* mask */

if (vcf && mask)                                        /* opening VC? */
    ci_open_vc (&kci_unit, node);
else if (mask)                                          /* closing VC? */
    ci_close_vc (&kci_unit, node);
kci_vcf[node] = (kci_vcf[node] & ~mask) | (vcf & mask); /* update flags */
PUTW (rsc + HM_VCF, kci_vcf[node]);                     /* return updated value */
}

/* Local K.ci commands */

void kci_local_cmd (uint32 rsc, uint32 *cq)
{
uint32 opc, p1, p2, p3;

opc = GETB (rsc + HM_MSG);
p1 = GETW (rsc + HM_MSG + 002);
p2 = GETW (rsc + HM_MSG + 004);
p3 = GETW (rsc + HM_MSG + 006);
switch (opc) {
    case 001:                                           /* get/set virtual circuit flags */
        sim_printf ("local cmd: opc = %d, node = %d, p2 = %06o, p3 = %06o, cq = %06o\n", opc, p1, p2, p3, *cq);
        kci_set_vc (rsc);
        break;
    case 002:                                           /* get counters */
        kci_counters (rsc, p1);
        break;
    case 003:                                           /* enqueue message buffer (SRR) */
    case 004:                                           /* enqueue datagram buffer (DRR) */
        sim_printf ("local cmd: opc = %d, type = %06o, dma = %08o, p3 = %06o, cq = %06o\n", opc, p1, (p2 << 6), p3, *cq);
        PUTW (rsc + HM_LOF, 060077);
        break;
    default:
        sim_printf ("unknown local cmd\n");
        break;
    }
}

t_stat kci_send_int (UNIT *uptr, CI_PKT *pkt)
{
uint32 dg_type;

switch (pkt->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_SNDDAT:
        return ci_send_data (uptr, pkt);
        }
return ci_send_ppd (uptr, pkt);
}

/* Send a datagram */

void kci_send (uint32 rsc, uint32 *cq)
{
uint32 len, opc, flgs, node, dma, cb, srccon, dstcon, rcc, off;
uint32 kh = kci_dib.cma;
CI_PKT pkt;
t_stat r = SCPE_OK;

opc = GETW (rsc + HM_LOF) & 074000;
flgs = GETW (rsc + HM_LOF) & 03700;
len = GETW (rsc + HM_LOF) & 077;
node = GETB (rsc + HM_CB);                              /* destination node */

pkt.data[PPD_PORT] = GETB (rsc + HM_CB);

if ((opc == 020000) || (opc == 040000)) {               /* SCS application message */
    cb = GETP (rsc + HM_CB);                            /* get connection block */
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
        len = GETW (rsc + HM_BC);                       /* actual packet length */
        dma = GETW (rsc + HM_BMB);
        dma = (dma << 6) & DMEMMASK;                    /* data memory address */
        memcpy (&pkt.data[SCS_APPL], &DM[dma >> 1], len);
        }
    else
        memcpy (&pkt.data[SCS_APPL], &CM[(rsc + HM_MSG) >> 1], len);
    CI_PUT16 (pkt.data, PPD_LENGTH, len + SCS_APPL - PPD_MSGHDR);
    len = len + SCS_APPL;

    rcc = GETW (cb + CB_RCC);                           /* adjust credits */
    PUTW (cb + CB_RCC, rcc - 1);
    }
else {
    if (len == 077) {                                   /* packet in data memory */
        len = GETW (rsc + HM_BC);                       /* actual packet length */
        dma = GETW (rsc + HM_BMB);
        dma = (dma << 6) & DMEMMASK;                    /* data memory address */
        pkt.data[PPD_OPC] = GETDB (dma + 000);          /* opcode */
        pkt.data[PPD_FLAGS] = GETDB (dma + 001);
        if (pkt.data[PPD_OPC] <= OPC_SNDMSG)            /* datagram or message? */
            off = PPD_MTYPE;
        else
            off = PPD_LENGTH;
        memcpy (&pkt.data[off], &DM[(dma + 002) >> 1], len - 2);
        }
    else {
        pkt.data[PPD_OPC] = GETB (rsc + HM_MSG);        /* opcode */
        pkt.data[PPD_FLAGS] = GETB (rsc + HM_MSG + 001);
        if (pkt.data[PPD_OPC] <= OPC_SNDMSG)            /* datagram or message? */
            off = PPD_MTYPE;
        else
            off = PPD_LENGTH;
        memcpy (&pkt.data[off], &CM[(rsc + HM_MSG + 002) >> 1], len - 2);
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
r = kci_send_int (&kci_unit, &pkt);

if (*cq == 0) {
    if (GETW (rsc + HM_CTX) == 0) {
        if (pkt.data[PPD_OPC] == OPC_SNDMSG)
            *cq = kh + KH_SRR;
        else
            *cq = kh + KH_DRR;
        sim_printf ("Returning buffer %07o to queue at %07o\n", rsc, *cq);
//        put_to_queue (cq, rsc);
        }
    else {
        kci_req[node] = rsc;
        sim_printf ("K.ci start pending request for %07o\n", rsc);
        }
    }
}

void kci_process_frb (uint32 rsc, uint32 *cq)
{
uint32 hm, cb, con, bnm, bct, roff, soff, len, bd, dma;
uint32 kh = kci_dib.cma;
CI_PKT pkt;
t_stat r;

*cq = 0;
sim_printf ("K.ci FRB:\n");
kci_print_frb (rsc);
hm = GETP (rsc + F_PCTR);                               /* get host message */
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
soff = GETL (rsc + F_OFF);

CI_PUT32 (pkt.data, PPD_LCONID, con);                   /* local connection ID */
CI_PUT32 (pkt.data, PPD_RSPID, 1);                      /* local response ID */
CI_PUT32 (pkt.data, PPD_SBNAM, rsc);                    /* send buffer name */
CI_PUT32 (pkt.data, PPD_RBNAM, bnm);                    /* receive buffer name */

while (bct) {
    len = (bct > CI_MAXDAT) ? CI_MAXDAT : bct;
    pkt.length = len + CI_DATHDR;
    CI_PUT32 (pkt.data, PPD_XFRSZ, bct);                /* total transfer size */
    CI_PUT32 (pkt.data, PPD_SBOFF, soff);               /* send buffer offset */
    CI_PUT32 (pkt.data, PPD_RBOFF, roff);               /* receive buffer offset */
    if (len <= CI_MAXDAT)
        pkt.data[PPD_FLAGS] |= PPD_LP;                  /* last packet */
    bd = GETP (rsc + F_BFHD);                           /* get next buffer descriptor */
    dma = GETW (bd + 002);
    dma = (dma << 6) & DMEMMASK;                        /* data memory address */
    dma = dma + B_DATA;                                 /* point to sector data */
    sim_printf ("DMEM addr = %o\n", dma);
    memcpy (&pkt.data[CI_DATHDR], &DM[dma >> 1], len);  /* copy to packet buffer */
    r = ci_send_ppd (&kci_unit, &pkt);                  /* send to host */
    if (r != SCPE_OK) {
        sim_printf ("K.ci stalled during SNDDAT operation\n");
        break;
        }
    bd = get_from_queue (rsc + F_BFHD);                 /* dequeue buffer descriptor */
    put_to_queue (kh + KG_FBU, bd);                     /* put to free buffer queue */
    soff += len;
    roff += len;
    bct -= len;
    }
}

void kci_process (uint32 rsc, uint32 *cq)
{
uint16 opc = GETW (rsc + HM_LOF) & 074000;
        for (int i = 0; i < 12; i++)
            sim_printf ("S %07o : %06o\n", rsc + (i << 1), GETW (rsc + (i << 1)));
switch (opc) {
    case 060000:                                        /* local command */
        kci_local_cmd (rsc, cq);
        break;
    case 034000:                                        /* send datagram (path A) */
    case 054000:                                        /* send datagram (path B) */
    case 074000:                                        /* send datagram (path auto?) */
    case 030000:                                        /* send message */
    case 020000:                                        /* send SCS message? */
    case 040000:                                        /* send SCS message? */
        kci_send (rsc, cq);
        break;
    case 000000:
        kci_process_frb (rsc, cq);
        break;
    default:
        sim_printf ("K.ci unknown opcode: %o\n", opc);
        for (int i = 12; i < 32; i++)
            sim_printf ("S %07o : %06o\n", rsc + (i << 1), GETW (rsc + (i << 1)));
        break;
    }
}

void kci_process_queue (uint32 kh, uint32 wq)
{
uint32 cq, rsc, i, opc;

for (i = 0; i < 20; i++) {                              /* process maximum of 20 items */
    if (Q_LOCKED (wq + Q_HEAD))                         /* source queue locked */
        break;
    if (Q_EMPTY (wq + Q_HEAD))                          /* source queue empty */
        break;
    rsc = GETP (wq + Q_HEAD);                           /* get the resource pointer */
    cq = GETP (rsc + HM_CQ);                            /* get completion queue pointer */
    if (cq == wq)                                       /* target is self? */
        cq = GETP (kh + KG_FCB);                        /* FIXME: put to free queue */
    opc = GETW (rsc + HM_LOF);
    if (opc == 020077)
        cq = GETP (kh + KG_FCB);                        /* FIXME: put to free queue */
    if (cq) {
        if (Q_LOCKED (cq))                              /* completion queue locked */
            break;
        }

    rsc = get_from_queue (wq);                          /* get resource from work queue */
    sim_printf ("Processing packet %07o from queue %07o\n", rsc, wq);
    kci_process (rsc, &cq);                             /* process resource */

    if (cq)
        put_to_queue (cq, rsc);                         /* put resource to completion queue */
    }
}

t_stat kci_svc (UNIT *uptr)
{
uint32 kh = kci_dib.cma;
CI_PKT pkt;
t_stat r;

kci_process_queue (kh, kh + KH_0WK);                    /* process work queues */
kci_process_queue (kh, kh + KH_1WK);
kci_process_queue (kh, kh + KH_2WK);
kci_process_queue (kh, kh + KH_3WK);

for (int i = 0; i < 20; i++) {                          /* avoid endless loop */
    r = ci_receive_ppd (uptr, &pkt);                    /* get next packet */
    // TODO: handle errors from ci_receive
    if (r != SCPE_OK)
        break;
    r = kci_receive (&pkt);                             /* process packet */
    if (r != SCPE_OK)
        return r;
    };

return ci_port_svc (uptr);
}

t_stat kci_reset (DEVICE *dptr)
{
for (int i = 0; i < CI_MAX_NODES; i++) {
    kci_req[i] = 0;
    kci_vcf[i] = 0;
    }
return ci_port_reset (dptr);
}
