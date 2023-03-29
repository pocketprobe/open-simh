/* hsc70_ksdi.c: HSC70 K.sdi simulation

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

uint16 ksdi_kh;

/* The version must be at least DS = 3, MC = 00 */

void ksdi_init (uint32 req)
{
ksdi_kh = k_init_common ();
sim_printf ("K.sdi %d startup: KH = %06o at %o\n", req, ksdi_kh, (uint16)inst_pc);
PUTW (ksdi_kh + KG_RSL, 1);                             /* KG$RSL - result of self test */
PUTW (ksdi_kh + KG_VRS, 01452);                         /* KG$VRS - version (DS = 3, MC = 42) */
}

t_stat ksdi_show (int32 flag, CONST char *ptr)
{
uint16 kh = ksdi_kh;
uint16 itm;

print_word (kh, "KT$BAS", 0000, 0);
print_word (kh, "KT$IPQ", 0000, 4);
print_word (kh, "KT$WQH", 0004, 2);
print_word (kh, "KT$WQT", 0006, 2);
print_word (kh, "KT$FLA", 0010, 2);
print_word (kh, "KT$SQP", 0012, 2);
print_word (kh, "KT$TCT", 0014, 2);
print_word (kh, "KT$LAH", 0016, 2);
print_word (kh, "KT$WAI", 0020, 2);
print_word (kh, "KT$SEM", 0022, 12);
print_word (kh, "KT$BW ", 0036, 2);

print_word (kh, "KG$TLK", 0040, 2);
print_word (kh, "KG$SLT", 0042, 1);
print_word (kh, "KG$TYP", 0043, 1);
print_word (kh, "KG$VRS", 0044, 2);
print_word (kh, "KG$FCB", 0046, 2);
print_word (kh, "KG$FBU", 0050, 2);
print_word (kh, "KG$BWS", 0052, 2);
print_word (kh, "KG$TMR", 0054, 2);
print_word (kh, "KG$DMP", 0056, 2);

itm = kh + 0056;
itm = GETP (itm);
if (itm != 0) {
    print_word (itm, "   DMP", 0000, 2);
    print_word (itm, "   DMP", 0002, 2);
    print_word (itm, "   DMP", 0004, 2);
    print_word (itm, "   DMP", 0006, 2);
    print_word (itm, "   DMP", 0010, 2);
    print_word (itm, "   DMP", 0012, 2);
    print_word (itm, "   DMP", 0014, 2);
    print_word (itm, "   DMP", 0016, 2);
    print_word (itm, "   DMP", 0020, 2);
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

print_word (kh, "KT$S0 ", 0140, 16);
print_word (kh, "KT$S1 ", 0160, 16);
print_word (kh, "KT$S2 ", 0200, 16);
print_word (kh, "KT$S3 ", 0220, 16);
print_word (kh, "UNUSED", 0240, 64);
print_word (kh, "KT$DMP", 0340, 66);
print_word (kh, "KT$AQH", 0442, 2);
print_word (kh, "KT$AQT", 0444, 2);
print_word (kh, "KT$BCN", 0446, 2);
print_word (kh, "KT$TMO", 0450, 2);
print_word (kh, "KT$FCN", 0452, 2);
print_word (kh, "KT$FCT", 0454, 2);
print_word (kh, "KT$T1 ", 0456, 2);
print_word (kh, "KT$T2 ", 0460, 2);
print_word (kh, "KT$T3 ", 0462, 2);
print_word (kh, "KT$T4 ", 0464, 2);

return SCPE_OK;
}

t_stat ksdi_process (int32 flag, CONST char *ptr)
{
uint16 kh = ksdi_kh;
uint16 fb, tq, rsc;

//rsc = GETP (kh + KT_S0 + 004);                         /* get the resource pointer */

#if 0
if (GETW (kh + KT_S0 + 004) == 0) {
    tq = GETP (kh + KT_S0);
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    put_to_queue (tq, kh + KT_S0);                      /* put resource to destination queue */
    return SCPE_OK;
    }
//PUTW (kh + KT_S0 + 016, 3);
#endif

if (GETW (kh + KT_S1 + 004) == 0) {
    tq = GETP (kh + KT_S1);
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    put_to_queue (tq, kh + KT_S1);                      /* put resource to destination queue */
    return SCPE_OK;
    }
else {
    rsc = GETP (kh + KT_S1 + 004);
    tq = GETP (rsc + 014);
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    rsc = get_from_queue (kh + KT_S1 + 004);
    put_to_queue (tq, rsc);
    }

#if 0
if (GETW (kh + KT_S2 + 004) == 0) {
    tq = GETP (kh + KT_S2);
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    put_to_queue (tq, kh + KT_S2);                      /* put resource to destination queue */
    return SCPE_OK;
    }

if (GETW (kh + KT_S3 + 004) == 0) {
    tq = GETP (kh + KT_S3);
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    put_to_queue (tq, kh + KT_S3);                      /* put resource to destination queue */
    }
#endif    
return SCPE_OK;
}
