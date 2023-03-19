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

#define RdPMemW(pa)     (M[(pa) >> 1])
#define RdPMemB(pa)     ((((pa) & 1)? M[(pa) >> 1] >> 8: M[(pa) >> 1]) & 0377)
#define WrPMemW(pa,d)   M[(pa) >> 1] = (d)
#define WrPMemB(pa,d)   M[(pa) >> 1] = ((pa) & 1)? \
                            ((M[(pa) >> 1] & 0377) | (((d) & 0377) << 8)): \
                            ((M[(pa) >> 1] & ~0377) | ((d) & 0377))

extern int32 PCSR;
extern int32 trap_req, ipl;
extern int32 CPUERR;
extern jmp_buf save_env;
extern FILE *sim_log;
extern int16 inst_pc;

extern int32 calc_ints (int32 nipl, int32 trq);

int32 DADR = 0;                                         /* data memory address */
int32 EADR = 0;                                         /* error address */

int32 hsc_RdMemW (int32 pa)
{
int32 data;

if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "DMEM rdw: %o at %o\n", pa, (uint16)inst_pc);
    return RdPMemW (pa);
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & PCSR_NMA)                                /* non-memory access */
//        data = 0;
//    else
    if (PCSR & PCSR_CBK)                                /* bank swap? */
        data = RdPMemW (pa - 04000000);                 /* FIXME - temporary relocation */
    else
        data = RdPMemW (pa);
    if (PCSR & PCSR_CIE) {
        if (data & 010)
            SET_INT (CB7);
        if (data & 04)
            SET_INT (CB6);
        if (data & 02)
            SET_INT (CB5);
        if (data & 01)
            SET_INT (CB4);
        trap_req = calc_ints (ipl, trap_req);           /* recalc int req */
        }
    if (PCSR & PCSR_LCK) {
        WrPMemW (pa, 0100000);
        }
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "CMEM rdw: %o at %o\n", pa, (uint16)inst_pc);
    return data;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        return RdPMemW (pa ^ 02000000);                 /* yes, invert A19 */
    else
        return RdPMemW (pa);
    }
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

int32 hsc_RdMemB (int32 pa)
{
int32 data;

if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);        /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "DMEM rdb: %o at %o\n", pa, (uint16)inst_pc);
    return RdPMemB (pa);
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & PCSR_NMA)                                /* non-memory access */
//        data = 0;
//    else
    if (PCSR & PCSR_CBK)                                /* bank swap? */
        data = RdPMemB (pa - 04000000);                 /* FIXME - temporary relocation */
    else
        data = RdPMemB (pa);
    if (PCSR & PCSR_CIE) {
        if (data & 010)
            SET_INT (CB7);
        if (data & 04)
            SET_INT (CB6);
        if (data & 02)
            SET_INT (CB5);
        if (data & 01)
            SET_INT (CB4);
        trap_req = calc_ints (ipl, trap_req);           /* recalc int req */
        }
    if (PCSR & PCSR_LCK) {
        WrPMemW (pa, 0100000);
        }
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "CMEM rdb: %o at %o\n", pa, (uint16)inst_pc);
    return data;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        return RdPMemB (pa ^ 02000000);                 /* yes, invert A19 */
    else
        return RdPMemB (pa);
    }
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

void hsc_WrMemW (int32 pa, int32 data)
{
if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "DMEM wrw: %o = %o at %o\n", pa, data, (uint16)inst_pc);
    WrPMemW (pa, data);
    return;
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & PCSR_NMA) {                             /* non-memory access */
//        PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK); /* reset special cycle */
//        return;
//        }
    if (PCSR & PCSR_CIE) {
        if (data & 010)
            SET_INT (CB7);
        if (data & 04)
            SET_INT (CB6);
        if (data & 02)
            SET_INT (CB5);
        if (data & 01)
            SET_INT (CB4);
        trap_req = calc_ints (ipl, trap_req);           /* recalc int req */
        }
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "CMEM wrw: %o = %o at %o\n", pa, data, (uint16)inst_pc);
    if (PCSR & PCSR_CBK)                                /* bank swap? */
        WrPMemW (pa - 04000000, data);                  /* FIXME - temporary relocation */
    else
        WrPMemW (pa, data);
    return;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        WrPMemW (pa ^ 02000000, data);                  /* yes, invert A19 */
    else
        WrPMemW (pa, data);
    return;
    }
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

void hsc_WrMemB (int32 pa, int32 data)
{
if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "DMEM wrb: %o = %o at %o\n", pa, data, (uint16)inst_pc);
    WrPMemB (pa, data);
    return;
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & PCSR_NMA) {                             /* non-memory access */
//        PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK); /* reset special cycle */
//        return;
//        }
    if (PCSR & PCSR_CIE) {
        if (data & 010)
            SET_INT (CB7);
        if (data & 04)
            SET_INT (CB6);
        if (data & 02)
            SET_INT (CB5);
        if (data & 01)
            SET_INT (CB4);
        trap_req = calc_ints (ipl, trap_req);           /* recalc int req */
        }
    PCSR = PCSR & ~(PCSR_NMA | PCSR_CIE | PCSR_LCK);    /* reset special cycle */
    if (sim_log)
        fprintf (sim_log, "CMEM wrb: %o = %o at %o\n", pa, data, (uint16)inst_pc);
    if (PCSR & PCSR_CBK)                                /* bank swap? */
        WrPMemB (pa - 04000000, data);                  /* FIXME - temporary relocation */
    else
        WrPMemB (pa, data);
    return;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    if (PCSR & PCSR_PBK)                                /* bank swap? */
        WrPMemB (pa ^ 02000000, data);                  /* yes, invert A19 */
    else
        WrPMemB (pa, data);
    return;
    }             
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}
