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

/* status codes */

#define K_PIO           0000
#define K_CI            0001                            /* K.ci host interface */
#define K_SDI           0002                            /* K.sdi disk data channel */
#define K_STI           0203                            /* K.sti tape data channel */
#define K_SI4           0004                            /* K.si 4-port data channel - no microcode loaded */
#define K_SI4_DISK      0205                            /* K.si 4-port data channel - disk microcode loaded */
#define K_SI4_TAPE      0206                            /* K.si 4-port data channel - tape microcode loaded */
#define K_SI8           0212                            /* K.si 8-port data channel - no microcode loaded */
#define K_SI8_MICRO     0007                            /* K.si 8-port data channel - microcode loaded */
#define K_SCSI          0015                            /* K.scsi data channel module */
#define K_NONE          0377                            /* No requestor present */

/* actions */

#define K_NOACTION      0000
#define K_RUN           0001
#define K_HALT          0002

#define GETW(off)       RdxMemW (CM, off)
#define GETB(off)       RdxMemB (CM, off)
#define PUTW(off, val)  WrxMemW (CM, off, val)
#define PUTB(off, val)  WrxMemB (CM, off, val)
#define GETP(off)       ((GETW (off) & 0077776) ? (GETW (off) << 2) : GETW (off))
#define PUTP(off, val)  PUTW (off, (val >> 2))

#define GETDW(off)      RdxMemW (DM, off)
#define GETDB(off)      RdxMemB (DM, off)
#define PUTDW(off, val) WrxMemW (DM, off, val)
#define PUTDB(off, val) WrxMemB (DM, off, val)

#define Q_LOCKED(q)     (GETW (q) == 0100000)
#define Q_EMPTY(q)      (GETW (q) == 0)

/* transmission queue */

#define Q_SRVC          0004                            /* service queue pointer */
#define Q_PROC          0002
#define Q_FLGS          0001
#define Q_HEAD          0000                            /* resource queue head */
#define Q_TAIL          0002                            /* resource queue tail */
#define Q_WTHD          0004                            /* waiter queue head */
#define Q_WTTL          0006                            /* waiter queue tail */

/* queue flags */

#define FLGS_IQ         0100                            /* interrupt queue */
#define FLGS_SVC        0200                            /* service pending */

/* K initialisation structure */

#define KI_C1           0000                            /* check value 1 - 125252 */
#define KI_C2           0002                            /* check value 2 - 052525 */
#define KI_KIA          0004                            /* K init address */
#define KI_CHK          0006                            /* check value */

/* K generic control area */

#define KG_VRS          0044                            /* microcode version */
#define KG_FCB          0046                            /* free control memory buffers */
#define KG_FBU          0050                            /* free data memory buffers */
#define KG_RSL          0074                            /* microdiagnostic result? */

/* K.sti control area */

#define KT_IPQ          0000
#define KT_S0           0140                            /* drive state area (port 0) */
#define KT_S1           0160                            /* drive state area (port 1) */
#define KT_S2           0200                            /* drive state area (port 2) */
#define KT_S3           0220                            /* drive state area (port 3) */

/* debugging bitmaps */

#define KREQ_DBG_TRC    0x0001                          /* trace routine calls */

extern uint32 kg_init (t_bool *run);
extern t_stat kg_show_addr (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern t_stat kg_show_req (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern t_stat kg_show_queue (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern t_stat kg_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern void print_word (FILE *st, uint32 kg, char *name, int16 off, uint16 len);
extern uint32 get_from_queue (uint32 q);
extern void put_to_queue (uint32 q, uint32 itm);

extern int16 inst_pc;

#endif
