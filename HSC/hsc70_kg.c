/* hsc70_kg.c: HSC70 common K requestor functions

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
#include "hsc70_kg.h"

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

uint32 kg_init (t_bool *run)
{
uint32 ki, kg = 0, chk;

for (ki = 0; ki < 20; ki = ki + 2) {                    /* search for KI structure */
    if ((GETW (ki) == 0125252) && (GETW (ki + 2) == 052525)) {
        kg = GETW (ki + KI_KIA);                        /* get offset to KG structure */
        if (GETW (ki + KI_CHK) != (kg - 1))             /* verify offset */
            break;                                      /* failed */
        PUTW (ki + KI_KIA, -kg);
        kg = kg << 2;                                   /* convert to byte offset */
        *run = (ki == 4);                               /* run if not diagnostic startup */
        break;
        }
    }
return kg;
}

t_stat kg_show_addr (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
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

t_stat kg_show_req (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
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

t_stat kg_show_queue (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint32 qh;

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

t_stat kg_show_ctrl (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint32 kg;
t_stat r;

if (uptr == NULL)
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL)
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;
if (dibp == NULL)
    return SCPE_IERR;
kg = dibp->cma;

print_word (st, kg, "KG$TLK", 0040, 2);
print_word (st, kg, "KG$SLT", 0042, 1);                 /* structure type? */
print_word (st, kg, "KG$TYP", 0043, 1);                 /* structure type? */
print_word (st, kg, "KG$VRS", 0044, 2);                 /* version */
print_word (st, kg, "KG$FCB", 0046, 2);                 /* free control memory buffers? */
print_word (st, kg, "KG$FBU", 0050, 2);
print_word (st, kg, "KG$BWS", 0052, 2);
print_word (st, kg, "KG$TMR", 0054, 2);
print_word (st, kg, "KG$DMP", 0056, 2);
print_word (st, kg, "KG$DCA", 0060, 2);
print_word (st, kg, "KG$TST", 0062, 2);
print_word (st, kg, "KG$FRS", 0064, 2);
print_word (st, kg, "KG$LAS", 0066, 2);
print_word (st, kg, "KG$MDL", 0070, 0);
print_word (st, kg, "KG$MTY", 0070, 2);
print_word (st, kg, "KG$PFL", 0072, 2);
print_word (st, kg, "KG$RSL", 0074, 2);                 /* microdiagnostic test result */
print_word (st, kg, "KG$LTR", 0076, 2);
print_word (st, kg, "KG$PSA", 0100, 2);
print_word (st, kg, "KG$PSB", 0102, 2);
print_word (st, kg, "KG$DSA", 0104, 20);
print_word (st, kg, "KG$MC0", 0130, 8);
print_word (st, kg, "KG$KSA", 0140, 0);

return SCPE_OK;
}

void sim_print_word (uint32 kg, char *name, int16 off, uint16 len)
{
if (len == 0)
    sim_printf ("%s %06o\n", name, kg + off);
else if (len == 1) {
    sim_printf ("%s %06o\\  %03o\n", name, kg + off, GETB (kg + off));
    }
else {
    sim_printf ("%s %06o/", name, kg + off);
    for (int i = 1; i <= (len >> 1); i++) {
        sim_printf ("  %06o", GETW (kg + off + ((i - 1) << 1)));
        if (i % 8 == 0)
            sim_printf ("\r\n              ");
        }
    sim_printf ("\r\n");
    }
}

void print_word (FILE *st, uint32 kg, char *name, int16 off, uint16 len)
{
if (st == NULL) {
    sim_print_word (kg, name, off, len);
    return;
    }
if (len == 0)
    fprintf (st, "%s %06o\n", name, kg + off);
else if (len == 1) {
    fprintf (st, "%s %06o\\  %03o\n", name, kg + off, GETB (kg + off));
    }
else {
    fprintf (st, "%s %06o/", name, kg + off);
    for (int i = 1; i <= (len >> 1); i++) {
        fprintf (st, "  %06o", GETW (kg + off + ((i - 1) << 1)));
        if (i % 8 == 0)
            fprintf (st, "\n              ");
        }
    fprintf (st, "\n");
    }
}

void print_string (FILE *st, uint32 kg, char *name, int16 off, uint16 len)
{
fprintf (st, "%s %06o   ", name, kg + off);
for (int i = 0; i < len; i++)
    fprintf (st, "%c", GETB (kg + off + i));
fprintf (st, "\n");
}

void print_route (uint32 rte)
{
print_word (NULL, rte, "V.FLGS", V_FLGS, 1);
print_word (NULL, rte, "V.OPCD", V_OPCD, 1);
print_word (NULL, rte, "V.SUCC", V_SUCC, 2);
print_word (NULL, rte, "V.CTX ", V_CTX , 2);
print_word (NULL, rte, "V.AERR", V_AERR, 2);
}

void print_counter (uint32 ctr)
{
print_word (NULL, ctr, "C.SRVC", C_SRVC, 2);
print_word (NULL, ctr, "C.CNT ", C_CNT , 2);
}

void kg_route_frb (uint32 kg, uint32 frb, t_bool sts)
{
uint32 rte, ctr, flg, val, bd, sc, cq;
    
ctr = GETP (frb + F_PCTR);
rte = GETP (frb + F_RTE);                               /* get route */

print_counter (ctr);
print_route (rte);
print_route (rte + 010);
print_route (rte + 020);

flg = GETW (rte);                                       /* get opcode/flags from route */
sim_printf ("OPC: %o, FLG: %o\n", (flg >> 8), (flg & 0377));

if (sts) {
    if (flg & V_FB) {                                   /* free buffers? */
        sc = GETW (frb + F_SCNT);                       /* get sector count */
        for (int k = 0; k < sc; k++) {
            bd = kg_receive (frb + F_BFHD);             /* dequeue buffer descriptor */
            if (bd == 0)
                break;
            kg_send (kg + KG_FBU, bd);                  /* put to free buffer queue */
            }
        }

    if (flg & V_DC) {                                   /* decrement counter? */
        val = GETW (ctr + C_CNT);                       /* get counter value */
        val = val - 1;                                  /* another operation completed */
        PUTW (ctr + C_CNT, val);                        /* update counter value */
        if (val == 0) {                                 /* all operations complete? */
            cq = GETP (ctr);
            sim_printf ("CTR dest = %07o\n", cq);
            PUTP (ctr, 0);                              /* clear queue pointer */
            kg_send (cq, ctr);                          /* send counter to completion queue */
            }
        }
    rte = rte + 010;                                    /* advance route pointer */
    PUTP (frb + F_RTE, rte);
    cq = GETW (rte + V_SUCC);                           /* get success destination */
    }
else {
    PUTW (frb + F_ESUM, 1);
    cq = GETW (rte + V_AERR);                           /* get error destination */
    }    

if (cq & 0177400)
    cq = cq << 2;                                       /* direct address */
else {
    cq = frb + (cq << 2);                               /* offset into FRB */
    cq = GETP (cq);
    }
sim_printf ("FRB dest = %07o\n", cq);
kg_send (cq, frb);                                      /* send FRB to completion queue */
}

uint32 kg_receive (uint32 q)
{
uint32 itm;

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

void kg_send (uint32 q, uint32 itm)
{
uint32 qh, qt, qn, qf, tq, sq, mask, pcb;

if (Q_LOCKED (q + Q_HEAD)) {
    //sim_printf ("kg_send: queue at address %06o is locked\n", q);
    return;
    }
qh = GETP (q + Q_HEAD);
if (qh == 1) {                                          /* software process waiting on queue */
    qh = 0;
    qf = GETB (q - Q_FLGS);                             /* get queue flags */
    pcb = GETW (q + Q_WTHD);
    if (qf & FLGS_IQ) {                                 /* interrupt queue */
        if ((qf & FLGS_SVC) == 0) {                     /* no interrupt pending? */
            mask = GETW (q - Q_SRVC);                   /* get interrupt mask */
            if (mask & 017)
                qf |= FLGS_SVC;
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
            //sim_printf ("Interrupt for queue %06o, PCB = %06o\n", q, pcb);
            }
        }
    else {                                              /* transmission queue */
        tq = q - Q_SRVC;                                /* get base of transmission queue */
        sq = GETP (tq);                                 /* get service queue pointer */
        if (((qf & FLGS_SVC) == 0) && (sq)) {           /* not on service queue and queue valid? */
            PUTP (tq, 0);                               /* clear service queue pointer */
            //sim_printf ("Sending transmission queue %06o to service queue %06o, PCB = %06o\n", tq, sq, pcb);
            qf |= FLGS_SVC;
            PUTB (q - Q_FLGS, qf);                      /* now on service queue */
            kg_send (sq, tq);                           /* send transmission queue to service queue */
            }
        }
    }
if (qh == 0) {                                          /* queue empty */
    PUTP (q + Q_HEAD, itm);                             /* set head */
    PUTP (q + Q_TAIL, itm);                             /* set tail */
    }
else {                                                  /* queue is not empty */
    qt = GETP (q + Q_TAIL);                             /* get the tail pointer */
    while (GETW (qt) != 0) {                            /* find the real end of the queue */
        qn = GETP (qt);
        if (qn == qt)                                   /* circular pointer? */
            break;
        qt = qn;                                        /* point to next */
        }
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
    if ((dptr->flags & DEV_HSC) == 0)                   /* only HSC devices */
        continue;
    if (dptr->flags & DEV_DIS)                          /* skip if disabled */
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
