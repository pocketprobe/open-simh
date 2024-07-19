/* dec_ctp.c: CI Test Protocol

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
#include "dec_ctp.h"

/* SYSAP - CTP$RESPONDER */

const char* ctp_types[] = {                             /* CTP debug definitions */
    "FUNCT",
    "BUFMAP",
    "BUFUNM",
    "MOVBUF",
    "GENMSG",
    "GENDGR",
    "GENRST",
    "GENSTR",
    "NOACT",
    "CONFIG",
    "COUNTS",
    "CONNECT",
    "FINISH",
    "MBUFMAP",
    "MBUFUNM",
    "MOVMBUF",
    "MSTATE"
    };

char ctp_buf[MAX_BUF_MAP][MAX_BUF_LEN];
uint32 ctp_map;

void ctp_debug_pkt (UNIT *uptr, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, CTP_OPCODE);
uint8 rsp = opcode & CTP_RESPONSE;
opcode = opcode & ~CTP_RESPONSE;

if (opcode <= CTP_MSTATEREQ) {
    if (rsp)
        sim_debug_unit (DBG_SCS, uptr, "==> %sRSP\n", ctp_types[opcode]);
    else
        sim_debug_unit (DBG_SCS, uptr, "<== %sREQ\n", ctp_types[opcode]);
    }
else
    sim_debug_unit (DBG_SCS, uptr, "??? CTP\n");
}

t_stat ctp_send_rsp (UNIT *uptr, int32 cid, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, CTP_OPCODE);
CI_PUTB (pkt->data, CTP_OPCODE, (opcode | CTP_RESPONSE));
ctp_debug_pkt (uptr, pkt);
return scs_send_msg (uptr, cid, pkt);                   /* send CTP response */
}

t_stat ctp_send_dg (UNIT *uptr, int32 cid, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, CTP_OPCODE);
CI_PUTB (pkt->data, CTP_OPCODE, (opcode | CTP_RESPONSE));
ctp_debug_pkt (uptr, pkt);
return scs_send_dg (uptr, cid, pkt);                    /* send CTP response */
}

t_stat ctp_fill (UNIT *uptr, CI_PKT *ctp, CI_PKT *pkt)
{
uint8 fill = CI_GETB (ctp->data, CTP_GENFUNCT);
uint16 len = CI_GETW (ctp->data, CTP_GENLENGTH);
uint8 val;
uint32 i;

switch (fill) {
    case CTP_GENFILL:                                   /* fill buffer with constant */
        val = CI_GETB (ctp->data, CTP_GENCONST);
        memset (&pkt->data[CTP_GENDATA], val, len);
        break;

    case CTP_GENFBPAIR:                                 /* fill buffer with byte pairs */
        for (i = 0; i < len; i = i + 2) {
            CI_PUTW (pkt->data, CTP_GENDATA + i, (i >> 1));
            }
        break;

    case CTP_GENFNODE:                                  /* fill buffer with local port number */
        memset (&pkt->data[CTP_GENDATA], uptr->ci_node, len);
        break;

    case CTP_GENFIMAGE:                                 /* fill buffer with image data */
        memcpy (&pkt->data[CTP_GENDATA], &ctp->data[CTP_IMAGEDATA], len);
        break;
    }
pkt->length = CTP_GENDATA + len;
return SCPE_OK;
}

/* function set */

t_stat ctp_funct (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
CI_PUTB (pkt->data, CTP_STATUS, CTP_SUCCESS);
CI_PUTL (pkt->data, CTP_FMASK, 0x0001FFFF);
pkt->length = CTP_FMASK + 4;
return SCPE_OK;
}

/* buffer map */

t_stat ctp_bufmap (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint32 len = CI_GETL (pkt->data, CTP_BUFLENGTH);        /* length to map */
uint32 name;
t_stat r;

if (len > MAX_BUF_LEN) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NORESOURCE);
    return SCPE_OK;
    }
if (ctp_map == MAX_BUF_MAP) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_BUFLIMIT);
    return SCPE_OK;
    }
r = scs_map (uptr, ctp_buf[ctp_map], len, 0, &name);
if (r == SCPE_OK) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_SUCCESS);
    //CI_PUTL (pkt->data, CTP_BUFLENGTH, len);            /* actual length */
    CI_PUTL (pkt->data, CTP_BUFLNAME, name);            /* buffer name */
    ctp_map++;
    }
else {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NORESOURCE);
    }
return SCPE_OK;
}

/* buffer unmap */

t_stat ctp_bufunmap (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint32 name = CI_GETL (pkt->data, CTP_BUFLNAME);
t_stat r;

r = scs_unmap (uptr, name);
if (r == SCPE_OK) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_SUCCESS);
    ctp_map--;
    }
else {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NOBUFMAPPED);
    }
return SCPE_OK;
}

/* buffer move */

t_stat ctp_move (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
// CTP_BUFLNAME
// CTP_OTHERNODE
// CTP_BUFLENGTH
// CTP_BUFLOFSET
// CTP_BUFROFSET
// CTP_BUFRNAME
// CTP_MOVETYPE

// int32 scs_write (UNIT *uptr, uint32 *bd, uint8 *buf, size_t xfrsz, UNIT *aptr)
// int32 scs_read (UNIT *uptr, uint32 *bd, uint8 *buf, size_t xfrsz, UNIT *aptr)

// CTP_STATUS
// CTP_ACTCOUNT
return SCPE_OK;
}

/* generate message */

t_stat ctp_genmsg (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
CI_PKT ci_pkt;
uint32 rep, i;
t_stat r;

rep = CI_GETW (pkt->data, CTP_REPCOUNT);
CI_PUTB (pkt->data, CTP_STATUS, CTP_SUCCESS);           /* assume success */
memcpy (ci_pkt.data, pkt->data, CTP_IMAGEDATA);         /* copy packet and CTP header */
ci_pkt.length = pkt->length;
ctp_fill (uptr, pkt, &ci_pkt);                          /* fill packet with generated data */
for (i = 0; i <= rep; i++) {
    CI_PUTW (ci_pkt.data, CTP_ACTCOUNT, i);             /* set actual count */
    r = ctp_send_rsp (uptr, cid, &ci_pkt);              /* send response */
    if (r != SCPE_OK)
        break;
    }
//CI_PUTW (pkt->data, CTP_ACTCOUNT, i);                   /* set actual count */
return r;
}

/* generate datagram */

t_stat ctp_gendg (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
CI_PKT ci_pkt;
uint32 rep, i;
t_stat r;

rep = CI_GETW (pkt->data, CTP_REPCOUNT);
CI_PUTB (pkt->data, CTP_STATUS, CTP_SUCCESS);           /* assume success */
memcpy (ci_pkt.data, pkt->data, CTP_IMAGEDATA);         /* copy packet and CTP header */
ci_pkt.length = pkt->length;
ctp_fill (uptr, pkt, &ci_pkt);                          /* fill packet with generated data */
for (i = 0; i < rep; i++) {
    CI_PUTW (ci_pkt.data, CTP_ACTCOUNT, i);             /* set actual count */
    r = ctp_send_dg (uptr, cid, &ci_pkt);               /* send response */
    if (r != SCPE_OK)
        break;
    }
CI_PUTW (pkt->data, CTP_ACTCOUNT, i);                   /* set actual count */
return r;
}

/* generate reset */

t_stat ctp_genrst (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
CI_PKT ci_pkt;
uint32 ext, flags;

ext = CI_GETB (pkt->data, CTP_EXTEND);                  /* get reset type */
if (ext == CTP_UNCONDRST)
    flags = CI_F;
else if (ext == CTP_CONDRST)
    flags = 0;
else {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NONSENSE);
    return SCPE_OK;
    }
ci_pkt.node = CI_GETB (pkt->data, CTP_OTHERNODE);
if (ci_pkt.node >= CI_MAX_NODES) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NONSENSE);
    return SCPE_OK;
    }
CI_PUTB (ci_pkt.data, CI_OPC, OPC_RST);
CI_PUTB (ci_pkt.data, CI_FLAGS, flags);
ci_pkt.path = CI_PATH_AUTO;
ci_pkt.length = LEN_RST;
return ci_send (uptr, &ci_pkt);                         /* send CI packet */
}

/* generate start */

t_stat ctp_genstr (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
CI_PKT ci_pkt;
uint32 addr, ext, flags, rsv;

ext = CI_GETB (pkt->data, CTP_EXTEND);                  /* get start type */
if (ext == CTP_DEFALTADR)
    flags = CI_DSA;
else if (ext == CTP_SPECIFADR)
    flags = 0;
else {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NONSENSE);
    return SCPE_OK;
    }
rsv = CI_GETW (pkt->data, CTP_RESERV10);
if (rsv != 0) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_NONSENSE);
    return SCPE_OK;
    }
addr = CI_GETL (pkt->data, CTP_STARTADR);               /* get start address */
CI_PUTB (ci_pkt.data, CI_OPC, OPC_STRT);
CI_PUTB (ci_pkt.data, CI_FLAGS, flags);
CI_PUTL (ci_pkt.data, CI_ADDR, addr);
ci_pkt.node = CI_GETB (pkt->data, CTP_OTHERNODE);
ci_pkt.path = CI_PATH_AUTO;
ci_pkt.length = LEN_STRT;
return ci_send (uptr, &ci_pkt);                         /* send CI packet */
}

/* start/stop poller */

t_stat ctp_noact (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* configuration */

t_stat ctp_config (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint32 state[2];
uint32 node;
t_stat r;

node = CI_GETB (pkt->data, CTP_OTHERNODE);
r = scs_config_path (uptr, node, state);
if (r == SCPE_OK) {
    state[0] = (state[0] != 0) ? 1 : 0;                 /* convert to boolean */
    state[1] = (state[1] != 0) ? 1 : 0;
    CI_PUTB (pkt->data, CTP_STATUS, CTP_SUCCESS);
    CI_PUTW (pkt->data, CTP_CFGP0STS, state[0]);
    CI_PUTW (pkt->data, CTP_CFGP1STS, state[1]);
    pkt->length = CTP_CFGP1STS + 2;
    }
return r;
}

/* read counters */

t_stat ctp_counts (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* third party connect */

t_stat ctp_connect (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* listener disconnect */

t_stat ctp_finish (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* maintenance buffer map */

t_stat ctp_mbufmap (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* maintenance buffer unmap */

t_stat ctp_mbufunmap (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* maintenance buffer move */

t_stat ctp_mmove (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* set maintenance state */

t_stat ctp_mstate (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

t_stat app_ctp (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, CTP_OPCODE);
uint32 mtype = CI_GETW (pkt->data, SCS_MTYPE);
t_stat r = SCPE_OK;
uint32 rep, delay;

ctp_debug_pkt (uptr, pkt);

switch (opcode) {
    case CTP_MOVBUFREQ:                                 /* opcodes with repeat and delay */
    case CTP_GENMSGREQ:
    case CTP_GENDGRREQ:
    case CTP_GENRSTREQ:
    case CTP_GENSTRREQ:
    case CTP_MOVMBUFREQ:
        rep = CI_GETW (pkt->data, CTP_REPCOUNT);
        delay = CI_GETB (pkt->data, CTP_DELAY);
        break;

    default:
        rep = 1;
        delay = 0;
        break;
    }

switch (opcode) {
    case CTP_FUNCTREQ:                                  /* function set */
        r = ctp_funct (uptr, cid, pkt);
        break;

    case CTP_BUFMAPREQ:                                 /* buffer map */
        r = ctp_bufmap (uptr, cid, pkt);
        break;

    case CTP_BUFUNMREQ:                                 /* buffer unmap */
        r = ctp_bufunmap (uptr, cid, pkt);
        break;

    case CTP_MOVBUFREQ:                                 /* buffer move */
        return ctp_move (uptr, cid, pkt);

    case CTP_GENMSGREQ:                                 /* generate message */
        return ctp_genmsg (uptr, cid, pkt);

    case CTP_GENDGRREQ:                                 /* generate datagram */
        r = ctp_gendg (uptr, cid, pkt);
        break;

    case CTP_GENRSTREQ:                                 /* generate reset */
        return ctp_genrst (uptr, cid, pkt);

    case CTP_GENSTRREQ:                                 /* generate start */
        r = ctp_genstr (uptr, cid, pkt);
        break;

    case CTP_NOACTREQ:                                  /* start/stop poller */
        return ctp_noact (uptr, cid, pkt);

    case CTP_CONFIGREQ:                                 /* configuration */
        r = ctp_config (uptr, cid, pkt);
        break;

    case CTP_COUNTSREQ:                                 /* read counters */
        return ctp_counts (uptr, cid, pkt);

    case CTP_CONNECTREQ:                                /* third party connect */
        return ctp_connect (uptr, cid, pkt);

    case CTP_FINISHREQ:                                 /* listener disconnect */
        return ctp_finish (uptr, cid, pkt);

    case CTP_MBUFMAPREQ:                                /* maintenance buffer map */
        return ctp_mbufmap (uptr, cid, pkt);

    case CTP_MBUFUNMREQ:                                /* maintenance buffer unmap */
        return ctp_mbufunmap (uptr, cid, pkt);

    case CTP_MOVMBUFREQ:                                /* maintenance buffer move */
        return ctp_mmove (uptr, cid, pkt);

    case CTP_MSTATEREQ:                                 /* set maintenance state */
        return ctp_mstate (uptr, cid, pkt);

    default:
        CI_PUTB (pkt->data, CTP_STATUS, CTP_NONSENSE);
        break;
    }

if (r != SCPE_OK) {
    CI_PUTB (pkt->data, CTP_STATUS, CTP_FAILURE);
    }
if (mtype == SCS_APPMSG)
    return ctp_send_rsp (uptr, cid, pkt);               /* send message response */
return ctp_send_dg (uptr, cid, pkt);                    /* send datagram response */
}
