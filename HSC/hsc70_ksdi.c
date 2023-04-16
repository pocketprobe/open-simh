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
if (act == K_RUN) {
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
uint32 kh = ksdi_dib.cma;
uint32 itm;

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
uint32 kh = ksdi_dib.cma;
uint32 tq;

if (GETW (kh + KT_S0 + 004) == 0) {                     /* no work item? */
    tq = GETP (kh + KT_S0);                             /* get service queue pointer */
    if (Q_LOCKED (tq)) {
        sim_printf ("Desitnation queue currently locked\n");
        return SCPE_OK;
        }
    PUTW (kh + KT_S0, 0);                               /* clear service queue pointer */
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

uint16 ksdi_online (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: ONLINE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
online = TRUE;
return 002;
}

/* Disconnect */

uint16 ksdi_disc (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: DISCONNECT\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
online = FALSE;
return 002;
}

/* Diagnose */

uint16 ksdi_diag (uint32 cmd, uint32 rsp)
{
uint8 test = GETW (cmd + 002);                          /* drive resident test to run */
sim_printf ("SDI level 2 command: DIAGNOSE, test = %02X\n", test);
PUTW (rsp, 0xFC);                                       /* response opcode */
return 004;
}

/* Drive clear */

uint16 ksdi_clear (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: DRIVE CLEAR\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
online = FALSE;
return 002;
}

/* Get common characteristics */

uint16 ksdi_getchar (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: GET COMMON CHARACTERISTICS\n");
PUTW (rsp + 000, 0x7100 | 0x78);                        /* response opcode */
PUTW (rsp + 002, 0xD400 | 0x33);                        /* short TO / SDI version */
PUTW (rsp + 004, 0xD400 | 0x61);                        /* transfer rate */
PUTW (rsp + 006, 0xD400 | 0x57);                        /* long TO / retry # */
PUTW (rsp + 010, 0xD400 | 4);                           /* RCT copies / reserved */
PUTW (rsp + 012, 0xD400 | 0x00);                        /* error recovery levels */
PUTW (rsp + 014, 0xD400 | 0x02);                        /* ECC error threshold */
PUTW (rsp + 016, 0xD400 | 0x0A);                        /* software revision */
PUTW (rsp + 020, 0xD400 | 0x00);                        /* hardware revision */
PUTW (rsp + 022, 0xD400 | 0x7A);                        /* unique drive ID #1 */
PUTW (rsp + 024, 0xD400 | 0x0A);
PUTW (rsp + 026, 0xD400 | 0x00);                        /* unique drive ID #2 */
PUTW (rsp + 030, 0xD400 | 0x00);
PUTW (rsp + 032, 0xD400 | 0x00);                        /* unique drive ID #3 */
PUTW (rsp + 034, 0xD400 | 0x00);
PUTW (rsp + 036, 0xD400 | 1);                           /* drive type identifier */
PUTW (rsp + 040, 0xD400 | 0x24);                        /* drive revs/s */
PUTW (rsp + 042, 0xD400 | 0x00);
PUTW (rsp + 044, 0xD400 | 0x00);
PUTW (rsp + 046, 0xD400 | 0x00);
PUTW (rsp + 050, 0xD400 | 0x00);
PUTW (rsp + 052, 0xD400 | 0x00);
PUTW (rsp + 054, 0xD400 | 0x00);
PUTW (rsp + 056, 0xB200 | 0x00);
return 030;
}

/* Get subunit characteristics */

uint16 ksdi_getsubchar (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: GET SUBUNIT CHARACTERISTICS\n");
PUTW (rsp + 000, 0x7100 | 0x77);                        /* response opcode */
PUTW (rsp + 002, 0xD400 | 0x13);                        /* low order LBN space in cylinder */
PUTW (rsp + 004, 0xD400 | 0x01);
PUTW (rsp + 006, 0xD400 | 0x00);                        /* high order LBN space in cylinder */
PUTW (rsp + 010, 0xD400 | 0x00);                            /* cyl # */
PUTW (rsp + 012, 0xD400 | 2);                           /* groups/cylinder */
PUTW (rsp + 014, 0xD400 | 0x00);                            /* start LBN / start XBN */
PUTW (rsp + 016, 0xD400 | 14);                          /* tracks/group */
PUTW (rsp + 020, 0xD400 | 0x00);                            /* start RBN / start DBN */
PUTW (rsp + 022, 0xD400 | 1);                           /* RBNs/track */
PUTW (rsp + 024, 0xD400 | 0x00);                            /* reserved */
PUTW (rsp + 026, 0xD400 | 11);                          /* data preamble size */
PUTW (rsp + 030, 0xD400 | 4);                           /* header preamble size */
PUTW (rsp + 032, 0xD400 | 0x50);                        /* media type low */
PUTW (rsp + 034, 0xD400 | 0x10);
PUTW (rsp + 036, 0xD400 | 0x64);                        /* media type high */
PUTW (rsp + 040, 0xD400 | 0x25);
PUTW (rsp + 042, 0xD400 | 0xE0);                        /* FCT copy size in XBNs */
PUTW (rsp + 044, 0xD400 | 0x01);
PUTW (rsp + 046, 0xD400 | 31);                          /* LBNs/track */
PUTW (rsp + 050, 0xD400 | 16);                          /* group offset */
PUTW (rsp + 052, 0xD400 | 0x9C);                        /* LBNs in host area low */
PUTW (rsp + 054, 0xD400 | 0x9E);
PUTW (rsp + 056, 0xD400 | 0x03);                        /* LBNs in host area high */
PUTW (rsp + 060, 0xD400 | 0x00);
PUTW (rsp + 062, 0xD400 | 0xD1);                        /* RCT size in LBNs  */
PUTW (rsp + 064, 0xD400 | 0x01);
PUTW (rsp + 066, 0xD400 | 0x00);
PUTW (rsp + 070, 0xD400 | 0x00);
PUTW (rsp + 072, 0xD400 | 0x00);
PUTW (rsp + 074, 0xD400 | 0x00);
PUTW (rsp + 076, 0xD400 | 0x00);
PUTW (rsp + 100, 0xD400 | 0x00);
PUTW (rsp + 102, 0xD400 | 0x00);
PUTW (rsp + 104, 0xD400 | 0x00);
PUTW (rsp + 106, 0xD400 | 0x00);
PUTW (rsp + 110, 0xD400 | 0x00);
PUTW (rsp + 112, 0xD400 | 0x00);
PUTW (rsp + 114, 0xD400 | 0x00);
PUTW (rsp + 116, 0xB200 | 0x00);
return 050;
}

/* Get status */

uint16 ksdi_getsts (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: GET STATUS\n");
PUTW (rsp + 000, 0x7100 | 0xF6);                        /* response opcode */
PUTW (rsp + 002, 0xD400 | 0x00);                        /* unit number (low) */
PUTW (rsp + 004, 0xD400 | UMSK_U1);                     /* unit number (high) / subunit mask */
PUTW (rsp + 006, 0xD400 | REQ_SR | REQ_PS | REQ_RU);    /* request byte */
PUTW (rsp + 010, 0xD400 | MODE_OB | MODE_FO);           /* mode byte */
PUTW (rsp + 012, 0xD400 | 0x00);                        /* error byte */
PUTW (rsp + 014, 0xD400 | 0x00);                        /* controller byte */
PUTW (rsp + 016, 0xD400 | 0x00);                        /* retry-fail code */
PUTW (rsp + 020, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 022, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 024, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 026, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 030, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 032, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 034, 0xD400 | 0x00);                        /* extended drive status */
PUTW (rsp + 036, 0xB200 | 0x00);                        /* end of frame */
return 020;
}

/* Read memory */

uint16 ksdi_rdmem (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: READ MEMORY\n");
PUTW (rsp, 0x72);                                       /* response opcode */
return 030;
}

/* Recalibrate */

uint16 ksdi_recal (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: RECALIBRATE\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* Seek */

uint16 ksdi_seek (uint32 cmd, uint32 rsp)
{
sim_printf ("SDI level 2 command: SEEK\n");
PUTW (rsp, 0x7E);                                       /* response opcode - COMPLETED */
return 002;
}

/* SDI level 2 command */

uint16 ksdi_l2cmd (uint32 cmd, uint32 rsp)
{
uint8 opc = GETB (cmd);                                 /* get SDI opcode */
uint16 rsz = 0;

switch (opc) {
    case 0x03:                                          /* DIAGNOSE */
        rsz = ksdi_diag (cmd, rsp);
        break;

    case 0x84:                                          /* DISCONNECT */
        rsz = ksdi_disc (cmd, rsp);
        break;

    case 0x05:                                          /* DRIVE CLEAR */
        rsz = ksdi_clear (cmd, rsp);
        break;

    case 0x87:                                          /* GET COMMON CHARACTERISTICS */
        rsz = ksdi_getchar (cmd, rsp);
        break;

    case 0x88:                                          /* GET SUBUNIT CHARACTERISTICS */
        rsz = ksdi_getsubchar (cmd, rsp);
        break;

    case 0x09:                                          /* GET STATUS */
        rsz = ksdi_getsts (cmd, rsp);
        break;

    case 0x0A:                                          /* SEEK */
        rsz = ksdi_seek (cmd, rsp);
        break;

    case 0x8B:                                          /* ONLINE drive */
        rsz = ksdi_online (cmd, rsp);
        break;

    case 0x8D:                                          /* READ MEMORY */
        rsz = ksdi_rdmem (cmd, rsp);
        break;

    case 0x8E:                                          /* RECALIBRATE */
        rsz = ksdi_recal (cmd, rsp);
        break;

    default:
        sim_printf ("SDI level 2 command: opc = 0x%02X\n", opc);
        PUTW (rsp, 0x7E);                               /* response opcode */
        rsz = 2;
        break;
        }
return rsz;
}

/* Get realtime drive status */

void ksdi_rtds (uint32 rsp)
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
uint32 kh = ksdi_dib.cma;
uint32 sq, eq, tq, rsc, opc, cmd, rsp, rsz, tmp;

for (int j = 0; j < 20; j++) {
    rsc = GETP (kh + KT_IPQ);                           /* get work resource */
    if (rsc == 0)
        break;

    tq = GETP (rsc + 014);                              /* get ?? queue */
    if (tq == 0)
        break;

    if (Q_LOCKED (tq))                                  /* ?? queue locked? */
        break;

    rsc = get_from_queue (kh + KT_IPQ);                 /* dequeue */
    sim_printf ("IPQ work item processed\n");
    put_to_queue (tq, rsc);                             /* send to ?? queue */
    }

for (int j = 0; j < 20; j++) {
    rsc = GETP (kh + KT_S0 + 004);                      /* get work resource */
    if (rsc == 0)
        break;

    sq = GETP (rsc + 014);                              /* get success queue */
    if (sq == 0)
        break;

    if (Q_LOCKED (sq))                                  /* success queue locked? */
        break;

    eq = GETP (rsc + 016);                              /* get error queue */
    if (eq == 0)
        break;

    if (Q_LOCKED (eq))                                  /* error queue locked? */
        break;

    rsc = get_from_queue (kh + KT_S0 + 004);            /* dequeue from drive state area */

    for (int i = 0; i < 17; i++)
        sim_printf ("%06o: %06o\n", rsc + (i << 1), GETW (rsc + (i << 1)));

    rsp = GETP (rsc + 004);                             /* get response */
    opc = GETW (rsc + 010);
    rsz = 0;
    tq = sq;                                            /* target is success queue */
    switch (opc & 0177400) {
        case 0100400:
        case 004400:
        case 000400:                                    /* level 2 SDI command */
            cmd = GETP (rsc + 002);                     /* get command */
            rsz = ksdi_l2cmd (cmd, rsp);
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
    tmp = GETW (rsc + 006);
    tmp = tmp - rsz;
    PUTW (rsc + 006, tmp);                              /* set response length */
    sim_printf ("----\n");

    put_to_queue (tq, rsc);                             /* send to service queue */
    }
return SCPE_OK;
}

t_stat ksdi_svc (UNIT *uptr)
{
uint32 kh = ksdi_dib.cma;
uint32 tq, rsc;

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
