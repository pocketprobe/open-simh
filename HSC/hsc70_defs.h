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

#define INIMODEL        MOD_HSC70
#define INIOPTNS        SOP_1173
//#define INIMEMSIZE      004000000                       /* 2**20 */
#define INIMEMSIZE      020000000                         // temp to cover control and data memory
#define ADDR_IS_MEM(x)  (((t_addr) (x)) < 017000000)

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

#define ADDR_IS_PMEM(x) (((t_addr) (x)) < 004000000)

/* I/O page layout */

#define IOBA_CWIN       IOPAGEBASE
#define IOLN_CWIN       010000
#define IOBA_IOC        (IOPAGEBASE + 010000)
#define IOLN_IOC        060
#define IOBA_ROM        (IOPAGEBASE + 013000)
#define IOLN_ROM        04000
#define IOBA_FL         (IOPAGEBASE + 017400)
#define IOLN_FL         010
#define IOBA_DL         (IOPAGEBASE + 017520)
#define VEC_DL          0300

#define RdMemW(pa)      hsc_RdMemW (pa)
#define RdMemB(pa)      hsc_RdMemB (pa)
#define WrMemW(pa,d)    hsc_WrMemW (pa, d)
#define WrMemB(pa, d)   hsc_WrMemB (pa, d)

int32 hsc_RdMemW (int32 pa);
int32 hsc_RdMemB (int32 pa);
void hsc_WrMemW (int32 pa, int32 d);
void hsc_WrMemB (int32 pa, int32 d);

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

# if 0

/* IO parameters */

#define DLX_LINES       2                               /* max # of KL11/DL11's */

/* Interrupt assignments; within each level, priority is right to left
   PIRQn has the highest priority with a level and is always bit <0>
   On level 6, the clock is second highest priority */

#define INT_V_CB7       1                               /* BR7 */

#define INT_V_CB6       5                               /* BR6 */

#define INT_V_CB5       20                              /* BR5 */

#define INT_V_FL        13                              /* BR4 */
#define INT_V_CB4       14

#define INT_FL          (1u << INT_V_FL)
#define INT_CB7         (1u << INT_V_CB7)
#define INT_CB6         (1u << INT_V_CB6)
#define INT_CB5         (1u << INT_V_CB5)
#define INT_CB4         (1u << INT_V_CB4)

#define IPL_FL          4                               /* int pri levels */
#define IPL_CB7         7
#define IPL_CB6         6
#define IPL_CB5         5
#define IPL_CB4         4

/* Processor specific internal fixed vectors */

#define VEC_FL          0230
#define VEC_CB7         0134
#define VEC_CB6         0130
#define VEC_CB5         0124
#define VEC_CB4         0120

#endif

#endif
