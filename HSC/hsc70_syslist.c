/* hsc70_syslist.c: HSC-70 device list

   Copyright (c) 2017, Matt Burke

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

char sim_name[] = "HSC70";

extern DEVICE cpu_dev;
extern DEVICE sys_dev;
extern DEVICE tti_dev;
extern DEVICE tto_dev;
extern DEVICE clk_dev;
extern DEVICE dli_dev;
extern DEVICE dlo_dev;
extern DEVICE fl_dev;
extern DEVICE rom_dev;
extern DEVICE ioc_dev;

DEVICE *sim_devices[] = {
    &cpu_dev,
    &sys_dev,
    &clk_dev,
    &tti_dev,
    &tto_dev,
    &dli_dev,
    &dlo_dev,
    &fl_dev,
    &rom_dev,
    &ioc_dev,
    NULL
    };
