/* dec_ppd.h: Port-to-Port Driver

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

#ifndef _DEC_PPD_H_
#define _DEC_PPD_H_

#if !defined(PPD_BASE)
#if defined(PPD_TEXT)
#define PPD_BASE        PPD_TEXT                        /* CI port layer */
#elif defined(CI_TEXT)
#define PPD_BASE        CI_TEXT                         /* CI layer */
#else
#define PPD_BASE        0
#endif
#endif

/* Datagram and Message Offsets */

#define PPD_MTYPE       (PPD_BASE + 0x00)               /* datagram type */
#define  PPD_M_MTYPE    0x7FFF                          /* datagram type */
#define  PPD_M_CCLR     0x8000                          /* cache clear */

#define PPD_SYSID       (PPD_BASE + 0x02)               /* sending system ID */
#define PPD_PROTO       (PPD_BASE + 0x08)               /* protocol revision */
#define  PPD_C_BASE     0                               /* 1st rev */
#define  PPD_C_ELOG     1                               /* 2nd rev, supports ELOG */
#define PPD_MAXDG       (PPD_BASE + 0x0A)               /* max datagram size */
#define PPD_MAXMSG      (PPD_BASE + 0x0C)               /* max message size */
#define PPD_SWTYPE      (PPD_BASE + 0x0E)               /* software type */
#define PPD_SWVER       (PPD_BASE + 0x12)               /* software version */
#define PPD_SWIN        (PPD_BASE + 0x16)               /* software incarnation */
#define PPD_HWTYPE      (PPD_BASE + 0x1E)               /* hardware type */
#define PPD_HWVER       (PPD_BASE + 0x22)               /* hardware version */
#define PPD_NODE        (PPD_BASE + 0x2E)               /* node name */
#define PPD_TIME        (PPD_BASE + 0x36)               /* current time */

#define PPD_SCS         (PPD_BASE + 0x02)               /* SCS message base */

#if defined(CI_XCTID)
#define PPD_LCONID      (CI_XCTID + 0)                  /* local connection id */
#define PPD_RSPID       (CI_XCTID + 4)                  /* local response id */
#endif

/* Datagram and Message Types */

#define PPD_START       0                               /* start */
#define PPD_STACK       1                               /* start acknowledge */
#define PPD_ACK         2                               /* acknowledge */
#define PPD_SCSDG       3                               /* SCS datagram */
#define PPD_SCSMSG      4                               /* SCS message */
#define PPD_ELOG        5                               /* error log */
#define PPD_HOSTSHUT    6                               /* host shutdown */
#define PPD_FUDG        7                               /* firmware update datagram */

/* Datagram and Message Lengths */

#define LEN_START       (PPD_TIME + 8)
#define LEN_STACK       (PPD_TIME + 8)
#define LEN_ACK         (PPD_MTYPE + 2)

extern t_stat ppd_snddg (UNIT *uptr, CI_PKT *pkt);
extern t_stat ppd_sndmsg (UNIT *uptr, CI_PKT *pkt);
extern t_stat ppd_sndlb (UNIT *uptr, CI_PKT *pkt);
extern int32 ppd_snddat (UNIT *uptr, uint32 sbnam);
extern int32 ppd_reqdat (UNIT *uptr, uint32 rbnam);
extern t_stat ppd_receive (UNIT *uptr, CI_PKT *pkt);
extern t_stat ppd_reqid (UNIT *uptr, CI_PKT *pkt);
extern t_stat ppd_timer (UNIT *uptr);
extern t_stat ppd_reset (DEVICE *dptr);
extern t_stat ppd_show_sysid (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern t_stat ppd_set_sysid (UNIT *uptr, int32 val, CONST char *cptr, void *desc);

#endif
