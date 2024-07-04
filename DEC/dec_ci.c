/* dec_ci.c: Computer Interconnect

   Copyright (c) 2017-2019, Matt Burke

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

   Datagrams:
      Datagrams are sent via UDP multicast

   Virtual Circuits:
      Virtual circuits are implemented using a TCP connection.
*/

#if defined (VM_VAX)                                    /* VAX version */
#include "vax_defs.h"
#else                                                   /* PDP-11 version */
#include "pdp11_defs.h"
#endif
#include "sim_sock.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include <time.h>

/* CI Link States */

#define LINK_CLOSED     0                               /* link closed */
#define LINK_STARTING   1                               /* link opening */
#define LINK_OPEN       2                               /* link open */

/* Unit Specific Data */

#define port_ctx        up7                             /* port context */

#define LEN_TCP_PORT    5

const char* ci_types[] = {                              /* OPC debug definitions */
    "(invalid)",                                        /* 00 */
    "DG",
    "MSG",
    "CNF",
    "(invalid)",
    "IDREQ",                                            /* 05 */
    "RST",
    "STRT",
    "DATREQ0",
    "DATREQ1",
    "DATREQ2",                                          /* 10 */
    "ID",
    "(invalid)",
    "LB",
    "MDATREQ",
    "(invalid)",                                        /* 15 */
    "SNTDAT",
    "RETDAT",
    "SNTMDAT",
    "RETMDAT"
    };

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

const char* ci_path_names[] = {
    "AUTO",
    "A",
    "B",
    "RESERVED"
    };

const char* ci_link_states[] = {
    "CLOSED",
    "STARTING",
    "OPEN"
    };

typedef struct {
    CI_PKT      pkt;
} CI_ITEM;

typedef struct {
    int32       max;
    int32       count;
    int32       head;
    int32       tail;
    int32       loss;
    int32       high;
    CI_ITEM    *item;
} CI_QUE;

#define CI_QUE_MAX           200                        /* message queue array */

typedef struct {
    SOCKET    socket;                                   /* TCP link */
    CI_PKT    rx_pkt;                                   /* receive packet buffer */
} CI_LINK;

typedef struct {
    uint32    conn;                                     /* link state */
    CI_LINK   *link;                                    /* remote link */
    char      host[CBUFSIZE];                           /* IP address of remote system */
    UNIT      *uptr;                                    /* local unit */
    uint32    vcd;                                      /* VC descriptor */
} CI_NODE;

typedef struct {
    char      group[CBUFSIZE];                          /* multicast group address */
    char      tcp_port[CBUFSIZE];                       /* VC port */
    SOCKET    multi_sock;                               /* multicast socket */
    SOCKET    tcp_sock;                                 /* VC socket */
    CI_NODE   nodes[CI_MAX_NODES];                      /* remote node context */
    CI_LINK   links[CI_MAX_NODES];                      /* remote links */
    CI_QUE    rx_queue;                                 /* packet receive queue */
    CI_QUE    tx_queue;                                 /* packet transmit queue */
} CI_PORT;

CI_PKT rcv_pkt;                                         /* receive packet buffer */

extern int32 tmxr_poll;

t_stat ci_send_packet (UNIT *uptr, CI_PKT *pkt);
t_stat ci_receive_packet (UNIT *uptr, CI_PKT *pkt);
t_stat ci_link_fail (UNIT *uptr, CI_LINK *link);
t_stat ciq_init (CI_QUE *que, int32 max);               /* initialize FIFO queue */
void ciq_clear  (CI_QUE *que);                          /* clear FIFO queue */
void ciq_remove (CI_QUE *que);                          /* remove item from FIFO queue */
void ciq_insert (CI_QUE *que, CI_PKT *pkt);             /* insert item into FIFO queue */

void ci_dump_pkt (uint32 dctrl, UNIT *uptr, char* buffer, uint32 length)
{
uint8 tbyte;
uint32 i;
int32 j;

if ((uptr->dptr->dctrl & (DBG_PKT | dctrl)) != (DBG_PKT | dctrl))
    return;

for (i = 0; i < length; i+=4) {
    sim_debug_unit (DBG_PKT, uptr, "  %04X: ", i);
    sim_debug_unit (DBG_PKT, uptr, "  ");
    for (j = 3; j >= 0; j--) {
        if ((i + j) < length) {
            tbyte = buffer[i+j];
            sim_debug_unit (DBG_PKT, uptr, "%02X ", tbyte);
            }
        else
            sim_debug_unit (DBG_PKT, uptr, "   ");
        }
    sim_debug_unit (DBG_PKT, uptr, " ");
    for (j = 0; j < 4; j++) {
        if ((i + j) < length) {
            tbyte = buffer[i+j];
            if ((tbyte > 0x1F) && (tbyte < 0x7F))
                sim_debug_unit (DBG_PKT, uptr, "%c", tbyte);
            else
                sim_debug_unit (DBG_PKT, uptr, ".");
            }
        else
            sim_debug_unit (DBG_PKT, uptr, " ");
        }
    sim_debug_unit (DBG_PKT, uptr, "\n");
    }
}

void scs_debug_pkt (uint32 dctrl, UNIT *uptr, CI_PKT *pkt)
{
uint32 mtype = CI_GETW (pkt->data, SCS_MTYPE);

if (mtype <= SCS_APPDG)
    sim_debug_unit (dctrl, uptr, "%s", scs_types[mtype]); /* show SCS layer */
else
    sim_debug_unit (dctrl, uptr, "0x%X?", mtype);       /* unrecognised */
}

void ppd_debug_pkt (uint32 dctrl, UNIT *uptr, CI_PKT *pkt)
{
uint32 mtype = CI_GETW (pkt->data, PPD_MTYPE);

if (mtype == PPD_SCSMSG)
    scs_debug_pkt (dctrl, uptr, pkt);                   /* show SCS layer */
else if (mtype <= PPD_FUDG)
    sim_debug_unit (dctrl, uptr, "%s", ppd_types[mtype]); /* show PPD layer */
else if (mtype & PPD_M_CCLR)
    sim_debug_unit (dctrl, uptr, "CACHECLR");
else
    sim_debug_unit (dctrl, uptr, "0x%X?", mtype);       /* unrecognised */
}

void ci_debug_pkt (uint32 dctrl, UNIT *uptr, CI_PKT *pkt, int rcv)
{
uint32 opcode = pkt->data[CI_OPC];
uint32 port = pkt->node;
uint32 path = pkt->path;

if (rcv)                                                /* show direction */
    sim_debug_unit (dctrl, uptr, "<== ");
else
    sim_debug_unit (dctrl, uptr, "==> ");
if (opcode <= OPC_MSG)                                  /* datagram or message? */
    ppd_debug_pkt (dctrl, uptr, pkt);                   /* show PPD layer */
else
    sim_debug_unit (dctrl, uptr, "%s", ci_types[opcode]); /* show CI layer */
sim_debug_unit (dctrl, uptr, ", %s: %d, path: %s\n",    /* show routing */
    (rcv ? "src" : "dst"), port, ci_path_names[path]);
ci_dump_pkt (dctrl, uptr, pkt->data, pkt->length);
}

/* CI port state change */

void ci_set_state (UNIT *uptr, uint32 state)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
int32 r;

if (state < PORT_INIT) {                                /* change to uninit */
    if (uptr->ci_state >= PORT_INIT) {                  /* currently initialised? */
        sim_cancel (uptr);                              /* stop poll */
        sim_debug_unit (DBG_CONN, uptr, "deactivating unit\n");
        ciq_clear (&cp->rx_queue);                      /* clear queue */
        }
    }
else if (state >= PORT_INIT) {                          /* change to init */
    if (uptr->ci_state < PORT_INIT) {                   /* currently uninit? */
        sim_clock_coschedule (uptr, tmxr_poll);         /* start poll */
        sim_debug_unit (DBG_CONN, uptr, "activating unit\n");
        ciq_clear (&cp->rx_queue);                      /* clear queue */
        if (cp->multi_sock) {
            do r = sim_read_sock (cp->multi_sock, rcv_pkt.data, CI_MAXFR);
            while (r > 0);                              /* clear network buffer */
            }
        }
   }
uptr->ci_state = state;                                 /* update state */
}

t_stat ci_send (UNIT *uptr, CI_PKT *pkt)
{
uint32 opcode = pkt->data[CI_OPC];
uint8 path_flags;
t_stat r;

if (pkt->node >= CI_MAX_NODES)                          /* port valid? */
    return SCPE_OK;
if (pkt->length > CI_MAXFR) {                           /* check length */
    sim_printf ("CI packet length error in ci_send_ppd: %d\n", pkt->length);
    return SCPE_IERR;
    }

path_flags = (pkt->path << CI_V_PATH) & CI_M_PATH;      /* get send path */
if (!path_flags)                                        /* auto select? */
    path_flags = (CI_PATH_A << CI_V_PATH);              /* use path A */
pkt->data[CI_LEN_HI] = ((pkt->length >> 8) & 0xF);      /* packet length high */
pkt->data[CI_LEN_HI] |= path_flags;                     /* use the reserved bits to store the send path */
pkt->data[CI_LEN_LO] = pkt->length & 0xFF;              /* packet length low */
pkt->data[CI_DEST] = pkt->node;                         /* destination node */
pkt->data[CI_NOT_DEST] = ~pkt->node;
pkt->data[CI_SRC] = uptr->ci_node;                      /* source node */

switch (opcode) {

    case OPC_DG:
        ci_debug_pkt (DBG_PPD, uptr, pkt, 0);
        break;

    case OPC_MSG:
        ci_debug_pkt (DBG_SCS, uptr, pkt, 0);
        break;

    case OPC_CNF:
        ci_debug_pkt (DBG_BLK, uptr, pkt, 0);
        break;

    case OPC_IDREQ:
        ci_debug_pkt (DBG_CONF, uptr, pkt, 0);
        break;

    case OPC_RST:
        ci_debug_pkt (DBG_DIAG, uptr, pkt, 0);
        break;

    case OPC_STRT:
        ci_debug_pkt (DBG_DIAG, uptr, pkt, 0);
        break;

    case OPC_DATREQ0:
    case OPC_DATREQ1:
    case OPC_DATREQ2:
        ci_debug_pkt (DBG_BLK, uptr, pkt, 0);
        break;

    case OPC_ID:
        ci_debug_pkt (DBG_CONF, uptr, pkt, 0);
        break;

    case OPC_LB:
        ci_debug_pkt (DBG_CONF, uptr, pkt, 0);
        break;

    case OPC_SNTDAT:
        ci_debug_pkt (DBG_BLK, uptr, pkt, 0);
        break;

    case OPC_RETDAT:
        ci_debug_pkt (DBG_BLK, uptr, pkt, 0);
        break;

    default:
        return CISE_INVOPC;
        }
return ci_send_packet (uptr, pkt);
}

t_stat ci_receive (UNIT *uptr, CI_PKT *pkt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
uint32 opcode;
t_stat r;

r = ci_receive_packet (uptr, pkt);                      /* get next packet */
if (r != SCPE_OK)                                       /* none available? */
    return r;                                           /* done */

pkt->path = (pkt->data[CI_LEN_HI] & CI_M_PATH) >> CI_V_PATH;
pkt->data[CI_LEN_HI] &= ~CI_M_PATH;                     /* clear reserved bits */
pkt->node = pkt->data[CI_SRC];
opcode = pkt->data[CI_OPC];

switch (opcode) {

    case OPC_DG:                                        /* datagram */
        ci_debug_pkt (DBG_PPD, uptr, pkt, 1);
        break;

    case OPC_MSG:                                       /* message */
        ci_debug_pkt (DBG_SCS, uptr, pkt, 1);
        break;

    case OPC_IDREQ:                                     /* ID request */
        ci_debug_pkt (DBG_CONF, uptr, pkt, 1);
        break;

    case OPC_RST:                                       /* port reset */
        ci_debug_pkt (DBG_DIAG, uptr, pkt, 1);
        break;

    case OPC_STRT:                                      /* port start */
        ci_debug_pkt (DBG_DIAG, uptr, pkt, 1);
        break;

    case OPC_DATREQ0:                                   /* data requested */
    case OPC_DATREQ1:
    case OPC_DATREQ2:
        ci_debug_pkt (DBG_BLK, uptr, pkt, 1);
        break;

    case OPC_ID:                                        /* ID response */
        ci_debug_pkt (DBG_CONF, uptr, pkt, 1);
        break;

    case OPC_RETDAT:                                    /* data received */
        ci_debug_pkt (DBG_BLK, uptr, pkt, 1);
        break;

    case OPC_SNTDAT:                                    /* data received */
        ci_debug_pkt (DBG_BLK, uptr, pkt, 1);
        break;

    case OPC_CNF:                                       /* confirmation */
        ci_debug_pkt (DBG_BLK, uptr, pkt, 1);
        break;

    case OPC_LB:                                        /* loopback */
        ci_debug_pkt (DBG_CONF, uptr, pkt, 1);
        break;

    default:
        sim_printf ("CI: unimplemented opcode (receive) %02X\n", opcode);
        return CISE_INVOPC;
        }
return SCPE_OK;
}

/* Open a link to a remote node */

t_stat ci_open_vc (UNIT *uptr, uint8 port)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_NODE *node;
CI_LINK *link = NULL;
SOCKET newsock;
uint32 i;

if (port > CI_MAX_NODES)
    return SCPE_ARG;
node = &cp->nodes[port];
sim_debug_unit (DBG_CONN, uptr, "VC Open: Link in state %d\n", node->conn);
if (node->conn > LINK_CLOSED)                           /* already open? */
    return SCPE_OK;                                     /* yes, done */
sim_debug_unit (DBG_CONN, uptr, "Opening VC with node %d...\n", port);
if (node->uptr) {                                       /* local port? */
    sim_debug_unit (DBG_CONN, uptr, "Node is local\n", port);
    node->conn = LINK_OPEN;
    return SCPE_OK;
    }
if (uptr->ci_node > port) {                             /* are we the server? */
    sim_debug_unit (DBG_CONN, uptr, "Local node is server\n", port);
    node->conn = LINK_STARTING;                         /* wait for client */
    return SCPE_OK;
    }
for (i = 0; i < CI_MAX_NODES; i++) {                    /* find free entry */
    if (cp->links[i].socket == 0) {
        sim_debug_unit (DBG_CONN, uptr, "New connection setup at link %d\n", i);
        link = &cp->links[i];
        break;
        }
    }
if (link == NULL) {
    sim_debug_unit (DBG_CONN, uptr, "No free links for new connection\n");
    return CISE_VCERR;
    }
sim_debug_unit (DBG_CONN, uptr, "Connecting to node %d at %s...\n", port, node->host);
newsock = sim_connect_sock_ex (NULL, node->host, NULL, NULL, 0);
if (newsock == INVALID_SOCKET) {
    sim_debug_unit (DBG_CONN, uptr, "Unable to establish VC to node %d\n", port);
    return CISE_VCERR;
    }
sim_debug_unit (DBG_CONN, uptr, "Connected to node %d, socket %d\n", port, newsock);
link->socket = newsock;
node->link = link;
node->conn = LINK_OPEN;
return SCPE_OK;
}

/* Close a link to a remote node */

t_stat ci_close_vc (UNIT *uptr, uint8 port)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_NODE *node;
CI_LINK *link;

if (port > CI_MAX_NODES)
    return SCPE_ARG;
node = &cp->nodes[port];
sim_debug_unit (DBG_CONN, uptr, "VC Close: Link in state %d\n", node->conn);
if (node->conn == LINK_CLOSED)                          /* already closed? */
    return SCPE_OK;                                     /* yes, done */
sim_debug_unit (DBG_CONN, uptr, "Closing VC with node %d...\n", port);
if (node->uptr) {                                       /* local port? */
    node->conn = LINK_CLOSED;
    return SCPE_OK;                                     /* yes, done */
    }
link = node->link;
if (link) {
    if (link->socket != 0)
        sim_close_sock (link->socket);
    link->socket = 0;
    link->rx_pkt.length = 0;
    link->rx_pkt.offset = 0;
    }
node->conn = LINK_CLOSED;
node->link = NULL;
return SCPE_OK;
}

t_stat ci_check_vc (UNIT *uptr, uint8 port)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_NODE *node;
if (port > CI_MAX_NODES)
    return SCPE_ARG;
node = &cp->nodes[port];
return (node->conn > LINK_CLOSED) ? TRUE : FALSE;
}

/* Notifies the operating system that the connection with the *
 * remote node has been lost by inserting a START datagram on *
 * the response queue in order to trigger VC failure          */

t_stat ci_link_fail (UNIT *uptr, CI_LINK *link)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_PKT pkt;
int32 i, port = -1;

for (i = 0; i < CI_MAX_NODES; i++) {                    /* find link owner */
    if (cp->nodes[i].link == link) {
        port = i;
        break;
        }
    }
if (port < 0) {                                         /* no owner? */
    sim_close_sock (link->socket);                      /* just close connection */
    link->socket = 0;                                   /* reset link state */
    link->rx_pkt.offset = 0;
    link->rx_pkt.length = 0;
    return SCPE_OK;
    }

ci_close_vc (uptr, port);                               /* close the VC */

pkt.length = LEN_START;
pkt.path = CI_PATH_A;
pkt.data[CI_LEN_HI] = ((pkt.length >> 8) & 0xF);        /* packet length high */
pkt.data[CI_LEN_HI] |= (pkt.path << CI_V_PATH);         /* use the reserved bits to store the send path */
pkt.data[CI_LEN_LO] = pkt.length & 0xFF;                /* packet length low */
CI_PUTB (pkt.data, CI_DEST, uptr->ci_node);             /* destination node */
CI_PUTB (pkt.data, CI_NOT_DEST, ~uptr->ci_node);
CI_PUTB (pkt.data, CI_SRC, port);                       /* source node */
CI_PUTB (pkt.data, CI_OPC, OPC_DG);                     /* opcode */
CI_PUTB (pkt.data, CI_FLAGS, 0);                        /* flags */
CI_PUTW (pkt.data, PPD_MTYPE, PPD_START);               /* message type: START */
ciq_insert (&cp->rx_queue, &pkt);                       /* add to receive queue */

return SCPE_OK;
}

t_stat ci_poll_mc (UNIT *uptr)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
char *src_addr;
uint8 src, dst, opc;
t_stat r;

for (;;) {
    if (cp->rx_queue.count == CI_QUE_MAX)               /* queue full? */
        break;                                          /* yes, done */
    r = sim_read_sock_ex (cp->multi_sock, rcv_pkt.data, CI_MAXFR, &src_addr);
    if (r < 0) {
        sim_printf ("CI: multicast socket read error\n");
        return SCPE_IOERR;
        }
    if (r == 0)                                         /* no packet? */
        break;                                          /* done */
    src = rcv_pkt.data[CI_SRC];
    dst = rcv_pkt.data[CI_DEST];
    opc = rcv_pkt.data[CI_OPC];
    if (dst == uptr->ci_node) {
        rcv_pkt.length = ((rcv_pkt.data[CI_LEN_HI] & 0xF) << 8);
        rcv_pkt.length |= rcv_pkt.data[CI_LEN_LO];
        if (opc == OPC_IDREQ) {
            sprintf (cp->nodes[src].host, "[%s]:%.5s",
                src_addr, &rcv_pkt.data[LEN_IDREQ]);    /* get remote TCP port */
            }
        ciq_insert (&cp->rx_queue, &rcv_pkt);
        }
    free (src_addr);
    }
return SCPE_OK;
}

t_stat ci_poll_tx (UNIT *uptr)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_ITEM *item;
CI_LINK *link;
uint8  port;
t_stat r;

if (cp->tx_queue.loss > 0)
    sim_printf ("Warning: CI queue packet loss is %d\n", cp->tx_queue.loss);
while (cp->tx_queue.count > 0) {                        /* process transmit queue */
    item = &cp->tx_queue.item[cp->tx_queue.head];
    port = item->pkt.data[CI_DEST];
    link = cp->nodes[port].link;
    if (link == NULL) {
        sim_debug_unit (DBG_CONN, uptr, "Unable to send queued packet to node %d, link: closed\n", port);
        ciq_remove (&cp->tx_queue);                     /* drop packet */
        continue;
        }
    if (link->socket == 0) {
        sim_debug_unit (DBG_CONN, uptr, "Unable to send queued packet to node %d, socket: closed\n", port);
        ciq_remove (&cp->tx_queue);                     /* drop packet */
        continue;
        }
    sim_debug_unit (DBG_CONN, uptr, "Sending queued packet to node %d, socket: %d\n", port, link->socket);
    r = sim_write_sock (link->socket, &item->pkt.data[item->pkt.offset], (item->pkt.length - item->pkt.offset));
    if (r < 0) {                                        /* socket error? */
        sim_debug_unit (DBG_CONN, uptr, "Node %d closed VC, socket: %d\n", port, link->socket);
        ciq_remove (&cp->tx_queue);                     /* drop packet */
        ci_link_fail (uptr, link);                      /* notify VC failure */
        continue;
        }
    item->pkt.offset = item->pkt.offset + r;
    if (item->pkt.offset < item->pkt.length) {
        sim_debug_unit (DBG_CONN, uptr, "Sent %d of %d bytes, still queued\n", item->pkt.offset, item->pkt.length);
        break;                                          /* try later - FIXME: This will block other queued packets */
        }
    ciq_remove (&cp->tx_queue);                         /* remove processed packet from queue */
    }
return SCPE_OK;
}

t_stat ci_poll_rx (UNIT *uptr)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_LINK *link;
CI_PKT *pkt;
uint32 i;
uint8 src, dst, opc;
t_stat r;

for (i = 0; i < CI_MAX_NODES; i++) {
    if (cp->rx_queue.count == CI_QUE_MAX)               /* queue full? */
        break;                                          /* yes, don't receive any more */
    link = &cp->links[i];
    if (link->socket == 0)                              /* no connection? */
        continue;                                       /* yes, skip */
    pkt = &link->rx_pkt;
    do {
        if (cp->rx_queue.count == CI_QUE_MAX)           /* queue full? */
            return SCPE_OK;                             /* yes, don't receive any more */
        if (pkt->offset == 0)                           /* new packet? */
            pkt->length = LEN_HEADER + 2;               /* yes, get header first (plus opcode) */
        r = sim_read_sock (link->socket, &pkt->data[pkt->offset], (pkt->length - pkt->offset));
        if (r < 0) {
            sim_debug_unit (DBG_CONN, uptr, "Node at link ID %d closed VC, socket: %d\n", i, link->socket);
            ci_link_fail (uptr, link);                  /* notify VC failure */
            break;
            }
        pkt->offset = pkt->offset + r;
        if (pkt->offset < pkt->length)                  /* not all bytes received? */
            break;                                      /* try again later */
        if (pkt->length == LEN_HEADER + 2) {
            pkt->length = ((pkt->data[CI_LEN_HI] & 0xF) << 8);
            pkt->length |= pkt->data[CI_LEN_LO];        /* get total packet length */
            if (pkt->length > CI_MAXFR) {               /* check length is valid */
                sim_printf ("Received packet with invalid length %d, socket: %d\n", pkt->length, link->socket);
                ci_link_fail (uptr, link);              /* close the VC */
                break;
                }
            opc = CI_GETB (pkt->data, CI_OPC);          /* get opcode */
            if (opc == OPC_IDREQ)                       /* ID request? */
                pkt->length += LEN_TCP_PORT;            /* yes, get remote TCP port field too */
            if (pkt->offset < pkt->length)              /* more to receive? */
                continue;                               /* yes, receive packet body */
            }
        pkt->offset = 0;                                /* got whole packet now */
        src = pkt->data[CI_SRC];
        dst = pkt->data[CI_DEST];
        if (dst != uptr->ci_node) {                     /* not for this node? */
            sim_printf ("Received packet not destined for this node: %d\n", dst);
            ci_dump_pkt (DBG_PKT, uptr, pkt->data, pkt->length);
            return SCPE_IERR;                           /* should not happen */
            }
        if ((cp->nodes[src].link == NULL) && (cp->nodes[src].conn < LINK_OPEN)) {
            sim_debug_unit (DBG_CONN, uptr, "Accepting connection from node %d, socket: %d\n", src, link->socket);
            cp->nodes[src].conn = LINK_OPEN;
            cp->nodes[src].link = link;                 /* reassign connection */
            }
        else if (cp->nodes[src].link != link) {
            sim_debug_unit (DBG_CONN, uptr, "Rejecting connection from node %d, socket: %d\n", src, link->socket);
            sim_close_sock (link->socket);              /* reject connection */
            link->socket = 0;                           /* clear connection */
            break;
            }
        ciq_insert (&cp->rx_queue, pkt);
        } while (r > 0);
    }
return SCPE_OK;
}

t_stat ci_send_packet (UNIT *uptr, CI_PKT *pkt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
uint32 opcode = pkt->data[CI_OPC];
CI_NODE *node;
CI_PKT tpkt;
int32 r;

node = &cp->nodes[pkt->node];

memcpy (&tpkt, pkt, sizeof (CI_PKT));                   /* make local copy */

if (node->uptr) {                                       /* loopback? */
    uptr = node->uptr;                                  /* get local unit */
    if (uptr->ci_state < PORT_ENABLED)  
        return SCPE_OK;                                 /* FIXME: return "no path" status */
    cp = (CI_PORT *)uptr->port_ctx;                     /* get local port */
    if (cp->rx_queue.count == CI_QUE_MAX)
        return CISE_QFULL;
    ciq_insert (&cp->rx_queue, &tpkt);                  /* add to queue */
    sim_activate_abs (uptr, 100);
    return SCPE_OK;
    }

if (opcode == OPC_IDREQ) {
    strncpy (&tpkt.data[LEN_IDREQ], cp->tcp_port, LEN_TCP_PORT);
    tpkt.length += LEN_TCP_PORT;                        /* append local TCP port to IDREQ packets */
    }
tpkt.offset = 0;                                        /* initial send offset */

if (node->conn > LINK_STARTING) {                       /* link established? */
    sim_debug_unit (DBG_CONN, uptr, "using TCP/IP link\n");
    ciq_insert (&cp->tx_queue, &tpkt);
    ci_poll_tx (uptr);
    return SCPE_OK;
    }
else if ((node->conn == LINK_STARTING) && (opcode == OPC_MSG)) {
    sim_debug_unit (DBG_CONN, uptr, "TCPIP link pending, queued\n");
    ciq_insert (&cp->tx_queue, &tpkt);
    return SCPE_OK;
    }
else if (uptr->flags & UNIT_ATT) {                      /* use multicast */
    /* FIXME: This should be for datagrams only */
    sim_debug_unit (DBG_CONN, uptr, "using UDP multicast link\n");
    r = sim_write_sock_ex (cp->multi_sock, tpkt.data, tpkt.length, cp->group, SIM_SOCK_OPT_DATAGRAM);
    return SCPE_OK;
    }
return SCPE_OK;                                         /* FIXME: return "no path" status */
}

t_stat ci_receive_packet (UNIT *uptr, CI_PKT *pkt)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_ITEM *item;
uint8 opc;
uint32 path;
t_stat r;

if (cp->rx_queue.count == 0) {                          /* receive queue empty? */
    if (uptr->flags & UNIT_ATT) {
        if (uptr->ci_state >= PORT_ENABLED) {           /* port enabled? */
            r = ci_poll_mc (uptr);                      /* poll multicast channel */
            if (r != SCPE_OK)
                return r;
            r = ci_poll_rx (uptr);                      /* poll open VCs */
            if (r != SCPE_OK)
                return r;
            }
        }
    }
if (cp->rx_queue.count > 0) {                           /* process receive queue */
    if (cp->rx_queue.loss > 0)
        sim_printf ("Warning: CI queue packet loss is %d\n", cp->rx_queue.loss);
    item = &cp->rx_queue.item[cp->rx_queue.head];
    memcpy (pkt, &item->pkt, sizeof (CI_PKT));          /* copy to output packet */
    ciq_remove (&cp->rx_queue);                         /* remove processed packet from queue */
    opc = pkt->data[CI_OPC];
    if (opc == OPC_IDREQ)                               /* check for ID request */
        pkt->length -= LEN_TCP_PORT;                    /* remove remote TCP port */
    return SCPE_OK;
    }

return CISE_NOPKT;
}

t_stat ciq_init (CI_QUE* que, int32 max)
{

if (!que->item) {                                       /* create dynamic queue if it does not exist */
    size_t size = sizeof(CI_ITEM) * max;
    que->max = max;
    que->item = (CI_ITEM *)malloc (size);
    if (que->item)
        memset (que->item, 0, size);                    /* init dynamic memory */
    else {                                              /* failed to allocate memory */
        sim_printf ("CI: failed to allocate dynamic queue[%d]\n", max);
        return SCPE_MEM;
        };
    };
return SCPE_OK;
}

void ciq_clear (CI_QUE* que)
{
memset (que->item, 0, sizeof(CI_ITEM) * que->max);      /* clear packet array */
que->count = que->head = que->tail = que->loss = que->high = 0; /* clear rest of structure */
}

void ciq_remove (CI_QUE* que)
{
CI_ITEM* item = &que->item[que->head];

if (que->count) {
    memset (item, 0, sizeof(CI_ITEM));
    if (++que->head == que->max)
        que->head = 0;
    que->count--;
    }
}

void ciq_insert (CI_QUE* que, CI_PKT *pkt)
{
CI_ITEM* item;

if (!que->count) {                                      /* if queue empty, set pointers to beginning */
    que->head = 0;
    que->tail = -1;
    }
if (++que->tail == que->max)
    que->tail = 0;                                      /* find new tail of the circular queue */
if (++que->count > que->max) {
    que->count = que->max;                              /* lose oldest packet */
    if (++que->head == que->max)
        que->head = 0;
    que->loss++;
    }
if (que->count > que->high)
    que->high = que->count;

item = &que->item[que->tail];                           /* set information in (new) tail item */
memcpy (&item->pkt, pkt, sizeof (CI_PKT)); // TODO: don't copy whole packet?
}

t_stat ci_port_svc (UNIT *uptr)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
CI_ITEM *item;
CI_LINK *link;
SOCKET newsock;
uint32 i;
uint8  port;
t_stat r;

sim_clock_coschedule (uptr, tmxr_poll);                 /* continue poll */

if ((uptr->flags & UNIT_ATT) == 0)
    return SCPE_OK;

r = ci_poll_tx (uptr);                                  /* poll open VCs */
if (r != SCPE_OK)
    return r;

newsock = sim_accept_conn (cp->tcp_sock, NULL);         /* check for new VCs */
if (newsock != INVALID_SOCKET)  {
    sim_debug_unit (DBG_CONN, uptr, "New connection request socket: %d\n", newsock);
    link = NULL;
    for (i = 0; i < CI_MAX_NODES; i++) {                /* find free entry */
        if (cp->links[i].socket == 0) {
            sim_debug_unit (DBG_CONN, uptr, "New connection setup at link %d\n", i);
            link = &cp->links[i];
            break;
            }
        }
    if (link)
        cp->links[i].socket = newsock;
    else {
        sim_debug_unit (DBG_CONN, uptr, "No free links for new connection\n");
        sim_close_sock (newsock);
        }
    }

return SCPE_OK;
}

/* Reset CI adapter */

t_stat ci_port_reset (DEVICE *dptr)
{
UNIT *uptr = dptr->units;
UNIT *ruptr;
DEVICE *rdptr;
CI_PORT *cp, *rcp;
t_stat r;
int32 i;

uptr->dptr = dptr;
uptr->port_ctx = (void *) realloc (uptr->port_ctx, sizeof (CI_PORT));
cp = (CI_PORT *)uptr->port_ctx;

for (i = 0; i < CI_MAX_NODES; i++) {
    if (cp->links[i].socket)                            /* reset links */
        sim_close_sock (cp->links[i].socket);
    cp->links[i].rx_pkt.offset = 0;
    cp->links[i].rx_pkt.length = 0;
    cp->nodes[i].conn = LINK_CLOSED;                    /* reset node context */
    cp->nodes[i].host[0] = '\0';
    cp->nodes[i].vcd = 0;
    cp->nodes[i].uptr = NULL;
    cp->nodes[i].link = NULL;
    }

for (i = 0; (rdptr = sim_devices[i]) != NULL; i++) {    /* loop thru dev */
    if (!(rdptr->flags & DEV_DIS)) {                    /* enabled? */
        if (rdptr->flags & DEV_CI) {                    /* CI? */
            if (rdptr == dptr)                          /* self? */
                continue;                               /* skip */
            ruptr = rdptr->units;
            rcp = ruptr->port_ctx;
            if (rcp == NULL)                            /* port not setup? */
                continue;
            if (((ruptr->flags ^ uptr->flags) & UNIT_ATT) != 0) /* same attach state? */
                continue;
            if ((ruptr->flags & uptr->flags) & UNIT_ATT) { /* both attached? */
                if (strcmp (rcp->group, cp->group) != 0)   /* same group? */
                    continue;
                }
            cp->nodes[ruptr->ci_node].uptr = ruptr;     /* found local node */
            }
        }
    }

cp->nodes[uptr->ci_node].uptr = uptr;                   /* add self */
uptr->ci_state = PORT_UNINIT;                           /* now uninitialised */

r = ciq_init (&cp->rx_queue, CI_QUE_MAX);               /* init read queue */
if (r != SCPE_OK)
    return r;
ciq_clear (&cp->rx_queue);

r = ciq_init (&cp->tx_queue, CI_QUE_MAX);               /* init write queue */
if (r != SCPE_OK)
    return r;
ciq_clear (&cp->tx_queue);

sim_cancel (uptr);                                      /* stop poll */
return SCPE_OK;
}

t_stat ci_attach (UNIT *uptr, CONST char *cptr)
{
char* tptr;
t_stat r;
SOCKET multi_sock, tcp_sock;
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
if (cp->group[0] == '\0') {
    sim_printf ("Must set group address first\n");
    return SCPE_ARG;
    }

multi_sock = sim_master_sock_ex (cp->group, NULL,
    (SIM_SOCK_OPT_REUSEADDR | SIM_SOCK_OPT_MULTICAST)); /* make multicast socket */
if (multi_sock == INVALID_SOCKET)
    return SCPE_OPENERR;                                /* open error */
sim_printf ("Listening on multicast address %s (socket %d)\n", cp->group, multi_sock);

sim_parse_addr (cptr, NULL, 0, NULL, cp->tcp_port, sizeof (cp->tcp_port), NULL, NULL);
tcp_sock = sim_master_sock (cptr, NULL);                /* make tcp socket */
if (tcp_sock == INVALID_SOCKET)
    return SCPE_OPENERR;                                /* open error */
sim_printf ("Listening on TCP address %s (socket %d)\n", cptr, tcp_sock);

tptr = (char *) malloc (strlen (cptr) + 1);             /* get string buf */
if (tptr == NULL)                                       /* no more mem? */
    return SCPE_MEM;

cp->multi_sock = multi_sock;                            /* save master socket */
cp->tcp_sock = tcp_sock;
strcpy (tptr, cptr);                                    /* copy port */
uptr->filename = tptr;                                  /* save */
uptr->flags = uptr->flags | UNIT_ATT;
return SCPE_OK;
}

t_stat ci_detach (UNIT *uptr)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

if (!(uptr->flags & UNIT_ATT))                          /* attached? */
    return SCPE_OK;
sim_close_sock (cp->multi_sock);                        /* close master socket */
sim_close_sock (cp->tcp_sock);                          /* close master socket */
free (uptr->filename);                                  /* free port string */
uptr->filename = NULL;
uptr->flags = uptr->flags & ~UNIT_ATT;                  /* not attached */
return SCPE_OK;
}

uint8 ci_get_node (UNIT *uptr)
{
return uptr->ci_node;
}

/* Show CI adapter parameters */

t_stat ci_show_links (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;
uint32 i;

fprintf (st, "Node   State     Host\n");
fprintf (st, "----   -----     ----\n");
for (i = 0; i < CI_MAX_NODES; i++) {
    if (cp->nodes[i].uptr)
        fprintf (st, "%02d     %-9s Local\n", i, ci_link_states[cp->nodes[i].conn]);
    else if (cp->nodes[i].host[0])
        fprintf (st, "%02d     %-9s %s\n", i, ci_link_states[cp->nodes[i].conn], cp->nodes[i].host);
    }
return SCPE_OK;
}

t_stat ci_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "node=%d", uptr->ci_node);
return SCPE_OK;
}

t_stat ci_show_group (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

if (cp->group[0])
    fprintf (st, "group=%s", cp->group);
else
    fprintf (st, "no group");
return SCPE_OK;
}

t_stat ci_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 r;
uint32 node;

if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
node = (uint32) get_uint (cptr, 10, CI_MAX_NODES, &r);
if (r != SCPE_OK)
    return r;
if (node >= CI_MAX_NODES)
    return SCPE_ARG;
uptr->ci_node = node;
return SCPE_OK;
}

t_stat ci_set_group (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
char host[CBUFSIZE], port[CBUFSIZE];
CI_PORT *cp = (CI_PORT *)uptr->port_ctx;

if ((!cptr) || (!*cptr))
    return SCPE_ARG;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
if (sim_parse_addr (cptr, host, sizeof(host), NULL, port, sizeof(port), NULL, NULL))
    return SCPE_ARG;
if (host[0] == '\0')
    return SCPE_ARG;
strncpy (cp->group, cptr, CBUFSIZE-1);
return SCPE_OK;
}
