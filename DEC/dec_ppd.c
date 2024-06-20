
#include "sim_defs.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"

const char* ppd_types[] = {                             /* PPD debug definitions */
    "START",
    "STACK",
    "ACK",
    "SCSDG",
    "SCSMSG",
    "ELOG",
    "HOSTSHUT",
    "FU_DG"
    };

extern int32 tmxr_poll;

void ppd_debug_pkt (uint32 dctrl, UNIT *uptr, CI_PKT *pkt)
{
uint32 mtype = CI_GETW (pkt->data, PPD_MTYPE) & PPD_M_MTYPE;

if (mtype == PPD_SCSMSG)
    scs_debug_pkt (dctrl, uptr, pkt);                   /* show SCS layer */
else
    sim_debug_unit (dctrl, uptr, "%s", ppd_types[mtype]); /* show PPD layer */
}

/* Send SCS message */

t_stat ppd_sndmsg (UNIT *uptr, CI_PKT *pkt)
{
    CI_PUTB (pkt->data, CI_OPC, OPC_MSG);
    CI_PUTW (pkt->data, PPD_MTYPE, PPD_SCSMSG);
    pkt->path = CI_PATH_AUTO;
    return ci_send (uptr, pkt);
}

/* Process START datagram */

t_stat ppd_start (UNIT *uptr, CI_PKT *pkt, uint32 dg_type)
{
    SCA_CTX *ctx = uptr->sca_ctx;
    SCA_SB *sb = &ctx->sb[uptr->ci_node];

    CI_PUTW (pkt->data, PPD_MTYPE, dg_type);            /* send START */
    CI_PUTL (pkt->data, PPD_SYSID, sb->sysid);          /* sending system ID */
    CI_PUTW (pkt->data, PPD_SYSID + 4, 0x0800);
    CI_PUTB (pkt->data, PPD_PROTO, PPD_BASE);
    CI_PUTW (pkt->data, PPD_MAXDG, 0x3E);               /* max datagram size */
    CI_PUTW (pkt->data, PPD_MAXMSG, 0x42);              /* max message size */
    sprintf (&pkt->data[PPD_SWTYPE], "HSC ");           /* software type */
    sprintf (&pkt->data[PPD_SWVER], "V600");            /* software version */
    CI_PUTL (pkt->data, PPD_SWIN + 0, 0);               /* software incarnation */
    CI_PUTL (pkt->data, PPD_SWIN + 4, 0);
    sprintf (&pkt->data[PPD_HWTYPE], "HS70");           /* hardware type */
    CI_PUTW (pkt->data, PPD_HWVER + 0, 0x0222);         /* hardware version */
    CI_PUTW (pkt->data, PPD_HWVER + 2, 0x0222);
    CI_PUTW (pkt->data, PPD_HWVER + 4, 0x0222);
    CI_PUTW (pkt->data, PPD_HWVER + 6, 0x0227);
    CI_PUTW (pkt->data, PPD_HWVER + 8, 0x0222);
    CI_PUTW (pkt->data, PPD_HWVER + 10, 0x0222);
    sprintf (&pkt->data[PPD_NODE], "CIPGA   ");         /* node name */
    memset (&pkt->data[PPD_TIME], 0, 8);                /* current time */
    pkt->length = (dg_type == PPD_START) ? LEN_START : LEN_STACK;
    pkt->path = CI_PATH_AUTO;

    return ci_send (uptr, pkt);
}

/* Process STACK datagram */

t_stat ppd_stack (UNIT *uptr, CI_PKT *pkt)
{
    CI_PUTW (pkt->data, PPD_MTYPE, PPD_ACK);
    pkt->length = LEN_ACK;
    pkt->path = CI_PATH_AUTO;

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

    case PPD_ACK:
        if (pb->vc_state == START_REC) {
            ci_open_vc (uptr, port);                    /* open VC */
            pb->vc_state = VC_OPEN;
            }
        break;

    case PPD_SCSMSG:
        return scs_receive (uptr, pkt);

    default:
        break;
        }
return SCPE_OK;
}

t_stat ppd_id (UNIT *uptr, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
SCA_PB *pb = &ctx->sb[pkt->node].pb;

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
pkt.path = CI_PATH_AUTO;
pkt.data[CI_OPC] = OPC_SNTDAT;                          /* opcode */
// TODO: Need to set flags (multiple)
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
pkt.path = CI_PATH_AUTO;
CI_PUTB (pkt.data, CI_OPC, OPC_DATREQ0);                /* opcode */
// TODO: Need to set flags (multiple, last packet)
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

//    case OPC_LB:
//        return ppd_lb (uptr, pkt);

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

/* Request remote port ID */

t_stat ppd_reqid (UNIT *uptr, CI_PKT *pkt)
{
pkt->length = LEN_IDREQ;
pkt->data[CI_OPC] = OPC_IDREQ;
return ci_send (uptr, pkt);
}

t_stat ppd_reset (DEVICE *dptr)
{
UNIT *uptr = dptr->units;
SCA_CTX *ctx;
int i;

uptr->dptr = dptr;
uptr->sca_ctx = (void *) realloc (uptr->sca_ctx, sizeof (SCA_CTX));
ctx = (SCA_CTX *)uptr->sca_ctx;

for (i = 0; i < CI_MAX_NODES; i++)
    ctx->sb[i].pb.vc_state = VC_CLOSED;

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
