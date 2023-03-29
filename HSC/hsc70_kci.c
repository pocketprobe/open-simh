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

t_stat kci_svc (UNIT *uptr);
t_stat kci_reset (DEVICE *dptr);
t_stat kci_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat kci_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc);

/* debugging bitmaps */

#define DBG_TRC         KREQ_DBG_TRC                    /* trace routine calls */

DEBTAB kci_debug[] = {
    { "TRACE",  DBG_TRC,   "trace routine calls" },
    { 0 }
};

DIB kci_dib = {
    0, 0, NULL, NULL,
    0, 0, 0, { NULL }
    };

UNIT kci_unit = { UDATA (&kci_svc, 0, 0) };

REG kci_reg[] = {
    { NULL }
    };

MTAB kci_mod[] = {
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      NULL, &k_show_addr, NULL },
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
    &kci_dib, DEV_DEBUG,
    0, kci_debug
    };


/* The version must be at least DS = 2, MC = 44 */

void kci_init ()
{
kci_dib.ba = k_init_common ();
sim_debug (DBG_TRC, &kci_dev, "K.ci startup: KH = %06o at %o\n", kci_dib.ba, (uint16)inst_pc);
PUTW (kci_dib.ba + KH_NAD, 3);                          /* KH$NAD - CI node address */
PUTW (kci_dib.ba + KG_VRS, 01511);                      /* KG$VRS - version (DS = 3, MC = 73) */
}

t_stat kci_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint16 kh = kci_dib.ba;

print_word (st, kh, "KH$LNK", KH_LNK, 2);
print_word (st, kh, "KH$FLG", 0002, 2);
print_word (st, kh, "KH$3WK", 0004, 4);
print_word (st, kh, "KH$2WK", 0010, 4);
print_word (st, kh, "KH$1WK", 0014, 4);
print_word (st, kh, "KH$0WK", 0020, 4);
print_word (st, kh, "KH$EQ ", 0024, 4);
print_word (st, kh, "KH$SRR", 0030, 4);
print_word (st, kh, "KH$DRR", 0034, 4);
print_word (st, kh, "KG$TLK", 0040, 2);
print_word (st, kh, "KG$SLT", 0042, 1);
print_word (st, kh, "KG$TYP", 0043, 1);
print_word (st, kh, "KG$VRS", 0044, 2);
print_word (st, kh, "KG$FCB", 0046, 2);
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
print_word (st, kh, "KG$RSL", 0074, 2);
print_word (st, kh, "KG$LTR", 0076, 2);
print_word (st, kh, "KG$PSA", 0100, 2);
print_word (st, kh, "KG$PSB", 0102, 2);
print_word (st, kh, "KG$DSA", 0104, 20);
print_word (st, kh, "KG$MC0", 0130, 8);
print_word (st, kh, "KG$KSA", 0140, 0);
print_word (st, kh, "KH$D2T", 0140, 2);
print_word (st, kh, "KH$CIM", 0142, 2);
print_word (st, kh, "KH$D1T", 0144, 2);
print_word (st, kh, "KH$VA ", 0146, 2);
print_word (st, kh, "KH$D0T", 0150, 2);
print_word (st, kh, "KH$CVS", 0152, 2);
print_word (st, kh, "KH$TMR", 0154, 4);
print_word (st, kh, "KH$BFR", 0160, 4);
print_word (st, kh, "KH$NAD", 0164, 2);
print_word (st, kh, "KH$PDS", 0166, 1);
print_word (st, kh, "KH$KDS", 0167, 1);
print_word (st, kh, "KH$MBA", 0170, 1);
print_word (st, kh, "KH$MXD", 0171, 1);
print_word (st, kh, "KH$BSV", 0172, 2);
print_word (st, kh, "KH$ABQ", 0174, 2);

return SCPE_OK;
}

t_stat kci_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return kci_svc (uptr);
}

t_stat kci_svc (UNIT *uptr)
{
uint16 kh = kci_dib.ba;
uint16 wq, tq, rsc;

wq = kh + KH_2WK;
for (;;) {
    rsc = GETP (wq + Q_HEAD);                           /* get the resource pointer */
    if (rsc == 0)                                       /* queue is empty */
        break;
    if (rsc == 0100000) {                               /* queue is locked */
        sim_printf ("Source queue currently locked\n");
        break;
        }
    tq = GETP (rsc + 0004);                             /* get destination queue pointer */
    if (tq == 0) {                                      /* should not happen */
        sim_printf ("Resource has no desitnation queue\n");
        break;
        }
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        break;
        }

    rsc = get_from_queue (wq);                          /* get resource from work queue */
                                                        /* TODO: process resource */
//    for (int i = 0; i < 20; i = i + 2)
//        sim_printf ("   %06o : %06o\n", i, GETW (rsc + i));
    put_to_queue (tq, rsc);                             /* put resource to destination queue */
    }

return SCPE_OK;
}

t_stat kci_reset (DEVICE *dptr)
{
return SCPE_OK;
}
