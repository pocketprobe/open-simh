/* scs_mscp.c: MSCP Disk Server

   Copyright (c) 2024, Matt Burke

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

#if defined (VM_VAX)                                    /* VAX version */
#include "vax_defs.h"
#else                                                   /* PDP-10 version */
#include "kx10_defs.h"
#endif
#include "dec_uqssp.h"
#include "dec_rf.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"

/* SYSAP - MSCP$DISK */

t_stat app_mscp (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint16 rf_pkt;
MSC *cp = &rf_ctx;

rf_pkt = 0;
if (!rf_deqf (cp, &rf_pkt))                             /* get cmd pkt */
    return SCPE_IERR;
cp->hat = 0;                                            /* dsbl hst timer */
cp->cid = cid;
cp->uptr = uptr;
memcpy (cp->pak[rf_pkt].d, &pkt->data[SCS_APPL + UQ_HDR_OFF], RF_PKT_SIZE);
rf_mscp (cp, rf_pkt, TRUE);
return SCPE_OK;
}

t_stat app_mscp_done (MSC *cp, uint16 rf_pkt)
{
CI_PKT pkt;

rf_enqh (cp, &cp->freq, rf_pkt);                        /* pkt is free */
cp->pbsy = cp->pbsy - 1;                                /* decr busy cnt */
if (cp->pbsy == 0)                                      /* idle? strt hst tmr */
    cp->hat = cp->htmo;
pkt.length = (SCS_APPL + cp->pak[rf_pkt].d[UQ_HLNT]);
memcpy (&pkt.data[SCS_APPL + UQ_HDR_OFF], cp->pak[rf_pkt].d, RF_PKT_SIZE); /* application message */
return scs_send_msg (cp->uptr, cp->cid, &pkt);
}
