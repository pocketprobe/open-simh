/* vax_ci.c: Computer Interconnect adapter (VAX Common)

   Copyright (c) 2019-2024, Matt Burke

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

   Port Initialisation:
      In order to get the operating system to use the CI port, the console
      storage device must contain the port microcode.
*/

#include "vax_defs.h"
#include "dec_ci.h"
#include "vax_ci.h"
#include "vax_gvp.h"

/* Port Status */

#define PSR_RQA         0x00000001                      /* response queue available */
#define PSR_MFQE        0x00000002                      /* message free queue empty */
#define PSR_PDC         0x00000004                      /* port disable complete */
#define PSR_PIC         0x00000008                      /* port initialisation complete */
#define PSR_DSE         0x00000010                      /* data structure error */
#define PSR_MSE         0x00000020                      /* memory system error */
#define PSR_SE          0x00000040                      /* maintenance timer expiration */

/* Port Queue Block Base */

#define PQBB_V_ADDR     9                               /* port queue block base */
#define PQBB_M_ADDR     0x1FFFFF

#define PQBB_ADDR       (PQBB_M_ADDR << PQBB_V_ADDR)

/* Port Parameter */

#define PPR_M_NODE      0xF                             /* ci node number */
#define PPR_V_IBLEN     16                              /* internal buffer length */
#define PPR_M_IBLEN     0x1FFF
#define PPR_V_CS        31                              /* cluster size */

#define PPR_NODE        (PPR_M_NODE)
#define PPR_IBLEN       (PPR_M_IBLEN << PPR_V_IBLEN)
#define PPR_CS          (1u << PPR_V_CS)

/* PQB Offsets */

#define PQB_CMD0_OF     0x0                             /* Command queue 0 (low priority) */
#define PQB_CMD1_OF     0x8                             /* Command queue 1 (high priority) */
#define PQB_CMD2_OF     0x10                            /* Command queue 2 */
#define PQB_CMD3_OF     0x18                            /* Command queue 3 */
#define PQB_RESP_OF     0x20                            /* Response queue */
#define PQB_DFQA_OF     0x28                            /* Datagram free queue Address */
#define PQB_MFQA_OF     0x2C                            /* Message free queue Address */
#define PQB_DFQL_OF     0x30                            /* Datagram free queue entry length */
#define PQB_DFML_OF     0x34                            /* Message free queue entry length */
#define PQB_PQBBVA_OF   0x38                            /* VA of port queue block base */
#define PQB_BDTVA_OF    0x3C                            /* VA of buffer descriptor table */
#define PQB_BDTLEN_OF   0x40                            /* # entries in buffer descriptor table */
#define PQB_SBR_OF      0x44                            /* system page table PA */
#define PQB_SLR_OF      0x48                            /* system page table size */
#define PQB_GPT_OF      0x4C                            /* global page table VA */
#define PQB_GLR_OF      0x50                            /* global page table size */

/* Queue Names */

#define Q_CMD0          0                               /* Command queue 0 (low priority) */
#define Q_CMD1          1                               /* Command queue 1 (high priority) */
#define Q_CMD2          2                               /* Command queue 2 */
#define Q_CMD3          4                               /* Command queue 3 */
#define Q_RSP           5                               /* Response queue */
#define Q_DF            6                               /* Datagram free queue */
#define Q_MF            7                               /* Message free queue */

/* Generic VAXport buffer descriptor */

#define BD_FLAGS_OF     0x0
#define BD_M_OFF        0x1FF                           /* byte offset into buffer page */
#define BD_M_AC         0x1000                          /* access mode check enabled */
#define BD_M_ACMOD      0x6000                          /* access mode required */
#define BD_M_VALID      0x8000                          /* valid entry */
#define BD_KEY_OF       0x2                             /* sequence number */
#define BD_SIZE_OF      0x4                             /* buffer size */
#define BD_PTE_OF       0x8                             /* VA of buffer PTE */
#define BD_NEXT_OF      0xC                             /* next free buffer descriptor */
#define BD_LEN          0x10                            /* buffer descriptor length */

/* Virtual Circuit Flags */

#define VCD_SSN         0x0003                          /* send sequence number */
#define VCD_RSN         0x000C                          /* receive sequence number */
#define VCD_RDP         0x0040                          /* RDP supported */
#define VCD_FSN         0x0080                          /* full sequence number */
#define VCD_SAS         0x0100                          /* subnode addressing */
#define VCD_PAG         0x0200                          /* path A good */
#define VCD_PBG         0x0400                          /* path B good */
#define VCD_NADP        0x0800                          /* NADP supported */
#define VCD_IDG         0x1000                          /* inhibit datagrams */
#define VCD_SS          0x2000                          /* send sequence number */
#define VCD_RS          0x4000                          /* receive sequence number */
#define VCD_CST         0x8000                          /* circuit state */
#define VCD_OPEN(x)     (x & VCD_CST)
#define DG_INHIBIT(x)   (x & VCD_IDG)

/* Macros */

#define CI_ReadL(x)     GVP_ReadL (&ci_mmu, x)

#define port_ctx        up8

/* Action for received packets */

#define ACT_DISCARD     0                               /* discard packet */
#define ACT_RECEIVE     1                               /* receive packet */
#define ACT_UNRPKT      2                               /* process as unrecognised packet */

uint32 ci_psr;                                          /* port status reg */
uint32 ci_max_dg;                                       /* max datagram size */
uint32 ci_max_msg;                                      /* max message size */
uint32 ci_pqbb_pa;                                      /* port queue base pa */
uint32 ci_pqbb_va;                                      /* port queue base va */
uint32 ci_bdt_va;                                       /* buffer descriptor table va */
uint32 ci_bdt_len;                                      /* buffer descriptor table entries */
uint32 ci_spt_base;                                     /* buffer descriptor table entries */
uint32 ci_pfar;                                         /* port failing addr */
uint32 ci_pesr;                                         /* port error status */
uint32 ci_ppr;                                          /* port parameter reg */
uint16 ci_vcd[CI_MAX_VCD];                              /* virtual circuit descriptors */
uint32 ci_cnt;                                          /* counter port */
uint32 ci_dgd;                                          /* datagrams discarded */
GVPMMU ci_mmu;                                          /* memory management unit */

extern int32 tmxr_poll;

void ci_read_packet (UNIT *uptr, PPD_PKT *pkt, size_t length);
void ci_write_packet (UNIT *uptr, PPD_PKT *pkt, size_t length);
void ci_readb (UNIT *uptr, uint32 pte_ba, int32 bc, int32 offset, uint8 *buf);
void ci_writeb (UNIT *uptr, uint32 pte_ba, int32 bc, int32 offset, uint8 *buf);
void ci_writebp (UNIT *uptr, uint32 pte_ba, int32 bc, int32 offset, uint8 *buf);
void ci_read_pqb (uint32 pa);
t_stat ci_receive_int (UNIT *uptr, PPD_PKT *pkt);
t_stat ci_receive_ppd (UNIT *uptr, PPD_PKT *pkt);
t_stat ci_send_ppd (UNIT *uptr, PPD_PKT *pkt);
t_stat ci_fmt_receive (UNIT *uptr, CI_PKT *pkt, PPD_PKT *ppd);
t_stat ci_fmt_send (UNIT *uptr, PPD_PKT *ppd, CI_PKT *pkt);
t_stat ci_respond (UNIT *uptr, PPD_PKT *pkt);
t_stat ci_dispose (UNIT *uptr, PPD_PKT *pkt);

uint8 ci_get_length (UNIT *uptr, uint32 ma, PPD_PKT *pkt)
{
uint8 opc = GVP_Read (&ci_mmu, ma + PPD_OPC, L_BYTE);
uint16 len = GVP_Read (&ci_mmu, ma + PPD_LENGTH, L_WORD);
uint8 type, sts = STS_OK;

switch (opc) {
    case OPC_SNDDG:
        pkt->length = LEN_SNDDG + len;
        type = PPD_DG;
        break;

    case OPC_SNDMSG:
        pkt->length = LEN_SNDMSG + len;
        type = PPD_MSG;
        break;

    case OPC_REQID:
        pkt->length = LEN_REQID;
        type = PPD_DG;
        break;

    case OPC_SNDRST:
        pkt->length = LEN_SNDRST;
        type = PPD_DG;
        break;

    case OPC_SNDSTRT:
        pkt->length = LEN_SNDSTRT;
        type = PPD_DG;
        break;

    case OPC_REQDAT:
    case OPC_REQDAT1:
    case OPC_REQDAT2:
    case OPC_REQMDAT:
        pkt->length = LEN_REQDAT;
        type = PPD_MSG;
        break;

    case OPC_SNDLB:
        pkt->length = LEN_SNDLB + len;
        type = PPD_DG;
        break;

    case OPC_SNDDAT:
    case OPC_SNDMDAT:
        pkt->length = LEN_SNDDAT;
        type = PPD_MSG;
        break;

    case OPC_INVTC:
        pkt->length = LEN_INVTC;
        type = PPD_DG;
        break;

    case OPC_SETCKT:
        pkt->length = LEN_SETCKT;
        type = PPD_DG;
        break;

    case OPC_RDCNT:
        pkt->length = LEN_RDCNT;
        type = PPD_DG;
        break;

    default:
        pkt->length = PPD_HDR;
        sts = STS_URC;
        break;
    }
if ((type == PPD_DG) && (pkt->length > ci_max_dg)) {    /* datagram too big? */
    pkt->length = PPD_HDR;                              /* read header only */
    sts = STS_PSV;                                      /* packet size violation */
    }
else if ((type == PPD_MSG) && (pkt->length > ci_max_msg)) { /* message too big? */
    pkt->length = PPD_HDR;                              /* read header only */
    sts = STS_PSV;                                      /* packet size violation */
    }
if (pkt->length > CI_MAXFR) {                           /* apply safe limit */
    sim_printf ("packet length %ld limited to %d\n", pkt->length, CI_MAXFR);
    pkt->length = CI_MAXFR;
    }
return sts;
}

t_stat ci_port_command (UNIT *uptr, int32 queue)
{
uint32 ma;
int16 length;
PPD_PKT pkt;
t_stat r;
uint8 sts;

for ( ;; ) {
    ma = gvp_remqhi (&ci_mmu, ci_pqbb_va + queue);      /* dequeue next entry */

    if (ma == 0)                                        /* FIXME: dequeue error? */
        break;                                          /* done */
    if (ma == (ci_pqbb_va + queue))                     /* empty queue? */
        break;                                          /* done */

    if (ma & 0x3)
        sim_printf ("CI: packet not LW aligned\n");

    pkt.addr = ma;
    sts = ci_get_length (uptr, ma, &pkt);               /* get packet length */
    ci_read_packet (uptr, &pkt, pkt.length);            /* read packet */
    if (sts != STS_OK)                                  /* packet length error? */
        pkt.data[PPD_STATUS] = sts;
    else if ((pkt.data[PPD_PORT] >= CI_MAX_NODES) &&
            (pkt.data[PPD_PORT] != CI_ALL))             /* invalid port number? */
        pkt.data[PPD_STATUS] = STS_INVDP;
    else {
        r = ci_send_ppd (uptr, &pkt);                   /* process it */
        if (r == CISE_VCERR)
            pkt.data[PPD_STATUS] = STS_VCC;             /* VC closed */
        }
    if ((pkt.data[PPD_FLAGS] & PPD_RSP) ||
        (pkt.data[PPD_STATUS] != STS_OK))               /* response required? */
        r = ci_respond (uptr, &pkt);                    /* yes */
    else
        r = ci_dispose (uptr, &pkt);                    /* dispose of packet */
    if (r != SCPE_OK)                                   /* error? */
        break;                                          /* abort processing */
    }
return SCPE_OK;
}

/* Read CI port register */

t_stat ci_vax_rd (UNIT *uptr, int32 *val, int32 rg, int32 lnt)
{
switch (rg) {

    case CI_PSR:
        *val = ci_psr;
        sim_debug_unit (DBG_REG, uptr, "PSR rd: %08X at %08X\n", *val, fault_PC);
        break;

    case CI_PQBBR:
        *val = ci_pqbb_pa;
        sim_debug_unit (DBG_REG, uptr, "PQBBR rd: %08X at %08X\n", *val, fault_PC);
        break;

    case CI_PCQ0CR:
    case CI_PCQ1CR:
    case CI_PCQ2CR:
    case CI_PCQ3CR:
    case CI_PSRCR:
    case CI_PECR:
    case CI_PDCR:
    case CI_PICR:
    case CI_PDFQCR:
    case CI_PMFQCR:
    case CI_PMTCR:
    case CI_PMTECR:
        *val = 0;
        sim_debug_unit (DBG_REG, uptr, "other rd: %08X at %08X\n", *val, fault_PC);
        break;

    case CI_PFAR:
        *val = ci_pfar;
        sim_debug_unit (DBG_REG, uptr, "PFAR rd: %08X at %08X\n", *val, fault_PC);
        break;

    case CI_PESR:
        *val = ci_pesr;
        sim_debug_unit (DBG_REG, uptr, "PESR rd: %08X at %08X\n", *val, fault_PC);
        break;

    case CI_PPR:
        *val = ci_ppr | (uptr->ci_node & PPR_NODE);
        sim_debug_unit (DBG_REG, uptr, "PPR rd: %08X at %08X\n", *val, fault_PC);
        break;

    default:
        return SCPE_NXM;
        }

return SCPE_OK;
}

/* Write CI port register */

t_stat ci_vax_wr (UNIT *uptr, int32 val, int32 rg, int32 lnt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

switch (rg) {

    case CI_PSR:
        sim_debug_unit (DBG_WRN, uptr, "PSR wr! at %08X\n", fault_PC);
        break;

    case CI_PQBBR:
        sim_debug_unit (DBG_REG, uptr, "PQBBR wr: %08X at %08X\n", val, fault_PC);
        ci_pqbb_pa = val;
        break;

    case CI_PCQ0CR:
        sim_debug_unit (DBG_TRC, uptr, "port command queue 0 control at %08X\n", fault_PC);
        if (val & 1)
            return ci_port_command (uptr, PQB_CMD0_OF);

    case CI_PCQ1CR:
        sim_debug_unit (DBG_TRC, uptr, "port command queue 1 control at %08X\n", fault_PC);
        if (val & 1)
            return ci_port_command (uptr, PQB_CMD1_OF);

    case CI_PCQ2CR:
        sim_debug_unit (DBG_WRN, uptr, "port command queue 2 control at %08X\n", fault_PC);
        if (val & 1)
            return ci_port_command (uptr, PQB_CMD2_OF);

    case CI_PCQ3CR:
        sim_debug_unit (DBG_WRN, uptr, "port command queue 3 control at %08X\n", fault_PC);
        if (val & 1)
            return ci_port_command (uptr, PQB_CMD3_OF);

    case CI_PSRCR:
        sim_debug_unit (DBG_TRC, uptr, "port status release control at %08X\n", fault_PC);
        if (val & 1) {
            ci_psr = 0;
            cp->clr_int ();
            }
        break;

    case CI_PECR:
        sim_debug_unit (DBG_TRC, uptr, "port enable at %08X\n", fault_PC);
        if (val & 1) {
            if (uptr->ci_state & PORT_DIS) {            /* currently disabled? */
                ci_read_pqb (ci_pqbb_pa & PQBB_ADDR);
                if (uptr->ci_state & PORT_MAINT)        /* maintenance? */
                    uptr->ci_state = PORT_ENM;          /* now enabled maintenance */
                else {
                    uptr->ci_state = PORT_EN;           /* now enabled */
                    ci_rx_enable (uptr, (CI_PATH_A | CI_PATH_B)); /* enable receiver */
                    }
                }
            }
        break;

    case CI_PDCR:
        sim_debug_unit (DBG_TRC, uptr, "port disable at %08X\n", fault_PC);
        if (val & 1) {
            if (uptr->ci_state >= PORT_EN) {            /* currently enabled? */
                if (uptr->ci_state & PORT_MAINT)        /* maintenance? */
                    uptr->ci_state = PORT_DISM;         /* now disabled maintenance */
                else {
                    uptr->ci_state = PORT_DIS;          /* now disabled */
                    ci_rx_enable (uptr, 0);             /* disable receiver */
                    }
                }
            ci_psr |= PSR_PDC;
            cp->set_int ();
            }
        break;

    case CI_PICR:
        sim_debug_unit (DBG_TRC, uptr, "port init at %08X\n", fault_PC);
        if (val & 1) {
            if (uptr->ci_state & PORT_MAINT)            /* maintenance? */
                uptr->ci_state = PORT_DISM;             /* now disabled maintenance */
            else {
                uptr->ci_state = PORT_DIS;              /* now disabled */
                ci_rx_enable (uptr, 0);                 /* disable receiver */
                }
            ci_psr |= PSR_PIC;
            cp->set_int ();
            }
        break;

    case CI_PDFQCR:
        sim_debug_unit (DBG_TRC, uptr, "port datagram free queue at %08X\n", fault_PC);
        break;

    case CI_PMFQCR:
        sim_debug_unit (DBG_TRC, uptr, "port message free queue at %08X\n", fault_PC);
        break;

    case CI_PMTCR:
        sim_debug_unit (DBG_REG, uptr, "PMTCR wr: %08X at %08X\n", val, fault_PC);
        break;

    case CI_PMTECR:
        sim_debug_unit (DBG_REG, uptr, "PMTECR wr: %08X at %08X\n", val, fault_PC);
        if (val & 1) {
            uptr->ci_state = PORT_UIM;                  /* now uninitialised maintenance */
            ci_rx_enable (uptr, (CI_PATH_A | CI_PATH_B)); /* enable receiver */
            ci_psr |= PSR_SE;
            cp->set_int ();
            }
        break;

    case CI_PFAR:
        sim_debug_unit (DBG_WRN, uptr, "PFAR wr! at %08X\n", fault_PC);
        break;

    case CI_PESR:
        sim_debug_unit (DBG_WRN, uptr, "PESR wr! at %08X\n", fault_PC);
        break;

    case CI_PPR:
        sim_debug_unit (DBG_WRN, uptr, "PPR wr! at %08X\n", fault_PC);
        break;

    default:
        return SCPE_NXM;
        }
return SCPE_OK;
}

/* Read parameters from port queue block */

void ci_read_pqb (uint32 pa)
{
ci_max_dg = CI_ReadL (pa + PQB_DFQL_OF) & 0xFFF;        /* max packet sizes */
ci_max_msg = CI_ReadL (pa + PQB_DFML_OF) & 0xFFF;
ci_pqbb_va = CI_ReadL (pa + PQB_PQBBVA_OF);
ci_bdt_va = CI_ReadL (pa + PQB_BDTVA_OF);               /* buffer descriptors */
ci_bdt_len = CI_ReadL (pa + PQB_BDTLEN_OF) & 0xFFFF;
ci_mmu.sbr = CI_ReadL (pa + PQB_SBR_OF);                /* system page table */
ci_mmu.sbr = (ci_mmu.sbr - 0x1000000) & ~03;            /* VA<31> >> 7 */
ci_mmu.slr = CI_ReadL (pa + PQB_SLR_OF) & 0x1FFFFF;
ci_mmu.slr = (ci_mmu.slr << 2) + 0x1000000;             /* VA<31> >> 7 */
ci_mmu.gbr = CI_ReadL (pa + PQB_GPT_OF);                /* global page table */
ci_mmu.glr = CI_ReadL (pa + PQB_GLR_OF) & 0x1FFFFF;
ci_mmu.glr = (ci_mmu.glr << 2);
}

t_stat ci_send_data (UNIT *uptr, PPD_PKT *ppd)
{
uint32 len, xfrsz, bd, sbnam, sboff, rbnam, rboff, pte, opc;
uint16 snd_key, flags, key;
CI_PKT pkt;
t_stat r;

ci_dump_pkt (DBG_BLK, uptr, ppd->data, ppd->length);
xfrsz = CI_GETL (ppd->data, PPD_XFRSZ);
sbnam = CI_GETW (ppd->data, PPD_SBNAM);                 /* BDT table offset */
snd_key = CI_GETW (ppd->data, PPD_SBNAM + 2);
sboff = CI_GETL (ppd->data, PPD_SBOFF);                 /* send buffer offset */
rboff = CI_GETL (ppd->data, PPD_RBOFF);                 /* receive buffer offset */
bd = ci_bdt_va + (sbnam * BD_LEN);                      /* buffer descriptor VA */

flags = GVP_Read (&ci_mmu, bd + BD_FLAGS_OF, L_WORD);   /* buffer flags */
key = GVP_Read (&ci_mmu, bd + BD_KEY_OF, L_WORD);       /* buffer key */
pte = GVP_Read (&ci_mmu, bd + BD_PTE_OF, L_LONG);       /* page table entry addr */
sboff = sboff + (flags & BD_M_OFF);                     /* add page offset */

opc = CI_GETB (ppd->data, PPD_OPC);
if ((opc >= OPC_REQDATREC) && (opc <= OPC_REQDATREC2))  /* REQDAT0, 1, 2 */
    CI_PUTB (ppd->data, PPD_OPC, OPC_RETDAT);
else if (opc == OPC_REQMDATREC)                         /* REQMDAT */
    CI_PUTB (ppd->data, PPD_OPC, OPC_RETMDAT);

if (((flags & BD_M_VALID) == 0) || (snd_key != key)) {  /* check buffer key */
    sim_printf ("CI: send buffer invalid\n");
    ppd->data[PPD_STATUS] = STS_INVBN;
    return SCPE_IERR;
    }

r = ci_fmt_send (uptr, ppd, &pkt);                      /* convert to CI format */
if (r != SCPE_OK)
    return r;

rbnam = CI_GETL (ppd->data, PPD_RBNAM);                 /* BDT table offset */
CI_PUTB (pkt.data, CI_FLAGS, 0);                        /* flags */
CI_PUTL (pkt.data, CI_RBNAM, rbnam);                    /* receive buffer name */

while (xfrsz > 0) {
    if (xfrsz > CI_MAXDAT)                              /* limit to max transfer */
        len = CI_MAXDAT;
    else
        len = xfrsz;
    ci_readb (uptr, pte, len, sboff, &pkt.data[CI_DATA]); /* transfer from buffer */
    CI_PUTL (pkt.data, CI_RBOFF, rboff);                /* update packet */
    if (xfrsz <= CI_MAXDAT) {
        gvp_zap_tb (&ci_mmu);                           /* FIXME: should only zap entries for this buffer */
        pkt.data[CI_FLAGS] |= CI_LP;                    /* last packet */
        }
    pkt.length = len + CI_DATA;
    r = ci_send (uptr, &pkt);                           /* send data */
    if (r != SCPE_OK)
        return r;
    xfrsz -= len;
    sboff += len;
    rboff += len;
    }
return SCPE_OK;
}

t_stat ci_receive_data (UNIT *uptr, CI_PKT *pkt)
{
uint32 len, bd, rboff, pte, opc;
uint16 rbnam, rec_key, flags, key;
PPD_PKT ppd;
t_stat r = SCPE_OK;

opc = CI_GETB (pkt->data, CI_OPC);
rbnam = CI_GETW (pkt->data, CI_RBNAM);                  /* BDT table offset */
rec_key = CI_GETW (pkt->data, CI_RBNAM + 2);
rboff = CI_GETL (pkt->data, CI_RBOFF);                  /* receive buffer offset */
bd = ci_bdt_va + (rbnam * BD_LEN);

if ((opc == OPC_SNTDAT) || (opc == OPC_RETDAT)) {       /* FIXME: +++ indent +++ */
flags = GVP_Read (&ci_mmu, bd + BD_FLAGS_OF, L_WORD);   /* buffer flags */
key = GVP_Read (&ci_mmu, bd + BD_KEY_OF, L_WORD);       /* buffer key */
pte = GVP_Read (&ci_mmu, bd + BD_PTE_OF, L_LONG);       /* page table entry addr */
rboff = rboff + (flags & BD_M_OFF);                     /* add page offset */

if (((flags & BD_M_VALID) == 0) || (rec_key != key)) {  /* check buffer key */
    sim_printf ("CI: receive buffer invalid\n");
//#if 0
    pkt->length = LEN_DAT;
    r = ci_fmt_receive (uptr, pkt, &ppd);               /* convert to PPD format */
    if (opc == OPC_SNTDAT) {
        CI_PUTB (ppd.data, PPD_OPC, OPC_RETCNF);
        }
    CI_PUTB (ppd.data, PPD_STATUS, STS_INVBN);
    r = ci_receive_int (uptr, &ppd);                    /* pass to system */
    return r;
//#endif
//    return SCPE_IERR;
    }
}                                                       /* FIXME: +++ indent +++ */
len = pkt->length - CI_DATA;
if (len > CI_MAXDAT)                                    /* limit to max transfer */
    len = CI_MAXDAT;

if ((opc == OPC_SNTDAT) || (opc == OPC_RETDAT))
    ci_writeb (uptr, pte, len, rboff, &pkt->data[CI_DATA]); /* transfer to buffer */
else if ((opc == OPC_SNTMDAT) || (opc == OPC_RETMDAT))
    ci_writebp (uptr, rboff, len, 0, &pkt->data[CI_DATA]); /* transfer to buffer */
else
    return SCPE_IERR;

if (pkt->data[CI_FLAGS] & CI_LP) {                      /* last packet? */
    gvp_zap_tb (&ci_mmu);                               /* FIXME: should only zap entries for this buffer */
    if (opc == OPC_SNTDAT) {
        CI_PUTB (pkt->data, CI_OPC, OPC_CNF);
        pkt->length = LEN_CNF;
        r = ci_send (uptr, pkt);                        /* send confirmation */
        }
    else if (opc == OPC_SNTMDAT) {
        CI_PUTB (pkt->data, CI_OPC, OPC_MCNF);
        pkt->length = LEN_MCNF;
        r = ci_send (uptr, pkt);                        /* send confirmation */
        }
    else if (opc == OPC_RETDAT) {
        pkt->length = LEN_DAT;
        r = ci_fmt_receive (uptr, pkt, &ppd);           /* convert to PPD format */
        if (r == SCPE_OK)
            r = ci_receive_int (uptr, &ppd);            /* pass to system */
        }
    else
        r = SCPE_IERR;
    }
return r;
}

/* ID request - dealt with at CI layer */

t_stat ci_idreq (UNIT *uptr, CI_PKT *pkt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

CI_PUTB (pkt->data, CI_OPC, OPC_ID);                    /* set opcode */
if (pkt->path == CI_PATH_A)
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_0);           /* set received path */
else
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_1);
CI_PUTL (pkt->data, CI_MAINT_ID, cp->type);             /* remote port type */
CI_PUTL (pkt->data, CI_CODE_REV, cp->coderev);          /* code revision */
CI_PUTL (pkt->data, CI_PORT_FCN, cp->fmask);            /* function mask */
CI_PUTB (pkt->data, CI_RST_PORT, uptr->ci_node);        /* resetting port */
CI_PUTB (pkt->data, CI_SYS_STATE, CI_PTST_ENA);         /* port state */
memset (&pkt->data[CI_ID_MBZ], 0, LEN_ID - CI_ID_MBZ);
pkt->length = LEN_ID;

return ci_send (uptr, pkt);
}

/* Virtual circuit control */

t_stat ci_setckt (UNIT *uptr, PPD_PKT *pkt)
{
uint32 port = pkt->data[PPD_PORT];
uint16 vcd_mmsk = CI_GETW (pkt->data, PPD_VCMMSK);      /* VCD modify mask */
uint16 vcd_mval = CI_GETW (pkt->data, PPD_VCMVAL);      /* VCD modify value */
uint16 vcd_val = ci_vcd[port];                          /* VCD value */
t_stat r = SCPE_OK;

sim_debug_unit (DBG_LCMD, uptr, "==> SETCKT, port: %d, vcd: %04X, mask: %04X, mval: %04X\n", port, vcd_val, vcd_mmsk, vcd_mval);

vcd_val = vcd_val & ~vcd_mmsk;                          /* new VCD value */
vcd_val = vcd_val | (vcd_mval & vcd_mmsk);

if (port < CI_MAX_NODES) {
    if (VCD_OPEN (vcd_val) && !VCD_OPEN (ci_vcd[port])) /* opening VC */
        r = ci_open_vc (uptr, port);
    else if (!VCD_OPEN (vcd_val) && VCD_OPEN (ci_vcd[port])) /* closing VC */
        r = ci_close_vc (uptr, port);

    if (r != SCPE_OK)
        return r;
    }

CI_PUTW (pkt->data, PPD_VCPVAL, ci_vcd[port]);          /* previous VCD value */
ci_vcd[port] = vcd_val;                                 /* set new VCD value */
pkt->length = LEN_SETCKT;
ci_write_packet (uptr, pkt, pkt->length);
return r;
}

/* Invalidate translation cache */

t_stat ci_invtc (UNIT *uptr, PPD_PKT *pkt)
{
sim_debug_unit (DBG_LCMD, uptr, "==> INVTC\n");
gvp_zap_tb (&ci_mmu);
return SCPE_OK;
}

/* Read counters */

t_stat ci_rdcnt (UNIT *uptr, PPD_PKT *pkt)
{
ci_cnt = pkt->data[PPD_PORT];
sim_debug_unit (DBG_LCMD, uptr, "==> RDCNT, port = %d\n", ci_cnt);
CI_PUTL (pkt->data, 0x10, 0);                           /* ACKs on path 0 */
CI_PUTL (pkt->data, 0x14, 0);                           /* NAKs on path 0 */
CI_PUTL (pkt->data, 0x18, 0);                           /* NORSPs on path 0 */
CI_PUTL (pkt->data, 0x1c, 0);                           /* ACKs on path 1 */
CI_PUTL (pkt->data, 0x20, 0);                           /* NAKs on path 1 */
CI_PUTL (pkt->data, 0x24, 0);                           /* NORSPs on path 1 */
CI_PUTL (pkt->data, 0x28, ci_dgd);                      /* DGS discarded */
ci_dgd = 0;                                             /* reset counters */
pkt->length = LEN_RDCNT;
ci_write_packet (uptr, pkt, pkt->length);
return SCPE_OK;
}

/* Loopback received */

t_stat ci_lbrec (UNIT *uptr, PPD_PKT *pkt)
{
uint32 port = pkt->data[PPD_PORT];
uint32 path = GET_PATH (pkt->data[PPD_FLAGS]);
if (path == PPD_PS0)
    ci_vcd[port] |= VCD_PAG;                            /* path A good */
else if (path == PPD_PS1)
    ci_vcd[port] |= VCD_PBG;                            /* path B good */
return SCPE_OK;
}

/* Read a packet from memory */

void ci_read_packet (UNIT *uptr, PPD_PKT *pkt, size_t length)
{
int32 i;

sim_debug_unit (DBG_TRC, uptr, "ci_read_packet: addr: %08X, pkt->length: %ld, length: %ld\n", pkt->addr, pkt->length, length);
for (i = PPD_SIZE; i < length; i++)                     /* skip the queue pointers */
    pkt->data[i] = GVP_Read (&ci_mmu, (pkt->addr + i), L_BYTE);
}

/* Write a packet to memory */

void ci_write_packet (UNIT *uptr, PPD_PKT *pkt, size_t length)
{
int32 i;

sim_debug_unit (DBG_TRC, uptr, "ci_write_packet: addr: %08X, pkt->length: %ld, length: %ld\n", pkt->addr, pkt->length, length);
for (i = PPD_PORT; i < length; i++)                     /* don't overwrite the header */
    GVP_Write (&ci_mmu, (pkt->addr + i), pkt->data[i], L_BYTE);
}

// TODO: Move these to GVP?

void ci_readb (UNIT *uptr, uint32 pte_ba, int32 bc, int32 offset, uint8 *buf)
{
int32 i;
uint32 dat, pte_va, pte, pa;;

sim_debug_unit (DBG_TRC, uptr, "ci_readb: pte_ba: %08X, bc: %d, offset: %d", pte_ba, bc, offset);
if ((offset | bc) & 03) {                               /* check alignment */
    for (i = offset; i < (offset + bc); i++, buf++) {   /* by bytes */
        pte_va = pte_ba + ((i >> 7) & ~0x3);
        pte = GVP_Read (&ci_mmu, pte_va, L_LONG);
        pa = ((pte & PTE_PFN) << VA_N_OFF) | VA_GETOFF(i);
        if (i == offset)
            sim_debug_unit (DBG_TRC, uptr, ", pa: %08X", pa);
        *buf = ReadB (pa);
        }
    }
else {
    for (i = offset; i < (offset + bc); i = i + 4, buf++) { /* by longwords */
        pte_va = pte_ba + ((i >> 7) & ~0x3);
        pte = GVP_Read (&ci_mmu, pte_va, L_LONG);
        pa = ((pte & PTE_PFN) << VA_N_OFF) | VA_GETOFF(i);
        if (i == offset)
            sim_debug_unit (DBG_TRC, uptr, ", pa: %08X", pa);
        dat = ReadL (pa);                               /* get lw */
        *buf++ = dat & BMASK;                           /* low 8b */
        *buf++ = (dat >> 8) & BMASK;                    /* next 8b */
        *buf++ = (dat >> 16) & BMASK;                   /* next 8b */
        *buf = (dat >> 24) & BMASK;
        }
    }
sim_debug_unit (DBG_TRC, uptr, "-%08X\n", pa);
}

void ci_writeb (UNIT *uptr, uint32 pte_ba, int32 bc, int32 offset, uint8 *buf)
{
int32 i;
uint32 dat, pte_va, pte, pa;

sim_debug_unit (DBG_TRC, uptr, "ci_writeb: pte_ba: %08X, bc: %d, offset: %d", pte_ba, bc, offset);
if ((offset | bc) & 03) {                               /* check alignment */
    // TODO: This should only need to translate PTE when going cross page
    for (i = offset; i < (offset + bc); i++, buf++) {   /* by bytes */
        pte_va = pte_ba + ((i >> 7) & ~0x3);
        pte = GVP_Read (&ci_mmu, pte_va, L_LONG);
        pa = ((pte & PTE_PFN) << VA_N_OFF) | VA_GETOFF(i);
        if (i == offset)
            sim_debug_unit (DBG_TRC, uptr, ", pa: %08X", pa);
        WriteB (pa, *buf);
        }
    }
else {
    for (i = offset; i < (offset + bc); i = i + 4, buf++) { /* by longwords */
        pte_va = pte_ba + ((i >> 7) & ~0x3);
        pte = GVP_Read (&ci_mmu, pte_va, L_LONG);
        pa = ((pte & PTE_PFN) << VA_N_OFF) | VA_GETOFF(i);
        if (i == offset)
            sim_debug_unit (DBG_TRC, uptr, ", pa: %08X", pa);
        dat = (uint32) *buf++;                          /* get low 8b */
        dat = dat | (((uint32) *buf++) << 8);           /* merge next 8b */
        dat = dat | (((uint32) *buf++) << 16);          /* merge next 8b */
        dat = dat | (((uint32) *buf) << 24);            /* merge hi 8b */
        WriteL (pa, dat);                               /* store lw */
        }
    }
sim_debug_unit (DBG_TRC, uptr, "-%08X\n", pa);
}

void ci_writebp (UNIT *uptr, uint32 mpa, int32 bc, int32 offset, uint8 *buf)
{
int32 i;
uint32 dat, pa;

sim_debug_unit (DBG_TRC, uptr, "ci_writebp: pa: %08X, bc: %d, offset: %d", pa, bc, offset);
if ((offset | bc) & 03) {                               /* check alignment */
    // TODO: This should only need to translate PTE when going cross page
    for (i = offset; i < (offset + bc); i++, buf++) {   /* by bytes */
        pa = mpa + i;
        if (i == offset)
            sim_debug_unit (DBG_TRC, uptr, ", pa: %08X", pa);
        WriteB (pa, *buf);
        }
    }
else {
    for (i = offset; i < (offset + bc); i = i + 4, buf++) { /* by longwords */
        pa = mpa + i;
        if (i == offset)
            sim_debug_unit (DBG_TRC, uptr, ", pa: %08X", pa);
        dat = (uint32) *buf++;                          /* get low 8b */
        dat = dat | (((uint32) *buf++) << 8);           /* merge next 8b */
        dat = dat | (((uint32) *buf++) << 16);          /* merge next 8b */
        dat = dat | (((uint32) *buf) << 24);            /* merge hi 8b */
        WriteL (pa, dat);                               /* store lw */
        }
    }
sim_debug_unit (DBG_TRC, uptr, "-%08X\n", pa);
}

/* Put a packet on the response queue */

t_stat ci_put_rsq (UNIT *uptr, PPD_PKT *pkt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

sim_debug_unit (DBG_TRC, uptr, "ci_put_rsq: queue: %08X, pkt: %08X\n", ci_pqbb_va + PQB_RESP_OF, pkt->addr);
if (gvp_insqti (&ci_mmu, ci_pqbb_va + PQB_RESP_OF, pkt->addr) > 1) {
    ci_psr |= PSR_RQA;
    cp->set_int ();
    }
return SCPE_OK;
}

/* Put a packet on the datagram free queue */

t_stat ci_put_dfq (UNIT *uptr, PPD_PKT *pkt)
{
uint32 hdr;

hdr = GVP_Read (&ci_mmu, ci_pqbb_va + PQB_DFQA_OF, L_LONG);
sim_debug_unit (DBG_TRC, uptr, "ci_put_dfq: queue: %08X, pkt: %08X\n", hdr, pkt->addr);
gvp_insqti (&ci_mmu, hdr, pkt->addr);
return SCPE_OK;
}

/* Get a packet from the response queue */

t_stat ci_get_dfq (UNIT *uptr, PPD_PKT *pkt)
{
uint32 hdr;

hdr = GVP_Read (&ci_mmu, ci_pqbb_va + PQB_DFQA_OF, L_LONG);
pkt->addr = gvp_remqhi (&ci_mmu, hdr);
sim_debug_unit (DBG_TRC, uptr, "ci_get_dfq: queue: %08X, pkt: %08X\n", hdr, pkt->addr);
if (pkt->addr == 0)                                     /* queue error? */
    return CISE_QERR;
if (pkt->addr == hdr)                                   /* queue empty? */
    return CISE_NOPKT;
return SCPE_OK;
}

/* Put a packet on the message free queue */

t_stat ci_put_mfq (UNIT *uptr, PPD_PKT *pkt)
{
uint32 hdr;

hdr = GVP_Read (&ci_mmu, ci_pqbb_va + PQB_MFQA_OF, L_LONG);
sim_debug_unit (DBG_TRC, uptr, "ci_put_mfq: queue: %08X, pkt: %08X\n", hdr, pkt->addr);
gvp_insqti (&ci_mmu, hdr, pkt->addr);
return SCPE_OK;
}

/* Get a packet from the datagram free queue */

t_stat ci_get_mfq (UNIT *uptr, PPD_PKT *pkt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
uint32 hdr;

hdr = GVP_Read (&ci_mmu, ci_pqbb_va + PQB_MFQA_OF, L_LONG);
pkt->addr = gvp_remqhi (&ci_mmu, hdr);
sim_debug_unit (DBG_TRC, uptr, "ci_get_mfq: queue: %08X, pkt: %08X\n", hdr, pkt->addr);
if (pkt->addr == 0)                                     /* queue error? */
    return CISE_QERR;
if (pkt->addr == hdr) {                                 /* queue empty? */
    ci_psr |= PSR_MFQE;
    cp->set_int ();
    return CISE_NOPKT;
    }
return SCPE_OK;
}

/* Dispose of an outgoing packet that does not need a response */

t_stat ci_dispose (UNIT *uptr, PPD_PKT *pkt)
{
uint8 opc = CI_GETB (pkt->data, PPD_OPC);

if (pkt->addr != 0) {
    switch (opc) {                                      /* determine packet type */
        case OPC_SNDMSG:
        case OPC_REQDAT:
        case OPC_REQDAT1:
        case OPC_REQDAT2:
        case OPC_SNDDAT:
            return ci_put_mfq (uptr, pkt);              /* message */

        case OPC_SNDDG:
        case OPC_REQID:
        case OPC_SNDRST:
        case OPC_SNDSTRT:
        case OPC_SNDLB:
        case OPC_REQMDAT:
        case OPC_SNDMDAT:
        case OPC_INVTC:
        case OPC_SETCKT:
        case OPC_RDCNT:
            return ci_put_dfq (uptr, pkt);              /* datagram */

        default:
            sim_printf ("ci_dispose() - Unhandled opcode 0x%02X\n", opc);
            break;
        }
    }
return SCPE_IERR;
}

/* Respond to an outgoing packet */

t_stat ci_respond (UNIT *uptr, PPD_PKT *pkt)
{
if (pkt->addr != 0) {
    ci_write_packet (uptr, pkt, PPD_HDR);
    return ci_put_rsq (uptr, pkt);
    }
return SCPE_IERR;
}

/* New incoming packet received (internal version) */

t_stat ci_receive_int (UNIT *uptr, PPD_PKT *pkt)
{
uint8 opc = CI_GETB (pkt->data, PPD_OPC);
uint8 act;
t_stat r;

if (pkt->addr == 0) {
    switch (opc) {                                      /* determine packet type */
        case OPC_MSGREC:
        case OPC_DATREC:
        case OPC_CNFREC:
        case OPC_RETCNF:
        case OPC_RETDAT:
            r = ci_get_mfq (uptr, pkt);                 /* message */
            break;

        case OPC_DGREC:
        case OPC_MCNFREC:
        case OPC_RSTREC:
        case OPC_STRTREC:
        case OPC_IDREC:
        case OPC_LBREC:
        case OPC_REQMDATREC:
        case OPC_SNDMDATREC:
        case OPC_MDATREC:
            r = ci_get_dfq (uptr, pkt);                 /* datagram */
            if (r == CISE_NOPKT) {
                if (ci_cnt == CI_ALL)
                    ci_dgd++;
                else if (ci_cnt == pkt->data[PPD_PORT])
                    ci_dgd++;
                }
            break;

        default:
            sim_printf ("ci_receive_int() - Unhandled opcode 0x%02X\n", opc);
            r = SCPE_IERR;
            break;
        }
    if (r != SCPE_OK)
        return r;
    }

act = ACT_RECEIVE;                                      /* assume packet is OK */
switch (opc) {                                          /* determine packet type */
    case OPC_SNDMDATREC:
    case OPC_MDATREC:
        act = ACT_UNRPKT;
        break;

    case OPC_REQMDATREC:
        if (uptr->ci_state != PORT_UIM)
            act = ACT_UNRPKT;
        break;

    case OPC_STRTREC:
        if (uptr->ci_state == PORT_UIM)
            act = ACT_RECEIVE;
        else if (uptr->ci_state != PORT_DISM)
            act = ACT_UNRPKT;
        break;

    case OPC_RSTREC:
        if (uptr->ci_state == PORT_EN)
            act = ACT_UNRPKT;
        break;

    default:
        break;
    }
if (act == ACT_UNRPKT) {                                /* unrecognised? */
    pkt->data[PPD_OPC] &= ~OPC_M_RECV;                  /* clear receive flag */
    pkt->data[PPD_STATUS] = STS_URP;                    /* set status */
    }
ci_write_packet (uptr, pkt, pkt->length);
return ci_put_rsq (uptr, pkt);
}

/* New incoming packet received */

t_stat ci_receive_ppd (UNIT *uptr, PPD_PKT *pkt)
{
uint32 port = pkt->data[PPD_PORT];
t_stat r;

switch (pkt->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_DGREC:
        port = pkt->data[PPD_PORT];                     /* get remote port */
        if (DG_INHIBIT (ci_vcd[port])) {                /* inhibited? */
            if (ci_cnt == CI_ALL)
                ci_dgd++;
            else if (ci_cnt == pkt->data[PPD_PORT])
                ci_dgd++;
            return SCPE_OK;                             /* discard */
            }
        break;

    case OPC_LBREC:
        ci_lbrec (uptr, pkt);
        break;

    case OPC_REQDATREC:
    case OPC_REQDATREC1:
    case OPC_REQDATREC2:
    case OPC_REQMDATREC:
        r = ci_send_data (uptr, pkt);
        if (r == SCPE_OK)
            return r;
        break;
        }
return ci_receive_int (uptr, pkt);
}

t_stat ci_send_ppd (UNIT *uptr, PPD_PKT *ppd)
{
CI_PKT pkt;
t_stat r;

ppd->data[PPD_STATUS] = STS_OK;                         /* assume ok */
switch (ppd->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_INVTC:
        return ci_invtc (uptr, ppd);

    case OPC_SETCKT:
        return ci_setckt (uptr, ppd);

    case OPC_RDCNT:
        return ci_rdcnt (uptr, ppd);

    case OPC_SNDDAT:
    case OPC_SNDMDAT:
        return ci_send_data (uptr, ppd);
        }
r = ci_fmt_send (uptr, ppd, &pkt);                      /* convert to CI format */
if (r == SCPE_OK)
    r = ci_send (uptr, &pkt);                           /* send CI packet */
return r;
}

/* Translate CI packet to VAX CI port format */

t_stat ci_fmt_receive (UNIT *uptr, CI_PKT *pkt, PPD_PKT *ppd)
{
uint8 opc = CI_GETB (pkt->data, CI_OPC);
uint8 path;

ppd->length = pkt->length;
ppd->addr = 0;
if ((opc <= OPC_MSG) || (opc == OPC_LB)) {              /* datagram or message */
    ppd->data[PPD_OPC] = pkt->data[CI_OPC];
    ppd->data[PPD_FLAGS] = pkt->data[CI_FLAGS] & 0xF0;  /* only top bits are common */
    memcpy (&ppd->data[PPD_TEXT], &pkt->data[CI_TEXT], pkt->length - CI_TEXT);
    CI_PUTW (ppd->data, PPD_LENGTH, pkt->length - CI_TEXT);
    ppd->length += 2;                                    /* length field added */
    }
else memcpy (&ppd->data[PPD_OPC], &pkt->data[CI_OPC], pkt->length - CI_OPC);

ppd->length += (PPD_OPC - CI_OPC);                      /* set new length */

CI_PUTB (ppd->data, PPD_STATUS, STS_OK);
CI_PUTB (ppd->data, PPD_PORT, pkt->node);
ppd->data[PPD_OPC] |= OPC_M_RECV;                       /* convert to rcv opcode */

if (opc == OPC_ID)
    ppd->data[PPD_FLAGS] = 0;
//if ((opc == OPC_ID) || (opc == OPC_LB)) {
if (opc == OPC_LB) {
    if (pkt->path == CI_PATH_A)
        path = PPD_PS0;                                 /* received on path A */
    else if (pkt->path == CI_PATH_B)
        path = PPD_PS1;                                 /* received on path B */
    ppd->data[PPD_FLAGS] |= (path << PPD_V_PS);         /* set receive path */
    }
return SCPE_OK;
}

/* Translate VAX CI port format to CI packet */

t_stat ci_fmt_send (UNIT *uptr, PPD_PKT *ppd, CI_PKT *pkt)
{
uint32 opc = CI_GETB (ppd->data, PPD_OPC);

pkt->length = ppd->length;
if ((opc <= OPC_SNDMSG) || (opc == OPC_SNDLB)) {        /* datagram or message */
    pkt->data[CI_OPC] = ppd->data[PPD_OPC];
    pkt->data[CI_FLAGS] = ppd->data[PPD_FLAGS] & 0xF0;  /* only top bits are common */
    memcpy (&pkt->data[CI_TEXT], &ppd->data[PPD_TEXT], ppd->length - PPD_TEXT);
    pkt->length = CI_GETW (ppd->data, PPD_LENGTH);      /* get length */
    pkt->length += PPD_TEXT;                            /* add header */
    pkt->length -= 2;                                   /* length field removed */
    }
else memcpy (&pkt->data[CI_OPC], &ppd->data[PPD_OPC], ppd->length - PPD_OPC);

switch (opc) {                                          /* set new length */
    case OPC_REQID:
        pkt->length = LEN_IDREQ;
        break;

    case OPC_REQDAT:
    case OPC_REQDAT1:
    case OPC_REQDAT2:
    case OPC_REQMDAT:
        pkt->length = LEN_DATREQ;
        break;

    default:
        pkt->length -= (PPD_OPC - CI_OPC);
        break;
    }
pkt->node = CI_GETB (ppd->data, PPD_PORT);
pkt->path = GET_PATH (ppd->data[PPD_FLAGS]);

return SCPE_OK;
}

t_stat ci_vax_svc (UNIT *uptr)
{
CI_PKT pkt;
PPD_PKT ppd;
t_stat r;
uint8 opc, act;

do {
    r = ci_receive (uptr, &pkt);                        /* try to receive another packet */
    if (r == SCPE_OK) {
        opc = CI_GETB (pkt.data, CI_OPC);
        act = ACT_DISCARD;                              /* assume packet will be discarded */
        switch (opc) {                                  /* opcodes handled at CI layer */
            case OPC_IDREQ:
                r = ci_idreq (uptr, &pkt);
                break;

            case OPC_SNTDAT:
            case OPC_RETDAT:
                if (uptr->ci_state & PORT_EN)
                    r = ci_receive_data (uptr, &pkt);
                break;

            case OPC_SNTMDAT:
            case OPC_RETMDAT:
                if (uptr->ci_state == PORT_UIM)
                    r = ci_receive_data (uptr, &pkt);
                else if ((uptr->ci_state == PORT_ENM) && (pkt.node == uptr->ci_node))
                    r = ci_receive_data (uptr, &pkt);
                else
                    act = ACT_UNRPKT;
                break;

            case OPC_MDATREQ:
                if (uptr->ci_state == PORT_UIM)
                    act = ACT_RECEIVE;
                else if ((uptr->ci_state == PORT_ENM) && (pkt.node == uptr->ci_node))
                    act = ACT_RECEIVE;
                else
                    act = ACT_UNRPKT;
                break;

            case OPC_STRT:
                if (uptr->ci_state == PORT_UIM)
                    act = ACT_RECEIVE;
                else if (uptr->ci_state != PORT_DISM)
                    act = ACT_UNRPKT;
                break;

            case OPC_RST:
                if (uptr->ci_state != PORT_EN)          /* all but en */
                    act = ACT_RECEIVE;
                else
                    act = ACT_UNRPKT;
                break;

            default:
                if (uptr->ci_state & PORT_EN)           /* en or en maint */
                    act = ACT_RECEIVE;
                break;
            }

        if (act == ACT_RECEIVE) {                       /* receive packet? */
            r = ci_fmt_receive (uptr, &pkt, &ppd);      /* convert to PPD format */
            if (r == SCPE_OK)
                r = ci_receive_ppd (uptr, &ppd);
            }
        else if (act == ACT_UNRPKT) {                   /* unrecognised packet? */
            pkt.data[CI_OPC] = OPC_DG;
            r = ci_fmt_receive (uptr, &pkt, &ppd);      /* convert to PPD format */
            ppd.data[PPD_OPC] = opc | OPC_M_RECV;
            pkt.data[CI_OPC] = opc;
            ppd.data[PPD_FLAGS] = pkt.data[CI_FLAGS];
            if (r == SCPE_OK)
                r = ci_receive_ppd (uptr, &ppd);
            }
        }
} while (r == SCPE_OK);
if (r != CISE_NOPKT)
    return r;
return ci_port_svc (uptr);
}

/* Reset CI adapter */

t_stat ci_vax_reset (DEVICE *dptr, CI_PORT *cp)
{
UNIT *uptr = dptr->units;
uint32 i;

uptr->port_ctx = cp;                                    /* save port parameters */

for (i = 0; i < CI_MAX_NODES; i++)
    ci_vcd[i] = (VCD_PAG | VCD_PBG);                    /* clear virtual circuit descriptors */
ci_psr = 0;                                             /* reset port registers */
ci_pesr = 0;
ci_pfar = 0;
ci_pqbb_pa = 0;
ci_pqbb_va = 0;
ci_bdt_va = 0;
ci_bdt_len = 0;
ci_cnt = CI_ALL;
ci_dgd = 0;
ci_ppr |= ((cp->iblen & PPR_M_IBLEN) << PPR_V_IBLEN);   /* internal buffer length */
uptr->ci_state = PORT_UI;                               /* now uninitialised */
gvp_tlb_reset (&ci_mmu);                                /* reset MMU */
ci_port_reset (dptr);
return SCPE_OK;
}
