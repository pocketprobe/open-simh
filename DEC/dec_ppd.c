/* dec_ppd.c: Port-to-Port Driver

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

#include "sim_defs.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"

extern int32 tmxr_poll;

/* Set path in outgoing packet */

t_stat ppd_set_path (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_PB *pb = &ctx->sb[pkt->node].pb;
uint32 path[2] = { 0, 0 };
t_stat r = SCPE_OK;
uint32 i;

for (i = 0; i < 2; i++) {
    if ((pb->path_state[i] & PTH_MASK) == PTH_GOOD)     /* good path status */
        path[i]++;                                      /* increment priority */
    if ((pb->path_state[i] & PTH_MASK) >= PTH_CROSSED)  /* good or crossed path status */
        path[i]++;                                      /* increment priority */
    else
        path[i] = 0;                                    /* unusable */
    if ((ctx->lb_state[i] & PTH_MASK) == PTH_GOOD)      /* good loopback status */
        path[i]++;                                      /* increment priority */
    else
        path[i] = 0;                                    /* unusable */
    }
if ((path[0] >= 2) || (path[1] >= 2)) {                 /* at least one path usable? */
    if (path[0] > path[1])                              /* path A higher priority? */
        pkt->path = CI_PATH_A;                          /* use path A */
    else if (path[1] > path[0])                         /* path B higher priority? */
        pkt->path = CI_PATH_B;                          /* use path B */
    else                                                /* equal priority */
        pkt->path = CI_PATH_AUTO;                       /* use random path */
    }
else
    r = CISE_NOPATH;

return r;
}

/* Send SCS datagram */

t_stat ppd_snddg (UNIT *uptr, CI_PKT *pkt)
{
t_stat r;

CI_PUTB (pkt->data, CI_OPC, OPC_DG);
CI_PUTB (pkt->data, CI_FLAGS, 0);
CI_PUTW (pkt->data, PPD_MTYPE, PPD_SCSDG);
r = ppd_set_path (uptr, pkt);
if (r != SCPE_OK)
    return r;
return ci_send (uptr, pkt);
}

/* Send SCS message */

t_stat ppd_sndmsg (UNIT *uptr, CI_PKT *pkt)
{
t_stat r;

CI_PUTB (pkt->data, CI_OPC, OPC_MSG);
CI_PUTB (pkt->data, CI_FLAGS, 0);
CI_PUTW (pkt->data, PPD_MTYPE, PPD_SCSMSG);
r = ppd_set_path (uptr, pkt);
if (r != SCPE_OK)
    return r;
return ci_send (uptr, pkt);
}

/* Request remote port ID */

t_stat ppd_reqid (UNIT *uptr, CI_PKT *pkt)
{
pkt->length = LEN_IDREQ;
pkt->data[CI_OPC] = OPC_IDREQ;
pkt->data[CI_FLAGS] = 0;
CI_PUTL (pkt->data, CI_XCTID + 0, 0);
CI_PUTL (pkt->data, CI_XCTID + 4, 0);
return ci_send (uptr, pkt);
}

/* Send loopback */

t_stat ppd_sndlb (UNIT *uptr, CI_PKT *pkt)
{
uint8 i;

pkt->length = LEN_LB + 0x30;
pkt->data[CI_OPC] = OPC_LB;
pkt->data[CI_FLAGS] = 0;
for (i = 0; i < 0x30; i++)
    pkt->data[CI_TEXT + i] = i;                         /* datagram text */
return ci_send (uptr, pkt);
}

/* Process START datagram */

t_stat ppd_start (UNIT *uptr, CI_PKT *pkt, uint32 dg_type)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_SB *sb = &ctx->sb[uptr->ci_node];
t_stat r;

CI_PUTB (pkt->data, CI_OPC, OPC_DG);
CI_PUTB (pkt->data, CI_FLAGS, 0);
CI_PUTW (pkt->data, PPD_MTYPE, dg_type);                /* send START */
CI_PUTL (pkt->data, PPD_SYSID, sb->sysid);              /* sending system ID */
CI_PUTW (pkt->data, PPD_SYSID + 4, 0x0800);
CI_PUTB (pkt->data, PPD_PROTO, PPD_BASE);
CI_PUTW (pkt->data, PPD_MAXDG, 0x3E);                   /* max datagram size */
CI_PUTW (pkt->data, PPD_MAXMSG, 0x42);                  /* max message size */
sprintf (&pkt->data[PPD_SWTYPE], "HSC ");               /* software type */
sprintf (&pkt->data[PPD_SWVER], "V600");                /* software version */
CI_PUTL (pkt->data, PPD_SWIN + 0, 0);                   /* software incarnation */
CI_PUTL (pkt->data, PPD_SWIN + 4, 0);
sprintf (&pkt->data[PPD_HWTYPE], "HS70");               /* hardware type */
CI_PUTW (pkt->data, PPD_HWVER + 0, 0x0222);             /* hardware version */
CI_PUTW (pkt->data, PPD_HWVER + 2, 0x0222);
CI_PUTW (pkt->data, PPD_HWVER + 4, 0x0222);
CI_PUTW (pkt->data, PPD_HWVER + 6, 0x0227);
CI_PUTW (pkt->data, PPD_HWVER + 8, 0x0222);
CI_PUTW (pkt->data, PPD_HWVER + 10, 0x0222);
sprintf (&pkt->data[PPD_NODE], "CIPGA   ");             /* node name */
memset (&pkt->data[PPD_TIME], 0, 8);                    /* current time */
pkt->length = (dg_type == PPD_START) ? LEN_START : LEN_STACK;
r = ppd_set_path (uptr, pkt);
if (r != SCPE_OK)
    return r;

return ci_send (uptr, pkt);
}

/* Process STACK datagram */

t_stat ppd_stack (UNIT *uptr, CI_PKT *pkt)
{
t_stat r;

CI_PUTB (pkt->data, CI_OPC, OPC_DG);
CI_PUTB (pkt->data, CI_FLAGS, 0);
CI_PUTW (pkt->data, PPD_MTYPE, PPD_ACK);
pkt->length = LEN_ACK;
r = ppd_set_path (uptr, pkt);
if (r != SCPE_OK)
    return r;

return ci_send (uptr, pkt);
}

/* PPD datagram or SCS message received */

t_stat ppd_dg (UNIT *uptr, CI_PKT *pkt)
{
uint32 port = pkt->node;
uint32 mtype = CI_GETW (pkt->data, PPD_MTYPE) & PPD_M_MTYPE;
SCA_CTX *ctx = uptr->sca_ctx;
SCA_PB *pb = &ctx->sb[port].pb;

switch (mtype) {

    case PPD_START:
        if (pb->vc_state == START_SENT) {
            ppd_start (uptr, pkt, PPD_STACK);           /* send STACK */
            pb->vc_state = START_REC;
            }                
        else if (pb->vc_state == START_REC) {
            ppd_start (uptr, pkt, PPD_STACK);           /* send STACK */
            }
        else if (pb->vc_state == VC_OPEN) {
            scs_fail_vc (uptr, port);                   /* notify SCS */
            ci_close_vc (uptr, port);                   /* close VC */
            pb->vc_state = VC_CLOSED;
            }
        else if (pb->vc_state == VC_CLOSED) {
            ppd_reqid (uptr, pkt);                      /* request ID */
            }
        break;

    case PPD_STACK:
        if (pb->vc_state == START_SENT) {
            ppd_stack (uptr, pkt);                      /* send ACK */
            ci_open_vc (uptr, port);                    /* open VC */
            pb->vc_state = VC_OPEN;
            }
        else if (pb->vc_state == START_REC) {
            ppd_stack (uptr, pkt);                      /* send ACK */
            ci_open_vc (uptr, port);                    /* open VC */
            pb->vc_state = VC_OPEN;
            }
        else if (pb->vc_state == VC_OPEN) {
            ppd_stack (uptr, pkt);                      /* send ACK */
            }
        break;

    case PPD_ACK:
        if (pb->vc_state == START_REC) {
            ci_open_vc (uptr, port);                    /* open VC */
            pb->vc_state = VC_OPEN;
            }
        break;

    case PPD_SCSDG:
    case PPD_SCSMSG:
        return scs_receive (uptr, pkt);

    default:
        break;
        }
return SCPE_OK;
}

t_stat ppd_lb (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
uint32 i;

ctx->lb_state[ctx->poll_path - 1] &= ~PTH_TEST;
for (i = 0; i < 0x30; i++)
    if (pkt->data[CI_TEXT + i] != i)                         /* datagram text */
        break;
if (i == 0x30)
    ctx->lb_state[ctx->poll_path - 1] = PTH_GOOD;
else
    ctx->lb_state[ctx->poll_path - 1] = PTH_BAD;

return SCPE_OK;
}

t_stat ppd_id (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_PB *pb = &ctx->sb[pkt->node].pb;

pb->path_state[pkt->path - 1] &= ~PTH_TEST;
if (pkt->data[CI_FLAGS] & CI_PATH_0) {                  /* sent on path A */
    if (pkt->path == CI_PATH_A)                         /* received on path A */
        pb->path_state[pkt->path - 1] = PTH_GOOD;
    else
        pb->path_state[pkt->path - 1] = PTH_CROSSED;
    }
else {                                                  /* sent on path B */
    if (pkt->path == CI_PATH_B)                         /* received on path B */
        pb->path_state[pkt->path - 1] = PTH_GOOD;
    else
        pb->path_state[pkt->path - 1] = PTH_CROSSED;
    }

if (pb->vc_state == VC_CLOSED) {
    pb->vc_state = START_SENT;
    return ppd_start (uptr, pkt, PPD_START);            /* send START */
    }
return SCPE_OK;
}

/* Process request for port ID */

t_stat ppd_idreq (UNIT *uptr, CI_PKT *pkt)
{
CI_PUTB (pkt->data, CI_OPC, OPC_ID);                    /* set opcode */
if (pkt->path == CI_PATH_A)
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_0);           /* set received path */
else
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_1);
CI_PUTL (pkt->data, CI_MAINT_ID, (ID_DUAL | ID_HSC));   /* remote port type */
CI_PUTL (pkt->data, CI_CODE_REV, 0x03060D22);           /* code revision */
CI_PUTL (pkt->data, CI_PORT_FCN, 0xFFFF0D00);           /* function mask */
CI_PUTB (pkt->data, CI_RST_PORT, 0);                    /* resetting port */
CI_PUTB (pkt->data, CI_SYS_STATE, CI_PTST_ENA);         /* port state */
memset (&pkt->data[CI_ID_MBZ], 0, LEN_ID - CI_ID_MBZ);
pkt->length = LEN_ID;
return ci_send (uptr, pkt);
}

/* Send block data */

int32 ppd_snddat (UNIT *uptr, uint32 sbnam)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_BDT *bdt = &ctx->bdt[sbnam];
CI_PKT pkt;
uint32 len, rboff, xfrsz;
t_stat r = SCPE_OK;

pkt.node = bdt->node;                                   /* dest port */
r = ppd_set_path (uptr, &pkt);
if (r != SCPE_OK)
    return -1;                                          /* FIXME: handle errors other than buffer full */
pkt.data[CI_OPC] = OPC_SNTDAT;                          /* opcode */
CI_PUTB (pkt.data, CI_FLAGS, 0);                        /* flags */
CI_PUTL (pkt.data, PPD_LCONID, bdt->bd[BD_CON]);        /* local connection ID */
CI_PUTL (pkt.data, PPD_RSPID, bdt->rspid);              /* local response ID */
CI_PUTL (pkt.data, CI_RBNAM, bdt->bd[BD_NAM]);          /* receive buffer name */

rboff = bdt->bd[BD_OFF] + bdt->boff;
xfrsz = bdt->xfrsz - bdt->boff;
while (xfrsz) {
    len = (xfrsz > CI_MAXDAT) ? CI_MAXDAT : xfrsz;
    pkt.length = len + LEN_DAT;
    CI_PUTL (pkt.data, CI_RBOFF, rboff);                /* receive buffer offset */
    if (xfrsz <= CI_MAXDAT)
        pkt.data[CI_FLAGS] |= CI_LP;                    /* last packet */
    memcpy (&pkt.data[CI_DATA], &bdt->buf[bdt->boff], len);
    r = ci_send (uptr, &pkt);
    if (r != SCPE_OK) {
        sim_printf ("HSC stalled during SNDDAT operation\n");
        sim_activate (bdt->uptr, tmxr_poll);            /* retry */
        return -1;                                      /* FIXME: handle errors other than buffer full */
        }
    bdt->boff += len;
    rboff += len;
    xfrsz -= len;
    }
return -1;                                              /* I/O in progress */
}

/* Request block data */

int32 ppd_reqdat (UNIT *uptr, uint32 rbnam)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_BDT *bdt = &ctx->bdt[rbnam];
CI_PKT pkt;
t_stat r;

pkt.length = LEN_DATREQ;
pkt.node = bdt->node;                                   /* dest port */
r = ppd_set_path (uptr, &pkt);
if (r != SCPE_OK)
    return -1;                                          /* FIXME: handle errors other than buffer full */
CI_PUTB (pkt.data, CI_OPC, OPC_DATREQ0);                /* opcode */
CI_PUTB (pkt.data, CI_FLAGS, 0);                        /* flags */
CI_PUTL (pkt.data, PPD_LCONID, bdt->bd[BD_CON]);        /* local connection ID */
CI_PUTL (pkt.data, PPD_RSPID, bdt->rspid);              /* local response ID */
CI_PUTL (pkt.data, CI_REQ_XFRSZ, bdt->xfrsz);           /* transfer size */
CI_PUTL (pkt.data, CI_REQ_SBNAM, bdt->bd[BD_NAM]);      /* send buffer name */
CI_PUTL (pkt.data, CI_REQ_SBOFF, bdt->bd[BD_OFF]);      /* send buffer offset*/
CI_PUTL (pkt.data, CI_REQ_RBNAM, rbnam);                /* receive buffer name */
CI_PUTL (pkt.data, CI_REQ_RBOFF, 0);                    /* receive buffer offset */
r = ci_send (uptr, &pkt);
if (r != SCPE_OK) {
    sim_printf ("HSC stalled during REQDAT operation\n");
    sim_activate (bdt->uptr, tmxr_poll);                /* retry */
    return -1;                                          /* FIXME: handle errors other than buffer full */
    }
return -1;                                              /* I/O in progress */
}

/* Process received block data */

t_stat ppd_datrec (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
uint32 rbnam = CI_GETW (pkt->data, CI_RBNAM);
uint32 rboff = CI_GETL (pkt->data, CI_RBOFF);
uint32 len = pkt->length - CI_DATA;
SCA_BDT *bdt;

if (rbnam < MAX_BUFF) {                                 /* valid name? */
    bdt = &ctx->bdt[rbnam];
    if ((bdt->buf) && (rboff < bdt->xfrsz)) {           /* valid buffer? */
        memcpy (&bdt->buf[rboff], &pkt->data[CI_DATA], len);
        bdt->boff += len;
        }
    }

if (pkt->data[CI_FLAGS] & CI_LP)                        /* last packet? */
    sim_activate (bdt->uptr, 0);                        /* xfer done, reactivate */
return SCPE_OK;
}

/* Process confirmation of block data */

t_stat ppd_cnfrec (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_BDT *bdt = NULL;
uint32 rspid = CI_GETL (pkt->data, PPD_RSPID);
uint32 sbnam;

for (sbnam = 0; sbnam < MAX_BUFF; sbnam++) {            /* find existing buffer descriptor */
    if (ctx->bdt[sbnam].rspid == rspid) {
        bdt = &ctx->bdt[sbnam];
        break;
        }
    }
if (bdt != NULL)                                        /* valid response ID? */
    sim_activate (bdt->uptr, 0);                        /* xfer done, reactivate */
return SCPE_OK;
}

t_stat ppd_receive (UNIT *uptr, CI_PKT *pkt)
{
uint32 opcode = pkt->data[CI_OPC];

switch (opcode) {

    case OPC_LB:
        return ppd_lb (uptr, pkt);

    case OPC_IDREQ:
        return ppd_idreq (uptr, pkt);

    case OPC_DG:
        return ppd_dg (uptr, pkt);

    case OPC_MSG:
        return ppd_dg (uptr, pkt);

    case OPC_ID:
        return ppd_id (uptr, pkt);

    case OPC_SNTDAT:
    case OPC_RETDAT:
        return ppd_datrec (uptr, pkt);

    case OPC_CNF:
        return ppd_cnfrec (uptr, pkt);
        }
return SCPE_IERR;
}

t_stat ppd_timer (UNIT *uptr)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_PB *pb;
CI_PKT pkt;
uint32 i;

if (ctx->lb_state[ctx->poll_path - 1] & PTH_TEST) {
    ctx->lb_state[ctx->poll_path - 1] = PTH_BAD;
    if ((ctx->lb_state[(ctx->poll_path - 1) ^ 1] & PTH_MASK) == PTH_BAD) {
        for (i = 0; i < CI_MAX_NODES; i++) {
            pb = &ctx->sb[i].pb;
            if (pb->vc_state == VC_OPEN) {
                scs_fail_vc (uptr, i);                  /* notify SCS */
                ci_close_vc (uptr, i);                  /* close VC */
                pb->vc_state = VC_CLOSED;
                }
            }
        }
    }

for (i = 0; i < CI_MAX_NODES; i++) {
    pb = &ctx->sb[i].pb;
    if (pb->path_state[ctx->poll_path - 1] & PTH_TEST) {
        pb->path_state[ctx->poll_path - 1] = PTH_BAD;
        if ((pb->path_state[(ctx->poll_path - 1) ^ 1] & PTH_MASK) == PTH_BAD) {
            if (pb->vc_state == VC_OPEN) {
                scs_fail_vc (uptr, i);                  /* notify SCS */
                ci_close_vc (uptr, i);                  /* close VC */
                pb->vc_state = VC_CLOSED;
                }
            }
        }
    }

if (ctx->poll_path == CI_PATH_A)                        /* switch path */
    ctx->poll_path = CI_PATH_B;
else
    ctx->poll_path = CI_PATH_A;

pkt.node = uptr->ci_node;                               /* send loopback */
pkt.path = ctx->poll_path;
ctx->lb_state[ctx->poll_path - 1] |= PTH_TEST;
ppd_sndlb (uptr, &pkt);

for (i = 0; i < CI_MAX_NODES; i++) {                    /* request IDs */
    pb = &ctx->sb[i].pb;
    pb->path_state[ctx->poll_path - 1] |= PTH_TEST;
    pkt.node = i;
    ppd_reqid (uptr, &pkt);
    }
return SCPE_OK;
}

t_stat ppd_reset (DEVICE *dptr)
{
UNIT *uptr = dptr->units;
SCA_CTX *ctx;
int i;

uptr->dptr = dptr;
uptr->sca_ctx = (void *) realloc (uptr->sca_ctx, sizeof (SCA_CTX));
ctx = (SCA_CTX *)uptr->sca_ctx;

ctx->poll_path = CI_PATH_B;
ctx->lb_state[0] = PTH_GOOD;
ctx->lb_state[1] = PTH_GOOD;
for (i = 0; i < CI_MAX_NODES; i++) {
    ctx->sb[i].pb.vc_state = VC_CLOSED;
    ctx->sb[i].pb.path_state[0] = PTH_BAD;
    ctx->sb[i].pb.path_state[1] = PTH_BAD;
    }

return SCPE_OK;
}

t_stat ppd_show_sysid (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_SB *sb = &ctx->sb[uptr->ci_node];

fprintf (st, "sysid=%d", sb->sysid);
return SCPE_OK;
}

t_stat ppd_set_sysid (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_SB *sb = &ctx->sb[uptr->ci_node];
uint32 newid;
int32 r;

newid = (uint32) get_uint (cptr, 10, 0xFFFFFFFF, &r);
if (r != SCPE_OK)
    return r;
sb->sysid = newid;
return SCPE_OK;
}
