/* vax_hsc.c: Hierarchical Storage Controller

   Copyright (c) 2019, Matt Burke

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

   hsc            HSC50/HSC70
*/

#if defined (VM_VAX)                                    /* VAX version */
#include "vax_defs.h"
#else                                                   /* PDP-10 version */
#include "kx10_defs.h"
#endif
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"

#define HSC_TIMER       1

extern int32 tmxr_poll;

t_stat hsc_tmrsvc (UNIT *uptr);
t_stat hsc_svc (UNIT *uptr);
t_stat hsc_reset (DEVICE *dptr);

/* HSC data structures

   hsc_dev    HSC device descriptors
   hsc_unit   HSC unit
   hsc_reg    HSC register list
*/

UNIT hsc_unit[] = {
    { UDATA (&hsc_svc, UNIT_IDLE|UNIT_ATTABLE, 0) },
    { UDATA (&hsc_tmrsvc, UNIT_IDLE|UNIT_DIS, 0) }
    };

REG hsc_reg[] = {
    { NULL }
    };

MTAB hsc_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "NODE", "NODE",
      &ci_set_node, &ci_show_node },
    { MTAB_XTD|MTAB_VDV, 0, "GROUP", "GROUP",
      &ci_set_group, &ci_show_group },
    { MTAB_XTD|MTAB_VDV, 0, "SYSID", "SYSID",
      &ppd_set_sysid, &ppd_show_sysid },
    { 0 }
    };

DEBTAB hsc_debug[] = {
    { "REG",   DBG_REG },
    { "WARN",  DBG_WRN },
    { "DIAG",  DBG_DIAG },
    { "CONF",  DBG_CONF },
    { "PPD",   DBG_PPD },
    { "SCS",   DBG_SCS },
    { "BLK",   DBG_BLK },
    { "LCMD",  DBG_LCMD },
    { "CONN",  DBG_CONN },
    { "TRACE", DBG_TRC },
    { "PKT",   DBG_PKT },
    { 0 }
    };

DEVICE hsc_dev = {
    "HSC", hsc_unit, hsc_reg, hsc_mod,
    2, 0, 0, 0, 0, 0,
    NULL, NULL, &hsc_reset,
    NULL, &ci_attach, &ci_detach,
    NULL, DEV_DEBUG | DEV_DISABLE | DEV_DIS | DEV_CI, 0,
    hsc_debug, 0, 0
    };

/*
CI_NODE ci_node = {
    RPORT_TYPE,
    CODE_REV,
    FUNC_MASK,
    &ci_receive
    };
*/

/* Clock service (roughly once per second) */

t_stat hsc_tmrsvc (UNIT *uptr)
{
t_stat r;

r = ppd_timer (&hsc_unit[0]);
if (r == SCPE_OK)
    sim_activate_after (uptr, 10000000);                /* reactivate */
return r;
}

t_stat hsc_svc (UNIT *uptr)
{
t_stat r;
CI_PKT pkt;

do {
    r = ci_receive (uptr, &pkt);
    if (r == SCPE_OK)
        r = ppd_receive (uptr, &pkt);
} while (r == SCPE_OK);
if (r != CISE_NOPKT)
    return r;
return ci_port_svc (uptr);
}

/* Reset controller */

t_stat hsc_reset (DEVICE *dptr)
{
ci_port_reset (dptr);
if (dptr->flags & DEV_DIS)
    return SCPE_OK;
ppd_reset (dptr);
scs_reset (dptr);
sim_activate_after (dptr->units + HSC_TIMER, 10000000);
ci_rx_enable (&hsc_unit[0], (CI_PATH_A | CI_PATH_B));
return SCPE_OK;
}
