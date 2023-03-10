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

/* Other memory spaces */

#define ADDR_IS_CMEM(x) ((((t_addr) (x)) < 017000000) && (((t_addr) (x)) >= 016000000))
#define ADDR_IS_DMEM(x) ((((t_addr) (x)) < 016000000) && (((t_addr) (x)) >= 014000000))
#define ADDR_IS_PMEM(x) (((t_addr) (x)) < 004000000)

/* IO parameters */

#define DLX_LINES       2                               /* max # of KL11/DL11's */

/* I/O page layout */

#define IOBA_FL         (IOPAGEBASE + 017400)
#define IOLN_FL         010

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
