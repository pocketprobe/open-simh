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

uint16 kci_kh;

/* The version must be at least DS = 2, MC = 44 */

void kci_init ()
{
kci_kh = k_init_common ();
sim_printf ("K.ci startup: KH = %06o at %o\n", kci_kh, (uint16)inst_pc);
PUTW (kci_kh + KH_NAD, 3);                              /* KH$NAD - CI node address */
PUTW (kci_kh + KG_VRS, 01511);                          /* KG$VRS - version (DS = 3, MC = 73) */
}

t_stat kci_show (int32 flag, CONST char *ptr)
{
uint16 kh = kci_kh;
uint16 wk2h, wk2t;

print_word (kh, "KH$LNK", KH_LNK, 2);

wk2h = kh + KH_LNK;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   LNK", 0000, 2);
    print_word (wk2h, "   LNK", 0002, 2);
    print_word (wk2h, "   LNK", 0004, 2);
    print_word (wk2h, "   LNK", 0006, 2);
    print_word (wk2h, "   LNK", 0010, 2);
    print_word (wk2h, "   LNK", 0012, 2);
    print_word (wk2h, "   LNK", 0014, 2);
    print_word (wk2h, "   LNK", 0016, 2);
    print_word (wk2h, "   LNK", 0020, 2);
}

print_word (kh, "KH$FLG", 0002, 2);
print_word (kh, "KH$3WK", 0004, 4);
print_word (kh, "KH$2WK", 0010, 4);
wk2h = kh + KH_2WK;
for (;;) {
    wk2h = GETP (wk2h);
    if ((wk2h == 0) || wk2h == 0100000)
        break;
    print_word (wk2h, "   2WK", 0000, 2);
    print_word (wk2h, "   2WK", 0002, 2);
    print_word (wk2h, "   2WK", 0004, 2);
    print_word (wk2h, "   2WK", 0006, 2);
    print_word (wk2h, "   2WK", 0010, 2);
    print_word (wk2h, "   2WK", 0012, 2);
    print_word (wk2h, "   2WK", 0014, 2);
    print_word (wk2h, "   2WK", 0016, 2);
    print_word (wk2h, "   2WK", 0020, 2);
    sim_printf ("   ---\n");
    }
print_word (kh, "KH$1WK", 0014, 4);
print_word (kh, "KH$0WK", 0020, 4);
print_word (kh, "KH$EQ ", 0024, 4);
print_word (kh, "KH$SRR", 0030, 4);
print_word (kh, "KH$DRR", 0034, 4);
print_word (kh, "KG$TLK", 0040, 2);
print_word (kh, "KG$SLT", 0042, 1);
print_word (kh, "KG$TYP", 0043, 1);
print_word (kh, "KG$VRS", 0044, 2);
print_word (kh, "KG$FCB", 0046, 2);
print_word (kh, "KG$FBU", 0050, 2);
print_word (kh, "KG$BWS", 0052, 2);
print_word (kh, "KG$TMR", 0054, 2);
print_word (kh, "KG$DMP", 0056, 2);

wk2h = kh + 0056;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   DMP", 0000, 2);
    print_word (wk2h, "   DMP", 0002, 2);
    print_word (wk2h, "   DMP", 0004, 2);
    print_word (wk2h, "   DMP", 0006, 2);
    print_word (wk2h, "   DMP", 0010, 2);
    print_word (wk2h, "   DMP", 0012, 2);
    print_word (wk2h, "   DMP", 0014, 2);
    print_word (wk2h, "   DMP", 0016, 2);
    print_word (wk2h, "   DMP", 0020, 2);
}

print_word (kh, "KG$DCA", 0060, 2);
print_word (kh, "KG$TST", 0062, 2);
print_word (kh, "KG$FRS", 0064, 2);
print_word (kh, "KG$LAS", 0066, 2);
print_word (kh, "KG$MDL", 0070, 0);
print_word (kh, "KG$MTY", 0070, 2);
print_word (kh, "KG$PFL", 0072, 2);
print_word (kh, "KG$RSL", 0074, 2);
print_word (kh, "KG$LTR", 0076, 2);
print_word (kh, "KG$PSA", 0100, 2);
print_word (kh, "KG$PSB", 0102, 2);
print_word (kh, "KG$DSA", 0104, 20);
print_word (kh, "KG$MC0", 0130, 8);
print_word (kh, "KG$KSA", 0140, 0);
print_word (kh, "KH$D2T", 0140, 2);
print_word (kh, "KH$CIM", 0142, 2);

wk2h = kh + 0142;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   CIM", 0000, 2);
    print_word (wk2h, "   CIM", 0002, 2);
    print_word (wk2h, "   CIM", 0004, 2);
    print_word (wk2h, "   CIM", 0006, 2);
    print_word (wk2h, "   CIM", 0010, 2);
    print_word (wk2h, "   CIM", 0012, 2);
    print_word (wk2h, "   CIM", 0014, 2);
    print_word (wk2h, "   CIM", 0016, 2);
    print_word (wk2h, "   CIM", 0020, 2);
}

print_word (kh, "KH$D1T", 0144, 2);
print_word (kh, "KH$VA ", 0146, 2);
print_word (kh, "KH$D0T", 0150, 2);
print_word (kh, "KH$CVS", 0152, 2);
print_word (kh, "KH$TMR", 0154, 4);
print_word (kh, "KH$BFR", 0160, 4);
print_word (kh, "KH$NAD", 0164, 2);
print_word (kh, "KH$PDS", 0166, 1);
print_word (kh, "KH$KDS", 0167, 1);
print_word (kh, "KH$MBA", 0170, 1);
print_word (kh, "KH$MXD", 0171, 1);
print_word (kh, "KH$BSV", 0172, 2);
print_word (kh, "KH$ABQ", 0174, 2);

wk2h = kh + 0174;
wk2h = GETP (wk2h);
if (wk2h != 0) {
    print_word (wk2h, "   ABQ", 0000, 2);
    print_word (wk2h, "   ABQ", 0002, 2);
    print_word (wk2h, "   ABQ", 0004, 2);
    print_word (wk2h, "   ABQ", 0006, 2);
    print_word (wk2h, "   ABQ", 0010, 2);
    print_word (wk2h, "   ABQ", 0012, 2);
    print_word (wk2h, "   ABQ", 0014, 2);
    print_word (wk2h, "   ABQ", 0016, 2);
    print_word (wk2h, "   ABQ", 0020, 2);
}

return SCPE_OK;
}

t_stat kci_process (int32 flag, CONST char *ptr)
{
uint16 kh = kci_kh;
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
