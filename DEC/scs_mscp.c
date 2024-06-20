
//#include "sim_defs.h"
#include "vax_defs.h"
#include "pdp11_uqssp.h"
#include "vax_rf.h"
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
