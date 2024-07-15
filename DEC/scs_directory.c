/* scs_directory.c: SCS Directory Service

   Copyright (c) 2024, Matt Burke

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

#include "sim_defs.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"

/* Request format */

#define DIR_FORM        (SCS_APPL + 0x00)               /* lookup form */
#define  DIR_C_NAME     0                               /* by name */
#define  DIR_C_ENTRY    1                               /* by entry */
#define DIR_STATUS      (SCS_APPL + 0x00)               /* match status */
#define DIR_ENTRY       (SCS_APPL + 0x02)               /* entry number */
#define DIR_PRCNAM      (SCS_APPL + 0x04)               /* process name */
#define DIR_PRCINFO     (SCS_APPL + 0x14)               /* process information */

#define LEN_DIR         (DIR_PRCINFO + 16)

/* SYSAP - SCS$DIRECTORY */

t_stat app_scsdir (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
SCA_CTX *ctx = uptr->sca_ctx;
SYSAP *sysap = ctx->sysap;
uint32 entry;

for (entry = 1; sysap; sysap++, entry++) {              /* entry numbers start from one */
    if (!sysap->name) {                                 /* no more processes? */
        sysap = NULL;                                   /* no match */
        break;
        }
    if (pkt->data[DIR_FORM] == DIR_C_NAME) {            /* lookup by name */
        if (strncmp (&pkt->data[DIR_PRCNAM], sysap->name, strlen (sysap->name)) == 0)
            break;                                      /* matched */
        }
    else if (entry == pkt->data[DIR_ENTRY])             /* lookup by entry */
        break;                                          /* matched */
    }

if (sysap) {
    CI_PUTW (pkt->data, DIR_STATUS, SCS_STNORM);        /* status OK */
    sprintf (&pkt->data[DIR_PRCINFO], "%-16s", sysap->name);
    }
else {
    CI_PUTW (pkt->data, DIR_STATUS, SCS_STNOMAT);       /* no match */
    }
pkt->length = LEN_DIR;

return scs_send_msg (uptr, cid, pkt);                   /* send response */
}
