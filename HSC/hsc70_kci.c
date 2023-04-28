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
#define KH_2WK          0010
#define KH_DRR          0034
#define KH_CIM          0142                            /* CI manager queue pointer */
#define KH_NAD          0164                            /* node address */

/* host message block */

#define HM_LNK          0000                            /* link to next */
#define HM_DUO          0002
#define HM_CTX          0002
#define HM_CQ           0004                            /* completion queue */
#define HM_DX           0004
#define HM_ERR          0006
#define HM_LOF          0010                            /* length, opcode, flags */
#define HM_CB           0012
#define HM_MSG          0014                            /* CI message */

extern int32 tmxr_poll;                                 /* calibrated delay */

t_stat kci_svc (UNIT *uptr);
t_stat kci_reset (DEVICE *dptr);
t_stat kci_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
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
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "CONTROL", NULL,
      NULL, &kci_show_ctrl, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, KH_2WK, "2WK", NULL,
      NULL, &kg_show_queue, NULL, "Display work queue 2" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "RUN", "RUN",
      &kci_run, NULL, NULL },
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
if (act == K_RUN) {
    kci_dib.cma = kg_init ();
    sim_debug (DBG_TRC, &kci_dev, "K.ci startup: KH = %06o at %o\n", kci_dib.cma, (uint16)inst_pc);
    PUTW (kci_dib.cma + KH_NAD, node);                      /* KH$NAD - CI node address */
    PUTW (kci_dib.cma + KG_VRS, (3 << 8) | 73);             /* KG$VRS - version (DS = 3, MC = 73) */
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
print_word (st, kh, "KH$3WK", 0004, 4);                 /* work queue 3 */
print_word (st, kh, "KH$2WK", KH_2WK, 4);               /* work queue 2 */
print_word (st, kh, "KH$1WK", 0014, 4);                 /* work queue 1 */
print_word (st, kh, "KH$0WK", 0020, 4);                 /* work queue 0 */
print_word (st, kh, "KH$EQ ", 0024, 4);                 /* error queue? */
print_word (st, kh, "KH$SRR", 0030, 4);
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

t_stat kci_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
uint32 kh = kci_dib.cma;
uint32 rsc, dma, cq;
rsc = get_from_queue (kh + KH_DRR);

PUTW (rsc + HM_LOF, 0044000 | 077);
PUTW (rsc + HM_CB, 0175404);

dma = GETW (rsc + HM_MSG + 004);                        /* data mem addr */
dma = (dma << 6) & DMEMMASK;
dma = dma >> 1;

for (int i = 0; i < 20; i++) {                          /* clear buffer */
    DM[dma + i] = 0;
    }

DM[dma + 000] = 013;                                    /* IDREC */
DM[dma + 013] = 02000;                                  /* port enabled */

cq = GETP (kh + KH_CIM);
put_to_queue (cq, rsc);

return SCPE_OK;
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

/* New incoming packet received (internal version) */

t_stat kci_receive_int (UNIT *uptr, CI_PKT *pkt)
{
uint32 kh = kci_dib.cma;
uint32 rsc, dma, cq, opc;
rsc = get_from_queue (kh + KH_DRR);

PUTW (rsc + HM_LOF, 0044000 | 077);
PUTB (rsc + HM_CB + 00, pkt->data[PPD_PORT]);           /* source */
PUTB (rsc + HM_CB + 01, ~(pkt->data[PPD_PORT]));        /* complement */
PUTW (rsc + HM_MSG, pkt->length);

dma = GETW (rsc + HM_MSG + 004);                        /* data mem addr */
dma = (dma << 6) & DMEMMASK;

opc = pkt->data[PPD_OPC] & ~OPC_M_RECV;
PUTDB (dma + 000, opc);
PUTDB (dma + 001, pkt->data[PPD_FLAGS]);
if ((opc == OPC_SNDDG) || (opc == OPC_SNDMSG))
    memcpy (&DM[(dma + 002) >> 1], &pkt->data[PPD_MTYPE], pkt->length);
else
    memcpy (&DM[(dma + 002) >> 1], &pkt->data[PPD_LENGTH], pkt->length);

cq = GETP (kh + KH_CIM);
put_to_queue (cq, rsc);

return SCPE_OK;
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
    case OPC_IDREC:
        if (ci_check_vc (uptr, pkt->data[PPD_PORT]))    /* VC open? */
            return SCPE_OK;                             /* discard */
        break;

    case OPC_REQDATREC:
        return ci_send_data (uptr, pkt);

    case OPC_SNDDATREC:
    case OPC_DATREC:
        return ci_receive_data (uptr, pkt);

    case OPC_REQREC:
        return ci_request_id (uptr, pkt);
        }
return kci_receive_int (uptr, pkt);
}

void kci_counters (uint32 rsc, uint8 node)
{
PUTW (rsc + 020, 0);                                    /* ACKs path A - low word */
PUTW (rsc + 022, 0);                                    /* ACKs path A - high word */
PUTW (rsc + 024, 0);                                    /* NAKs path A - low word */
PUTW (rsc + 026, 0);                                    /* NAKs path A - high word */
PUTW (rsc + 030, 0);                                    /* No response path A - low word */
PUTW (rsc + 032, 0);                                    /* No response path A - high word */

PUTW (rsc + 034, 0);                                    /* ACKs path B - low word */
PUTW (rsc + 036, 0);                                    /* ACKs path B - high word */
PUTW (rsc + 040, 0);                                    /* NAKs path B - low word */
PUTW (rsc + 042, 0);                                    /* NAKs path B - high word */
PUTW (rsc + 044, 0);                                    /* No response path B - low word */
PUTW (rsc + 046, 0);                                    /* No response path B - high word */

PUTW (rsc + 050, 0);                                    /* Discarded Datagrams - low word */
PUTW (rsc + 052, 0);                                    /* Discarded Datagrams - high word */
PUTW (rsc + 054, 0);                                    /* Discarded RTNDATs - low word */
PUTW (rsc + 056, 0);                                    /* Discarded RTNDATs - high word */
}

/* Local K.ci commands */

void kci_local_cmd (uint32 rsc, uint32 cq)
{
uint32 opc, arg1, arg2;

opc = GETB (rsc + HM_MSG);
arg1 = GETW (rsc + HM_MSG + 002);
arg2 = GETW (rsc + HM_MSG + 004);
switch (opc) {
    case 001:
//        sim_printf ("local cmd: opc = %d, node = %06o, cq = %06o\n", opc, arg1, cq);
        break;
    case 002:                                           /* show counters */
//        sim_printf ("show counters: cq = %06o\n", cq);
        kci_counters (rsc, arg1);
        break;
    case 003:
    case 004:
//        sim_printf ("local cmd: opc = %d, arg1 = %06o, data mem = %08o, cq = %06o\n", opc, arg1, (arg2 << 6), cq);
        break;
    default:
//        sim_printf ("local cmd: opc = %d, arg1 = %06o, arg2 = %06o, cq = %06o\n", opc, arg1, arg2, cq);
        break;
    }
}

t_stat kci_send_int (UNIT *uptr, CI_PKT *pkt)
{
switch (pkt->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_SNDDAT:
        return ci_send_data (uptr, pkt);
        }
return ci_send_ppd (uptr, pkt);

}
/* Send a datagram */

void kci_send (uint32 rsc, uint32 cq)
{
uint32 len, flgs, node, dma;
CI_PKT pkt;
t_stat r = SCPE_OK;

flgs = GETW (rsc + HM_LOF) & 074000;
len = GETW (rsc + HM_LOF) & 077;
node = GETB (rsc + HM_CB);                              /* destination node */

pkt.data[PPD_PORT] = GETB (rsc + HM_CB);
if (len == 077) {                                       /* packet in data memory */
    len = GETW (rsc + HM_MSG);
    dma = GETW (rsc + HM_MSG + 004);
    dma = (dma << 6) & DMEMMASK;

    pkt.data[PPD_OPC] = GETDB (dma + 000);             /* opcode */
    pkt.data[PPD_FLAGS] = GETDB (dma + 001);
    if ((pkt.data[PPD_OPC] == OPC_SNDDG) || (pkt.data[PPD_OPC] == OPC_SNDMSG)) {
        memcpy (&pkt.data[PPD_MTYPE], &DM[(dma + 002) >> 1], len - 2);
        CI_PUT16 (pkt.data, PPD_LENGTH, len - 2);
        }
    else
        memcpy (&pkt.data[PPD_LENGTH], &DM[(dma + 002) >> 1], len - 2);
    }
else {
    pkt.data[PPD_OPC] = GETB (rsc + HM_MSG);           /* opcode */
    pkt.data[PPD_FLAGS] = GETB (rsc + HM_MSG + 001);
    if ((pkt.data[PPD_OPC] == OPC_SNDDG) || (pkt.data[PPD_OPC] == OPC_SNDMSG)) {
        memcpy (&pkt.data[PPD_MTYPE], &CM[(rsc + HM_MSG + 002) >> 1], len - 2);
        CI_PUT16 (pkt.data, PPD_LENGTH, len - 2);
        }
    else
        memcpy (&pkt.data[PPD_LENGTH], &CM[(rsc + HM_MSG + 002) >> 1], len - 2);
    }
if (flgs == 034000)
    pkt.data[PPD_FLAGS] |= (PPD_PS0 << PPD_V_PS);       /* path A */
else if (flgs == 054000)
    pkt.data[PPD_FLAGS] |= (PPD_PS1 << PPD_V_PS);       /* path B */
else
    pkt.data[PPD_FLAGS] |= (PPD_PSAUTO << PPD_V_PS);    /* path auto? */
pkt.length = len;
r = kci_send_int (&kci_unit, &pkt);
}

void kci_process (uint32 rsc, uint32 cq)
{
uint16 opc = GETW (rsc + HM_LOF) & 074000;
switch (opc) {
    case 060000:                                        /* local command */
        kci_local_cmd (rsc, cq);
        break;
    case 034000:                                        /* send datagram (path A) */
    case 054000:                                        /* send datagram (path B) */
    case 074000:                                        /* send datagram (path auto?) */
        kci_send (rsc, cq);
        break;
    default:
//        sim_printf ("K.ci unknown opcode: %o\n", opc);
//        for (int i = 0; i < 10; i++)
//            sim_printf ("%04o : %06o\n", (i << 1), GETW (rsc + (i << 1)));
//        sim_printf ("----\n");
        break;
    }
}

t_stat kci_svc (UNIT *uptr)
{
uint32 kh = kci_dib.cma;
uint32 wq, cq, rsc, i;
CI_PKT pkt;
t_stat r;

wq = kh + KH_2WK;                                       /* process 2WK queue */
for (i = 0; i < 20; i++) {                              /* process maximum of 20 items */
    if (Q_LOCKED (wq + Q_HEAD))                         /* source queue locked */
        break;
    if (Q_EMPTY (wq + Q_HEAD))                          /* source queue empty */
        break;
    rsc = GETP (wq + Q_HEAD);                           /* get the resource pointer */
    cq = GETP (rsc + HM_CQ);                            /* get completion queue pointer */
    if (cq == 0)                                        /* resource has no completion */
        break;
    if (Q_LOCKED (cq))                                  /* completion queue locked */
        break;

    rsc = get_from_queue (wq);                          /* get resource from work queue */
    kci_process (rsc, cq);                              /* process resource */
    put_to_queue (cq, rsc);                             /* put resource to completion queue */
    }

while (GETW (kh + KH_DRR) != 0) {                       /* receive buffers available? */
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
return ci_port_reset (dptr);
}
