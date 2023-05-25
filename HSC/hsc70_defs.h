/* hsc70_defs.h HSC70 simulator definitions

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

#ifndef _HSC_DEFS_H
#define _HSC_DEFS_H   0

#ifndef VM_HSC
#define VM_HSC          0
#endif

#define INIMODEL        MOD_HSC70
#define INIOPTNS        SOP_1173
#define INIMEMSIZE      004000000                       /* 2**20 */
#define ADDR_IS_MEM(x)  (((t_addr) (x)) < IOPAGEBASE)   /* All of memory space handled by hsc70_mem.c */

/* Other memory spaces */

#define CMEMWIDTH       18                              /* Control memory addr width */
#define CMEMSIZE        (1u << CMEMWIDTH)               /* Control memory size */
#define CMEMMASK        (CMEMSIZE - 1)                  /* Control memory addr mask */
#define CMEMBASE        016000000                       /* Control memory base addr */
#define ADDR_IS_CMEM(x) ((((t_addr) (x)) >= CMEMBASE) && \
                        (((t_addr) (x)) < (CMEMBASE + CMEMSIZE)))

#define DMEMWIDTH       19                              /* Data memory addr width */
#define DMEMSIZE        (1u << DMEMWIDTH)               /* Data memory size */
#define DMEMMASK        (DMEMSIZE - 1)                  /* Data memory addr mask */
#define DMEMBASE        014000000                       /* Data memory base addr */
#define ADDR_IS_DMEM(x) ((((t_addr) (x)) >= DMEMBASE) && \
                        (((t_addr) (x)) < (DMEMBASE + DMEMSIZE)))

#define ADDR_IS_PMEM(x) (((t_addr) (x)) < INIMEMSIZE)

/* I/O page layout */

#define IOBA_CWIN       IOPAGEBASE                      /* control memory window */
#define IOLN_CWIN       010000
#define IOBA_PIO        (IOPAGEBASE + 010000)           /* P.ioj registers */
#define IOLN_PIO        060
#define IOBA_ROM        (IOPAGEBASE + 013000)           /* boot ROM */
#define IOLN_ROM        04000
#define IOBA_FL         (IOPAGEBASE + 017400)           /* floppy controller */
#define IOLN_FL         010
#define IOBA_DL         (IOPAGEBASE + 017520)           /* serial ports */
#define VEC_DL          0300

#define DEV_V_HSC       (DEV_V_FFUF + 0)
#define DEV_V_CI        (DEV_V_FFUF + 1)
#define DEV_HSC         (1u << DEV_V_HSC)
#define DEV_CI          (1u << DEV_V_CI)

/* HSC Device information block */

struct hsc_dib {
    DIB                 dib;                            /* PDP-11 device information block */
    uint16              sts;                            /* status code */
    uint32              cma;                            /* control memory address */
    t_stat              (*init)(uint16 req, uint16 act);
    };

typedef struct hsc_dib HDIB;

#define RdMemW(pa)      hsc_RdMemW (pa)
#define RdMemB(pa)      hsc_RdMemB (pa)
#define WrMemW(pa,d)    hsc_WrMemW (pa, d)
#define WrMemB(pa, d)   hsc_WrMemB (pa, d)

int32 hsc_RdMemW (int32 pa);
int32 hsc_RdMemB (int32 pa);
void hsc_WrMemW (int32 pa, int32 d);
void hsc_WrMemB (int32 pa, int32 d);

#define RdxMemW(x,pa)   (x[(pa) >> 1])
#define RdxMemB(x,pa)   ((((pa) & 1)? x[(pa) >> 1] >> 8: x[(pa) >> 1]) & 0377)
#define WrxMemW(x,pa,d) x[(pa) >> 1] = (d)
#define WrxMemB(x,pa,d) x[(pa) >> 1] = ((pa) & 1)? \
                            ((x[(pa) >> 1] & 0377) | (((d) & 0377) << 8)): \
                            ((x[(pa) >> 1] & ~0377) | ((d) & 0377))

extern uint16 *CM;                                      /* Control memory */
extern uint16 *DM;                                      /* Data memory */

/* P.ioj control/status register */

#define PCSR_ENA        0100000                         /* OCP secure/enable */
#define PCSR_CBK        0004000                         /* control mem bank swap */
#define PCSR_PBK        0002000                         /* program mem bank swap */
#define PCSR_PG2        0000400                         /* boot ROM page select */
#define PCSR_CBE        0000200                         /* control bus enable */
#define PCSR_HIP        0000040                         /* high byte parity */
#define PCSR_LOP        0000020                         /* low byte parity */
#define PCSR_LED        0000010                         /* P.ioj module LED */
#define PCSR_NMA        0000004                         /* non-memory access */
#define PCSR_CIE        0000002                         /* control mem interrupt enable */
#define PCSR_LCK        0000001                         /* control mem lock enable */

#endif
