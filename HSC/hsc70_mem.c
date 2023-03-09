/* hsc70_mem.c: HSC-70 memory

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
extern int32 EADR;
extern int32 CPUERR;
extern jmp_buf save_env;

extern int32 calc_ints (int32 nipl, int32 trq);

int32 DADR = 0;                                         /* data memory address */

int32 hsc_RdMemW (int32 pa)
{
int32 data;

if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~07;
//    sim_printf ("DMEM PReadW: %o\n", pa);
    return RdPMemW (pa);
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & 04)                                      /* non-memory access */
//        data = 0;
//    else
        data = RdPMemW (pa);
    if (PCSR & 02) {
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
    if (PCSR & 01) {
        WrPMemW (pa, 0100000);
        }
    PCSR = PCSR & ~07;
//    sim_printf ("CMEM rd: %o\n", pa);
    return data;
    }
if (ADDR_IS_PMEM (pa))                                  /* program memory address? */
    return RdPMemW (pa);
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

int32 hsc_RdMemB (int32 pa)
{
int32 data;

if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~07;
//    sim_printf ("DMEM PReadB: %o\n", pa);
    return RdPMemB (pa);
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & 04)                                      /* non-memory access */
//        data = 0;
//    else
        data = RdPMemB (pa);
    if (PCSR & 02) {
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
    if (PCSR & 01) {
        WrPMemW (pa, 0100000);
        }
    PCSR = PCSR & ~07;
//    sim_printf ("CMEM rd: %o\n", pa);
    return data;
    }
if (ADDR_IS_PMEM (pa))                                  /* program memory address? */
    return RdPMemB (pa);
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}

void hsc_WrMemW (int32 pa, int32 data)
{
if (ADDR_IS_DMEM (pa)) {
    DADR = pa;
    PCSR = PCSR & ~07;
//    sim_printf ("DMEM PWriteW: %o = %o\n", pa, data);
    WrPMemW (pa, data);
    return;
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & 04) {                                    /* non-memory access */
//        PCSR = PCSR & ~07;
//        return;
//        }
    if (PCSR & 02) {
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
    PCSR = PCSR & ~07;
//    sim_printf ("CMEM wr: %o = %o\n", pa, data);
    WrPMemW (pa, data);
    return;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
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
    PCSR = PCSR & ~07;
//    sim_printf ("DMEM PWriteB: %o = %o\n", pa, data);
    WrPMemB (pa, data);
    return;
    }
if (ADDR_IS_CMEM (pa)) {
//    if (PCSR & 04) {                                    /* non-memory access */
//        PCSR = PCSR & ~07;
//        return;
//        }
    if (PCSR & 02) {
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
    PCSR = PCSR & ~07;
//    sim_printf ("CMEM wr: %o = %o\n", pa, data);
    WrPMemB (pa, data);
    return;
    }
if (ADDR_IS_PMEM (pa)) {                                /* program memory address? */
    WrPMemB (pa, data);
    return;
    }             
EADR = pa;
setCPUERR (CPUE_NXM);
ABORT (TRAP_NXM);
}
