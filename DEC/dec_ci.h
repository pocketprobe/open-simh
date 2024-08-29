/* dec_ci.h: Computer Interconnect

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
*/

#ifndef _DEC_CI_H_
#define _DEC_CI_H_

#include "ci.h"

/* Debug Streams */

#define DBG_REG         0x00100000                      /* registers */
#define DBG_WRN         0x00200000                      /* warnings */
#define DBG_DIAG        0x00400000                      /* diagnostic packets */
#define DBG_CONF        0x00800000                      /* configuration packets */
#define DBG_PPD         0x01000000                      /* PPD packets */
#define DBG_SCS         0x02000000                      /* SCS packets */
#define DBG_BLK         0x04000000                      /* block transfer packets */
#define DBG_LCMD        0x08000000                      /* local commands */
#define DBG_CONN        0x10000000                      /* socket connections */
#define DBG_TRC         0x20000000                      /* trace */
#define DBG_PKT         0x40000000                      /* packet data */

/* Unit Specific Data */

#define ci_node         u3                              /* port addr */

/* Return status codes */

#define CISE_OK         0                               /* status OK */
#define CISE_INVOPC     1                               /* invalid opcode */
#define CISE_VCERR      2                               /* VC error */
#define CISE_QFULL      3                               /* queue full */
#define CISE_NOPKT      4                               /* queue empty */
#define CISE_QERR       5                               /* queue error */
#define CISE_MEMERR     6                               /* memory read/write error */
#define CISE_NOPATH     7                               /* no path to remote node */

/* Path Selection */

#define CI_PATH_AUTO    0                               /* auto select */
#define CI_PATH_A       1                               /* use path A */
#define CI_PATH_B       2                               /* use path B */
#define CI_PATH_RSVD    3                               /* reserved */

/* Packet data access */

#define CI_GETB(p,w)    ((uint8) p[w])
#define CI_GETW(p,w)    (((uint16) p[w]) | \
                        (((uint16) p[(w)+1]) << 8))
#define CI_GETL(p,w)    (((uint32) p[w]) | \
                        (((uint32) p[(w)+1]) << 8) | \
                        (((uint32) p[(w)+2]) << 16) | \
                        (((uint32) p[(w)+3]) << 24))

#define CI_PUTB(p,w,x)  p[w] = (x) & 0xFF
#define CI_PUTW(p,w,x)  p[w] = (x) & 0xFF; \
                        p[(w)+1] = ((x) >> 8) & 0xFF
#define CI_PUTL(p,w,x)  p[w] = (x) & 0xFF; \
                        p[(w)+1] = ((x) >> 8) & 0xFF; \
                        p[(w)+2] = ((x) >> 16) & 0xFF; \
                        p[(w)+3] = ((x) >> 24) & 0xFF

typedef struct {
    uint32 type;                                        /* adpater specific use */
    uint32 addr;                                        /* associated memory address */
    uint32 node;
    uint32 path;
    size_t size;                                        /* associated structure size */
    size_t length;                                      /* packet length */
    t_offset offset;                                    /* receive/send offset */
    uint8 data[CI_MAXFR];                               /* packet data */
} CI_PKT;

extern void ci_dump_pkt (uint32 dctrl, UNIT *uptr, char* buffer, uint32 length);
void ci_rx_enable (UNIT *uptr, uint32 state);
t_stat ci_send (UNIT *uptr, CI_PKT *pkt);
t_stat ci_receive (UNIT *uptr, CI_PKT *pkt);
t_stat ci_open_vc (UNIT *uptr, uint8 port);
t_stat ci_close_vc (UNIT *uptr, uint8 port);
t_stat ci_check_vc (UNIT *uptr, uint8 port);
uint8 ci_get_node (UNIT *uptr);
t_stat ci_show_links (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ci_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ci_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ci_show_group (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ci_set_group (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ci_attach (UNIT *uptr, CONST char *cptr);
t_stat ci_detach (UNIT *uptr);
t_stat ci_port_svc (UNIT *uptr);
t_stat ci_port_reset (DEVICE *dptr);

#endif
