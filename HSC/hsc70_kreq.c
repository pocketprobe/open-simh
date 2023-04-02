/* hsc70_kreq.c: HSC70 common K requestor functions

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

#include "pdp11_defs.h"
#include "hsc70_kreq.h"

extern int32 trap_req;
extern int32 ipl;
extern HDIB *KSTS[10];

extern int32 calc_ints (int32 nipl, int32 trq);

/* Search control memory to find the KI structure.
 * This starts with a pattern of alternating
 * 1s and 0s. The offset to the K control area can
 * then be found. The following location should be
 * the offset - 1. The software expects the requestor
 * to negate the offset as an indication that it
 * has been read.
 */

uint16 k_init_common ()
{
uint16 ki, kh = 0, chk;

for (ki = 0; ki < 20; ki = ki + 2) {                     /* search for KI structure */
    if ((GETW (ki) == 0125252) && (GETW (ki + 2) == 052525)) {
        kh = GETW (ki + KI_KIA);                        /* get offset to KH structure */
        if (GETW (ki + KI_CHK) != (kh - 1))             /* verify offset */
            break;                                      /* failed */
        PUTW (ki + KI_KIA, -kh);
        kh = kh << 2;                                   /* convert to byte offset */
        }
    }
return kh;
}

t_stat k_show_addr (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;
fprintf (st, "address=");
fprint_val (st, (t_value) dibp->cma + CMEMBASE, DEV_RDX, 32, PV_LEFT);
return SCPE_OK;
}

t_stat k_show_req (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint32 slot, i;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;
for (i = 0; i < 10; i++) {
    if (KSTS[i] == dibp) {
        slot = i;
        break;
        }
    }
fprintf (st, "slot=");
fprint_val (st, (t_value) slot, 10, 32, PV_LEFT);
return SCPE_OK;
}

t_stat k_show_queue (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint16 qh;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;

qh = dibp->cma + val;
for (;;) {
    qh = GETP (qh);
    if ((qh == 0) || qh == 0100000)
        break;
    print_word (st, qh, "", 0000, 2);
    print_word (st, qh, "", 0002, 2);
    print_word (st, qh, "", 0004, 2);
    print_word (st, qh, "", 0006, 2);
    print_word (st, qh, "", 0010, 2);
    print_word (st, qh, "", 0012, 2);
    print_word (st, qh, "", 0014, 2);
    print_word (st, qh, "", 0016, 2);
    print_word (st, qh, "", 0020, 2);
    fprintf (st, "   ---\n");
    }
return SCPE_OK;
}

void print_word (FILE *st, uint16 kh, char *name, uint16 off, uint16 len)
{
if (len == 0)
    fprintf (st, "%s %06o\n", name, kh + off);
else if (len == 1) {
    fprintf (st, "%s %06o\\  %03o\n", name, kh + off, GETB (kh + off));
    }
else {
    fprintf (st, "%s %06o/", name, kh + off);
    for (int i = 1; i <= (len >> 1); i++) {
        fprintf (st, "  %06o", GETW (kh + off + ((i - 1) << 1)));
        if (i % 8 == 0)
            fprintf (st, "\n              ");
        }
    fprintf (st, "\n");
    }
}

uint16 get_from_queue (uint16 q)
{
uint16 itm;

if (Q_LOCKED (q + Q_HEAD))                              /* check if queue locked */
    return 0;
if (Q_EMPTY (q + Q_HEAD))                               /* check if queue empty */
    return 0;
itm = GETP (q + Q_HEAD);
PUTP (q + Q_HEAD, GETP (itm));                          /* point queue head to next item */
if (GETP (itm) == 0)                                    /* no next item? */
    PUTP (q + Q_TAIL, 0);                               /* queue now empty */
PUTP (itm, 0);                                          /* remove next link from removed item */
//sim_printf ("Item %06o removed from queue at address %06o\n", itm, q);
return itm;
}

void put_to_queue (uint16 q, uint16 itm)
{
uint16 qh, qt, qf, tq, sq, mask, pcb;

if (Q_LOCKED (q + Q_HEAD)) {
//    sim_printf ("put_to_queue: queue at address %06o is locked\n", q);
    return;
    }
qh = GETP (q + Q_HEAD);
if (qh == 1) {                                          /* software process waiting on queue */
    qh = 0;
    qf = GETB (q - Q_FLGS);                             /* get queue flags */
    pcb = GETW (q + Q_WTHD);
    if (qf & FLGS_IQ) {                                 /* interrupt queue */
        if ((qf & FLGS_INT) == 0) {                     /* no interrupt pending */
            mask = GETW (q - Q_SRVC);                   /* get interrupt mask */
            if (mask & 017)
                qf |= FLGS_INT;
            if (mask & 010)
                SET_INT (CB7);
            if (mask & 04)
                SET_INT (CB6);
            if (mask & 02)
                SET_INT (CB5);
            if (mask & 01)
                SET_INT (CB4);
            trap_req = calc_ints (ipl, trap_req);       /* recalc int req */
            PUTB (q - Q_FLGS, qf);                      /* interrupt now pending */
//            sim_printf ("Interrupt for queue %06o, PCB = %06o\n", q, pcb);
            }
        }
    else {                                              /* transmission queue */
        tq = q - Q_SRVC;                                /* get base of transmission queue */
        sq = GETP (tq);                                 /* get service queue pointer */
        if (sq) {
            PUTP (tq, 0);                               /* clear service queue pointer */
//            sim_printf ("Sending transmission queue %06o to service queue %06o, PCB = %06o\n", tq, sq, pcb);
            put_to_queue (sq, tq);                      /* send transmission queue to service queue */
            }
        }
    }
if (qh == 0) {                                          /* queue empty */
    PUTP (q + Q_HEAD, itm);                             /* set head */
    PUTP (q + Q_TAIL, itm);                             /* set tail */
    }
else {                                                  /* queue is not empty */
    qt = GETP (q + Q_TAIL);                             /* get the tail pointer */
    while (GETW (qt) != 0)                              /* find the real end of the queue */
        qt = GETP (qt);
    PUTP (qt, itm);                                     /* add link to item */
    PUTP (q + Q_TAIL, itm);                             /* update queue tail pointer */
    }
//sim_printf ("Item %06o added to queue at address %06o\n", itm, q);
}

t_stat hsc_build_dib ()
{
DEVICE *dptr;
HDIB *dibp;
t_stat r;
uint32 i, j;

for (j = 0; j < 10; j++)
    KSTS[j] = NULL;                                     /* set all slots empty */

for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {     /* locate all HSC devices */
    if ((dptr->flags & DEV_HSC) == 0)
        continue;
    dibp = (HDIB *)dptr->ctxt;
    if (dibp->init == NULL)                             /* K.pio? */
        KSTS[0] = dibp;                                 /* must be in slot 0 */
    else if (dibp->sts == K_CI)                         /* K.ci? */
        KSTS[1] = dibp;                                 /* must be in slot 1 */
    else {
        for (j = 2; j < 10; j++) {                      /* find a free slot */
            if (KSTS[j] == NULL) {
                KSTS[j] = dibp;
                break;
                }
            }
        }
    }

return SCPE_OK;
}
