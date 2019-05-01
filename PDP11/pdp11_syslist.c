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

/* SCP data structures and interface routines

   sim_name             simulator name string
   sim_devices          array of pointers to simulated devices
*/

#if !defined (UC15)
char sim_name[] = "PDP-11";
#else
char sim_name[] = "UC-15";
#endif

extern DEVICE cpu_dev;
extern DEVICE sys_dev;
extern DEVICE ptr_dev;
extern DEVICE ptp_dev;
extern DEVICE tti_dev;
extern DEVICE tto_dev;
extern DEVICE lpt_dev;
extern DEVICE cr_dev;
extern DEVICE clk_dev;
extern DEVICE pclk_dev;
extern DEVICE dli_dev;
extern DEVICE dlo_dev;
extern DEVICE dci_dev;
extern DEVICE dco_dev;
extern DEVICE tdc_dev;
extern DEVICE dz_dev;
extern DEVICE vh_dev;
extern DEVICE dh_dev;
extern DEVICE dt_dev;
extern DEVICE rc_dev;
extern DEVICE rf_dev;
extern DEVICE rk_dev;
extern DEVICE rl_dev;
extern DEVICE hk_dev;
extern DEVICE rx_dev;
extern DEVICE ry_dev;
extern DEVICE mba_dev[];
extern DEVICE rp_dev;
extern DEVICE rr_dev;
extern DEVICE rs_dev;
extern DEVICE rq_dev, rqb_dev, rqc_dev, rqd_dev;
extern DEVICE tm_dev;
extern DEVICE tq_dev;
extern DEVICE ts_dev;
extern DEVICE tu_dev;
extern DEVICE ta_dev;
#ifdef USE_DISPLAY
extern DEVICE vt_dev;
#endif
extern DEVICE xq_dev, xqb_dev;
extern DEVICE xu_dev, xub_dev;
extern DEVICE ke_dev;
extern DEVICE kg_dev;
extern DEVICE dmc_dev;
extern DEVICE dup_dev;
extern DEVICE dpv_dev;
extern DEVICE kmc_dev;
extern DEVICE uca_dev, ucb_dev;
extern DEVICE rom_dev;
extern DEVICE ch_dev;
#ifdef USE_DISPLAY
extern DEVICE ng_dev;
extern DEVICE daz_dev;
extern DEVICE tv_dev;
#endif
extern DEVICE mb_dev;

DEVICE *sim_devices[] = {
    &cpu_dev,
    &sys_dev,
#if !defined (UC15)
    &mba_dev[0],
    &mba_dev[1],
    &mba_dev[2],
    &clk_dev,
    &pclk_dev,
    &ptr_dev,
    &ptp_dev,
    &tti_dev,
    &tto_dev,
    &tdc_dev,
    &cr_dev,
    &lpt_dev,
    &dli_dev,
    &dlo_dev,
    &dci_dev,
    &dco_dev,
    &dz_dev,
    &vh_dev,
    &dh_dev,
    &rc_dev,
    &rf_dev,
    &rk_dev,
    &rl_dev,
    &hk_dev,
    &rx_dev,
    &ry_dev,
    &rp_dev,
    &rr_dev,
    &rs_dev,
    &rq_dev,
    &rqb_dev,
    &rqc_dev,
    &rqd_dev,
    &dt_dev,
    &tm_dev,
    &ts_dev,
    &tq_dev,
    &tu_dev,
    &ta_dev,
#ifdef USE_DISPLAY
    &vt_dev,
#endif
    &xq_dev,
    &xqb_dev,
    &xu_dev,
    &xub_dev,
    &kg_dev,
    &dmc_dev,
    &dup_dev,
    &dpv_dev,
    &kmc_dev,
    &ke_dev,
    &rom_dev,
    &ch_dev,
#ifdef USE_DISPLAY
    &ng_dev,
    &daz_dev,
    &tv_dev,
#endif
    &mb_dev,
#else
    &clk_dev,
    &tti_dev,
    &tto_dev,
    &cr_dev,
    &lpt_dev,
    &rk_dev,
    &uca_dev,
    &ucb_dev,
#endif
    NULL
    };
