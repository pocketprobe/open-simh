/* pdp11_defs.h: PDP-11 simulator definitions

   Copyright (c) 1993-2011, Robert M Supnik

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
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   The author gratefully acknowledges the help of Max Burnet, Megan Gentry,
   and John Wilson in resolving questions about the PDP-11

   11-Dec-11    RMS     Fixed priority of PIRQ vs IO; added INT_INTERNALn
   22-May-10    RMS     Added check for 64b definitions
   19-Nov-08    RMS     Moved I/O support routines to I/O library
   16-May-08    RMS     Added KE11A, DC11 support
   02-Feb-08    RMS     Fixed DMA memory address limit test (found by John Dundas)
   25-Jan-08    RMS     Added RC11, KG11A support (from John Dundas)
   16-Dec-06    RMS     Added TA11 support
   29-Oct-06    RMS     Added clock coscheduling
   06-Jul-06    RMS     Added multiple KL11/DL11 support
   26-Jun-06    RMS     Added RF11 support
   24-May-06    RMS     Added 11/44 DR support (from CIS diagnostic)
   17-May-06    RMS     Added CR11/CD11 support (from John Dundas)
   30-Sep-04    RMS     Added Massbus support
                        Removed Map_Addr prototype
                        Removed map argument from Unibus routines
                        Added framework for model selection
   28-May-04    RMS     Added DHQ support
   25-Jan-04    RMS     Removed local debug logging support
   22-Dec-03    RMS     Added second DEUNA/DELUA support
   18-Oct-03    RMS     Added DECtape off reel message
   19-May-03    RMS     Revised for new conditional compilation
   05-Apr-03    RMS     Fixed bug in MMR1 update (found by Tim Stark)
   28-Feb-03    RMS     Added TM logging support
   19-Jan-03    RMS     Changed mode definitions for Apple Dev Kit conflict
   11-Nov-02    RMS     Changed log definitions to be VAX compatible
   10-Oct-02    RMS     Added vector information to DIB
                        Changed DZ11 vector to Unibus standard
                        Added DEQNA/DELQA, DEUNA/DELUA support
                        Added multiple RQDX3, autoconfigure support
   12-Sep-02    RMS     Added TMSCP, KW11P,and RX211 support
   28-Apr-02    RMS     Clarified PDF ACF mnemonics
   22-Apr-02    RMS     Added HTRAP, BPOK maint register flags, MT_MAXFR
   06-Mar-02    RMS     Changed system type to KDJ11A
   20-Jan-02    RMS     Added multiboard DZ11 support
   09-Nov-01    RMS     Added bus map support
   07-Nov-01    RMS     Added RQDX3 support
   26-Oct-01    RMS     Added symbolic definitions for IO page
   19-Oct-01    RMS     Added DZ definitions
   15-Oct-01    RMS     Added logging capabilities
   07-Sep-01    RMS     Revised for multilevel interrupts
   01-Jun-01    RMS     Added DZ11 support
   23-Apr-01    RMS     Added RK611 support
   05-Apr-01    RMS     Added TS11/TSV05 support
   10-Feb-01    RMS     Added DECtape support
*/

#ifndef _HSC_DEFS_H
#define _HSC_DEFS_H   0

#include "pdp11_defs.h"                                 /* simulator defns */

#define IOPAGEBASE      017760000                       /* 2**22 - 2**13 */
#define IOPAGESIZE      000020000                       /* 2**13 */
#define IOPAGEMASK      (IOPAGESIZE - 1)                /* 2**13 - 1 */

/* CPU models */

#define MOD_HSC50       21
#define MOD_HSC70       22

#define CPUT_HSC50      (1u << MOD_HSC50)               /* HSC50 */
#define CPUT_HSC70      (1u << MOD_HSC70)               /* HSC70 */

#define CPUT_F          (CPUT_HSC50)
#define CPUT_J          (CPUT_HSC70)

/* Other memory spaces */

#define ADDR_IS_CMEM(x) ((((t_addr) (x)) < 017000000) && (((t_addr) (x)) >= 016000000))
#define ADDR_IS_DMEM(x) ((((t_addr) (x)) < 016000000) && (((t_addr) (x)) >= 014000000))

/* IO parameters */

#define DLX_LINES       2                               /* max # of KL11/DL11's */

/* Device information block */

/* Processor registers which have I/O page addresses */

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

/* Function prototypes */

int32 clk_cosched (int32 wait);

#include "pdp11_io_lib.h"

#endif
