
#include "sim_defs.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"

const char* scs_types[] = {                             /* SCS message debug definitions */
    "Connect Request",
    "Connect Response",
    "Accept Request",
    "Accept Response",
    "Reject Request",
    "Reject Response",
    "Disconnect Request",
    "Disconnect Response",
    "Credit Request",
    "Credit Response",
    "Application Sequenced Message"
    };

extern t_stat app_scsdir (UNIT *uptr, uint32 cid, CI_PKT *pkt);
extern t_stat app_mscp (UNIT *uptr, uint32 cid, CI_PKT *pkt);

SYSAP scs_sysap[] = {
    { "SCS$DIRECTORY", 1, 0, app_scsdir },
    { "MSCP$DISK", 10, 1, app_mscp },
    { 0 }
    };

void scs_debug_pkt (uint32 dctrl, UNIT *uptr, CI_PKT *pkt)
{
uint32 mtype = CI_GETW (pkt->data, SCS_MTYPE);
sim_debug_unit (dctrl, uptr, "%s", scs_types[mtype]);   /* show SCS layer */
}

/* Find a connection with the given id */

SCA_CB *scs_getconn (UNIT *uptr, uint32 cid)
{
SCA_CTX *ctx = uptr->sca_ctx;
if (cid < MAX_CONN) {                                   /* valid? */
    if (ctx->cb[cid].sysap)                             /* connected? */
        return &ctx->cb[cid];
    }
return NULL;
}

void scs_fail_vc (UNIT *uptr, uint32 port)
{
SCA_CTX *ctx = uptr->sca_ctx;
uint32 i;

for (i = 0; i < MAX_CONN; i++) {
    if (ctx->cb[i].node == port)
        memset (&ctx->cb[i], 0, sizeof (SCA_CB));
    }
}

/* Find a SYSAP with the given name */

SYSAP* scs_getsysap (UNIT *uptr, char *name)
{
SCA_CTX *ctx = uptr->sca_ctx;
SYSAP *sysap;

for (sysap = &ctx->sysap[0]; sysap->name; sysap++) {
    if (strncmp (name, sysap->name, strlen (sysap->name)) == 0)
        break;                                          /* name matches */
    }
if (sysap->name)
    return sysap;                                       /* SYSAP found */
else
    return NULL;                                        /* not found */
}

/* Write to named buffer */

int32 scs_write (UNIT *uptr, uint32 *bd, uint8 *buf, size_t xfrsz, UNIT *aptr)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_CB *cb = &ctx->cb[bd[BD_CON]];
SCA_BDT *bdt = NULL;
uint32 sbnam;

for (sbnam = 0; sbnam < MAX_BUFF; sbnam++) {            /* find existing buffer descriptor */
    if (ctx->bdt[sbnam].buf == buf) {
        bdt = &ctx->bdt[sbnam];
        break;
        }
    }
if (bdt == NULL) {
    for (sbnam = 0; sbnam < MAX_BUFF; sbnam++) {        /* find free buffer descriptor */
        if (ctx->bdt[sbnam].xfrsz == 0) {
            bdt = &ctx->bdt[sbnam];
            bdt->xfrsz = xfrsz;
            bdt->buf = buf;                             /* setup buffer */
            bdt->boff = 0;
            bdt->bd = bd;
            bdt->node = cb->node;
            bdt->uptr = aptr;
            bdt->rspid = ctx->rspid++;
            break;
            }
        }
    }
if (bdt == NULL)                                        /* none available? */
    return xfrsz;                                       /* error */

if (bdt->boff == bdt->xfrsz) {                          /* transfer done? */
    bd[BD_OFF] += bdt->boff;                            /* update descriptor */
    bdt->xfrsz = 0;
    bdt->buf = NULL;                                    /* remove buffer */
    bdt->boff = 0;
    bdt->bd = NULL;
    bdt->node = 0;
    bdt->uptr = NULL;
    bdt->rspid = 0;
    return 0;                                           /* done */
    }

return ppd_snddat (uptr, sbnam);
}

/* Read from named buffer */

int32 scs_read (UNIT *uptr, uint32 *bd, uint8 *buf, size_t xfrsz, UNIT *aptr)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_CB *cb = &ctx->cb[bd[BD_CON]];
SCA_BDT *bdt = NULL;
uint32 rbnam;

for (rbnam = 0; rbnam < MAX_BUFF; rbnam++) {            /* find existing buffer descriptor */
    if (ctx->bdt[rbnam].buf == buf) {
        bdt = &ctx->bdt[rbnam];
        break;
        }
    }
if (bdt == NULL) {
    for (rbnam = 0; rbnam < MAX_BUFF; rbnam++) {        /* find free buffer descriptor */
        if (ctx->bdt[rbnam].xfrsz == 0) {
            bdt = &ctx->bdt[rbnam];
            bdt->xfrsz = xfrsz;
            bdt->buf = buf;                             /* setup buffer */
            bdt->boff = 0;
            bdt->bd = bd;
            bdt->node = cb->node;
            bdt->uptr = uptr;
            bdt->rspid = ctx->rspid++;
            break;
            }
        }
    }
if (bdt == NULL)                                        /* none available? */
    return xfrsz;

if (bdt->boff == bdt->xfrsz) {                          /* transfer done? */
    bd[BD_OFF] += bdt->boff;                            /* update descriptor */
    bdt->xfrsz = 0;
    bdt->buf = NULL;                                    /* remove buffer */
    bdt->boff = 0;
    bdt->bd = NULL;
    bdt->node = 0;
    bdt->uptr = NULL;
    bdt->rspid = 0;
    return 0;                                           /* done */
    }

return ppd_reqdat (uptr, rbnam);
}

/* SCS connection request */

t_stat scs_conreq (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
SYSAP *sysap = scs_getsysap (uptr, &pkt->data[SCS_DSTPROC]);
uint32 srccon = CI_GETL (pkt->data, SCS_SRCCON);
uint16 credits = CI_GETW (pkt->data, SCS_CREDIT);
uint32 dat_len = pkt->length - LEN_CONREQ;              /* connection data length */
uint16 stat = SCS_STNORM;
uint32 cid = 0;
uint32 i;

CI_PUTW (pkt->data, SCS_MTYPE, SCS_CONRSP);             /* build CONRSP */
CI_PUTW (pkt->data, SCS_CREDIT, 0);
CI_PUTL (pkt->data, SCS_DSTCON, srccon);
if (!sysap)                                             /* SYSAP not found? */
    stat = SCS_STNOMAT;                                 /* no matching listener */
else {
    for (i = 1; i < MAX_CONN; i++) {
        if (ctx->cb[i].sysap == NULL) {                 /* free connection? */
            ctx->cb[i].sysap = sysap;                   /* init connection */
            ctx->cb[i].node = pkt->node;
            ctx->cb[i].cid = srccon;
            ctx->cb[i].credits = credits;
            cid = i;
            break;
            }
        }
    if (!cid)
        stat = SCS_STNORS;                              /* no resources */
    }
CI_PUTL (pkt->data, SCS_SRCCON, cid);
CI_PUTW (pkt->data, SCS_STATUS, stat);
pkt->length = LEN_CONRSP;
ppd_sndmsg (uptr, pkt);                                 /* send CONRSP */

if (stat == SCS_STNORM)
{
    CI_PUTW (pkt->data, SCS_MTYPE, SCS_ACCREQ);         /* build ACCREQ */
    CI_PUTW (pkt->data, SCS_CREDIT, sysap->inicr);      /* initial credits */
    CI_PUTL (pkt->data, SCS_SRCCON, cid);
    CI_PUTW (pkt->data, SCS_MINCR, sysap->mincr);       /* min credits */
    strncpy (&pkt->data[SCS_DSTPROC], &pkt->data[SCS_SRCPROC], 16);
    sprintf (&pkt->data[SCS_SRCPROC], "%-16s", sysap->name);
    sprintf (&pkt->data[SCS_CONDAT], "%-16s", "");
    pkt->length = LEN_ACCREQ + dat_len;

    ppd_sndmsg (uptr, pkt);                             /* send ACCREQ */
}
return SCPE_OK;
}

/* SCS accept response */

t_stat scs_accrsp (UNIT *uptr, CI_PKT *pkt)
{
return SCPE_OK;
}

/* SCS reject response */

t_stat scs_rejrsp (UNIT *uptr, CI_PKT *pkt)
{
return SCPE_OK;
}

/* SCS disconnect request */

t_stat scs_disreq (UNIT *uptr, CI_PKT *pkt)
{
uint32 cid = CI_GETL (pkt->data, SCS_DSTCON);
uint32 srccon = CI_GETL (pkt->data, SCS_SRCCON);
SCA_CB *cb = scs_getconn (uptr, cid);

CI_PUTW (pkt->data, SCS_MTYPE, SCS_DISRSP);
CI_PUTW (pkt->data, SCS_CREDIT, 0);
CI_PUTL (pkt->data, SCS_DSTCON, srccon);
CI_PUTL (pkt->data, SCS_SRCCON, 0);
if (cb->sysap) {                                        /* connection found? */
    CI_PUTL (pkt->data, SCS_SRCCON, cid);
    cb->node = 0;
    cb->cid = 0;
    cb->sysap = NULL;
    pkt->length = LEN_DISRSP;
    ppd_sndmsg (uptr, pkt);                             /* send DISRSP */
    CI_PUTW (pkt->data, SCS_MTYPE, SCS_DISREQ);
    }
pkt->length = LEN_DISRSP;
ppd_sndmsg (uptr, pkt);                                 /* send DISRSP */
return SCPE_OK;
}

/* SCS credit request */

t_stat scs_crdreq (UNIT *uptr, CI_PKT *pkt)
{
int16 credits = CI_GETW (pkt->data, SCS_CREDIT);
uint32 cid = CI_GETL (pkt->data, SCS_DSTCON);
uint32 srccon = CI_GETL (pkt->data, SCS_SRCCON);
SCA_CB *cb = scs_getconn (uptr, cid);

CI_PUTW (pkt->data, SCS_MTYPE, SCS_CRRSP);
CI_PUTL (pkt->data, SCS_DSTCON, srccon);
CI_PUTL (pkt->data, SCS_SRCCON, 0);
if (cb->sysap) {                                        /* connection found? */
    CI_PUTL (pkt->data, SCS_SRCCON, cid);
    if (credits > 0)
        cb->credits = cb->credits + credits;
    else {
        if ((cb->credits - credits) < cb->sysap->mincr) {
            credits = cb->credits - cb->sysap->mincr;
            cb->credits = cb->sysap->mincr;
            }
        else
            cb->credits = cb->credits - credits;
        }
    }
else
    credits = 0;
CI_PUTW (pkt->data, SCS_CREDIT, credits);
pkt->length = LEN_CRRSP;
ppd_sndmsg (uptr, pkt);                                 /* send CRRSP */
return SCPE_OK;
}

/* Send SCS application message */

t_stat scs_send_msg (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
SCA_CB *cb = scs_getconn (uptr, cid);

CI_PUTW (pkt->data, SCS_CREDIT, 1);
cb->credits--;
CI_PUTL (pkt->data, SCS_DSTCON, cb->cid);
CI_PUTL (pkt->data, SCS_SRCCON, cid);
CI_PUTW (pkt->data, SCS_MTYPE, SCS_APPMSG);
pkt->node = cb->node;

return ppd_sndmsg (uptr, pkt);                          /* send APPMSG */
}

/* SCS application message */

t_stat scs_appmsg (UNIT *uptr, CI_PKT *pkt)
{
uint32 cid = CI_GETL (pkt->data, SCS_DSTCON);
uint16 credits = CI_GETW (pkt->data, SCS_CREDIT);
SCA_CB *cb = scs_getconn (uptr, cid);

if (cb)
{
    cb->credits += credits;
    return cb->sysap->msgrec (uptr, cid, pkt);
}
return SCPE_IERR;
}

/* SCS packet received */

t_stat scs_receive (UNIT *uptr, CI_PKT *pkt)
{
uint32 msg_type = CI_GETW (pkt->data, SCS_MTYPE);

switch (msg_type)
{
    case SCS_CONREQ:                                    /* connect request */
        return scs_conreq (uptr, pkt);

    case SCS_ACCRSP:                                    /* accept response */
        return scs_accrsp (uptr, pkt);

    case SCS_REJRSP:                                    /* reject respone */
        return scs_rejrsp (uptr, pkt);

    case SCS_DISREQ:                                    /* disconnect request */
        return scs_disreq (uptr, pkt);

    case SCS_CRREQ:                                     /* credit request */
        return scs_crdreq (uptr, pkt);

    case SCS_APPMSG:                                    /* application message */
    case SCS_APPDG:                                     /* application datagram */
        return scs_appmsg (uptr, pkt);

    default:
        printf ("Unknown SCS message received: 0x%X\r\n", msg_type);
        break;
}

return SCPE_IERR;
}

t_stat scs_reset (DEVICE *dptr)
{
UNIT *uptr = dptr->units;
SCA_CTX *ctx;
int i;

//uptr->dptr = dptr;
//uptr->sca_ctx = (void *) realloc (uptr->sca_ctx, sizeof (SCA_CTX));
ctx = (SCA_CTX *)uptr->sca_ctx;
if (!ctx)
    return SCPE_IERR;

for (i = 0; i < MAX_CONN; i++) {
    ctx->cb[i].cid = 0;
    ctx->cb[i].credits = 0;
    ctx->cb[i].sysap = NULL;
    ctx->cb[i].node = 0;
    ctx->cb[i].pb = NULL;
    }
for (i = 0; i < MAX_BUFF; i++) {
    ctx->bdt[i].xfrsz = 0;
    ctx->bdt[i].buf = NULL;
    ctx->bdt[i].boff = 0;
    ctx->bdt[i].bd = NULL;
    ctx->bdt[i].node = 0;
    ctx->bdt[i].uptr = NULL;
    ctx->bdt[i].rspid = 0;
    }
ctx->sysap = scs_sysap;
ctx->rspid = 1;

return SCPE_OK;
}
