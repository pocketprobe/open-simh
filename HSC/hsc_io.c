/* hsc_io.c: HSC I/O simulator

   Copyright (c) 2017, Matt Burke
   This module incorporates code from SimH, Copyright (c) 1993-2012, Robert M Supnik

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
   THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name(s) of the author(s) shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author(s).
*/

#include "pdp11_defs.h"

extern uint16 *M;
extern int32 int_req[IPL_HLVL];
extern int32 trap_req, ipl;
extern int32 EADR;

int32 calc_ints (int32 nipl, int32 trq);

extern t_stat cpu_build_dib (void);
extern t_stat hsc_build_dib (void);
extern t_stat pio_build_dib (void);

/* I/O data structures */

t_stat (*iodispR[IOPAGESIZE >> 1])(int32 *dat, int32 ad, int32 md);
t_stat (*iodispW[IOPAGESIZE >> 1])(int32 dat, int32 ad, int32 md);
DIB *iodibp[IOPAGESIZE >> 1];

int32 int_vec[IPL_HLVL][32];                            /* int req to vector */
int32 (*int_ack[IPL_HLVL][32])(void);                   /* int ack routines */

static const int32 pirq_bit[7] = {
    INT_V_PIR1, INT_V_PIR2, INT_V_PIR3, INT_V_PIR4,
    INT_V_PIR5, INT_V_PIR6, INT_V_PIR7
    };

static const int32 int_internal[IPL_HLVL] = {
    0,             INT_INTERNAL1, INT_INTERNAL2, INT_INTERNAL3,
    INT_INTERNAL4, INT_INTERNAL5, INT_INTERNAL6, INT_INTERNAL7
    };

/* I/O page lookup and linkage routines

   Inputs:
        *data   =       pointer to data to read, if READ
        data    =       data to store, if WRITE or WRITEB
        pa      =       address
        access  =       READ, WRITE, or WRITEB
   Outputs:
        status  =       SCPE_OK or SCPE_NXM
*/

t_stat iopageR (int32 *data, uint32 pa, int32 access)
{
int32 idx;
t_stat stat;

idx = (pa & IOPAGEMASK) >> 1;
if (iodispR[idx]) {
    stat = iodispR[idx] (data, pa, access);
    if (stat != SCPE_OK)
        EADR = pa;
    trap_req = calc_ints (ipl, trap_req);
    return stat;
    }
EADR = pa;
return SCPE_NXM;
}

t_stat iopageW (int32 data, uint32 pa, int32 access)
{
int32 idx;
t_stat stat;

idx = (pa & IOPAGEMASK) >> 1;
if (iodispW[idx]) {
    stat = iodispW[idx] (data, pa, access);
    if (stat != SCPE_OK)
        EADR = pa;
    trap_req = calc_ints (ipl, trap_req);
    return stat;
    }
EADR = pa;
return SCPE_NXM;
}

/* Calculate interrupt outstanding */

int32 calc_ints (int32 nipl, int32 trq)
{
int32 i, t;
t_bool all_int = (UNIBUS || (nipl < IPL_HMIN));

for (i = IPL_HLVL - 1; i > nipl; i--) {
    t = all_int? int_req[i]: (int_req[i] & int_internal[i]);
    if (t)
        return (trq | TRAP_INT);
    }
return (trq & ~TRAP_INT);
}

/* Find vector for highest priority interrupt */

int32 get_vector (int32 nipl)
{
int32 i, j, t, vec;
t_bool all_int = (UNIBUS || (nipl < IPL_HMIN));

for (i = IPL_HLVL - 1; i > nipl; i--) {                 /* loop thru lvls */
    t = all_int? int_req[i]: (int_req[i] & int_internal[i]);
    for (j = 0; t && (j < 32); j++) {                   /* srch level */
        if ((t >> j) & 1) {                             /* irq found? */
            int_req[i] = int_req[i] & ~(1u << j);       /* clr irq */
            if (int_ack[i][j])
                vec = int_ack[i][j]();
            else vec = int_vec[i][j];
            return vec;                                 /* return vector */
            }                                           /* end if t */
        }                                               /* end for j */
    }                                                   /* end for i */
return 0;
}

/* Functions required by pdp11_cpumod.c */

t_stat ubm_rd (int32 *data, int32 addr, int32 access)
{
return SCPE_OK;
}

t_stat ubm_wr (int32 data, int32 addr, int32 access)
{
return SCPE_OK;
}

/* Build tables from device list */

t_stat build_dib_tab (void)
{
int32 i;
DEVICE *dptr;
DIB *dibp;
t_stat r;

init_ubus_tab ();                                       /* init Unibus tables */
for (i = 0; i < 7; i++)                                 /* seed PIRQ intr */
    int_vec[i + 1][pirq_bit[i]] = VEC_PIRQ;
int_vec[IPL_CB7][INT_V_CB7] = VEC_CB7;
int_vec[IPL_CB6][INT_V_CB6] = VEC_CB6;
int_vec[IPL_CB5][INT_V_CB5] = VEC_CB5;
int_vec[IPL_CB4][INT_V_CB4] = VEC_CB4;
if ((r = cpu_build_dib ()))                             /* build CPU entries */
    return r;
if ((r = hsc_build_dib ()))                             /* build HSC entries */
    return r;
if ((r = pio_build_dib ()))                             /* build PIO entries */
    return r;
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {     /* loop thru dev */
    dibp = (DIB *) dptr->ctxt;                          /* get DIB */
    if (dibp && !(dptr->flags & DEV_DIS)) {             /* defined, enabled? */
        if ((r = build_ubus_tab (dptr, dibp)))          /* add to Unibus tab */
            return r;
        }                                               /* end if enabled */
    }                                                   /* end for */
return SCPE_OK;
}
