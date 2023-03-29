/* hsc70_kreq.h: HSC70 common K requestor functions

   Copyright (c) 2023, Matt Burke

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

#ifndef _HSC_KREQ_H
#define _HSC_KREQ_H   0

#define GETW(off)       RdxMemW (CM, off)
#define GETB(off)       RdxMemB (CM, off)
#define PUTW(off, val)  WrxMemW (CM, off, val)
#define PUTB(off, val)  WrxMemB (CM, off, val)
#define GETP(off)       ((GETW (off) & 0077776) ? (GETW (off) << 2) : GETW (off))
#define PUTP(off, val)  PUTW (off, (val >> 2))

#define Q_LOCKED(q)     (GETW (q) & 0100000)

/* transmission queue */

#define Q_SRVC          0004                            /* service queue pointer */
#define Q_PROC          0002
#define Q_FLGS          0001
#define Q_HEAD          0000                            /* Resource queue head */
#define Q_TAIL          0002                            /* Resource queue tail */
#define Q_WTHD          0004                            /* Waiter queue head */
#define Q_WTTL          0006                            /* Waiter queue tail */

/* queue flags */

#define FLGS_IQ         0100                            /* interrupt queue */
#define FLGS_INT        0200                            /* interrupt pending */

/* K initialisation structure */

#define KI_C1           0000                            /* check value 1 - 125252 */
#define KI_C2           0002                            /* check value 2 - 052525 */
#define KI_KIA          0004                            /* K init address */
#define KI_CHK          0006                            /* check value */

/* K generic control area */

#define KG_VRS          0044                            /* microcode version */
#define KG_FBU          0050
#define KG_RSL          0074

/* K.ci control area */

#define KH_LNK          0000
#define KH_2WK          0010
#define KH_NAD          0164                            /* node address */

/* K.sdi control area */

#define KT_S0           0140
#define KT_S1           0160
#define KT_S2           0200
#define KT_S3           0220

/* debugging bitmaps */

#define KREQ_DBG_TRC    0x0001                          /* trace routine calls */

extern uint16 k_init_common ();
extern t_stat k_show_addr (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern t_stat k_show_queue (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern void print_word (FILE *st, uint16 kh, char *name, uint16 off, uint16 len);
extern uint16 get_from_queue (uint16 q);
extern void put_to_queue (uint16 q, uint16 itm);
extern t_stat hsc_find (int32 flag, CONST char *ptr);

extern int16 inst_pc;

#endif
