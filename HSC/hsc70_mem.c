/* hsc70_mem.c: HSC70 memory

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

#define RdxMemW(x,pa)   (x[(pa) >> 1])
#define RdxMemB(x,pa)   ((((pa) & 1)? x[(pa) >> 1] >> 8: x[(pa) >> 1]) & 0377)
#define WrxMemW(x,pa,d) x[(pa) >> 1] = (d)
#define WrxMemB(x,pa,d) x[(pa) >> 1] = ((pa) & 1)? \
                            ((x[(pa) >> 1] & 0377) | (((d) & 0377) << 8)): \
                            ((x[(pa) >> 1] & ~0377) | ((d) & 0377))

const char *cycles[] = {
    "STD",
    "INT",
    "LCK",
    "NMA"
};

extern int32 PCSR;
extern int32 trap_req, ipl;
extern int32 CPUERR;
extern jmp_buf save_env;
extern FILE *sim_log;
extern int16 inst_pc;

extern int32 calc_ints (int32 nipl, int32 trq);

uint16 *CM = NULL;                                      /* control memory */
uint16 *DM = NULL;                                      /* data memory */
int32 CADR = 0;                                         /* control memory address */
int32 CDATA = 0;                                        /* control memory data */
int32 CCYCLE = 0;                                       /* control memory cycle */
int32 DADR = 0;                                         /* data memory address */
int32 EADR = 0;                                         /* error address */

int32 hsc_RdMemW (int32 pa)
{
int32 data;

if (ADDR_IS_DMEM (pa)) {
    pa = pa & DMEMMASK;
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    data = RdxMemW (DM, pa);
    if (sim_log)
        fprintf (sim_log, "DMEM rd: %07o = %06o at %o\n", pa, data, (uint16)inst_pc);
    return data;
    }
if (ADDR_IS_CMEM (pa)) {
    pa = pa & CMEMMASK;
    CADR = (pa >> 1);                                   /* save address */
    if (PCSR & PCSR_NMA) {                              /* non-memory access cycle */
        CCYCLE = 07;
        data = CDATA;
        }
    else {
        if (PCSR & PCSR_CBK)                            /* bank swap? */
            data = RdxMemW (CM, pa + (CMEMSIZE >> 1));  /* select upper half of memory */
        else
            data = RdxMemW (CM, pa);
        if (PCSR & PCSR_CIE) {                          /* interrupt cycle */
            CCYCLE = 02;
            if (data & 010)
                SET_INT (CB7);
            if (data & 04)
                SET_INT (CB6);
            if (data & 02)
                SET_INT (CB5);
            if (data & 01)
                SET_INT (CB4);
            trap_req = calc_ints (ipl, trap_req);       /* recalc int req */
            }
        else if (PCSR & PCSR_LCK) {                     /* lock cycle */
            CCYCLE = 04;
            WrxMemW (CM, pa, 0100000);                  /* indicate locked */
            }
        else CCYCLE = 01;                               /* read/write cycle */
        CDATA = data;                                   /* data history */
        }
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "CMEM rd (%s): %07o = %06o at %o\n", cycles[CCYCLE >> 1], pa, data, (uint16)inst_pc);
    return data;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        return RdxMemW (M, pa ^ 02000000);              /* yes, invert A19 */
    else
        return RdxMemW (M, pa);
    }
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

int32 hsc_RdMemB (int32 pa)
{
int32 data;

if (ADDR_IS_DMEM (pa)) {
    pa = pa & DMEMMASK;
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    data = RdxMemB (DM, pa);
    if (sim_log)
        fprintf (sim_log, "DMEM rd: %07o = %03o at %o\n", pa, data, (uint16)inst_pc);
    return data;
    }
if (ADDR_IS_CMEM (pa)) {
    pa = pa & CMEMMASK;
    CADR = (pa >> 1);                                   /* save address */
    if (PCSR & PCSR_NMA) {                              /* non-memory access cycle */
        CCYCLE = 07;
        data = CDATA;
        }
    else {
        if (PCSR & PCSR_CBK)                            /* bank swap? */
            data = RdxMemB (CM, pa + (CMEMSIZE >> 1));  /* select upper half of memory */
        else
            data = RdxMemB (CM, pa);
        if (PCSR & PCSR_CIE) {                          /* interrupt cycle */
            CCYCLE = 02;
            if (data & 010)
                SET_INT (CB7);
            if (data & 04)
                SET_INT (CB6);
            if (data & 02)
                SET_INT (CB5);
            if (data & 01)
                SET_INT (CB4);
            trap_req = calc_ints (ipl, trap_req);       /* recalc int req */
            }
        else if (PCSR & PCSR_LCK) {                     /* lock cycle */
            CCYCLE = 04;
            WrxMemW (CM, pa, 0100000);                  /* indicate locked */
            }
        else CCYCLE = 01;                               /* read/write cycle */
        CDATA = data;                                   /* data history */
        }
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "CMEM rd (%s): %07o = %03o at %o\n", cycles[CCYCLE >> 1], pa, data, (uint16)inst_pc);
    return data;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        return RdxMemB (M, pa ^ 02000000);              /* yes, invert A19 */
    else
        return RdxMemB (M, pa);
    }
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

void hsc_WrMemW (int32 pa, int32 data)
{
if (ADDR_IS_DMEM (pa)) {
    pa = pa & DMEMMASK;
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "DMEM wr: %07o = %06o at %o\n", pa, data, (uint16)inst_pc);
    WrxMemW (DM, pa, data);
    return;
    }
if (ADDR_IS_CMEM (pa)) {
    pa = pa & CMEMMASK;
    CADR = (pa >> 1);                                   /* save address */
    CDATA = data;                                       /* data history */
    if (PCSR & PCSR_NMA)                                /* non-memory access cycle */
        CCYCLE = 07;
    else {
        if (PCSR & PCSR_CIE) {                          /* interrupt cycle */
            CCYCLE = 02;
            if (data & 010)
                SET_INT (CB7);
            if (data & 04)
                SET_INT (CB6);
            if (data & 02)
                SET_INT (CB5);
            if (data & 01)
                SET_INT (CB4);
            trap_req = calc_ints (ipl, trap_req);       /* recalc int req */
            }
        else if (PCSR & PCSR_LCK) {                     /* lock cycle */
            CCYCLE = 04;
            WrxMemW (CM, pa, 0100000);                  /* indicate locked */
            }
        else CCYCLE = 01;                               /* read/write cycle */
        if (PCSR & PCSR_CBK)                            /* bank swap? */
            WrxMemW (CM, pa + (CMEMSIZE >> 1), data);   /* select upper half of memory */
        else
            WrxMemW (CM, pa, data);
        }
    if (sim_log)
        fprintf (sim_log, "CMEM wr (%s): %07o = %06o at %o\n", cycles[CCYCLE >> 1], pa, data, (uint16)inst_pc);
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    return;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        WrxMemW (M, pa ^ 02000000, data);               /* yes, invert A19 */
    else
        WrxMemW (M, pa, data);
    return;
    }
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

void hsc_WrMemB (int32 pa, int32 data)
{
if (ADDR_IS_DMEM (pa)) {
    pa = pa & DMEMMASK;
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "DMEM wr: %07o = %03o at %o\n", pa, data, (uint16)inst_pc);
    WrxMemB (DM, pa, data);
    return;
    }
if (ADDR_IS_CMEM (pa)) {
    pa = pa & CMEMMASK;
    CADR = (pa >> 1);                                   /* save address */
    CDATA = data;                                       /* data history */
    if (PCSR & PCSR_NMA)                                /* non-memory access cycle */
        CCYCLE = 07;
    else {
        if (PCSR & PCSR_CIE) {                          /* interrupt cycle */
            CCYCLE = 02;
            if (data & 010)
                SET_INT (CB7);
            if (data & 04)
                SET_INT (CB6);
            if (data & 02)
                SET_INT (CB5);
            if (data & 01)
                SET_INT (CB4);
            trap_req = calc_ints (ipl, trap_req);       /* recalc int req */
            }
        else if (PCSR & PCSR_LCK) {                     /* lock cycle */
            CCYCLE = 04;
            WrxMemW (CM, pa, 0100000);                  /* indicate locked */
            }
        else CCYCLE = 01;                               /* read/write cycle */
        if (PCSR & PCSR_CBK)                            /* bank swap? */
            WrxMemB (CM, pa + (CMEMSIZE >> 1), data);   /* select upper half of memory */
        else
            WrxMemB (CM, pa, data);
        }
    if (sim_log)
        fprintf (sim_log, "CMEM wr (%s): %07o = %03o at %o\n", cycles[CCYCLE >> 1], pa, data, (uint16)inst_pc);
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    return;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        WrxMemB (M, pa ^ 02000000, data);               /* yes, invert A19 */
    else
        WrxMemB (M, pa, data);
    return;
    }             
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}
