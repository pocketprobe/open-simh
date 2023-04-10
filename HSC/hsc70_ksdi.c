/* hsc70_ksdi.c: HSC70 K.sdi simulation

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
#include "hsc70_kreq.h"

extern int32 tmxr_poll;                                 /* calibrated delay */

t_stat ksdi_svc (UNIT *uptr);
t_stat ksdi_reset (DEVICE *dptr);
t_stat ksdi_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ksdi_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ksdi_init (uint16 req, uint16 act);

/* debugging bitmaps */

#define DBG_TRC         KREQ_DBG_TRC                    /* trace routine calls */

DEBTAB ksdi_debug[] = {
    { "TRACE",  DBG_TRC,   "trace routine calls" },
    { 0 }
};

HDIB ksdi_dib = {
    { 0, 0, NULL, NULL,
      0, 0, 0, { NULL } },
    K_SDI, 0, &ksdi_init
    };

UNIT ksdi_unit = { UDATA (&ksdi_svc, 0, 0) };

REG ksdi_reg[] = {
    { NULL }
    };

MTAB ksdi_mod[] = {
    { MTAB_XTD|MTAB_VDV, 004, "SLOT", "SLOT",
      NULL, &k_show_req, NULL },
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", "ADDRESS",
      NULL, &k_show_addr, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "CONTROL", "CONTROL",
      NULL, &ksdi_show, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 004, "RUN", "RUN",
      &ksdi_run, NULL, NULL },
    { 0 }
    };

DEVICE ksdi_dev = {
    "KSDI", &ksdi_unit, ksdi_reg, ksdi_mod,
    1, DEV_RDX, 16, 1, DEV_RDX, 16,
    NULL, NULL, &ksdi_reset,
    NULL, NULL, NULL,
    &ksdi_dib, DEV_DEBUG | DEV_HSC,
    0, ksdi_debug
    };


/* The version must be at least DS = 3, MC = 00 */

t_stat ksdi_init (uint16 req, uint16 act)
{
if (act) {
    ksdi_dib.cma = k_init_common ();
    sim_debug (DBG_TRC, &ksdi_dev, "K.sdi %d startup: KH = %06o at %o\n", req, ksdi_dib.cma, (uint16)inst_pc);
    PUTW (ksdi_dib.cma + KG_RSL, 1);                        /* KG$RSL - result of self test */
    PUTW (ksdi_dib.cma + KG_VRS, 01452);                    /* KG$VRS - version (DS = 3, MC = 42) */
    sim_clock_coschedule (&ksdi_unit, tmxr_poll);           /* activate */
    }
else {
    sim_debug (DBG_TRC, &ksdi_dev, "K.sdi %d halt at %o\n", req, (uint16)inst_pc);
    sim_cancel (&ksdi_unit);
    ksdi_dib.cma = 0;
    }
return SCPE_OK;
}

t_stat ksdi_show (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
uint16 kh = ksdi_dib.cma;
uint16 itm;

print_word (st, kh, "KT$BAS", 0000, 0);
print_word (st, kh, "KT$IPQ", 0000, 4);
print_word (st, kh, "KT$WQH", 0004, 2);                 /* work queue head? */
print_word (st, kh, "KT$WQT", 0006, 2);                 /* work queue tail? */
print_word (st, kh, "KT$FLA", 0010, 2);
print_word (st, kh, "KT$SQP", 0012, 2);
print_word (st, kh, "KT$TCT", 0014, 2);
print_word (st, kh, "KT$LAH", 0016, 2);
print_word (st, kh, "KT$WAI", 0020, 2);
print_word (st, kh, "KT$SEM", 0022, 12);
print_word (st, kh, "KT$BW ", 0036, 2);

print_word (st, kh, "KG$TLK", 0040, 2);
print_word (st, kh, "KG$SLT", 0042, 1);                 /* structure type? */
print_word (st, kh, "KG$TYP", 0043, 1);                 /* structure type? */
print_word (st, kh, "KG$VRS", 0044, 2);                 /* version */
print_word (st, kh, "KG$FCB", 0046, 2);                 /* free control memory buffers? */
print_word (st, kh, "KG$FBU", 0050, 2);
print_word (st, kh, "KG$BWS", 0052, 2);
print_word (st, kh, "KG$TMR", 0054, 2);
print_word (st, kh, "KG$DMP", 0056, 2);
print_word (st, kh, "KG$DCA", 0060, 2);
print_word (st, kh, "KG$TST", 0062, 2);
print_word (st, kh, "KG$FRS", 0064, 2);
print_word (st, kh, "KG$LAS", 0066, 2);
print_word (st, kh, "KG$MDL", 0070, 0);
print_word (st, kh, "KG$MTY", 0070, 2);
print_word (st, kh, "KG$PFL", 0072, 2);
print_word (st, kh, "KG$RSL", 0074, 2);                 /* microdiagnostic test result */
print_word (st, kh, "KG$LTR", 0076, 2);
print_word (st, kh, "KG$PSA", 0100, 2);
print_word (st, kh, "KG$PSB", 0102, 2);
print_word (st, kh, "KG$DSA", 0104, 20);
print_word (st, kh, "KG$MC0", 0130, 8);
print_word (st, kh, "KG$KSA", 0140, 0);

print_word (st, kh, "KT$S0 ", 0140, 16);                /* drive state area */
print_word (st, kh, "KT$S1 ", 0160, 16);
print_word (st, kh, "KT$S2 ", 0200, 16);
print_word (st, kh, "KT$S3 ", 0220, 16);
print_word (st, kh, "UNUSED", 0240, 64);
print_word (st, kh, "KT$DMP", 0340, 66);
print_word (st, kh, "KT$AQH", 0442, 2);
print_word (st, kh, "KT$AQT", 0444, 2);
print_word (st, kh, "KT$BCN", 0446, 2);
print_word (st, kh, "KT$TMO", 0450, 2);
print_word (st, kh, "KT$FCN", 0452, 2);
print_word (st, kh, "KT$FCT", 0454, 2);
print_word (st, kh, "KT$T1 ", 0456, 2);
print_word (st, kh, "KT$T2 ", 0460, 2);
print_word (st, kh, "KT$T3 ", 0462, 2);
print_word (st, kh, "KT$T4 ", 0464, 2);

return SCPE_OK;
}

t_bool online = FALSE;

t_stat ksdi_run (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
uint16 kh = ksdi_dib.cma;
uint16 tq;

if (GETW (kh + KT_S0 + 004) == 0) {                     /* no work item? */
    tq = GETP (kh + KT_S0);                             /* get service queue pointer */
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    put_to_queue (tq, kh + KT_S0);                      /* send drive state area to service queue */
    sim_printf ("Drive state area sent to service queue\n");
    }
return SCPE_OK;
}

/* Realtime drive status */

#define RTDS_RWR        0001                            /* read/write ready */
#define RTDS_RR         0002                            /* receiver ready */
#define RTDS_ATN        0010                            /* attention */
#define RTDS_AVL        0020                            /* available */

/* Drive status - Unit mask */

#define UMSK_U1         0020                            /* subunit 1 present */
#define UMSK_U2         0040                            /* subunit 2 present */
#define UMSK_U3         0100                            /* subunit 3 present */
#define UMSK_U4         0200                            /* subunit 4 present */

/* Drive status - Request byte */

#define REQ_RU          0001                            /* run/stop switch in */
#define REQ_PS          0002                            /* port switch in */
#define REQ_EL          0010                            /* send log packet */
#define REQ_SR          0020                            /* spindle at speed */
#define REQ_DR          0040                            /* diagnostic load required */
#define REQ_RR          0100                            /* readjustment required */
#define REQ_OA          0200                            /* online to other port */

/* Drive status - Mode byte */

#define MODE_S7         0001                            /* 0 = 512 bytes/sector, 1 = 576 bytes/sector */
#define MODE_OB         0002                            /* diag cylinder access */
#define MODE_FO         0004                            /* formatting enabled */
#define MODE_DD         0010                            /* drive disabled for diagnostics */
#define MODE_W1         0020                            /* write protect subunit 1 */
#define MODE_W2         0040                            /* write protect subunit 2 */
#define MODE_W3         0100                            /* write protect subunit 3 */
#define MODE_W4         0200                            /* write protect subunit 4 */

/* Drive status - Error byte */

#define ERR_WE          0010                            /* write lock error */
#define ERR_DF          0020                            /* init diag failure */
#define ERR_PE          0040                            /* protocol error */
#define ERR_RE          0100                            /* retryable error */
#define ERR_DE          0200                            /* drive error */

/* Online */

void ksdi_online (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: ONLINE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
online = TRUE;
}

/* Disconnect */

void ksdi_disc (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: DISCONNECT\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
online = FALSE;
}

/* Diagnose */

void ksdi_diag (uint16 cmd, uint16 rsp)
{
uint8 test = GETW (cmd + 002);                          /* drive resident test to run */
sim_printf ("SDI level 2 command: DIAGNOSE, test = %02X\n", test);
PUTW (rsp, 0xFC);                                       /* response opcode */
}

/* Drive clear */

void ksdi_clear (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: DRIVE CLEAR\n");
PUTW (rsp, 0);
online = FALSE;
}

/* Get common characteristics */

void ksdi_getchar (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: GET COMMON CHARACTERISTICS\n");
PUTW (rsp, 0x78);                                       /* response opcode */
}

/* Get status */

void ksdi_getsts (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: GET STATUS\n");
PUTW (rsp + 000, 0xF6);                                 /* response opcode */
PUTW (rsp + 002, 0);                                    /* unused? */
PUTW (rsp + 004, UMSK_U1);                              /* subunit mask */
PUTW (rsp + 006, REQ_SR | REQ_PS | REQ_RU);             /* request byte */
PUTW (rsp + 010, 0);                                    /* mode byte */
PUTW (rsp + 012, 0);                                    /* error byte */
PUTW (rsp + 014, 0);                                    /* controller byte */
PUTW (rsp + 016, 0);                                    /* retry-fail code */
PUTW (rsp + 020, 0);                                    /* extended drive status */
PUTW (rsp + 022, 0);                                    /* extended drive status */
PUTW (rsp + 024, 0);                                    /* extended drive status */
PUTW (rsp + 026, 0);                                    /* extended drive status */
PUTW (rsp + 030, 0);                                    /* extended drive status */
PUTW (rsp + 032, 0);                                    /* extended drive status */
PUTW (rsp + 034, 0);                                    /* extended drive status */
}

/* Read memory */

void ksdi_rdmem (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: READ MEMORY\n");
PUTW (rsp, 0x72);                                       /* response opcode */
}

/* Recalibrate */

void ksdi_recal (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: RECALIBRATE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
}

/* Seek */

void ksdi_seek (uint16 cmd, uint16 rsp)
{
sim_printf ("SDI level 2 command: SEEK\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
}

/* SDI level 2 command */

void ksdi_l2cmd (uint16 cmd, uint16 rsp)
{
uint16 opc = GETB (cmd);                                /* get SDI opcode */
switch (opc) {
    case 0x03:                                          /* DIAGNOSE */
        ksdi_diag (cmd, rsp);
        break;

    case 0x84:                                          /* DISCONNECT */
        ksdi_disc (cmd, rsp);
        break;

    case 0x05:                                          /* DRIVE CLEAR */
        ksdi_clear (cmd, rsp);
        break;

    case 0x87:                                          /* GET COMMON CHARACTERISTICS */
        ksdi_getchar (cmd, rsp);
        break;

    case 0x09:                                          /* GET STATUS */
        ksdi_getsts (cmd, rsp);
        break;

    case 0x0A:                                          /* SEEK */
        ksdi_seek (cmd, rsp);
        break;

    case 0x8B:                                          /* ONLINE drive */
        ksdi_online (cmd, rsp);
        break;

    case 0x8D:                                          /* READ MEMORY */
        ksdi_rdmem (cmd, rsp);
        break;

    case 0x8E:                                          /* RECALIBRATE */
        ksdi_recal (cmd, rsp);
        break;

    default:
        sim_printf ("SDI level 2 command: opc = 0x%02X\n", opc);
        PUTW (rsp, 0x7E);                               /* response opcode */
        break;
        }
}

/* Get realtime drive status */

void ksdi_rtds (uint16 rsp)
{
uint16 sts = 0;
sim_printf ("LINE STATUS\n");
if (online)
    sts = RTDS_RWR;                                     /* READ/WRITE READY */
else
    sts = RTDS_AVL | RTDS_ATN | RTDS_RR;                /* AVAILABLE, RECEIVER READY */
PUTW (rsp, sts);
}

t_stat ksdi_process ()
{
uint16 kh = ksdi_dib.cma;
uint16 tq, rsc, opc, cmd, rsp;

for (int j = 0; j < 20; j++) {
    rsc = GETP (kh + KT_S0 + 004);                      /* get work resource */
    if (rsc == 0)
        return SCPE_OK;

    tq = GETP (rsc + 014);                              /* get service queue */
    if (tq == 0)
        return SCPE_OK;

    if (Q_LOCKED (tq))                                  /* destination queue locked? */
        return SCPE_OK;

    if (GETB (rsc + 006) != 030)                        /* FIXME: valid packet? */
        return SCPE_OK;

    rsc = get_from_queue (kh + KT_S0 + 004);            /* dequeue from drive state area */

    for (int i = 0; i < 17; i++)
        sim_printf ("%06o: %06o\n", rsc + (i << 1), GETW (rsc + (i << 1)));

    rsp = GETP (rsc + 004);                             /* get response */
    opc = GETW (rsc + 010);
    switch (opc & 0177400) {
        case 000400:                                    /* level 2 SDI command */
            cmd = GETP (rsc + 002);                     /* get command */
            ksdi_l2cmd (cmd, rsp);
            break;
        case 014400:                                    /* ECHO DATA */
            sim_printf ("ECHO DATA\n");
            cmd = GETW (rsc + 002);                     /* get immediate */
            PUTW (rsp, cmd);                            /* copy to response */
            break;
        case 010400:                                    /* LINE STATUS */
            ksdi_rtds (rsp);
            break;
        default:
            sim_printf ("Unknown\n");
            PUTW (rsp, 0);                              /* write response word */
            break;
        }
    PUTW (rsc + 006, 0);                                /* FIXME: clear byte count? */
    sim_printf ("----\n");

    put_to_queue (tq, rsc);                             /* send to service queue */
    }
}

t_stat ksdi_svc (UNIT *uptr)
{
uint16 kh = ksdi_dib.cma;
uint16 tq, rsc;

if (GETW (kh + KG_RSL) == 0) {                          /* test started? */
    PUTW (kh + KG_RSL, 1);                              /* KG$RSL - result of self test */
    rsc = get_from_queue (kh + KT_S0 + 004);            /* get DCB from drive service area */
    tq = GETP (rsc + 014);                              /* get destination queue */
    put_to_queue (tq, rsc);                             /* put DCB to destination */
    }
else
    ksdi_process ();
sim_clock_coschedule (uptr, tmxr_poll);                 /* reactivate */
return SCPE_OK;
}

t_stat ksdi_reset (DEVICE *dptr)
{
return SCPE_OK;
}
