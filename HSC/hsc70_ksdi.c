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

extern int32 tmxr_poll;                                 /* calibrated delay */

t_stat ksdi_svc (UNIT *uptr);
t_stat ksdi_reset (DEVICE *dptr);
t_stat ksdi_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ksdi_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ksdi_init (uint16 req, uint16 act);

/* debugging bitmaps */

#define DBG_TRC         KREQ_DBG_TRC                    /* trace routine calls */

DEBTAB ksdi_debug[] = {
    { "TRACE",  DBG_TRC,   "trace routine calls" },
    { 0 }
};

HDIB ksdi_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    K_SDI, 0, &ksdi_init
    };

UNIT ksdi_unit = { UDATA (&ksdi_svc, 0, 0) };

REG ksdi_reg[] = {
    { NULL }
    };

MTAB ksdi_mod[] = {
    { MTAB_XTD|MTAB_VDV, 004, "SLOT", "SLOT",
      NULL, &k_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      NULL, &k_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "CONTROL", "CONTROL",
      NULL, &ksdi_show, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "RUN", "RUN",
      &ksdi_run, NULL, NULL },
    { 0 }
    };

DEVICE ksdi_dev = {
    "KSDI", &ksdi_unit, ksdi_reg, ksdi_mod,
    1, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &ksdi_reset,
    NULL, NULL, NULL,
    &ksdi_dib, DEV_DEBUG | DEV_HSC,
    0, ksdi_debug
    };


/* The version must be at least DS = 3, MC = 00 */

t_stat ksdi_init (uint16 req, uint16 act)
{
if (act) {
    ksdi_dib.cma = k_init_common ();
    sim_debug (DBG_TRC, &ksdi_dev, "K.sdi %d startup: KH = %06o at %o\n", req, ksdi_dib.cma, (uint16)inst_pc);
    PUTW (ksdi_dib.cma + KG_RSL, 1);                        /* KG$RSL - result of self test */
    PUTW (ksdi_dib.cma + KG_VRS, 01452);                    /* KG$VRS - version (DS = 3, MC = 42) */
    sim_clock_coschedule (&ksdi_unit, tmxr_poll);           /* activate */
    }
else {
    sim_debug (DBG_TRC, &ksdi_dev, "K.sdi %d halt at %o\n", req, (uint16)inst_pc);
    sim_cancel (&ksdi_unit);
    ksdi_dib.cma = 0;
    }
return SCPE_OK;
}

t_stat ksdi_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint16 kh = ksdi_dib.cma;
uint16 itm;

print_word (st, kh, "KT$BAS", 0000, 0);
print_word (st, kh, "KT$IPQ", 0000, 4);
print_word (st, kh, "KT$WQH", 0004, 2);
print_word (st, kh, "KT$WQT", 0006, 2);
print_word (st, kh, "KT$FLA", 0010, 2);
print_word (st, kh, "KT$SQP", 0012, 2);
print_word (st, kh, "KT$TCT", 0014, 2);
print_word (st, kh, "KT$LAH", 0016, 2);
print_word (st, kh, "KT$WAI", 0020, 2);
print_word (st, kh, "KT$SEM", 0022, 12);
print_word (st, kh, "KT$BW ", 0036, 2);

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

print_word (st, kh, "KT$S0 ", 0140, 16);
print_word (st, kh, "KT$S1 ", 0160, 16);
print_word (st, kh, "KT$S2 ", 0200, 16);
print_word (st, kh, "KT$S3 ", 0220, 16);
print_word (st, kh, "UNUSED", 0240, 64);
print_word (st, kh, "KT$DMP", 0340, 66);
print_word (st, kh, "KT$AQH", 0442, 2);
print_word (st, kh, "KT$AQT", 0444, 2);
print_word (st, kh, "KT$BCN", 0446, 2);
print_word (st, kh, "KT$TMO", 0450, 2);
print_word (st, kh, "KT$FCN", 0452, 2);
print_word (st, kh, "KT$FCT", 0454, 2);
print_word (st, kh, "KT$T1 ", 0456, 2);
print_word (st, kh, "KT$T2 ", 0460, 2);
print_word (st, kh, "KT$T3 ", 0462, 2);
print_word (st, kh, "KT$T4 ", 0464, 2);

return SCPE_OK;
}

t_stat ksdi_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return ksdi_svc (uptr);
}

t_stat ksdi_svc (UNIT *uptr)
{
uint16 kh = ksdi_dib.cma;
uint16 tq, rsc;

if (GETW (kh + KG_RSL) == 0) {                          /* test started? */
    PUTW (kh + KG_RSL, 1);                              /* KG$RSL - result of self test */
    rsc = get_from_queue (kh + KT_S0 + 004);            /* get DCB from drive service area */
    tq = GETP (rsc + 014);                              /* get destination queue */
    put_to_queue (tq, rsc);                             /* put DCB to destination */
    }
sim_clock_coschedule (uptr, tmxr_poll);                 /* reactivate */
return SCPE_OK;
}

t_stat ksdi_reset (DEVICE *dptr)
{
return SCPE_OK;
}
