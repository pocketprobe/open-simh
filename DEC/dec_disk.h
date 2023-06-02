/* dec_disk.h: DEC common disk definitions

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

#ifndef _DEC_DISK_H_
#define _DEC_DISK_H_

#define DECF_RMV        01                              /* removable */
#define DECF_RO         02                              /* read only */
#define DECF_SDI        04                              /* SDI drive */

#define RA60_SDIV       3
#define RA60_STO        3
#define RA60_LTO        7
#define RA60_RETRY      15
#define RA60_XFRT       158
#define RA60_ERL        0
#define RA60_EET        4
#define RA60_SWR        4
#define RA60_HWR        0                               /* unconfirmed */
#define RA60_HPS        5
#define RA60_DPS        13
#define RA60_SECT_16    42                              /* +1 spare/track */
#define RA60_SECT_18    38                              /* unknown */
#define RA60_SURF       6
#define RA60_LCYL       2388                            /* 0-274 user */
#define RA60_XCYL       6                               /* XBN cyls */
#define RA60_DCYL       6                               /* DBN cyls */
#define RA60_TPG        1                               /* tracks/group */
#define RA60_GPC        4                               /* groups/cyl */
#define RA60_GOF_16     16                              /* group offset */
#define RA60_GOF_18     15                              /* unknown */
#define RA60_DRG        2
#define RA60_LBN_16     400176
#define RA60_LBN_18     362064                          /* unknown */
#define RA60_FCTS       172
#define RA60_RCTS_16    168
#define RA60_RCTS_18    152                             /* unknown */
#define RA60_RCTC       4
#define RA60_RBN        1
#define RA60_REVS       60
#define RA60_MOD        4
#define RA60_MED        0x22A4103C
#define RA60_FLGS       (DECF_RMV | DECF_SDI)

#define RA70_SDIV       4
#define RA70_STO        3
#define RA70_LTO        7
#define RA70_RETRY      5
#define RA70_XFRT       116
#define RA70_ERL        10
#define RA70_EET        6
#define RA70_SWR        0                               /* unconfirmed */
#define RA70_HWR        0x80                            /* unconfirmed + limited diag support */
#define RA70_HPS        4
#define RA70_DPS        14
#define RA70_SECT_16    33                              /* +1 spare/track */
#define RA70_SECT_18    0                               /* not supported */
#define RA70_SURF       6
#define RA70_LCYL       1511                            /* 0-274 user */
#define RA70_XCYL       4                               /* XBN cyls */
#define RA70_DCYL       2                               /* DBN cyls */
#define RA70_TPG        1                               /* tracks/group */
#define RA70_GPC        11                              /* groups/cyl */
#define RA70_GOF_16     8                               /* group offset */
#define RA70_GOF_18     0                               /* not supported */
#define RA70_DRG        11
#define RA70_LBN_16     547041
#define RA70_LBN_18     0                               /* not supported */
#define RA70_FCTS       204
#define RA70_RCTS_16    198
#define RA70_RCTS_18    0                               /* not supported */
#define RA70_RCTC       4
#define RA70_RBN        1
#define RA70_REVS       67
#define RA70_MOD        18
#define RA70_MED        0x25641046
#define RA70_FLGS       (DECF_RMV | DECF_SDI)

#define RA80_SDIV       3
#define RA80_STO        3
#define RA80_LTO        7
#define RA80_RETRY      5
#define RA80_XFRT       97
#define RA80_ERL        0
#define RA80_EET        2
#define RA80_SWR        10                              /* unconfirmed */
#define RA80_HWR        0                               /* unconfirmed */
#define RA80_HPS        4
#define RA80_DPS        11
#define RA80_SECT_16    31                              /* +1 spare/track */
#define RA80_SECT_18    0                               /* unknown */
#define RA80_SURF       14
#define RA80_LCYL       275                             /* 0-274 user */
#define RA80_XCYL       2                               /* XBN cyls */
#define RA80_DCYL       2                               /* DBN cyls */
#define RA80_TPG        RA80_SURF                       /* tracks/group */
#define RA80_GPC        2                               /* groups/cyl */
#define RA80_GOF_16     16                              /* group offset */
#define RA80_GOF_18     0                               /* unknown */
#define RA80_DRG        1
#define RA80_LBN_16     237212
#define RA80_LBN_18     0                               /* unknown */
#define RA80_FCTS       480
#define RA80_RCTS_16    465
#define RA80_RCTS_18    0                               /* unknown */
#define RA80_RCTC       4
#define RA80_RBN        1
#define RA80_REVS       60
#define RA80_MOD        1
#define RA80_MED        0x25641050
#define RA80_FLGS       DECF_SDI

#define RA81_SDIV       3
#define RA81_STO        4
#define RA81_LTO        7
#define RA81_RETRY      5
#define RA81_XFRT       174
#define RA81_ERL        0
#define RA81_EET        6
#define RA81_SWR        7
#define RA81_HWR        0                               /* unconfirmed */
#define RA81_HPS        12
#define RA81_DPS        19
#define RA81_SECT_16    51                              /* +1 spare/track */
#define RA81_SECT_18    46
#define RA81_SURF       14
#define RA81_LCYL       1252                            /* 0-274 user */
#define RA81_XCYL       4                               /* XBN cyls */
#define RA81_DCYL       2                               /* DBN cyls */
#define RA81_TPG        1                               /* tracks/group */
#define RA81_GPC        RA81_SURF                       /* groups/cyl */
#define RA81_GOF_16     14                              /* group offset */
#define RA81_GOF_18     12
#define RA81_DRG        14
#define RA81_LBN_16     891072
#define RA81_LBN_18     803712
#define RA81_FCTS       780
#define RA81_RCTS_16    765
#define RA81_RCTS_18    690
#define RA81_RCTC       4
#define RA81_RBN        1
#define RA81_REVS       60
#define RA81_MOD        5
#define RA81_MED        0x25641051
#define RA81_FLGS       DECF_SDI

#define RA82_SDIV       4
#define RA82_STO        4
#define RA82_LTO        7
#define RA82_RETRY      5
#define RA82_XFRT       192
#define RA82_ERL        7
#define RA82_EET        6
#define RA82_SWR        7                               /* unconfirmed */
#define RA82_HWR        0                               /* unconfirmed */
#define RA82_HPS        6
#define RA82_DPS        18
#define RA82_SECT_16    57                              /* +1 spare/track */
#define RA82_SECT_18    51
#define RA82_SURF       14
#define RA82_LCYL       1427                            /* 0-274 user */
#define RA82_XCYL       4                               /* XBN cyls */
#define RA82_DCYL       4                               /* DBN cyls */
#define RA82_TPG        1                               /* tracks/group */
#define RA82_GPC        15                              /* groups/cyl */
#define RA82_GOF_16     11                              /* group offset */
#define RA82_GOF_18     14
#define RA82_DRG        15
#define RA82_LBN_16     1216665
#define RA82_LBN_18     1088595
#define RA82_FCTS       928
#define RA82_RCTS_16    912
#define RA82_RCTS_18    816
#define RA82_RCTC       4
#define RA82_RBN        1
#define RA82_REVS       60
#define RA82_MOD        11
#define RA82_MED        0x25641052
#define RA82_FLGS       DECF_SDI

//#define RA80_CYL        (RA80_LCYL + RA80_XCYL + RA80_DCYL)
//#define RA80_TPC        (RA80_TPG * RA80_GPC)           /* tracks/cyl */
//#define RA80_XBNC       (RA80_TPC * (RA80_SECT + RA80_RBN)) /* XBNs/cyl */
//#define RA80_XBN        (RA80_XCYL * RA80_XBNC)         /* XBNs */
//#define RA80_DBN        (RA80_DCYL * RA80_XBNC)         /* DBNs */

#endif
