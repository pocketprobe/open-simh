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

void print_word (uint16 kh, char *name, uint16 off, uint16 len)
{
if (len == 0)
    sim_printf ("%s %06o\n", name, kh + off);
else if (len == 1) {
    sim_printf ("%s %06o\\  %03o\n", name, kh + off, GETB (kh + off));
    }
else {
    sim_printf ("%s %06o/", name, kh + off);
    for (int i = 1; i <= (len >> 1); i++) {
        sim_printf ("  %06o", GETW (kh + off + ((i - 1) << 1)));
        if (i % 8 == 0)
            sim_printf ("\n              ");
        }
    sim_printf ("\n");
    }
}

uint16 get_from_queue (uint16 q)
{
uint16 qh;

qh = GETP (q + Q_HEAD);
if ((qh != 0) && (qh != 0100000)) {                     /* not empty or locked? */
    PUTP (q + Q_HEAD, GETP (qh));                       /* point queue head to next item */
    if (GETP (qh) == 0)                                 /* no next item? */
        PUTP (q + Q_TAIL, 0);                           /* queue now empty */
    PUTP (qh, 0);                                       /* remove next link from removed item */
    sim_printf ("Item %06o removed from queue at address %06o\n", qh, q);
    }
return qh;
}

void put_to_queue (uint16 q, uint16 itm)
{
uint16 qh, qt, qf, tq, sq, mask, pcb;

qh = GETP (q + Q_HEAD);
if (qh == 0100000) {                                    /* should not happen */
    sim_printf ("put_to_queue: queue at address %06o is locked\n", q);
    return;
    }
if (qh == 1) {    /* software process waiting on queue */
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
            sim_printf ("Interrupt for queue %06o, PCB = %06o\n", q, pcb);
            }
        }
    else {                                              /* transmission queue */
        tq = q - Q_SRVC;                                /* get base of transmission queue */
        sq = GETP (tq);                                 /* get service queue pointer */
        if (sq) {
            PUTP (tq, 0);                               /* clear service queue pointer */
            sim_printf ("Sending transmission queue %06o to service queue %06o, PCB = %06o\n", tq, sq, pcb);
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
    if (GETW (qt) != 0)                                 /* should not happen */
        sim_printf ("Queue tail item has non-zero pointer: %06o\n", GETW (qt));
    else {                                              /* add to end of queue */
        PUTP (qt, itm);                                 /* add link to item */
        PUTP (q + Q_TAIL, itm);                         /* update queue tail pointer */
        }
    }
sim_printf ("Item %06o added to queue at address %06o\n", itm, q);
}

t_stat hsc_find (int32 flag, CONST char *ptr)
{
uint32 i, j;
uint16 p[12] = {
    0000035,
    0035316,
    0001444,
    0140010,
    0000024,
    0001064,
    0160100,
    0006775,
    0167770,
    0177777,
    0177750,
    0177710
    };

for (i = 0; i < (CMEMSIZE >> 1); i++) {
    for (j = 0; j < 12; j++) {
        if (CM[i + j] != p[j]) {
            i = i + j;
            break;
            }
        }
    if (j == 12) {
        sim_printf ("Pattern found at %06o\n", (i << 1));
        break;
        }
    }
return SCPE_OK;
}
