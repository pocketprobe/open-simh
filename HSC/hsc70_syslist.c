/* pdp11_syslist.c: PDP-11 device list

   Copyright (c) 1993-2012, Robert M Supnik

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

   21-Jan-13    MB      First version
*/

#include "pdp11_defs.h"

char sim_name[] = "HSC70";

extern DEVICE cpu_dev;
extern DEVICE sys_dev;
extern DEVICE tti_dev;
extern DEVICE tto_dev;
extern DEVICE clk_dev;
extern DEVICE pclk_dev;
extern DEVICE dli_dev;
extern DEVICE dlo_dev;
extern DEVICE dci_dev;
extern DEVICE dco_dev;
extern DEVICE rq_dev, rqb_dev, rqc_dev, rqd_dev;

DEVICE *sim_devices[] = {
    &cpu_dev,
    &sys_dev,
    &clk_dev,
    &pclk_dev,
    &tti_dev,
    &tto_dev,
    &cr_dev,
    &dli_dev,
    &dlo_dev,
    &dci_dev,
    &dco_dev,
    &rq_dev,
    &rqb_dev,
    &rqc_dev,
    &rqd_dev,
    NULL
    };
