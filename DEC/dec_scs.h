/* dec_scs.h: Systems Communications Services

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

#ifndef _DEC_SCS_H_
#define _DEC_SCS_H_

#if !defined(SCS_BASE)
#if defined(PPD_SCS)
#define SCS_BASE        PPD_SCS                         /* PPD layer */
#else
#define SCS_BASE        0
#endif
#endif

/* SCS Offsets */

#define SCS_MTYPE       (SCS_BASE + 0x00)               /* message type */
#define SCS_CREDIT      (SCS_BASE + 0x02)               /* credit extension */
#define SCS_DSTCON      (SCS_BASE + 0x04)               /* destination connection ID */
#define SCS_SRCCON      (SCS_BASE + 0x08)               /* source connection ID */
#define SCS_APPL        (SCS_BASE + 0x0C)               /* application message */
#define SCS_MINCR       (SCS_BASE + 0x0C)               /* mininum send credit */
#define SCS_STATUS      (SCS_BASE + 0x0E)               /* status */
#define SCS_DSTPROC     (SCS_BASE + 0x10)               /* destination process */
#define SCS_SRCPROC     (SCS_BASE + 0x20)               /* source process */
#define SCS_CONDAT      (SCS_BASE + 0x30)               /* connection data */

/* SCS Message Types */

#define SCS_CONREQ      0x00                            /* connect request */
#define SCS_CONRSP      0x01                            /* connect response */
#define SCS_ACCREQ      0x02                            /* accept request */
#define SCS_ACCRSP      0x03                            /* accept respone */
#define SCS_REJREQ      0x04                            /* reject request */
#define SCS_REJRSP      0x05                            /* reject respone */
#define SCS_DISREQ      0x06                            /* disconnect request */
#define SCS_DISRSP      0x07                            /* disconnect response */
#define SCS_CRREQ       0x08                            /* credit request */
#define SCS_CRRSP       0x09                            /* credit response */
#define SCS_APPMSG      0x0A                            /* application message */
#define SCS_APPDG       0x0B                            /* application datagram */

/* SCS Message Lengths */

#define LEN_CONREQ      (SCS_CONDAT)                    /* excludes connection data */
#define LEN_CONRSP      (SCS_STATUS + 2)
#define LEN_ACCREQ      (SCS_CONDAT)                    /* excludes connection data */
#define LEN_ACCRSP      (SCS_STATUS + 2)
#define LEN_REJREQ      (SCS_STATUS + 2)
#define LEN_REJRSP      (SCS_SRCCON + 4)
#define LEN_DISREQ      (SCS_STATUS + 2)
#define LEN_DISRSP      (SCS_SRCCON + 4)
#define LEN_CRREQ       (SCS_SRCCON + 4)
#define LEN_CRRSP       (SCS_SRCCON + 4)
#define LEN_APPMSG      (SCS_APPL)                      /* excludes SYSAP data */
#define LEN_APPDG       (SCS_APPL)                      /* excludes SYSAP data */

/* SCS Status Codes */

#define SCS_STNORM      0x01                            /* success */
#define SCS_STNOMAT     0x0A                            /* no matching listener */
#define SCS_STNORS      0x12                            /* no resources */
#define SCS_STDISC      0x19                            /* disconnected */
#define SCS_STNOCR      0x21                            /* insufficient credit */
#define SCS_BALANCE     0x29                            /* load balance disconnect */
#define SCS_UAP         0x2A                            /* use alternate port */

extern void scs_fail_vc (UNIT *uptr, uint32 port);
extern t_stat scs_send_msg (UNIT *uptr, uint32 cid, CI_PKT *pkt);
extern int32 scs_write (UNIT *uptr, uint32 *bd, uint8 *buf, size_t xfrsz, UNIT *aptr);
extern int32 scs_read (UNIT *uptr, uint32 *bd, uint8 *buf, size_t xfrsz, UNIT *aptr);
extern t_stat scs_receive (UNIT *uptr, CI_PKT *pkt);
extern t_stat scs_reset (DEVICE *dptr);

#endif
