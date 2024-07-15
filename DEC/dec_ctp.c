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
return SCPE_OK;
}

/* buffer map */

t_stat ctp_bufmap (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* buffer unmap */

t_stat ctp_bufunmap (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* buffer move */

t_stat ctp_move (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* generate message */

t_stat ctp_genmsg (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
CI_PKT ci_pkt;
uint32 rep, i;
t_stat r;

rep = CI_GETW (pkt->data, CTP_REPCOUNT);
memcpy (ci_pkt.data, pkt->data, CTP_IMAGEDATA);         /* copy packet and CTP header */
ci_pkt.length = pkt->length;
ctp_fill (uptr, pkt, &ci_pkt);                          /* fill packet with generated data */
for (i = 0; i <= rep; i++) {                            /* send at least once */
    CI_PUTW (ci_pkt.data, CTP_ACTCOUNT, i);             /* set actual count */
    r = ctp_send_rsp (uptr, cid, &ci_pkt);              /* send response */
    if (r != SCPE_OK)
        break;
    }
return r;
}

/* generate datagram */

t_stat ctp_gendg (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
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
return SCPE_OK;
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
        return ctp_funct (uptr, cid, pkt);

    case CTP_BUFMAPREQ:                                 /* buffer map */
        return ctp_bufmap (uptr, cid, pkt);

    case CTP_BUFUNMREQ:                                 /* buffer unmap */
        return ctp_bufunmap (uptr, cid, pkt);

    case CTP_MOVBUFREQ:                                 /* buffer move */
        return ctp_move (uptr, cid, pkt);

    case CTP_GENMSGREQ:                                 /* generate message */
        return ctp_genmsg (uptr, cid, pkt);

    case CTP_GENDGRREQ:                                 /* generate datagram */
        return ctp_gendg (uptr, cid, pkt);

    case CTP_GENRSTREQ:                                 /* generate reset */
        return ctp_genrst (uptr, cid, pkt);

    case CTP_GENSTRREQ:                                 /* generate start */
        r = ctp_genstr (uptr, cid, pkt);
        break;

    case CTP_NOACTREQ:                                  /* start/stop poller */
        return ctp_noact (uptr, cid, pkt);

    case CTP_CONFIGREQ:                                 /* configuration */
        return ctp_config (uptr, cid, pkt);

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
return ctp_send_rsp (uptr, cid, pkt);                   /* send CTP response */
}
