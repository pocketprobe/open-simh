/* hsc_kg.c: HSC common K requestor functions

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
#include "pdp11_cpumod.h"
#include "hsc_kg.h"

#define KG_MAX_QUEUE    20

STDEF kg_stc[] = {
    { "KG$TLK", KG_TLK, 2, ST_INT },
    { "KG$SLT", KG_SLT, 1, ST_INT },
    { "KG$TYP", KG_TYP, 1, ST_INT },
    { "KG$VRS", KG_VRS, 2, ST_INT },
    { "KG$FCB", KG_FCB, 2, ST_INT },
    { "KG$FBU", KG_FBU, 2, ST_INT },
    { "KG$BWS", KG_BWS, 2, ST_INT },
    { "KG$TMR", KG_TMR, 2, ST_INT },
    { "KG$DMP", KG_DMP, 2, ST_INT },
    { "KG$DCA", KG_DCA, 2, ST_INT },
    { "KG$TST", KG_TST, 2, ST_INT },
    { "KG$FRS", KG_FRS, 2, ST_INT },
    { "KG$LAS", KG_LAS, 2, ST_INT },
    { "KG$MDL", KG_MDL, 0, ST_INT },
    { "KG$MTY", KG_MTY, 2, ST_INT },
    { "KG$PFL", KG_PFL, 2, ST_INT },
    { "KG$RSL", KG_RSL, 2, ST_INT },
    { "KG$LTR", KG_LTR, 2, ST_INT },
    { "KG$PSA", KG_PSA, 2, ST_INT },
    { "KG$PSB", KG_PSB, 2, ST_INT },
    { "KG$DSA", KG_DSA, 20, ST_INT },
    { "KG$MC0", KG_MC0, 8, ST_INT },
    { "KG$KSA", KG_KSA, 0, ST_INT },
    { NULL }
    };

STDEF v_stc[] = {
    { "V.FLGS", V_FLGS, 1, ST_INT },
    { "V.OPCD", V_OPCD, 1, ST_INT },
    { "V.SUCC", V_SUCC, 2, ST_INT },
    { "V.CTX ", V_CTX , 2, ST_INT },
    { "V.AERR", V_AERR, 2, ST_INT },
    { NULL }
    };

STDEF c_stc[] = {
    { "C.SRVC", C_SRVC, 2, ST_INT },
    { "C.CNT ", C_CNT , 2, ST_INT },
    { NULL }
    };

typedef struct {
    uint32 q;
    uint32 itm;
} KG_LKQ;

KG_LKQ kg_lkq[KG_MAX_QUEUE];

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

/* Show the control memory address of a K */

t_stat kg_show_addr (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;

if (uptr == NULL)                                       /* unit valid? */
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);                       /* get associated device */
if (dptr == NULL)                                       /* device valid? */
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;                             /* get associated DIB */
if (dibp == NULL)                                       /* DIB valid? */
    return SCPE_IERR;
if (dibp->cma) {                                        /* control mem addr set? */
    fprintf (st, "address=");                           /* yes, show address */
    fprint_val (st, (t_value) dibp->cma + CMEMBASE, DEV_RDX, 32, PV_LEFT);
    }
else
    fprintf (st, "halted");                             /* no, K is halted */
return SCPE_OK;
}

/* Show the slot number of a K */

t_stat kg_show_req (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr;
HDIB *dibp;
uint32 slot, i;

if (uptr == NULL)                                       /* unit valid? */
    return SCPE_IERR;
dptr = find_dev_from_unit (uptr);                       /* get associated device */
if (dptr == NULL)                                       /* device valid? */
    return SCPE_IERR;
dibp = (HDIB *) dptr->ctxt;                             /* get associated DIB */
if (dibp == NULL)                                       /* DIB valid? */
    return SCPE_IERR;
for (i = 0; i < 10; i++) {                              /* find slot for this DIB */
    if (KSTS[i] == dibp) {
        slot = i;
        break;
        }
    }
fprintf (st, "slot=");
fprint_val (st, (t_value) slot, 10, 32, PV_LEFT);
return SCPE_OK;
}

/* Macro to allow print functions to be used for both debug and show routines */

#define kg_printf(st, uptr, ...) \
    do { \
        if (st) \
            fprintf (st, __VA_ARGS__); \
        else \
            sim_debug_unit (KG_DBG_STRUCT, uptr, __VA_ARGS__); \
    } while (0)

/* Print an integer element of a structure */

void kg_print_word (FILE *st, UNIT *uptr, uint32 addr, char *name, uint16 len)
{
if (len == 0)                                           /* overlay */
    kg_printf (st, uptr, "%s %06o\n", name, addr);
else if (len == 1) {                                    /* byte */
    kg_printf (st, uptr, "%s %06o\\  %03o\n", name, addr, GETB (addr));
    }
else {                                                  /* word or multi-word */
    kg_printf (st, uptr, "%s %06o/", name, addr);
    for (int i = 1; i <= (len >> 1); i++) {
        kg_printf (st, uptr, "  %06o", GETW (addr + ((i - 1) << 1)));
        if (i % 8 == 0)
            kg_printf (st, uptr, "\n              ");
        }
    kg_printf (st, uptr, "\n");
    }
}

/* Print a string element of a structure */

void kg_print_string (FILE *st, UNIT *uptr, uint32 addr, char *name, uint16 len)
{
kg_printf (st, uptr, "%s %06o   ", name, addr);
for (int i = 0; i < len; i++)
    kg_printf (st, uptr, "%c", GETB (addr + i));
kg_printf (st, uptr, "\n");
}

/* Print a control memory structure */

void kg_print_struct (FILE *st, UNIT *uptr, uint32 kg, STDEF *stc)
{
while (stc->name != NULL) {
    if (stc->typ == ST_INT)                             /* integer element */
        kg_print_word (st, uptr, kg + stc->off, stc->name, stc->len);
    else if (stc->typ == ST_STR)                        /* string element */
        kg_print_string (st, uptr, kg + stc->off, stc->name, stc->len);
    else if (stc->typ == ST_LNK)                        /* link to another struct */
        kg_print_struct (st, uptr, kg, stc->lnk);
    stc++;
    }
}

/* Print the items on a queue */

void kg_print_queue (FILE *st, UNIT *uptr, uint32 qh, STDEF *stc)
{
for (;;) {
    qh = GETP (qh);                                     /* get next queue item */
    if ((qh == 0) || qh == 0100000)                     /* empty or locked? */
        break;                                          /* done */
    kg_print_struct (st, uptr, qh, stc);                /* print queue item */
    kg_printf (st, uptr, "---\n");
    }
}

/* Decrement a counter queue */

void kg_dec_ctr (UNIT *uptr, uint32 ctr)
{
uint32 val, cq;

kg_print_struct (NULL, uptr, ctr, c_stc);
val = GETW (ctr + C_CNT);                               /* get counter value */
sim_debug_unit (KG_DBG_TRC, uptr, "CTR %d -> %d\n", val, (val - 1));
val = val - 1;                                          /* another operation completed */
PUTW (ctr + C_CNT, val);                                /* update counter value */
if (val == 0) {                                         /* all operations complete? */
    cq = GETP (ctr);
    sim_debug_unit (KG_DBG_TRC, uptr, "CTR dest = %07o\n", cq);
    PUTP (ctr, 0);                                      /* clear queue pointer */
    kg_send (cq, ctr);                                  /* send counter to completion queue */
    }
}

/* Route a fragment request block to its next station */

void kg_route_frb (UNIT *uptr, uint32 kg, uint32 frb, t_bool sts)
{
uint32 rte, ctr, flg, val, bd, sc, cq, fq;
    
ctr = GETP (frb + F_PCTR);
rte = GETP (frb + F_RTE);                               /* get route */

kg_print_struct (NULL, uptr, rte, v_stc);
kg_print_struct (NULL, uptr, rte + 010, v_stc);
kg_print_struct (NULL, uptr, rte + 020, v_stc);

flg = GETW (rte);                                       /* get opcode/flags from route */

if (sts) {
    if (flg & V_FB) {                                   /* free buffers? */
        sc = GETW (frb + F_SCNT);                       /* get sector count */
        fq = GETP (kg + KG_FBU);                        /* get free buffer queue */
        for (int k = 0; k < sc; k++) {
            bd = kg_receive (frb + F_BFHD);             /* dequeue buffer descriptor */
            if (bd == 0)
                break;
            kg_send (fq, bd);                           /* put to free buffer queue */
            }
        }
    if (flg & V_DC)                                     /* decrement counter? */
        kg_dec_ctr (uptr, ctr);
    rte = rte + 010;                                    /* advance route pointer */
    PUTP (frb + F_RTE, rte);
    cq = GETW (rte + V_SUCC);                           /* get success destination */
    }
else
    cq = GETW (rte + V_AERR);                           /* get error destination */

if (cq & 0177400)
    cq = cq << 2;                                       /* direct address */
else {
    cq = frb + (cq << 2);                               /* offset into FRB */
    cq = GETP (cq);
    }
sim_debug_unit (KG_DBG_TRC, uptr, "FRB - OPC: %o, FLG: %o, STS: %d, DST: %o\n", (flg >> 8), (flg & 0377), sts, cq);
kg_send (cq, frb);                                      /* send FRB to completion queue */
}

/* Receive a resource from a transmission queue */

uint32 kg_receive (uint32 q)
{
uint32 itm;

if (Q_LOCKED (q + Q_HEAD)) {                            /* check if queue locked */
    sim_printf ("kg_receive: queue at address %06o is locked\n", q);
    return 0;
    }
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

/* Send a resource to a transmission queue */

void kg_send (uint32 q, uint32 itm)
{
uint32 qh, qt, qn, qf, tq, sq, mask, pcb, i;

if (Q_LOCKED (q + Q_HEAD)) {
    sim_printf ("kg_send: queue at address %06o is locked\n", q);
    for (i = 0; i < KG_MAX_QUEUE; i++) {
        if (kg_lkq[i].q == 0) {                         /* free entry? */
            kg_lkq[i].q = q;                            /* add to lock queue */
            kg_lkq[i].itm = itm;
            q = 0;
            }
        }
    if (q != 0)
        sim_printf ("kg_send: not enough free entries in lock queue\n");
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
    kg_poll (q + Q_HEAD, 0);                            /* poll devices */
    }
else {                                                  /* queue is not empty */
    qt = GETP (q + Q_TAIL);                             /* get the tail pointer */
    PUTP (qt, itm);                                     /* add link to item */
    PUTP (q + Q_TAIL, itm);                             /* update queue tail pointer */
    }
//sim_printf ("Item %06o added to queue at address %06o\n", itm, q);
}

/* Notify K devices of control memory activity */

void kg_poll (uint32 addr, int32 interval)
{
uint32 i;
HDIB *dibp;

for (i = 0; i < 10; i++) {                              /* for each HSC device */
    dibp = KSTS[i];
    if (dibp != NULL) {                                 /* device in this slot? */
        if (dibp->dptr->flags & DEV_DIS)                /* disabled? */
            continue;                                   /* skip */
        if (dibp->cma == 0)                             /* not initialised? */
            continue;                                   /* skip */
        if (dibp->poll)                                 /* supports poll? */
            dibp->poll (dibp->dptr, addr, interval);    /* notify device */
        }
    }
}

/* Generic K service routine */

t_stat kg_svc (UNIT *uptr)
{
uint32 i, q, itm;

for (i = 0; i < KG_MAX_QUEUE; i++) {
    q = kg_lkq[i].q;                                    /* get queue head */
    if (q != 0) {                                       /* valid entry? */
        if (!Q_LOCKED (q + Q_HEAD)) {                   /* not locked? */
            itm = kg_lkq[i].itm;                        /* get item */
            kg_lkq[i].q = 0;                            /* clear lock queue entry */
            kg_lkq[i].itm = 0;
            sim_printf ("kg_svc: completing send for item %07o to queue %07o\n", itm, q);
            kg_send (q, itm);                           /* send to transmission queue */
            }
        }
    }
return SCPE_OK;
}

/* Build the dispatch table for the Ks */

t_stat hsc_build_dib ()
{
DEVICE *dptr;
HDIB *dibp;
t_stat r;
uint32 i, j;

for (i = 0; i < KG_MAX_QUEUE; i++) {                    /* clear lock queue */
    kg_lkq[i].q = 0;
    kg_lkq[i].itm = 0;
    }

for (j = 0; j < 10; j++)
    KSTS[j] = NULL;                                     /* set all slots empty */

for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {     /* locate all HSC devices */
    if ((dptr->flags & DEV_HSC) == 0)                   /* only HSC devices */
        continue;
    if (dptr->flags & DEV_DIS)                          /* skip if disabled */
        continue;
    dibp = (HDIB *)dptr->ctxt;
    dibp->dptr = dptr;                                  /* setup back pointer */
    if (dibp->init == NULL)                             /* K.pio? */
        KSTS[0] = dibp;                                 /* must be in slot 0 */
    else if (dibp->sts == K_CI) {                       /* K.ci? */
        if (CPUT (CPUT_HSC60|CPUT_HSC90))               /* HSC60 or HSC90? */
            dibp->sts = K_CI2_MICRO;                    /* change type to K.ci2 */
        KSTS[1] = dibp;                                 /* must be in slot 1 */
        }
    else if (dibp->sts == K_CI2_MICRO) {                /* K.ci2? */
        if (CPUT (CPUT_HSC40|CPUT_HSC70))               /* HSC40 or HSC70? */
            dibp->sts = K_CI;                           /* change type to K.ci */
        KSTS[1] = dibp;                                 /* must be in slot 1 */
        }
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
