/* kl10_ci.c: CI 20 Network interface.

   Copyright (c) 2024, Matt Burke
   This module is based on the NIA 20, Copyright (c) 2020, Richard Cornwell.

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
   THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include "kx10_defs.h"
#include "dec_ci.h"

#if NUM_DEVS_CI > 0
#define CI_DEVNUM  (0540 + (7 * 4))

/* CSR - CONI */

#define CI_PPT     0400000000000LL                      /* Port present */
#define CI_DCC     0100000000000LL                      /* Diag CSR */
#define CI_CPE     0004000000000LL                      /* CRAM Parity error */
#define CI_MBE     0002000000000LL                      /* MBUS error */
#define CI_ILD     0000100000000LL                      /* Idle */
#define CI_DCP     0000040000000LL                      /* Disable complete */
#define CI_ECP     0000020000000LL                      /* Enable complete */
#define CI_PID     0000007000000LL                      /* Port ID */

/* CSR - CONO/CONI */

#define CI_CPT     0000000400000LL                      /* Clear Port */
#define CI_SEB     0000000200000LL                      /* Diag Select EBUF */
#define CI_GEB     0000000100000LL                      /* Diag Gen Ebus PE */
#define CI_LAR     0000000040000LL                      /* Diag select LAR */
#define CI_SSC     0000000020000LL                      /* Diag Single Cycle */
#define CI_EPE     0000000004000LL                      /* Ebus parity error */
#define CI_FQE     0000000002000LL                      /* Free Queue Error */
#define CI_DME     0000000001000LL                      /* Data mover error */
#define CI_CQA     0000000000400LL                      /* Command Queue Available */
#define CI_RQA     0000000000200LL                      /* Response Queue Available */
#define CI_DIS     0000000000040LL                      /* Disable */
#define CI_ENB     0000000000020LL                      /* Enable */
#define CI_MRN     0000000000010LL                      /* RUN */
#define CI_PIA     0000000000007LL                      /* PIA */

#define CI_LRA     0400000000000LL                      /* Load Ram address */
#define CI_RAR     0377760000000LL                      /* Microcode address mask */
#define CI_MSB     0000020000000LL                      /* Half word select */

/* PCB Offsets */

#define PCB_BDT     000                        /* Buffer descriptor table start address */
#define PCB_MQE     001                        /* Message free queue entry length */
#define PCB_DQE     002                        /* Datagram free queue entry length */
#define PCB_RS0     003                        /* Reserved */
#define PCB_C3I     004                        /* Command queue 3 interlock */
#define PCB_C3F     005                        /* Command queue 3 flink */
#define PCB_C3B     006                        /* Command queue 3 blink */
#define PCB_C2I     007                        /* Command queue 2 interlock */
#define PCB_C2F     010                        /* Command queue 2 flink */
#define PCB_C2B     011                        /* Command queue 2 blink */
#define PCB_C1I     012                        /* Command queue 1 interlock */
#define PCB_C1F     013                        /* Command queue 1 flink */
#define PCB_C1B     014                        /* Command queue 1 blink */
#define PCB_C0I     015                        /* Command queue 0 interlock */
#define PCB_C0F     016                        /* Command queue 0 flink */
#define PCB_C0B     017                        /* Command queue 0 blink */
#define PCB_RQI     020                        /* Response queue interlock */
#define PCB_RQF     021                        /* Response queue flink */
#define PCB_RQB     022                        /* Response queue blink */
#define PCB_MQI     023                        /* Message free queue interlock */
#define PCB_MQF     024                        /* Message free queue flink */
#define PCB_MQB     025                        /* Message free queue blink */
#define PCB_DQI     026                        /* Datagram free queue interlock */
#define PCB_DQF     027                        /* Datagram free queue flink */
#define PCB_DQB     030                        /* Datagram free queue blink */
#define PCB_RS1     031                        /* Reserved */
#define PCB_RS2     032                        /* Reserved */
#define PCB_RS3     033                        /* Reserved */
#define PCB_RS4     034                        /* Reserved */
#define PCB_PE0     035                        /* Port error word 0 */
#define PCB_PE1     036                        /* Port error word 1 */
#define PCB_PE2     037                        /* Port error word 2 */
#define PCB_PE3     040                        /* Port error word 3 */
#define PCB_PE4     041                        /* Port error word 4 */
#define PCB_PCB     042                        /* PCB base address */
#define PCB_PIL     043                        /* PI level */
#define PCB_CL1     044                        /* Channel logout word 1 address */
#define PCB_CCW     045                        /* Channel command word */
#define PCB_RS5     046                        /* Reserved to port */

#define CHNERR      07762
#define SLFTST      07751
#define INTERR      07750
#define EBSERR      07752

/* Virtual Circuit Flags */

#define VC_LDST     0400000000000LL                     /* load circuit state */
#define VC_CST      0200000000000LL                     /* circuit state */
#define VC_NR       0100000000000LL                     /* receiving sequence number */
#define VC_NS       0040000000000LL                     /* sending sequence number */
#define VC_LDPS     0020000000000LL                     /* load path select */
#define VC_PS       0014000000000LL                     /* path select */

/* Diagnostic Registers */

#define DR_RXS          001                             /* read receiver status */
#define DR_RSX          002                             /* reset trasmiter status */
#define DR_RB           003                             /* read buffer */
#define DR_ENL          004                             /* enable link */
#define DR_RS           005                             /* read switches */
#define DR_SB           006                             /* select buffer */
#define DR_LXB          007                             /* load transmit buffer */
#define DR_TXS          010                             /* read trasmiter status */
#define DR_ABX          011                             /* abort tranmitter */
#define DR_SM           012                             /* set mode */
#define DR_TX           013                             /* transmit */
#define DR_NA           014                             /* read node address */
#define DR_DL           015                             /* disable link */
#define DR_RRB          016                             /* release receiver buffer */
#define DR_SY           017                             /* sync */

/* PPD Types */

#define PPD_DG          0                               /* datagram */
#define PPD_MSG         1                               /* message */

/* PPD Offsets */

#define PPD_FLINK       0x00                            /* forward link */
#define PPD_BLINK       0x04                            /* backward link */
#define PPD_RSVD        0x08                            /* reserved for software */
#define PPD_PORT        0x0C                            /* src/dest port */
#define PPD_OPC         0x0D                            /* opcode */
#define PPD_FLAGS       0x0E                            /* flags */
#define PPD_STATUS      0x0F                            /* status */

#define PPD_TEXT        0x10                            /* datagram/message text */

#define PPD_XCTID       0x10                            /* transaction ID */

#define PPD_WRDAT       0x10                            /* write register data */
#define PPD_REG         0x11                            /* register number */
#define PPD_RDDAT       0x13                            /* read register data */

/* PPD Lengths */

#define LEN_HDR         0x10                            /* PPD header size */
#define LEN_SNDDG       (LEN_HDR)
#define LEN_SNDMSG      (LEN_HDR)
#define LEN_REQID       (LEN_HDR + 8)
#define LEN_SNDRST      (LEN_HDR + 8)
#define LEN_SNDSTRT     (LEN_HDR + 12)
#define LEN_REQDAT      (LEN_HDR + 28)
#define LEN_SNDLB       (LEN_HDR)
//#define LEN_SNDDAT      0x2C
#define LEN_SETCKT      (LEN_HDR + 12)
#define LEN_CKTSET      (LEN_HDR + 16)
#define LEN_SETCNT      (LEN_HDR + 16)
#define LEN_RDCNT       (LEN_HDR + 8)
#define LEN_RDREG       (LEN_HDR + 4)
#define LEN_WRREG       (LEN_HDR + 4)
#define LEN_CLSBUF      (LEN_HDR + 4)

/* Opcodes */

#define OPC_SNDDG       001
#define OPC_SNDMSG      002
#define OPC_CNFRET      003
#define OPC_REQID       005
#define OPC_SNDRST      006
#define OPC_SNDSTRT     007
#define OPC_REQDAT0     010
#define OPC_REQDAT1     011
#define OPC_REQDAT2     012
#define OPC_RETID       013
#define OPC_SNDLB       015
//#define OPC_RETDAT      021
//#define OPC_SNTMDAT     022
//#define OPC_MCNF        044

#define OPC_M_RECV      040

#define OPC_DGREC       (OPC_SNDDG | OPC_M_RECV)
#define OPC_MSGREC      (OPC_SNDMSG | OPC_M_RECV)
#define OPC_CNFREC      (OPC_CNFREC | OPC_M_RECV)
#define OPC_RSTREC      (OPC_SNDRST | OPC_M_RECV)
#define OPC_STRTREC     (OPC_SNDSTRT | OPC_M_RECV)
#define OPC_IDREC       (OPC_RETID | OPC_M_RECV)
#define OPC_LBREC       (OPC_SNDLB | OPC_M_RECV)
#define OPC_DATREC      (OPC_RETDAT | OPC_M_RECV)

#define OPC_SETCKT      0200                            /* set circuit */
#define OPC_SETCNT      0201                            /* set counters */
#define OPC_RDCNT       0202                            /* read counters */
#define OPC_RDREG       0203                            /* read register */
#define OPC_WRREG       0204                            /* write register */
#define OPC_CLSBUF      0205                            /* close buffer */

/* PPD Flags */

#define PPD_RSP         1                               /* respond to command */
#define PPD_V_PS        1                               /* path select */
#define PPD_M_PS        0x3
#define  PPD_PSAUTO     0                               /* automatic */
#define  PPD_PS0        1                               /* use path 0 */
#define  PPD_PS1        2                               /* use path 1 */
#define GET_PATH(x)     ((x >> PPD_V_PS) & PPD_M_PS)
#define PPD_V_SP        4                               /* send path */
#define PPD_M_SP        0x3
#define PPD_V_M         4                               /* multiple value, blk xfer only */
#define PPD_M_M         0x7
#define PPD_LP          0x8                             /* last packet (blk xfer) */
#define PPD_P           0x80                            /* 0/1 for 512/576 data pkt size */
                                                        /*  or packing format for messages */
                                                        /*  to PDP 10/20 ports. */
/* Counters */

#define CI_CNT_UV    000                                /* Microcode version */
#define CI_CNT_AKA   001                                /* Path A ACK count */
#define CI_CNT_NKA   002                                /* Path A NAK count */
#define CI_CNT_NRA   003                                /* Path A no response count */
#define CI_CNT_AKB   004                                /* Path B ACK count */
#define CI_CNT_NKB   005                                /* Path B NAK count */
#define CI_CNT_NRB   006                                /* Path B no response count */
#define CI_CNT_DGD   007                                /* Datagrams discarded */
#define CI_CNT_PTX   010                                /* Packets transmitted */
#define CI_CNT_PRX   011                                /* Packets received */

typedef struct {
    uint32 addr;                                        /* associated memory address */
    size_t length;                                      /* packet length */
    uint8 data[CI_MAXFR];                               /* packet data */
} PPD_PKT;

struct ci_device {
    t_addr            cmd_entry;                        /* Pointer to current command entry */
    t_addr            cmd_rply;                         /* Pointer to reply entry */
    uint8             cmd_status;                       /* Status field of current command */
    t_addr            rec_entry;                        /* Pointer to current recieve entry */
    t_addr            pcb;                              /* address of PCB */
    int               pia;                              /* Interrupt channel */
    int               rar;
    uint64            ebuf;
    uint64            csr;                              /* control/status register */
    uint32            uver[4];                          /* Version information */
    int               r_pkt;                            /* Packet pending */
    int               poll;                             /* Need to poll receiver */
    uint64            vcd[CI_MAX_NODES];                /* virtual circuit descriptors */
} ci_data;


t_stat     ci_devio (uint32 dev, uint64 *data);
void       ci_start ();
void       ci_stop ();
void       ci_enable (UNIT *uptr);
void       ci_disable (UNIT *uptr);
void       ci_load_ptt ();
void       ci_load_mcast ();
void       ci_error (int);
t_stat     ci_port_command (UNIT *uptr);
t_stat     ci_svc (UNIT *uptr);
t_stat     ci_reset (DEVICE *dptr);
t_stat     ci_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, CONST char *cptr);
const char *ci_description (DEVICE *dptr);

struct rh_if   ci_rh = { NULL, NULL, NULL };

UNIT ci_unit = { UDATA (&ci_svc, UNIT_IDLE|UNIT_ATTABLE, 0) };

REG ci_reg[] = {
    { SAVEDATA(DATA, ci_data) },
    { 0 }
};

DIB ci_dib = { CI_DEVNUM | RH20_DEV, 1, &ci_devio, NULL, &ci_rh };

MTAB ci_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "NODE", "NODE",
      &ci_set_node, &ci_show_node },
    { MTAB_XTD|MTAB_VDV, 0, "GROUP", "GROUP",
      &ci_set_group, &ci_show_group },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "LINKS", NULL,
      NULL, &ci_show_links, NULL },
    { 0 }
    };

/* Simulator debug controls */
DEBTAB ci_debug[] = {
    { "CMD", DEBUG_CMD, "Show command execution to devices" },
    { "DATA", DEBUG_DATA, "Show data transfers" },
    { "DETAIL", DEBUG_DETAIL, "Show details about device" },
    { "EXP", DEBUG_EXP, "Show exception information" },
    { "CONI", DEBUG_CONI, "Show coni instructions" },
    { "CONO", DEBUG_CONO, "Show cono instructions" },
    { "DATAIO", DEBUG_DATAIO, "Show datai and datao instructions" },
    { "IRQ", DEBUG_IRQ, "Show IRQ requests" },
    { "DIAG",  DBG_DIAG, "Show diagnostic packets" },
    { "CONF",  DBG_CONF, "Show configuration packets" },
    { "PPD",   DBG_PPD, "Show port to port datagrams" },
    { "SCS",   DBG_SCS, "Show SCS messages" },
    { "BLK",   DBG_BLK, "Show block transfers" },
    { "LCMD",  DBG_LCMD, "Show local commands" },
    { "CONN",  DBG_CONN, "Show connection activity" },
    { "PKT",   DBG_PKT, "Dump packet data" },
    { 0, 0 }
    };

DEVICE ci_dev = {
    "CI", &ci_unit, ci_reg, ci_mod,
    1, DEV_RDX, 0, 1, DEV_RDX, 36,
    NULL, NULL, &ci_reset, NULL, &ci_attach, &ci_detach,
    &ci_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG | DEV_CI, 0, ci_debug,
    NULL, NULL, &ci_help, NULL, NULL, &ci_description
    };

t_stat ci_devio (uint32 dev, uint64 *data)
{
DEVICE *dptr = &ci_dev;
UNIT   *uptr = &ci_unit;

switch (dev & 03) {
    case CONO:
        if (*data & CI_CPT)                             /* clear port? */
            ci_reset (dptr);                            /* yes, reset */

        ci_data.csr &= ~(CI_SEB|CI_LAR|CI_SSC|CI_DIS|CI_ENB|CI_PIA);
        ci_data.csr |= *data & (CI_SEB|CI_LAR|CI_SSC|CI_DIS|CI_ENB|CI_PIA);
        ci_data.csr &= ~(*data & (CI_EPE|CI_FQE|CI_DME|CI_RQA));
        clr_interrupt (CI_DEVNUM);
        if (*data & CI_MRN) {                           /* microcode run */
           if ((ci_data.csr & CI_MRN) == 0)             /* currently halted? */
              ci_start ();                              /* start port */
        } else {
           if ((ci_data.csr & CI_MRN) != 0)             /* currently running? */
              ci_stop ();                               /* stop port */
        }
        if (*data & CI_ENB) {                           /* enable set? */
           if ((ci_data.csr & CI_MRN) != 0 &&           /* microcode running, not enabled? */
                   (ci_data.csr & CI_ECP) == 0)
               ci_enable (uptr);                        /* enable port */
           else
               ci_data.csr |= CI_ECP;                   /* enabled complete */
        } else                                          /* enable clear */
           ci_data.csr &= ~CI_ECP;                      /* enable not complete */
        if (*data & CI_DIS) {                           /* disable set? */
           if ((ci_data.csr & CI_MRN) != 0 &&           /* microcode running, not disabled? */
                   (ci_data.csr & CI_DCP) == 0)
               ci_disable (uptr);                       /* disable port */
           else
               ci_data.csr |= CI_DCP;                   /* disable complete */
        } else                                          /* disable clear */
           ci_data.csr &= ~CI_DCP;                      /* disable not compelte */
        if (*data & CI_CQA &&                           /* command available, microcode running? */
            (ci_data.csr & CI_CQA) == 0 &&
            (ci_data.csr & CI_MRN) != 0) {
            sim_debug (DEBUG_DETAIL, &ci_dev, "CI CQA\n");
           ci_data.csr |= CI_CQA;
            ci_port_command (uptr);
//           sim_activate (uptr, 200);                    /* process command queue */
        }
        if (ci_data.csr & (CI_CPE|CI_RQA))
            set_interrupt (CI_DEVNUM, ci_data.csr & CI_PIA);
        sim_debug (DEBUG_CONO, dptr, "CI %03o CONO %06o PC=%06o %012llo\n", dev,
                 (uint32)(*data & RMASK), PC, ci_data.csr);
        break;

    case CONI:
        *data = ci_data.csr|CI_PPT|CI_PID;
        sim_debug (DEBUG_CONI, dptr, "CI %03o CONI %012llo PC=%o\n", dev,
                           *data, PC);
        break;

    case DATAO:
        if (ci_data.csr & CI_SEB) {
            ci_data.ebuf = *data;
        } else {
            if (*data & CI_LRA) {
                ci_data.rar = (uint32)((*data & CI_RAR) >> 22);
//                sim_debug (DEBUG_DETAIL, dptr, "CI %03o set RAR=%o\n",
//                 dev, ci_data.rar);
            } else {
                if (ci_data.rar >= 0274 && ci_data.rar <= 0277)
                   ci_data.uver[ci_data.rar - 0274] = (uint32)(*data & RMASK);
//                sim_debug (DEBUG_DETAIL, dptr, "CI %03o set data=%o %06o\n",
//                 dev, ci_data.rar, (uint32)(*data & RMASK));
            }
        }
//        sim_debug (DEBUG_DATAIO, dptr, "CI %03o DATO %012llo PC=%o\n",
//                 dev, *data, PC);
        break;

    case DATAI:
        if (ci_data.csr & CI_SEB) {
            *data = ci_data.ebuf;
        } else {
            if (ci_data.csr & CI_LAR) {
                *data = ((uint64)ci_data.rar) << 20;
                *data &= ~CI_MSB;
                *data |= CI_LRA;
            } else if (ci_data.rar >= 0274 && ci_data.rar <= 0277) {
                   *data = (uint64)ci_data.uver[ci_data.rar - 0274];
            }
        }
//        sim_debug (DEBUG_DATAIO, dptr, "CI %03o DATI %012llo PC=%o\n",
//                 dev, *data, PC);
        break;
    }

return SCPE_OK;
}

/*
 * Set error code and stop.
 */
void ci_error (int err)
{
    ci_data.rar = err;
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI error %03o\n", err);
    ci_data.csr |= CI_CPE;
    set_interrupt (CI_DEVNUM, ci_data.csr & CI_PIA);
    ci_stop ();
}

/*
 * Start CI device, load in 2 words using RH20 mode.
 */
void ci_start ()
{
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI start\n");
    ci_rh.stcr = BIT7;                                  /* set up RH20 to read 2 words */
    ci_rh.imode = 2;
    rh20_setup (&ci_rh);
    if (!rh_read (&ci_rh)) {                            /* read PCB address */
          ci_error (CHNERR);
          return;
    }
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI PCB %012llo %o\n", ci_rh.buf,
                                                              ci_rh.wcr);
    ci_data.pcb = (t_addr)(ci_rh.buf & AMASK);

    if (!rh_read (&ci_rh)) {                            /* read PIA value */
          ci_error (CHNERR);
          return;
    }
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI PIA %012llo %o\n", ci_rh.buf,
                                                              ci_rh.wcr);
    ci_data.pia = (int)(ci_rh.buf & 7);
    ci_data.csr |= CI_MRN;                              /* now running */
}

void ci_stop ()
{
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI stop\n");
    ci_data.csr &= ~CI_MRN;
}

/*
 * Enable CI 20.
 *
 * Read in PTT and MACS table.
 */
void ci_enable (UNIT *uptr)
{
    uint64   word;
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI enable\n");
    ci_rx_enable (uptr, (CI_PATH_A | CI_PATH_B));       /* enable receiver */
    ci_data.csr |= CI_ECP;
    ci_data.csr &= ~CI_DCP;
}

/*
 * Disable CI 20.
 */
void ci_disable (UNIT *uptr)
{
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI disable\n");
    ci_rx_enable (uptr, 0);                             /* disable receiver */
    ci_data.csr |= CI_DCP;
    ci_data.csr &= ~CI_ECP;
}

/*
 * Copy memory to a packet - Industry standard format
 */
uint8 *ci_cpy_to_ind (t_addr addr, uint8 *data, int off, int len)
{
    uint64 word;

    addr = addr + (off >> 2);                           /* apply offset */
    word = M[addr++];                                   /* copy first partial word */
    switch (off & 0x3) {
        case 3:
            *data++ = (uint8)((word >> 28) & 0xff);
            break;
        case 2:
            *data++ = (uint8)((word >> 28) & 0xff);
            *data++ = (uint8)((word >> 20) & 0xff);
            break;
        case 1:
            *data++ = (uint8)((word >> 28) & 0xff);
            *data++ = (uint8)((word >> 20) & 0xff);
            *data++ = (uint8)((word >> 12) & 0xff);
            break;
        default:
            break;
        }
    len -= (off & 0x3);

    while (len > 3) {                                   /* copy full words */
        word = M[addr++];
        *data++ = (uint8)((word >> 28) & 0xff);
        *data++ = (uint8)((word >> 20) & 0xff);
        *data++ = (uint8)((word >> 12) & 0xff);
        *data++ = (uint8)((word >> 4) & 0xff);
         len -= 4;
    }

    if (len) {                                          /* copy last partial word */
        word = M[addr++];
        switch (len) {
        case 3:
            *data++ = (uint8)((word >> 28) & 0xff);
            *data++ = (uint8)((word >> 20) & 0xff);
            *data++ = (uint8)((word >> 12) & 0xff);
            break;
        case 2:
            *data++ = (uint8)((word >> 28) & 0xff);
            *data++ = (uint8)((word >> 20) & 0xff);
            break;
        case 1:
            *data++ = (uint8)((word >> 28) & 0xff);
            break;
        }
    }
    return data;
}

/*
 * Copy memory to a packet - Header
 */
uint8 *ci_cpy_to (t_addr addr, uint8 *data, int len)
{
    uint64    word;

    /* Copy full words */
    while (len > 3) {
        word = M[addr++];
        *data++ = (uint8)((word >> 4) & 0xff);
        *data++ = (uint8)((word >> 12) & 0xff);
        *data++ = (uint8)((word >> 20) & 0xff);
        *data++ = (uint8)((word >> 28) & 0xff);
         len -= 4;
    }
    /* Grab last partial word */
    if (len) {
        word = M[addr++];
        switch (len) {
        case 3:
            *data++ = (uint8)((word >> 12) & 0xff);
            *data++ = (uint8)((word >> 20) & 0xff);
            *data++ = (uint8)((word >> 28) & 0xff);
            break;
        case 2:
            *data++ = (uint8)((word >> 20) & 0xff);
            *data++ = (uint8)((word >> 28) & 0xff);
            break;
        case 1:
            *data++ = (uint8)((word >> 28) & 0xff);
            break;
        }
    }
    return data;
}

/*
 * Copy a packet to memory - Industry standard format
 */
uint8 *ci_cpy_from_ind (t_addr addr, uint8 *data, int off, int len)
{
    uint64    word;

    addr = addr + (off >> 2);                           /* apply offset */
    word = M[addr];                                     /* copy first partial word */
    switch (off & 0x3) {
        case 3:
            word = word & ~((uint64)0xFF << 28);
            word |= (uint64)(*data++) << 28;
            break;
        case 2:
            word = word & ~((uint64)0xFFFF << 20);
            word |= (uint64)(*data++) << 28;
            word |= (uint64)(*data++) << 20;
            break;
        case 1:
            word = word & ~((uint64)0xFFFFFF << 12);
            word |= (uint64)(*data++) << 28;
            word |= (uint64)(*data++) << 20;
            word |= (uint64)(*data++) << 12;
            break;
        default:
            break;
        }
    M[addr++] = word;
    len -= (off & 0x3);

    while (len > 3) {                                   /* copy full words */
        word =  (uint64)(*data++) << 28;
        word |= (uint64)(*data++) << 20;
        word |= (uint64)(*data++) << 12;
        word |= (uint64)(*data++) << 4;
        M[addr++] = word;
        len -= 4;
    }

    if (len) {                                          /* copy last partial word */
        word = M[addr];
        switch (len) {
        case 3:
            word = word & ~((uint64)0xFFFFFF << 12);
            word |= (uint64)(*data++) << 28;
            word |= (uint64)(*data++) << 20;
            word |= (uint64)(*data++) << 12;
            break;
        case 2:
            word = word & ~((uint64)0xFFFF << 20);
            word |= (uint64)(*data++) << 28;
            word |= (uint64)(*data++) << 20;
            break;
        case 1:
            word = word & ~((uint64)0xFF << 28);
            word |= (uint64)(*data++) << 28;
            break;
        default:
            word = 0;
            break;
        }
        M[addr++] = word;
    }
    return data;
}

/*
 * Copy a packet to memory - Header
 */
uint8 *ci_cpy_from (t_addr addr, uint8 *data, int len)
{
    uint64    word;

    /* Copy full words */
    while (len > 3) {
        word =  (uint64)(*data++) << 4;
        word |= (uint64)(*data++) << 12;
        word |= (uint64)(*data++) << 20;
        word |= (uint64)(*data++) << 28;
        M[addr++] = word;
        len -= 4;
    }

    if (len) {                                          /* copy last partial word */
        word = M[addr];
        switch (len) {
        case 3:
            word = word & ~((uint64)0xFFFFFF << 12);
            word |= (uint64)(*data++) << 12;
            word |= (uint64)(*data++) << 20;
            word |= (uint64)(*data++) << 28;
            break;
        case 2:
            word = word & ~((uint64)0xFFFF << 20);
            word |= (uint64)(*data++) << 20;
            word |= (uint64)(*data++) << 28;
            break;
        case 1:
            word = word & ~((uint64)0xFF << 28);
            word |= (uint64)(*data++) << 28;
            break;
        default:
            word = 0;
            break;
        }
        M[addr++] = word;
    }
    return data;
}

/*
 * Get next entry off a queue.
 * Return entry in *entry and 1 if successfull.
 * Returns 0 if fail, queue locked or memory out of bounds.
 */
int ci_getq (t_addr head, t_addr *entry)
{
    uint64    temp;
    t_addr    flink;
    t_addr    nlink;
    *entry = 0;  /* For safety */

    /* Try and get lock */
    if (Mem_read_word (head, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    /* Check if entry locked */
    if ((temp & SMASK) == 0)
        return 0;

    /* Increment lock here */

    /* Get head of queue */
    if (Mem_read_word (head + 1, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    flink = (t_addr)(temp & AMASK);
    /* Check if queue empty */
    if (flink == (head+1)) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "CI queue empty %08o\n", head);
        /* Decrement lock here */
        return 1;
    }
#if 0
    temp = M[head + 1];
    sim_debug (DEBUG_DETAIL, &ci_dev, "Queue prior to ci_getq()\n");
    for (int i = 0; i < 20; i++) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n", temp, M[temp]);
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n\n", temp + 1, M[temp + 1]);
        temp = M[temp];
        if (temp == (head + 1))
            break;
        }
#endif
    /* Get link to next entry */
    if (Mem_read_word (flink, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    nlink = (t_addr)(temp & AMASK);
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI head: q=%08o f=%08o n=%08o\n",
             head, flink, nlink);
    /* Set Head Flink to point to next */
    temp = (uint64)nlink;
    if (Mem_write_word(head+1, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    /* Set Next Blink to head */
    temp = (uint64)(head + 1);
    if (Mem_write_word(nlink+1, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    /* Return entry */
    *entry = flink;
#if 0
    temp = M[head + 1];
    sim_debug (DEBUG_DETAIL, &ci_dev, "Queue after ci_getq()\n");
    for (int i = 0; i < 20; i++) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n", temp, M[temp]);
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n\n", temp + 1, M[temp + 1]);
        temp = M[temp];
        if (temp == (head + 1))
            break;
        }
#endif
    /* Decrement lock here */
    return 1;
}

/*
 * Put entry on head of queue.
 * Return entry in *entry and 1 if successfull.
 * Returns 0 if fail, queue locked or memory out of bounds.
 */
int ci_putq (t_addr head, t_addr *entry)
{
    uint64    temp;
    t_addr    blink;

    /* Try and get lock */
    if (Mem_read_word (head, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    /* Check if entry locked */
    if ((temp & SMASK) == 0)
        return 0;

    /* Increment lock here */
#if 0
    temp = M[head + 1];
    sim_debug (DEBUG_DETAIL, &ci_dev, "Queue prior to ci_putq()\n");
    for (int i = 0; i < 20; i++) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n", temp, M[temp]);
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n\n", temp + 1, M[temp + 1]);
        temp = M[temp];
        if (temp == (head + 1))
            break;
        }
#endif
    /* Hook entry into tail of queue */
    if (Mem_read_word (head + 2, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }

    blink = (t_addr)(temp & AMASK);  /* Get back link */
    /* Get link to previous entry */
    temp = (uint64)*entry;
    if (Mem_write_word(blink, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    /* Old forward is new */
    if (Mem_write_word(head+2, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }

    /* Flink is head of queue */
    temp = (uint64)(head+1);
    if (Mem_write_word(*entry, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }

    /* Back link points to previous */
    temp = (uint64)blink;
    if (Mem_write_word(*entry+1, &temp, 0)) {
        ci_error (EBSERR);
        return 0;
    }
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI put: q=%08o i=%08o b=%08o\n",
                head, *entry, blink);
    *entry = 0;
    /* Decement lock here */
#if 0
    temp = M[head + 1];
    sim_debug (DEBUG_DETAIL, &ci_dev, "Queue after ci_putq()\n");
    for (int i = 0; i < 20; i++) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n", temp, M[temp]);
        sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo: %012llo\n\n", temp + 1, M[temp + 1]);
        temp = M[temp];
        if (temp == (head + 1))
            break;
        }
#endif
    /* Check if Queue was empty, and response queue */
    if (blink == (head+1) && head == (ci_data.pcb + PCB_RQI)) {
        ci_data.csr |= CI_RQA;
        set_interrupt (CI_DEVNUM, ci_data.pia);
        sim_debug (DEBUG_DETAIL, &ci_dev, "CI set response\n");
    }
    return 1;
}

/* Translate CI packet to CI 20 port format */

t_stat ci_fmt_receive (UNIT *uptr, CI_PKT *pkt, PPD_PKT *ppd)
{
uint8 opc = CI_GETB (pkt->data, CI_OPC);
uint8 flags = CI_GETB (pkt->data, CI_FLAGS);
uint8 path;

ppd->length = pkt->length;
ppd->addr = 0;
memcpy (&ppd->data[PPD_XCTID], &pkt->data[CI_XCTID], pkt->length - CI_XCTID);

ppd->length += (PPD_XCTID - CI_XCTID);                  /* set new length */

CI_PUTB (ppd->data, PPD_PORT, pkt->node);
CI_PUTB (ppd->data, PPD_OPC, opc);
CI_PUTB (ppd->data, PPD_FLAGS, flags);
CI_PUTB (ppd->data, PPD_STATUS, 0); // STS_OK);
ppd->data[PPD_OPC] |= OPC_M_RECV;                       /* convert to rcv opcode */

if (opc == OPC_ID)
    ppd->data[PPD_FLAGS] = 0;
#if 0
if (opc == OPC_LB) {
    if (pkt->path == CI_PATH_A)
        path = PPD_PS0;                                 /* received on path A */
    else if (pkt->path == CI_PATH_B)
        path = PPD_PS1;                                 /* received on path B */
    ppd->data[PPD_FLAGS] |= (path << PPD_V_PS);         /* set receive path */
    }
#endif
return SCPE_OK;
}

/* Translate CI 20 port format to CI packet */

t_stat ci_fmt_send (UNIT *uptr, PPD_PKT *ppd, CI_PKT *pkt)
{
uint8 opc = CI_GETB (ppd->data, PPD_OPC);
uint8 flags = CI_GETB (ppd->data, PPD_FLAGS);

pkt->length = ppd->length;
memcpy (&pkt->data[CI_XCTID], &ppd->data[PPD_XCTID], ppd->length - PPD_XCTID);
CI_PUTB (pkt->data, CI_OPC, opc);
CI_PUTB (pkt->data, CI_FLAGS, flags);

switch (opc) {                                          /* set new length */
    case OPC_REQID:
        pkt->length = LEN_IDREQ;
        break;

    case OPC_REQDAT0:
    case OPC_REQDAT1:
    case OPC_REQDAT2:
//    case OPC_REQMDAT:
        pkt->length = LEN_DATREQ;
        break;

    default:
        pkt->length -= (PPD_XCTID - CI_XCTID);
        break;
    }
pkt->node = CI_GETB (ppd->data, PPD_PORT);
pkt->path = GET_PATH (flags);

return SCPE_OK;
}

/* ID request - dealt with at CI layer */

t_stat ci_idreq (UNIT *uptr, CI_PKT *pkt)
{
CI_PUTB (pkt->data, CI_OPC, OPC_ID);                    /* set opcode */
if (pkt->path == CI_PATH_A)
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_0);           /* set received path */
else
    CI_PUTB (pkt->data, CI_FLAGS, CI_PATH_1);
CI_PUTL (pkt->data, CI_MAINT_ID, ID_KL10);              /* remote port type */
CI_PUTL (pkt->data, CI_CODE_REV, 0x12345678);           /* code revision */
CI_PUTL (pkt->data, CI_PORT_FCN, 0xABFFFF00);           /* function mask */
CI_PUTB (pkt->data, CI_RST_PORT, uptr->ci_node);        /* resetting port */
CI_PUTB (pkt->data, CI_SYS_STATE, CI_PTST_ENA);         /* port state */
memset (&pkt->data[CI_ID_MBZ], 0, LEN_ID - CI_ID_MBZ);
pkt->length = LEN_ID;

return ci_send (uptr, pkt);
}

t_stat ci_setckt (UNIT *uptr, PPD_PKT *ppd)
{
uint8 port = CI_GETB (ppd->data, PPD_PORT);
uint64 ovcd = ci_data.vcd[port];
uint64 nvcd = M[ppd->addr + 6];
t_stat r;

if (nvcd & VC_LDST) {
    if ((nvcd & VC_CST) && ((ovcd & VC_CST) == 0))      /* opening VC */
        r = ci_open_vc (uptr, port);
    else if (((nvcd & VC_CST) == 0) && (ovcd & VC_CST)) /* closing VC */
        r = ci_close_vc (uptr, port);
    if (r != SCPE_OK)
        return r;
    ci_data.vcd[port] = ci_data.vcd[port] & ~VC_CST;
    ci_data.vcd[port] = ci_data.vcd[port] | (nvcd & VC_CST);
    }
if (nvcd & VC_LDPS) {
    ci_data.vcd[port] = ci_data.vcd[port] & ~VC_PS;
    ci_data.vcd[port] = ci_data.vcd[port] | (nvcd & VC_PS);
    }
M[ppd->addr + 7] = ci_data.vcd[port];
sim_debug_unit (DBG_LCMD, uptr, "==> SETCKT, port: %d, vcd: %012llo -> %012llo\n", port, ovcd, ci_data.vcd[port]);
return SCPE_OK;
}

t_stat ci_setcnt (UNIT *uptr, PPD_PKT *ppd)
{
sim_debug_unit (DBG_LCMD, uptr, "==> SETCNT\n");
return SCPE_OK;
}

t_stat ci_rdcnt (UNIT *uptr, PPD_PKT *ppd)
{
sim_debug_unit (DBG_LCMD, uptr, "==> RDCNT\n");
return SCPE_OK;
}

t_stat ci_rdreg (UNIT *uptr, PPD_PKT *ppd)
{
uint8 rg = CI_GETB (ppd->data, PPD_REG);
uint8 data = 0;

switch (rg) {
    case DR_RXS:                                        /* read receiver status */
        break;

    case DR_RSX:                                        /* reset trasmiter status */
        break;

    case DR_RB:                                         /* read buffer */
        break;

    case DR_ENL:                                        /* enable link */
        break;

    case DR_RS:                                         /* read switches */
        break;

    case DR_SB:                                         /* select buffer */
        break;

    case DR_LXB:                                        /* load transmit buffer */
        break;

    case DR_TXS:                                        /* read trasmiter status */
        break;

    case DR_ABX:                                        /* abort tranmitter */
        break;

    case DR_SM:                                         /* set mode */
        break;

    case DR_TX:                                         /* transmit */
        break;

    case DR_NA:                                         /* read node address */
        data = uptr->ci_node;
        break;

    case DR_DL:                                         /* disable link */
        break;

    case DR_RRB:                                        /* release receiver buffer */
        break;

    case DR_SY:                                         /* sync */
        break;
    }
sim_debug_unit (DBG_LCMD, uptr, "==> RDREG, rg = %o, data = %o\n", rg, data);
CI_PUTB (ppd->data, PPD_RDDAT, data);
//ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], ppd->length - 12);
//ci_putq (ci_data.pcb + PCB_RQI, &ppd->addr);
return SCPE_OK;
}

t_stat ci_wrreg (UNIT *uptr, PPD_PKT *ppd)
{
uint8 rg = CI_GETB (ppd->data, PPD_REG);
uint8 data = 0;

data = CI_GETB (ppd->data, PPD_WRDAT);
switch (rg) {
    case DR_RXS:                                        /* read receiver status */
        break;

    case DR_RSX:                                        /* reset trasmiter status */
        break;

    case DR_RB:                                         /* read buffer */
        break;

    case DR_ENL:                                        /* enable link */
        break;

    case DR_RS:                                         /* read switches */
        break;

    case DR_SB:                                         /* select buffer */
        break;

    case DR_LXB:                                        /* load transmit buffer */
        break;

    case DR_TXS:                                        /* read trasmiter status */
        break;

    case DR_ABX:                                        /* abort tranmitter */
        break;

    case DR_SM:                                         /* set mode */
        break;

    case DR_TX:                                         /* transmit */
        break;

    case DR_NA:                                         /* read node address */
        break;

    case DR_DL:                                         /* disable link */
        break;

    case DR_RRB:                                        /* release receiver buffer */
        break;

    case DR_SY:                                         /* sync */
        break;
    }
sim_debug_unit (DBG_LCMD, uptr, "==> WRREG, rg = %o, data = %o\n", rg, data);
//ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], ppd->length - 12);
//ci_putq (ci_data.pcb + PCB_RQI, &ppd->addr);
return SCPE_OK;
}

t_stat ci_clsbuf (UNIT *uptr, PPD_PKT *ppd)
{
sim_debug_unit (DBG_LCMD, uptr, "==> CLSBUF\n");
return SCPE_OK;
}

t_stat ci_send_ppd (UNIT *uptr, PPD_PKT *ppd)
{
CI_PKT pkt;
t_stat r;

//ppd->data[PPD_STATUS] = 0; //STS_OK;                         /* assume ok */
switch (ppd->data[PPD_OPC]) {                           /* opcodes handled by port */
    case OPC_SETCKT:
        return ci_setckt (uptr, ppd);

    case OPC_SETCNT:
        return ci_setcnt (uptr, ppd);

    case OPC_RDCNT:
        return ci_rdcnt (uptr, ppd);

    case OPC_RDREG:
        return ci_rdreg (uptr, ppd);

    case OPC_WRREG:
        return ci_wrreg (uptr, ppd);

    case OPC_CLSBUF:
        return ci_clsbuf (uptr, ppd);

//    case OPC_SNDDAT:
//    case OPC_SNDMDAT:
//        return ci_send_data (uptr, ppd);
        }
r = ci_fmt_send (uptr, ppd, &pkt);                      /* convert to CI format */
if (r == SCPE_OK)
    r = ci_send (uptr, &pkt);                           /* send CI packet */
return r;
}

t_stat ci_process_entry (UNIT *uptr, t_addr entry)
{
PPD_PKT pkt;
uint8 opc, flags, type;
uint32 i;
uint64 word;

pkt.addr = entry;
for (i = 0; i < 20; i++)
    sim_debug (DEBUG_DETAIL, &ci_dev, "   %012llo\n", M[entry++]);
ci_cpy_to (pkt.addr, pkt.data, LEN_HDR);                /* read header */

opc = CI_GETB (pkt.data, PPD_OPC);
flags = CI_GETB (pkt.data, PPD_FLAGS);
switch (opc) {
    case OPC_SNDDG:
        pkt.length = LEN_SNDDG;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        word = M[pkt.addr + 4] & 0xFFFFF;
        ci_cpy_to_ind (pkt.addr, &pkt.data[PPD_TEXT], PPD_TEXT + 2, word);  /* FIXME: include length field in PPD? */
        pkt.length = LEN_SNDDG + word;
        type = PPD_DG;
        break;

    case OPC_SNDMSG:
        pkt.length = LEN_SNDMSG;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        word = M[pkt.addr + 4] & 0xFFFFF;
        ci_cpy_to_ind (pkt.addr, &pkt.data[PPD_TEXT], PPD_TEXT + 2, word);  /* FIXME: include length field in PPD? */
        pkt.length = LEN_SNDMSG + word;
        type = PPD_MSG;
        break;

    case OPC_REQID:
        pkt.length = LEN_REQID;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        if (GET_PATH (pkt.data[PPD_FLAGS]) == PPD_PS0)
            pkt.data[PPD_STATUS] = 0304;                /* no response on path A */
        else
            pkt.data[PPD_STATUS] = 0244;                /* no response on path B */
//        pkt.data[PPD_STATUS] = 0344;                    /* no response on either path */
        type = PPD_DG;
        break;

    case OPC_SNDRST:
        pkt.length = LEN_SNDRST;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_SNDSTRT:
        pkt.length = LEN_SNDSTRT;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_REQDAT0:
    case OPC_REQDAT1:
    case OPC_REQDAT2:
//    case OPC_REQMDAT:
        pkt.length = LEN_REQDAT;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_MSG;
        break;

    case OPC_SNDLB:
        pkt.length = LEN_SNDLB; // + len;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

//    case OPC_SNDDAT:
//    case OPC_SNDMDAT:
//        pkt.length = LEN_SNDDAT;
//        type = PPD_MSG;
//        break;

    case OPC_SETCKT:
        pkt.length = LEN_SETCKT;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_SETCNT:
        pkt.length = LEN_SETCNT;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_RDCNT:
        pkt.length = LEN_RDCNT;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_RDREG:
        pkt.length = LEN_RDREG;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_WRREG:
        pkt.length = LEN_WRREG;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    case OPC_CLSBUF:
        pkt.length = LEN_CLSBUF;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        type = PPD_DG;
        break;

    default:
        break;
    }

ci_send_ppd (uptr, &pkt);                   /* process it */
if ((opc == OPC_RDREG) || (flags & PPD_RSP) || (pkt.data[PPD_STATUS] != 0)) {
    ci_cpy_from (pkt.addr + 3, &pkt.data[PPD_PORT], pkt.length - 12);
    ci_putq (ci_data.pcb + PCB_RQI, &pkt.addr);
    }
else if (type == PPD_MSG)
    ci_putq (ci_data.pcb + PCB_MQI, &pkt.addr);
else
    ci_putq (ci_data.pcb + PCB_DQI, &pkt.addr);
}

t_stat ci_port_command (UNIT *uptr)
{
t_addr entry = 0;

do {
    ci_getq (ci_data.pcb + PCB_C3I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 3: %o\n", entry);
        ci_process_entry (uptr, entry);
        }
} while (entry != 0);
do {
    ci_getq (ci_data.pcb + PCB_C2I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 2: %o\n", entry);
        ci_process_entry (uptr, entry);
        }
} while (entry != 0);
do {
    ci_getq (ci_data.pcb + PCB_C1I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 1: %o\n", entry);
        ci_process_entry (uptr, entry);
        }
} while (entry != 0);
do {
    ci_getq (ci_data.pcb + PCB_C0I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 0: %o\n", entry);
        ci_process_entry (uptr, entry);
        }
} while (entry != 0);
ci_data.csr &= ~CI_CQA;                                 /* all commands processed */
return SCPE_OK;
}

t_stat ci_receive_ppd (UNIT *uptr, PPD_PKT *ppd)
{
t_stat r = SCPE_OK;
uint8 opc = CI_GETB (ppd->data, PPD_OPC);

switch (opc) {                                      /* determine packet type */
    case OPC_MSGREC:
    case OPC_DATREC:
//    case OPC_CNFREC:
//    case OPC_RETCNF:
    case OPC_RETDAT:
        ci_getq (ci_data.pcb + PCB_MQI, &ppd->addr);    /* message */
        break;

    case OPC_DGREC:
//    case OPC_MCNFREC:
    case OPC_RSTREC:
    case OPC_STRTREC:
    case OPC_IDREC:
    case OPC_LBREC:
//    case OPC_REQMDATREC:
//    case OPC_SNDMDATREC:
//    case OPC_MDATREC:
        ci_getq (ci_data.pcb + PCB_DQI, &ppd->addr);    /* datagram */
#if 0
        if (r == CISE_NOPKT) {
            if (ci_cnt == CI_ALL)
                ci_dgd++;
            else if (ci_cnt == pkt->data[PPD_PORT])
                ci_dgd++;
            }
#endif
        break;

    default:
        sim_printf ("ci_receive_ppd() - Unhandled opcode 0x%02X\n", opc);
        r = SCPE_IERR;
        break;
    }
if (r != SCPE_OK)
    return r;

if (ppd->addr == 0) {
    sim_printf ("ci_receive_ppd() - No free queue entry available\n");
    return SCPE_IERR;
    }
switch (opc) {
    case OPC_DGREC:
        ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], LEN_SNDDG - 12);
        ppd->length = ppd->length - LEN_SNDDG;
        M[ppd->addr + 4] = ppd->length;
        ci_cpy_from_ind (ppd->addr, &ppd->data[PPD_TEXT], PPD_TEXT + 2, ppd->length);
        break;

    case OPC_MSGREC:
        ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], LEN_SNDMSG - 12);
        ppd->length = ppd->length - LEN_SNDMSG;
        M[ppd->addr + 4] = ppd->length;
        ci_cpy_from_ind (ppd->addr, &ppd->data[PPD_TEXT], PPD_TEXT + 2, ppd->length);
        break;

    default:
        ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], ppd->length - 12);
        break;
    }
ci_putq (ci_data.pcb + PCB_RQI, &ppd->addr);
return SCPE_OK;
}

t_stat ci_svc (UNIT *uptr)
{
CI_PKT pkt;
PPD_PKT ppd;
t_stat r;
uint8 opc;

do {
    r = ci_receive (uptr, &pkt);                        /* try to receive another packet */
    if (r == SCPE_OK) {
        opc = CI_GETB (pkt.data, CI_OPC);
        switch (opc) {                                  /* opcodes handled at CI layer */
            case OPC_IDREQ:
                r = ci_idreq (uptr, &pkt);
                break;

            default:
                r = ci_fmt_receive (uptr, &pkt, &ppd);      /* convert to PPD format */
                if (r == SCPE_OK)
                    r = ci_receive_ppd (uptr, &ppd);
                break;
            }
        }
} while (r == SCPE_OK);
if (r != CISE_NOPKT)
    return r;
return ci_port_svc (uptr);
}

t_stat ci_reset (DEVICE *dptr)
{
uint32 i;

for (i = 0; i < CI_MAX_NODES; i++)
    ci_data.vcd[i] = 0;
return ci_port_reset (dptr);
}

t_stat ci_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "CI interface\n\n");
fprintf (st, "The CI interfaces to the network. Setting MAC defines default MAC address\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
return SCPE_OK;
}

const char *ci_description (DEVICE *dptr)
{
return "KL CI interface";
}
#endif
