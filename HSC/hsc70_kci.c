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

#define CI_MAX_NODES    32

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


uint32 kci_node = 0;                                    /* local node address */

extern int32 tmxr_poll;                                 /* calibrated delay */

t_stat kci_svc (UNIT *uptr);
t_stat kci_reset (DEVICE *dptr);
t_stat kci_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kci_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat kci_init (uint16 req, uint16 act);

/* debugging bitmaps */

#define DBG_TRC         KREQ_DBG_TRC                    /* trace routine calls */

DEBTAB kci_debug[] = {
    { "TRACE",  DBG_TRC,   "trace routine calls" },
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
    { MTAB_XTD|MTAB_VDV, 004, "NODE", "NODE",
      &kci_set_node, &kci_show_node, NULL },
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
    NULL, NULL, NULL,
    &kci_dib, DEV_DEBUG | DEV_HSC | DEV_DISABLE,
    0, kci_debug
    };


/* The version must be at least DS = 2, MC = 44 */

t_stat kci_init (uint16 req, uint16 act)
{
if (act == K_RUN) {
    kci_dib.cma = kg_init ();
    sim_debug (DBG_TRC, &kci_dev, "K.ci startup: KH = %06o at %o\n", kci_dib.cma, (uint16)inst_pc);
    PUTW (kci_dib.cma + KH_NAD, kci_node);                  /* KH$NAD - CI node address */
    PUTW (kci_dib.cma + KG_VRS, (3 << 8) | 73);             /* KG$VRS - version (DS = 3, MC = 73) */
    sim_clock_coschedule (&kci_unit, tmxr_poll);            /* activate */
    }
else {
    sim_debug (DBG_TRC, &kci_dev, "K.ci halt at %o\n", (uint16)inst_pc);
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

void kci_counters (uint32 rsc, uint16 node)
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

void kci_local_cmd (uint32 rsc, uint16 cq)
{
uint16 opc, arg1, arg2;

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

/* Send a datagram */

void kci_send (uint32 rsc, uint32 cq)
{
uint16 opc, len, flgs, node, bda;
char path = '?';

flgs = GETW (rsc + HM_LOF) & 074000;
len = GETW (rsc + HM_LOF) & 077;
node = GETB (rsc + HM_CB);                              /* destination node */
if (flgs == 034000)
    path = 'A';                                         /* path A */
else if (flgs == 054000)
    path = 'B';                                         /* path B */
else
    path = '*';                                         /* path auto? */

if (len == 077) {                                       /* packet in data memory */
    bda = GETW (rsc + HM_MSG);
    bda = (bda << 6) & DMEMMASK;
    bda = bda >> 1;

    opc = DM[bda + 0] & 0377;                           /* opcode */
    }
else {
    opc = GETB (rsc + HM_MSG);                          /* opcode */
    }
switch (opc) {
    case 005:                                           /* REQID */
//        sim_printf ("REQID: node = %d, path = %c, cq = %06o\n", node, path, cq);
        break;
    default:
//        sim_printf ("Send datagram: node = %d, path = %c, opc = %d, cq = %06o\n", node, path, opc, cq);
        break;
    }
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
sim_clock_coschedule (uptr, tmxr_poll);                 /* reactivate */
return SCPE_OK;
}

t_stat kci_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "node=%d", kci_node);
return SCPE_OK;
}

t_stat kci_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
t_stat r;
uint32 node;

node = (uint32) get_uint (cptr, 10, CI_MAX_NODES, &r);
if (r != SCPE_OK)
    return r;
if ((node >= CI_MAX_NODES) || (node < 0))
    return SCPE_ARG;
kci_node = node;
return SCPE_OK;
}

t_stat kci_reset (DEVICE *dptr)
{
return SCPE_OK;
}
