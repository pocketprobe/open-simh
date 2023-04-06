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

uint32 kci_node = 0;                                    /* local node address */

extern int32 tmxr_poll;                                 /* calibrated delay */

t_stat kci_svc (UNIT *uptr);
t_stat kci_reset (DEVICE *dptr);
t_stat kci_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
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
      NULL, &k_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      NULL, &k_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 004, "NODE", "NODE",
      &kci_set_node, &kci_show_node, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "CONTROL", NULL,
      NULL, &kci_show, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, KH_2WK, "2WK", NULL,
      NULL, &k_show_queue, NULL, "Display work queue 2" },
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
    kci_dib.cma = k_init_common ();
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

t_stat kci_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint16 kh = kci_dib.cma;

print_word (st, kh, "KH$LNK", KH_LNK, 2);
print_word (st, kh, "KH$FLG", 0002, 2);
print_word (st, kh, "KH$3WK", 0004, 4);                 /* work queue 3 */
print_word (st, kh, "KH$2WK", 0010, 4);                 /* work queue 2 */
print_word (st, kh, "KH$1WK", 0014, 4);                 /* work queue 1 */
print_word (st, kh, "KH$0WK", 0020, 4);                 /* work queue 0 */
print_word (st, kh, "KH$EQ ", 0024, 4);                 /* error queue? */
print_word (st, kh, "KH$SRR", 0030, 4);
print_word (st, kh, "KH$DRR", 0034, 4);

print_word (st, kh, "KG$TLK", 0040, 2);
print_word (st, kh, "KG$SLT", 0042, 1);                 /* structure type? */
print_word (st, kh, "KG$TYP", 0043, 1);                 /* structure type? */
print_word (st, kh, "KG$VRS", 0044, 2);                 /* version */
print_word (st, kh, "KG$FCB", 0046, 2);                 /* free control memory buffers? */
print_word (st, kh, "KG$FBU", 0050, 2);
print_word (st, kh, "KG$BWS", 0052, 2);
print_word (st, kh, "KG$TMR", 0054, 2);
print_word (st, kh, "KG$DMP", 0056, 2);
print_word (st, kh, "KG$DCA", 0060, 2);
print_word (st, kh, "KG$TST", 0062, 2);
print_word (st, kh, "KG$FRS", 0064, 2);
print_word (st, kh, "KG$LAS", 0066, 2);
print_word (st, kh, "KG$MDL", 0070, 0);
print_word (st, kh, "KG$MTY", 0070, 2);
print_word (st, kh, "KG$PFL", 0072, 2);
print_word (st, kh, "KG$RSL", 0074, 2);                 /* microdiagnostic test result */
print_word (st, kh, "KG$LTR", 0076, 2);
print_word (st, kh, "KG$PSA", 0100, 2);
print_word (st, kh, "KG$PSB", 0102, 2);
print_word (st, kh, "KG$DSA", 0104, 20);
print_word (st, kh, "KG$MC0", 0130, 8);
print_word (st, kh, "KG$KSA", 0140, 0);

print_word (st, kh, "KH$D2T", 0140, 2);
print_word (st, kh, "KH$CIM", 0142, 2);                 /* CI manager queue pointer? */
print_word (st, kh, "KH$D1T", 0144, 2);
print_word (st, kh, "KH$VA ", 0146, 2);
print_word (st, kh, "KH$D0T", 0150, 2);
print_word (st, kh, "KH$CVS", 0152, 2);
print_word (st, kh, "KH$TMR", 0154, 4);                 /* timer */
print_word (st, kh, "KH$BFR", 0160, 4);
print_word (st, kh, "KH$NAD", 0164, 2);                 /* node address */
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
return kci_svc (uptr);
}

void kci_counters (uint16 rsc, uint16 node)
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

void kci_local_cmd (uint16 rsc, uint16 tq)
{
uint16 opc, arg1, arg2;

opc = GETB (rsc + 014);
arg1 = GETW (rsc + 016);
arg2 = GETW (rsc + 020);
switch (opc) {
    case 001:
//        sim_printf ("local cmd: opc = %d, node = %06o, tq = %06o\n", opc, arg1, tq);
        break;
    case 002:                                           /* show counters */
//        sim_printf ("show counters: tq = %06o\n", tq);
        kci_counters (rsc, arg1);
        break;
    case 003:
    case 004:
//        sim_printf ("local cmd: opc = %d, arg1 = %06o, data mem = %08o, tq = %06o\n", opc, arg1, (arg2 << 6), tq);
        break;
    default:
//        sim_printf ("local cmd: opc = %d, arg1 = %06o, arg2 = %06o, tq = %06o\n", opc, arg1, arg2, tq);
        break;
    }
}

/* Sent to the local node with message "SELF" at offset 020 */

void kci_cmd010 (uint16 rsc, uint16 tq)
{
uint16 node, arg1;

node = GETB (rsc + 012);
arg1 = GETB (rsc + 014);
//sim_printf ("CMD010: node = %d, arg1 = %d, arg2 = ", node, arg1);
//for (int j = 0; j < 4; j++)
//    sim_printf ("%c", GETB (rsc + 020 + j));
//sim_printf (", tq = %06o\n", tq);
}

/* Send a datagram */

void kci_send (uint16 rsc, uint16 tq)
{
uint16 opc, flgs, node;
char path = '?';

flgs = GETB (rsc + 011);
node = GETB (rsc + 012);                                /* destination node */
opc = GETB (rsc + 014);                                 /* opcode */
if (flgs & 040)                                         /* path A */
    path = 'A';
else if (flgs & 0100)                                   /* path B */
    path = 'B';
switch (opc) {
    case 005:                                           /* REQID */
//        sim_printf ("REQID: node = %d, path = %c, tq = %06o\n", node, path, tq);
        break;
    default:
//        sim_printf ("Send datagram: node = %d, path = %c, opc = %d, tq = %06o\n", node, path, opc, tq);
        break;
    }
}

void kci_process (uint16 rsc, uint16 tq)
{
uint16 opc = GETB (rsc + 010);
switch (opc) {
    case 000:                                           /* local command */
        kci_local_cmd (rsc, tq);
        break;
    case 010:                                           /* unknown */
        kci_cmd010 (rsc, tq);
        break;
    case 012:                                           /* send datagram */
        kci_send (rsc, tq);
        break;
    default:
//        for (int i = 0; i < 10; i++)
//            sim_printf ("%04o : %06o\n", (i << 1), GETW (rsc + (i << 1)));
//        sim_printf ("----\n");
        break;
    }
}

t_stat kci_svc (UNIT *uptr)
{
uint16 kh = kci_dib.cma;
uint16 wq, tq, rsc, i;

wq = kh + KH_2WK;                                       /* process 2WK queue */
for (i = 0; i < 20; i++) {                              /* process maximum of 20 items */
    if (Q_LOCKED (wq + Q_HEAD))                         /* source queue locked */
        break;
    if (Q_EMPTY (wq + Q_HEAD))                          /* source queue empty */
        break;
    rsc = GETP (wq + Q_HEAD);                           /* get the resource pointer */
    tq = GETP (rsc + 0004);                             /* get destination queue pointer */
    if (tq == 0)                                        /* resource has no destination */
        break;
    if (Q_LOCKED (tq))                                  /* destination queue locked */
        break;

    rsc = get_from_queue (wq);                          /* get resource from work queue */
    kci_process (rsc, tq);                              /* process resource */
    put_to_queue (tq, rsc);                             /* put resource to destination queue */
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
