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

#define CI_PPT          0400000000000LL                 /* port present */
#define CI_DCC          0100000000000LL                 /* diag CSR */
#define CI_CPE          0004000000000LL                 /* CRAM Parity error */
#define CI_MBE          0002000000000LL                 /* MBUS error */
#define CI_ILD          0000100000000LL                 /* idle */
#define CI_DCP          0000040000000LL                 /* disable complete */
#define CI_ECP          0000020000000LL                 /* enable complete */
#define CI_PID          0000007000000LL                 /* port ID */

/* CSR - CONO/CONI */

#define CI_CPT          0000000400000LL                 /* clear Port */
#define CI_SEB          0000000200000LL                 /* diag Select EBUF */
#define CI_GEB          0000000100000LL                 /* diag Gen Ebus PE */
#define CI_LAR          0000000040000LL                 /* diag select LAR */
#define CI_SSC          0000000020000LL                 /* diag Single Cycle */
#define CI_EPE          0000000004000LL                 /* EBUS parity error */
#define CI_FQE          0000000002000LL                 /* free Queue Error */
#define CI_DME          0000000001000LL                 /* data mover error */
#define CI_CQA          0000000000400LL                 /* command Queue Available */
#define CI_RQA          0000000000200LL                 /* response Queue Available */
#define CI_DIS          0000000000040LL                 /* disable */
#define CI_ENB          0000000000020LL                 /* enable */
#define CI_MRN          0000000000010LL                 /* run */
#define CI_PIA          0000000000007LL                 /* PIA */

#define CI_LRA          0400000000000LL                 /* load RAM address */
#define CI_RAR          0377760000000LL                 /* microcode address mask */
#define CI_MSB          0000020000000LL                 /* half word select */

/* PCB Offsets */

#define PCB_BDT         000                             /* buffer descriptor table start address */
#define PCB_MQE         001                             /* message free queue entry length */
#define PCB_DQE         002                             /* datagram free queue entry length */
#define PCB_RS0         003                             /* reserved */
#define PCB_C3I         004                             /* command queue 3 interlock */
#define PCB_C3F         005                             /* command queue 3 flink */
#define PCB_C3B         006                             /* command queue 3 blink */
#define PCB_C2I         007                             /* command queue 2 interlock */
#define PCB_C2F         010                             /* command queue 2 flink */
#define PCB_C2B         011                             /* command queue 2 blink */
#define PCB_C1I         012                             /* command queue 1 interlock */
#define PCB_C1F         013                             /* command queue 1 flink */
#define PCB_C1B         014                             /* command queue 1 blink */
#define PCB_C0I         015                             /* command queue 0 interlock */
#define PCB_C0F         016                             /* command queue 0 flink */
#define PCB_C0B         017                             /* command queue 0 blink */
#define PCB_RQI         020                             /* response queue interlock */
#define PCB_RQF         021                             /* response queue flink */
#define PCB_RQB         022                             /* response queue blink */
#define PCB_MQI         023                             /* message free queue interlock */
#define PCB_MQF         024                             /* message free queue flink */
#define PCB_MQB         025                             /* message free queue blink */
#define PCB_DQI         026                             /* datagram free queue interlock */
#define PCB_DQF         027                             /* datagram free queue flink */
#define PCB_DQB         030                             /* datagram free queue blink */
#define PCB_RS1         031                             /* reserved */
#define PCB_RS2         032                             /* reserved */
#define PCB_RS3         033                             /* reserved */
#define PCB_RS4         034                             /* reserved */
#define PCB_PE0         035                             /* port error word 0 */
#define PCB_PE1         036                             /* port error word 1 */
#define PCB_PE2         037                             /* port error word 2 */
#define PCB_PE3         040                             /* port error word 3 */
#define PCB_PE4         041                             /* port error word 4 */
#define PCB_PCB         042                             /* PCB base address */
#define PCB_PIL         043                             /* PI level */
#define PCB_CL1         044                             /* channel logout word 1 address */
#define PCB_CCW         045                             /* channel command word */
#define PCB_RS5         046                             /* reserved to port */

#define CHNERR          07762
#define SLFTST          07751
#define INTERR          07750
#define EBSERR          07752

/* Virtual Circuit Flags */

#define VC_LDST         0400000000000LL                 /* load circuit state */
#define VC_CST          0200000000000LL                 /* circuit state */
#define VC_NR           0100000000000LL                 /* receiving sequence number */
#define VC_NS           0040000000000LL                 /* sending sequence number */
#define VC_LDPS         0020000000000LL                 /* load path select */
#define VC_PS           0014000000000LL                 /* path select */

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

#define PPD_XFRSZ       0x18                            /* transfer size */
#define PPD_SBNAM       0x1C                            /* send buffer name */
#define PPD_SBOFF       0x20                            /* send buffer offset */
#define PPD_RBNAM       0x24                            /* receive buffer name */
#define PPD_RBOFF       0x28                            /* receive buffer offset */

/* PPD Lengths */

#define LEN_HDR         0x10                            /* PPD header size */
#define LEN_SNDDG       (LEN_HDR)
#define LEN_SNDMSG      (LEN_HDR)
#define LEN_REQID       (LEN_HDR + 8)
#define LEN_SNDRST      (LEN_HDR + 8)
#define LEN_SNDSTRT     (LEN_HDR + 12)
#define LEN_REQDAT      (LEN_HDR + 28)
#define LEN_SNDLB       (LEN_HDR)
#define LEN_SNDDAT      (LEN_HDR + 28)
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
#define OPC_SNDDAT      0x10                            /* send data */
//#define OPC_RETDAT      021
//#define OPC_SNTMDAT     022
//#define OPC_MCNF        044

#define OPC_M_RECV      040

#define OPC_DGREC       (OPC_M_RECV | OPC_SNDDG)
#define OPC_MSGREC      (OPC_M_RECV | OPC_SNDMSG)
#define OPC_CNFREC      (OPC_M_RECV | OPC_CNFRET)
#define OPC_RSTREC      (OPC_M_RECV | OPC_SNDRST)
#define OPC_STRTREC     (OPC_M_RECV | OPC_SNDSTRT)
#define OPC_REQDATREC   (OPC_M_RECV | OPC_REQDAT0)       /* request data received */
#define OPC_IDREC       (OPC_M_RECV | OPC_RETID)
#define OPC_LBREC       (OPC_M_RECV | OPC_SNDLB)
#define OPC_DATREC      (OPC_M_RECV | OPC_RETDAT)

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

#define CI_CNT_UV       000                             /* microcode version */
#define CI_CNT_AKA      001                             /* path A ACK count */
#define CI_CNT_NKA      002                             /* path A NAK count */
#define CI_CNT_NRA      003                             /* path A no response count */
#define CI_CNT_AKB      004                             /* path B ACK count */
#define CI_CNT_NKB      005                             /* path B NAK count */
#define CI_CNT_NRB      006                             /* path B no response count */
#define CI_CNT_DGD      007                             /* datagrams discarded */
#define CI_CNT_PTX      010                             /* packets transmitted */
#define CI_CNT_PRX      011                             /* packets received */

/* Buffer Header Descriptor */

#define BHD_V_KEY       20                              /* buffer key */
#define BHD_M_KEY       0177777
#define BHD_NCV         010                             /* no clear valid */
#define BHD_VLD         004                             /* valid */
#define BHD_ERR         002                             /* error */
#define BHD_WRT         001                             /* writeable */
#define BHD_BSD         077777777                       /* BSD address */
#define BHD_LEN         0777777777777                   /* length */

/* Buffer Segment Descriptor */

#define BSD_V_MODE      28                              /* transfer mode */
#define BSD_M_MODE      003
#define  BSD_IND        0                               /* industry compatible */
#define  BSD_CD         1                               /* core dump */
#define  BSD_HD         2                               /* high density */
#define BSD_SA          077777777                       /* segment address */
#define BSD_BSD         077777777                       /* next BSD address */
#define BSD_LEN         0777777777777                   /* segment length */

typedef struct {
    uint32 addr;                                        /* associated memory address */
    size_t length;                                      /* packet length */
    uint8 data[CI_MAXFR];                               /* packet data */
} PPD_PKT;

struct ci_device {
    t_addr            pcb;                              /* address of PCB */
    int               pia;                              /* interrupt channel */
    int               rar;
    uint64            ebuf;
    uint64            csr;                              /* control/status register */
    uint32            uver[4];                          /* version information */
    uint64            vcd[CI_MAX_NODES];                /* virtual circuit descriptors */
    PPD_PKT           ppd;
    uint32            ppdq;
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
t_stat     ci_receive_ppd (UNIT *uptr, PPD_PKT *ppd);
t_stat     ci_svc (UNIT *uptr);
t_stat     ci_cmd_svc (UNIT *uptr);
t_stat     ci_reset (DEVICE *dptr);
t_stat     ci_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, CONST char *cptr);
const char *ci_description (DEVICE *dptr);

struct rh_if   ci_rh = { NULL, NULL, NULL };

UNIT ci_unit[] = {
    { UDATA (&ci_svc, UNIT_IDLE|UNIT_ATTABLE, 0) },
    { UDATA (&ci_cmd_svc, UNIT_IDLE, 0) }
    };

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
    { "CONI", DEBUG_CONI, "Show CONI instructions" },
    { "CONO", DEBUG_CONO, "Show CONO instructions" },
    { "DATAIO", DEBUG_DATAIO, "Show DATI and DATO instructions" },
    { "IRQ", DEBUG_IRQ, "Show IRQ requests" },
    { "DIAG", DBG_DIAG, "Show diagnostic packets" },
    { "CONF", DBG_CONF, "Show configuration packets" },
    { "PPD", DBG_PPD, "Show port to port datagrams" },
    { "SCS", DBG_SCS, "Show SCS messages" },
    { "BLK", DBG_BLK, "Show block transfers" },
    { "LCMD", DBG_LCMD, "Show local commands" },
    { "CONN", DBG_CONN, "Show connection activity" },
    { "PKT", DBG_PKT, "Dump packet data" },
    { 0, 0 }
    };

DEVICE ci_dev = {
    "CI", ci_unit, ci_reg, ci_mod,
    1, DEV_RDX, 0, 1, DEV_RDX, 36,
    NULL, NULL, &ci_reset, NULL, &ci_attach, &ci_detach,
    &ci_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG | DEV_CI, 0, ci_debug,
    NULL, NULL, &ci_help, NULL, NULL, &ci_description
    };

t_stat ci_devio (uint32 dev, uint64 *data)
{
DEVICE *dptr = &ci_dev;
UNIT   *uptr = &ci_unit[0];

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
           sim_activate (&ci_unit[1], 5000);             /* process command queue */
        }
        if (ci_data.csr & (CI_CPE|CI_MBE|CI_RQA))
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
        sim_debug (DEBUG_DATAIO, dptr, "CI %03o DATO %012llo PC=%o\n",
                 dev, *data, PC);
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
        sim_debug (DEBUG_DATAIO, dptr, "CI %03o DATI %012llo PC=%o\n",
                 dev, *data, PC);
        break;
    }

return SCPE_OK;
}

/* Set error code and stop */

void ci_error (int err)
{
    ci_data.rar = err;
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI error %03o\n", err);
    ci_data.csr |= CI_MBE;
    set_interrupt (CI_DEVNUM, ci_data.csr & CI_PIA);
    ci_stop ();
}

/* Start CI device, load in 2 words using RH20 mode */

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
    sim_printf ("CI PCB %012llo\n", ci_rh.buf);
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

/* Stop CI 20 */

void ci_stop ()
{
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI stop\n");
    ci_data.csr &= ~CI_MRN;
}

/* Enable CI 20 */

void ci_enable (UNIT *uptr)
{
    uint64   word;
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI enable\n");
    ci_rx_enable (uptr, (CI_PATH_A | CI_PATH_B));       /* enable receiver */
    ci_data.csr |= CI_ECP;
    ci_data.csr &= ~CI_DCP;
}

/* Disable CI 20 */

void ci_disable (UNIT *uptr)
{
    sim_debug (DEBUG_DETAIL, &ci_dev, "CI disable\n");
    ci_rx_enable (uptr, 0);                             /* disable receiver */
    ci_data.csr |= CI_DCP;
    ci_data.csr &= ~CI_ECP;
}

/* Copy memory to packet - High density format */

uint8 *ci_cpy_to_hd (t_addr addr, uint8 *data, int len, t_bool hi_word)
{
    uint64 word;

    while (len > 8) {                                   /* copy full words */
        word = M[addr++];
        if (hi_word) {
            *data = *data & ~0x0F;
            *data++ = *data | (uint8)((word >> 32) & 0x0F);
            *data++ = (uint8)((word >> 24) & 0xff);
            *data++ = (uint8)((word >> 16) & 0xff);
            *data++ = (uint8)((word >> 8) & 0xff);
            *data++ = (uint8)(word & 0xff);
            }
        else {
            *data++ = (uint8)((word >> 28) & 0xff);
            *data++ = (uint8)((word >> 20) & 0xff);
            *data++ = (uint8)((word >> 12) & 0xff);
            *data++ = (uint8)((word >> 4) & 0xff);
            *data = *data & ~0xF0;
            *data = *data | (uint8)((word << 4) & 0xf0);
            }
         len -= 9;
         hi_word = !hi_word;
    }

    if (len) {                                          /* copy last partial word */
        word = M[addr++];
        if (hi_word) {
            switch (len) {
                case 8:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data++ = (uint8)((word >> 24) & 0xff);
                    *data++ = (uint8)((word >> 16) & 0xff);
                    *data++ = (uint8)((word >> 8) & 0xff);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)(word & 0xF0);
                    break;
                case 7:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data++ = (uint8)((word >> 24) & 0xff);
                    *data++ = (uint8)((word >> 16) & 0xff);
                    *data++ = (uint8)((word >> 8) & 0xff);
                    break;
                case 6:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data++ = (uint8)((word >> 24) & 0xff);
                    *data++ = (uint8)((word >> 16) & 0xff);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 8) & 0xF0);
                    break;
                case 5:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data++ = (uint8)((word >> 24) & 0xff);
                    *data++ = (uint8)((word >> 16) & 0xff);
                    break;
                case 4:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data++ = (uint8)((word >> 24) & 0xff);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 16) & 0xF0);
                    break;
                case 3:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data++ = (uint8)((word >> 24) & 0xff);
                    break;
                case 2:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 24) & 0xF0);
                    break;
                case 1:
                    *data = *data & ~0x0F;
                    *data++ = *data | (uint8)((word >> 32) & 0x0F);
                    break;
                }
            }
        else {
            switch (len) {
                case 8:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    *data++ = (uint8)((word >> 20) & 0xff);
                    *data++ = (uint8)((word >> 12) & 0xff);
                    *data++ = (uint8)((word >> 4) & 0xff);
                    break;
                case 7:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    *data++ = (uint8)((word >> 20) & 0xff);
                    *data++ = (uint8)((word >> 12) & 0xff);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 4) & 0xF0);
                    break;
                case 6:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    *data++ = (uint8)((word >> 20) & 0xff);
                    *data++ = (uint8)((word >> 12) & 0xff);
                    break;
                case 5:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    *data++ = (uint8)((word >> 20) & 0xff);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 12) & 0xF0);
                    break;
                case 4:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    *data++ = (uint8)((word >> 20) & 0xff);
                    break;
                case 3:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 20) & 0xF0);
                    break;
                case 2:
                    *data++ = (uint8)((word >> 28) & 0xff);
                    break;
                case 1:
                    *data = *data & ~0xF0;
                    *data++ = *data | (uint8)((word >> 28) & 0xF0);
                    break;
                }
            }
        }
    return data;
}

/* Copy packet to memory - High density format */

uint8 *ci_cpy_from_hd (t_addr addr, uint8 *data, int len, t_bool hi_word)
{
    uint64    word;

    while (len > 8) {                                   /* copy full words */
        if (hi_word) {
            word =  (uint64)(*data++ & 0xF) << 32;
            word |= (uint64)(*data++) << 24;
            word |= (uint64)(*data++) << 16;
            word |= (uint64)(*data++) << 8;
            word |= (uint64)(*data++);
            }
        else {
            word =  (uint64)(*data++) << 28;
            word |= (uint64)(*data++) << 20;
            word |= (uint64)(*data++) << 12;
            word |= (uint64)(*data++) << 4;
            word |= (uint64)((*data) >> 4);
            }
        M[addr++] = word;
        len -= 9;
        hi_word = !hi_word;
    }

    if (len) {                                          /* copy last partial word */
        word = M[addr];
        if (hi_word) {
            switch (len) {
                case 8:
                    word = word & ~((uint64)0xFFFFFFFF0);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++) << 24;
                    word |= (uint64)(*data++) << 16;
                    word |= (uint64)(*data++) << 8;
                    word |= (uint64)(*data++ & 0xF0);
                    break;
                case 7:
                    word = word & ~((uint64)0xFFFFFFF00);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++) << 24;
                    word |= (uint64)(*data++) << 16;
                    word |= (uint64)(*data++) << 8;
                    break;
                case 6:
                    word = word & ~((uint64)0xFFFFFF000);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++) << 24;
                    word |= (uint64)(*data++) << 16;
                    word |= (uint64)(*data++ & 0xF0) << 8;
                    break;
                case 5:
                    word = word & ~((uint64)0xFFFFF0000);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++) << 24;
                    word |= (uint64)(*data++) << 16;
                    break;
                case 4:
                    word = word & ~((uint64)0xFFFF00000);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++) << 24;
                    word |= (uint64)(*data++ & 0xF0) << 16;
                    break;
                case 3:
                    word = word & ~((uint64)0xFFF000000);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++) << 24;
                    break;
                case 2:
                    word = word & ~((uint64)0xFF000000);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    word |= (uint64)(*data++ & 0xF0) << 24;
                    break;
                case 1:
                    word = word & ~((uint64)0xF0000000);
                    word =  (uint64)((*data++ & 0xF)) << 32;
                    break;
                default:
                    word = 0;
                    break;
                }
            }
        else {
            switch (len) {
                case 8:
                    word = word & ~((uint64)0xFFFFFFFF0);
                    word |= (uint64)(*data++) << 28;
                    word |= (uint64)(*data++) << 20;
                    word |= (uint64)(*data++) << 12;
                    word |= (uint64)(*data++) << 4;
                    break;
                case 7:
                    word = word & ~((uint64)0xFFFFFFF00);
                    word |= (uint64)(*data++) << 28;
                    word |= (uint64)(*data++) << 20;
                    word |= (uint64)(*data++) << 12;
                    word |= (uint64)(*data++ & 0xF0) << 4;
                    break;
                case 6:
                    word = word & ~((uint64)0xFFFFFF000);
                    word |= (uint64)(*data++) << 28;
                    word |= (uint64)(*data++) << 20;
                    word |= (uint64)(*data++) << 12;
                    break;
                case 5:
                    word = word & ~((uint64)0xFFFFF0000);
                    word |= (uint64)(*data++) << 28;
                    word |= (uint64)(*data++) << 20;
                    word |= (uint64)(*data++ & 0xF0) << 12;
                    break;
                case 4:
                    word = word & ~((uint64)0xFFFF00000);
                    word |= (uint64)(*data++) << 28;
                    word |= (uint64)(*data++) << 20;
                    break;
                case 3:
                    word = word & ~((uint64)0xFFF000000);
                    word |= (uint64)(*data++) << 28;
                    word |= (uint64)(*data++ & 0xF0) << 20;
                    break;
                case 2:
                    word = word & ~((uint64)0xFF000000);
                    word |= (uint64)(*data++) << 28;
                    break;
                case 1:
                    word = word & ~((uint64)0xF0000000);
                    word |= (uint64)(*data++ & 0xF0) << 28;
                    break;
                default:
                    word = 0;
                    break;
                }
            }
        M[addr++] = word;
        }
    return data;
}

/* Copy memory to packet - Industry standard format */

uint8 *ci_cpy_to_ind (t_addr addr, uint8 *data, int off, int len)
{
    uint64 word;

    addr = addr + (off >> 2);                           /* apply offset */
    if (off & 0x3) {
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
        }

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

/* Copy packet to memory - Industry standard format */

uint8 *ci_cpy_from_ind (t_addr addr, uint8 *data, int off, int len)
{
    uint64    word;

    addr = addr + (off >> 2);                           /* apply offset */
    if (off & 0x3) {
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
        }

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

/* Copy memory to packet - Header */

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

/* Copy packet to memory - Header */

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
CI_PUTB (ppd->data, PPD_FLAGS, flags & 0xF0);           /* only top bits are common */
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
CI_PUTB (pkt->data, CI_FLAGS, flags & 0xF0);            /* only top bits are common */

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

t_stat ci_send_data (UNIT *uptr, PPD_PKT *ppd)
{
uint64 bdt, bhd, bsd, bhd0, bsd0, tlen, sa, sl, seg, so, tl;
uint32 len, nlen, xfrsz, sbnam, sboff, rbnam, rboff, pkt_off;
uint16 snd_key, key, mode;
CI_PKT pkt;
t_stat r;

ci_dump_pkt (DBG_BLK, uptr, ppd->data, ppd->length);
xfrsz = CI_GETL (ppd->data, PPD_XFRSZ);
sbnam = CI_GETW (ppd->data, PPD_SBNAM);                 /* BDT table offset */
snd_key = CI_GETW (ppd->data, PPD_SBNAM + 2);
sboff = CI_GETL (ppd->data, PPD_SBOFF) << 1;                 /* send buffer offset */
rboff = CI_GETL (ppd->data, PPD_RBOFF);                 /* receive buffer offset */

bdt = M[ci_data.pcb + PCB_BDT];                         /* get BDT address */
bhd = bdt + sbnam;                                      /* get BHD address */
bhd0 = M[bhd];                                          /* get BHD */
key = (bhd0 >> BHD_V_KEY) & BHD_M_KEY;                  /* buffer key */

if (((bhd0 & BHD_VLD) == 0) || (snd_key != key)) {      /* check valid, buffer key */
    sim_printf ("CI: send buffer invalid - bdt = %012llo, sbnam = %o, snd_key = %d, key = %d, bhd0 = %012llo\n",
        bdt, sbnam, snd_key, key, bhd0);
    return SCPE_IERR;
    }

CI_PUTB (ppd->data, PPD_OPC, OPC_RETDAT);
r = ci_fmt_send (uptr, ppd, &pkt);                      /* convert to CI format */
if (r != SCPE_OK)
    return r;

rbnam = CI_GETL (ppd->data, PPD_RBNAM);                 /* BDT table offset */
CI_PUTB (pkt.data, CI_FLAGS, 0);                        /* flags */
CI_PUTL (pkt.data, CI_RBNAM, rbnam);                    /* receive buffer name */

while (xfrsz > 0) {
    if (xfrsz > CI_MAXDAT)                              /* limit to max transfer */
        len = CI_MAXDAT;
    else
        len = xfrsz;

    nlen = len << 1;                                    /* get length in nibbles */
    bsd = M[bhd + 1] & BHD_BSD;                         /* get BSD address */
    tlen = M[bhd + 2] & BHD_LEN;                        /* get total len */
    if (xfrsz > tlen) {
        sim_printf ("ci_send_data() - Transfer size limited to %d from %d\n", tlen, xfrsz);
        xfrsz = tlen;
        }
    seg = 0;
    do {
        bsd0 = M[bsd];                                  /* get BSD */

        mode = (bsd0 >> BSD_V_MODE) & BSD_M_MODE;       /* get transfer mode */
        sa = bsd0 & BSD_SA;                             /* get segment address */
        sl = M[bsd + 2] & BSD_LEN;                      /* get segment len */
        if ((sboff < (seg + sl)) && ((sboff + nlen) > seg)) {
            if (sboff <= seg)
                so = 0;                                 /* starts in a previous segment */
            else
                so = (sboff - seg);                     /* starts in this segment */
            if ((sboff + nlen) <= (seg + sl))
                tl = (sboff + nlen) - (seg + so);       /* ends in this segment */
            else
                tl = sl - so;                           /* ends in a subsequent segment */

            if ((so % 9) != 0) {
                sim_printf ("ci_send_data() - Segment offset %d not supported\n", so);
                return SCPE_IERR;
                }
            pkt_off = (seg >> 1) % CI_MAXDAT;
            if (mode == BSD_HD) {
                sa = sa + (so / 9);
                ci_cpy_to_hd (sa, &pkt.data[CI_DATA + pkt_off], tl, (seg & 1));
                }
            else {
                sim_printf ("ci_send_data() - Unsupported transfer mode %d\n", mode);
                return SCPE_IERR;
                }
            }
        seg = seg + sl;
        bsd = M[bsd + 1] & BSD_BSD;                     /* get next segment address */
    } while (bsd != 0);

    CI_PUTL (pkt.data, CI_RBOFF, rboff);                /* update packet */
    if (xfrsz <= CI_MAXDAT)
        pkt.data[CI_FLAGS] |= CI_LP;                    /* last packet */
    pkt.length = len + CI_DATA;
    r = ci_send (uptr, &pkt);                           /* send data */
    if (r != SCPE_OK)
        return r;
    xfrsz -= len;
    sboff += nlen;
    rboff += len;
    }
if ((bhd0 & BHD_NCV) == 0) {
    bhd0 = bhd0 & ~BHD_VLD;
    M[bhd] = bhd0;                                      /* set BHD */
    }
return SCPE_OK;
}

t_stat ci_receive_data (UNIT *uptr, CI_PKT *pkt)
{
uint64 bdt, bhd, bsd, bhd0, bsd0, tlen, sa, sl, seg, so, tl;
uint32 len, nlen, rboff, opc, pkt_off;
uint16 rbnam, rec_key, key, mode;
PPD_PKT ppd;
t_stat r = SCPE_OK;

opc = CI_GETB (pkt->data, CI_OPC);
rbnam = CI_GETW (pkt->data, CI_RBNAM);                  /* BDT table offset */
rec_key = CI_GETW (pkt->data, CI_RBNAM + 2);
rboff = CI_GETL (pkt->data, CI_RBOFF) << 1;             /* receive buffer offset */

bdt = M[ci_data.pcb + PCB_BDT];                         /* get BDT address */
bhd = bdt + rbnam;                                      /* get BHD address */
bhd0 = M[bhd];                                          /* get BHD */
key = (bhd0 >> BHD_V_KEY) & BHD_M_KEY;                  /* buffer key */

if ((bhd0 & BHD_VLD) == 0) {                            /* check valid */
    sim_printf ("ci_receive_data(): buffer not valid - bdt = %012llo, rbnam = %o, rec_key = %d, key = %d, bhd0 = %012llo\n",
        bdt, rbnam, rec_key, key, bhd0);
    return SCPE_IERR;
    }
if (rec_key != key) {                                   /* check buffer key */
    sim_printf ("ci_receive_data(): wrong buffer key - bdt = %012llo, rbnam = %o, rec_key = %d, key = %d, bhd0 = %012llo\n",
        bdt, rbnam, rec_key, key, bhd0);
    return SCPE_IERR;
    }
if ((bhd0 & BHD_WRT) == 0) {                            /* check buffer writeable */
    sim_printf ("ci_receive_data(): buffer not writeable - bdt = %012llo, rbnam = %o, rec_key = %d, key = %d, bhd0 = %012llo\n",
        bdt, rbnam, rec_key, key, bhd0);
    return SCPE_IERR;
    }
len = pkt->length - CI_DATA;
if (len > CI_MAXDAT)                                    /* limit to max transfer FIXME */
    len = CI_MAXDAT;

bsd = M[bhd + 1] & BHD_BSD;                             /* get BSD address */
tlen = M[bhd + 2] & BHD_LEN;                            /* get total len */
seg = 0;
nlen = len << 1;
do {
    bsd0 = M[bsd];                                      /* get BSD */

    mode = (bsd0 >> BSD_V_MODE) & BSD_M_MODE;           /* get transfer mode */
    sa = bsd0 & BSD_SA;                                 /* get segment address */
    sl = M[bsd + 2] & BSD_LEN;                          /* get segment len */
    if ((rboff < (seg + sl)) && ((rboff + nlen) > seg)) {
        if (rboff <= seg)
            so = 0;                                     /* starts in a previous segment */
        else
            so = (rboff - seg);                         /* starts in this segment */
        if ((rboff + nlen) <= (seg + sl))
            tl = (rboff + nlen) - (seg + so);           /* ends in this segment */
        else
            tl = sl - so;                               /* ends in a subsequent segment */

        if ((so % 9) != 0) {
            sim_printf ("ci_receive_data() - Segment offset %d not supported\n", so);
            return SCPE_IERR;
            }
        pkt_off = (seg >> 1) % CI_MAXDAT;
        if (mode == BSD_HD) {
            sa = sa + (so / 9);
            sim_printf ("ci_receive_data() - %012llo, %o\n", sa, tl);
            ci_cpy_from_hd (sa, &pkt->data[CI_DATA + pkt_off], tl, (seg & 1));
            }
        else {
            sim_printf ("ci_receive_data() - Unsupported transfer mode %d\n", mode);
            return SCPE_IERR;
            }
        }
    seg = seg + sl;
    bsd = M[bsd + 1] & BSD_BSD;                         /* get next segment address */
} while (bsd != 0);

if (pkt->data[CI_FLAGS] & CI_LP) {                      /* last packet? */
    if ((bhd0 & BHD_NCV) == 0) {
        bhd0 = bhd0 & ~BHD_VLD;
        M[bhd] = bhd0;                                  /* set BHD */
        }
    if (opc == OPC_SNTDAT) {
        CI_PUTB (pkt->data, CI_OPC, OPC_CNF);
        pkt->length = LEN_CNF;
        r = ci_send (uptr, pkt);                        /* send confirmation */
        }
    else if (opc == OPC_RETDAT) {
        pkt->length = LEN_DAT;
        r = ci_fmt_receive (uptr, pkt, &ppd);           /* convert to PPD format */
        if (r == SCPE_OK) {
            r = ci_receive_ppd (uptr, &ppd);            /* pass to system */
            if (r == CISE_QERR)
                r = SCPE_OK;
            }
        }
    else
        r = SCPE_IERR;
    }
return r;
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

    case OPC_SNDDAT:
        return ci_send_data (uptr, ppd);
        }
r = ci_fmt_send (uptr, ppd, &pkt);                      /* convert to CI format */
if (r == SCPE_OK)
    r = ci_send (uptr, &pkt);                           /* send CI packet */
return r;
}

t_stat ci_process_entry (UNIT *uptr, t_addr entry)
{
PPD_PKT pkt;
uint8 opc, flags, type, port;
uint32 i, queue;
uint64 word;

pkt.addr = entry;
for (i = 0; i < 20; i++)
    sim_debug (DEBUG_DETAIL, &ci_dev, "   RD: %012llo\n", M[entry++]);
ci_cpy_to (pkt.addr, pkt.data, LEN_HDR);                /* read header */

opc = CI_GETB (pkt.data, PPD_OPC);
port = CI_GETB (pkt.data, PPD_PORT);
flags = CI_GETB (pkt.data, PPD_FLAGS);
if (GET_PATH (pkt.data[PPD_FLAGS]) == PPD_PS1)
    CI_PUTB (pkt.data, PPD_STATUS, 0004);               /* ACK on path B */
else
    CI_PUTB (pkt.data, PPD_STATUS, 0040);               /* ACK on path A */
switch (opc) {
    case OPC_SNDDG:
        pkt.length = LEN_SNDDG;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        word = M[pkt.addr + 4] & 0xFFFFF;
        ci_cpy_to_ind (pkt.addr, &pkt.data[PPD_TEXT], PPD_TEXT + 2, 2);
        if (flags & 0x80)
            ci_cpy_to_hd (pkt.addr + 5, &pkt.data[PPD_TEXT + 2], (word - 2) * 2, 0);
        else
            ci_cpy_to_ind (pkt.addr, &pkt.data[PPD_TEXT + 2], PPD_TEXT + 4, word - 2);
        pkt.length = LEN_SNDDG + word;
        type = PPD_DG;
        break;

    case OPC_SNDMSG:
        pkt.length = LEN_SNDMSG;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        word = M[pkt.addr + 4] & 0xFFFFF;
        ci_cpy_to_ind (pkt.addr, &pkt.data[PPD_TEXT], PPD_TEXT + 2, 2);
        if (flags & 0x80)
            ci_cpy_to_hd (pkt.addr + 5, &pkt.data[PPD_TEXT + 2], (word - 2) * 2, 0);
        else
            ci_cpy_to_ind (pkt.addr, &pkt.data[PPD_TEXT + 2], PPD_TEXT + 4, word - 2);
        pkt.length = LEN_SNDMSG + word;
        type = PPD_MSG;
        break;

    case OPC_REQID:
        pkt.length = LEN_REQID;
        ci_cpy_to (pkt.addr, pkt.data, pkt.length);
        if (ci_check_vc (uptr, port) != SCPE_OK) {      /* no VC to destination node? */
            if (GET_PATH (pkt.data[PPD_FLAGS]) == PPD_PS0)
                pkt.data[PPD_STATUS] = 0304;            /* no response on path A */
            else
                pkt.data[PPD_STATUS] = 0244;            /* no response on path B */
            }
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

    case OPC_SNDDAT:
//    case OPC_SNDMDAT:
        pkt.length = LEN_SNDDAT;
        type = PPD_MSG;
        break;

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
        sim_printf ("Unhandled opcode %o\n", opc);
        break;
    }

ci_send_ppd (uptr, &pkt);                   /* process it */
if ((opc == OPC_RDREG) || (flags & PPD_RSP) || (pkt.data[PPD_STATUS] & 0200)) {
    ci_cpy_from (pkt.addr + 3, &pkt.data[PPD_PORT], pkt.length - 12);
    queue = PCB_RQI;
    }
else if (type == PPD_MSG)
    queue = PCB_MQI;
else
    queue = PCB_DQI;

if (ci_putq (ci_data.pcb + queue, &pkt.addr) == 0) {
    memcpy (&ci_data.ppd, &pkt, sizeof (PPD_PKT));
    ci_data.ppdq = queue;
    sim_activate (uptr, 5000);
    return CISE_QERR;
    }
return SCPE_OK;
}

t_stat ci_port_command (UNIT *uptr)
{
t_addr entry = 0;
t_stat r;

do {
    ci_getq (ci_data.pcb + PCB_C3I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 3: %o\n", entry);
        r = ci_process_entry (uptr, entry);
        if (r == CISE_QERR)
            return SCPE_OK;
        }
} while (entry != 0);
do {
    ci_getq (ci_data.pcb + PCB_C2I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 2: %o\n", entry);
        r = ci_process_entry (uptr, entry);
        if (r == CISE_QERR)
            return SCPE_OK;
        }
} while (entry != 0);
do {
    ci_getq (ci_data.pcb + PCB_C1I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 1: %o\n", entry);
        r = ci_process_entry (uptr, entry);
        if (r == CISE_QERR)
            return SCPE_OK;
        }
} while (entry != 0);
do {
    ci_getq (ci_data.pcb + PCB_C0I, &entry);
    if (entry != 0) {
        sim_debug (DEBUG_DETAIL, &ci_dev, "Got packet from command queue 0: %o\n", entry);
        r = ci_process_entry (uptr, entry);
        if (r == CISE_QERR)
            return SCPE_OK;
        }
} while (entry != 0);
ci_data.csr &= ~CI_CQA;                                 /* all commands processed */
return SCPE_OK;
}

t_stat ci_receive_ppd (UNIT *uptr, PPD_PKT *ppd)
{
t_stat r = SCPE_OK;
uint8 opc = CI_GETB (ppd->data, PPD_OPC);
uint8 flags = CI_GETB (ppd->data, PPD_FLAGS);

switch (opc) {                                          /* determine packet type */
    case OPC_MSGREC:
    case OPC_DATREC:
    case OPC_CNFREC:
    case OPC_CNFRET:
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

    case OPC_REQDATREC:
        return ci_send_data (uptr, ppd);

    default:
        sim_printf ("ci_receive_ppd() - Unhandled opcode 0x%02X\n", opc);
        r = SCPE_IERR;
        break;
    }
if (r != SCPE_OK)
    return r;

if (ppd->addr == 0) {
    memcpy (&ci_data.ppd, ppd, sizeof (PPD_PKT));
    ci_data.ppdq = PCB_RQI;
    sim_activate (uptr, 5000);
    return CISE_QERR;
    }
switch (opc) {
    case OPC_DGREC:
        ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], LEN_SNDDG - 12);
        ppd->length = ppd->length - LEN_SNDDG;
        M[ppd->addr + 4] = ppd->length;
        ci_cpy_from_ind (ppd->addr, &ppd->data[PPD_TEXT], PPD_TEXT + 2, 2);
        if (flags & 0x80)
            ci_cpy_from_hd (ppd->addr + 5, &ppd->data[PPD_TEXT + 2], (ppd->length - 2) * 2, 0);
        else
            ci_cpy_from_ind (ppd->addr, &ppd->data[PPD_TEXT + 2], PPD_TEXT + 4, ppd->length - 2);
        break;

    case OPC_MSGREC:
        ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], LEN_SNDMSG - 12);
        ppd->length = ppd->length - LEN_SNDMSG;
        M[ppd->addr + 4] = ppd->length;
        ci_cpy_from_ind (ppd->addr, &ppd->data[PPD_TEXT], PPD_TEXT + 2, 2);
        if (flags & 0x80)
            ci_cpy_from_hd (ppd->addr + 5, &ppd->data[PPD_TEXT + 2], (ppd->length - 2) * 2, 0);
        else
            ci_cpy_from_ind (ppd->addr, &ppd->data[PPD_TEXT + 2], PPD_TEXT + 4, ppd->length - 2);
        break;

    default:
        ci_cpy_from (ppd->addr + 3, &ppd->data[PPD_PORT], ppd->length - 12);
        break;
    }
for (uint32 i = 0; i < 20; i++)
    sim_debug (DEBUG_DETAIL, &ci_dev, "   WR: %012llo\n", M[ppd->addr + i]);

if (ci_putq (ci_data.pcb + PCB_RQI, &ppd->addr) == 0) {
    memcpy (&ci_data.ppd, ppd, sizeof (PPD_PKT));
    ci_data.ppdq = PCB_RQI;
    sim_activate (uptr, 5000);
    return CISE_QERR;
    }
return SCPE_OK;
}

t_stat ci_cmd_svc (UNIT *uptr)
{
return ci_port_command (&ci_unit[0]);
}

#include <sys/time.h>
uint64 stod = 0;
uint64 sbtim = 0;
uint64 sbftmg = 0;
struct timeval wtime;

t_stat ci_svc (UNIT *uptr)
{
CI_PKT pkt;
PPD_PKT ppd;
t_stat r;
uint8 opc;

#if 0
gettimeofday (&wtime, NULL);
if (M[0247607] != stod) {
    stod = M[0247607];
    sim_printf ("%d.%d TODCLK = %llo\n", wtime.tv_sec, wtime.tv_usec, stod);
    }

if (M[01400142] != sbtim) {
    sbtim = M[01400142];
    sim_printf ("%d.%d SBTIM  = %llo\n", wtime.tv_sec, wtime.tv_usec, sbtim);
    }

if (M[01400143] != sbftmg) {
    sbftmg = M[01400143];
    sim_printf ("%d.%d SBFTMG  = %llo\n", wtime.tv_sec, wtime.tv_usec, sbftmg);
    }
#endif

if (ci_data.ppdq != 0) {
    if (ci_data.ppd.addr == 0) {
        r = ci_receive_ppd (uptr, &ci_data.ppd);
        if (r == CISE_QERR)
            return SCPE_OK;
        }
    else if (ci_putq (ci_data.pcb + ci_data.ppdq, &ci_data.ppd.addr) == 0) {
        sim_activate (uptr, 5000);
        return SCPE_OK;
        }
    ci_data.ppdq = 0;
    }
if (ci_data.csr & CI_CQA)
    sim_activate (&ci_unit[1], 5000);
do {
    r = ci_receive (uptr, &pkt);                        /* try to receive another packet */
    if (r == SCPE_OK) {
        opc = CI_GETB (pkt.data, CI_OPC);
        switch (opc) {                                  /* opcodes handled at CI layer */
            case OPC_IDREQ:
                r = ci_idreq (uptr, &pkt);
                break;

            case OPC_SNTDAT:
            case OPC_RETDAT:
                r = ci_receive_data (uptr, &pkt);
                break;

            default:
                r = ci_fmt_receive (uptr, &pkt, &ppd);      /* convert to PPD format */
                if (r == SCPE_OK)
                    r = ci_receive_ppd (uptr, &ppd);
                break;
            }
        }
} while (r == SCPE_OK);
if (r == CISE_QERR)
    return SCPE_OK;
if ((r != SCPE_OK) && (r != CISE_NOPKT))
    return r;
return ci_port_svc (uptr);
}

t_stat ci_reset (DEVICE *dptr)
{
uint32 i;

for (i = 0; i < CI_MAX_NODES; i++)
    ci_data.vcd[i] = 0;
ci_data.pcb = 0;
ci_data.pia = 0;
ci_data.rar = 0;
ci_data.ebuf = 0;
ci_data.csr = 0;
ci_data.uver[0] = 0;
ci_data.uver[1] = 0;
ci_data.uver[2] = 0;
ci_data.uver[3] = 0;
ci_data.ppd.addr = 0;
ci_data.ppd.length = 0;
ci_data.ppdq = 0;
return ci_port_reset (dptr);
}

t_stat ci_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "CI interface\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
return SCPE_OK;
}

const char *ci_description (DEVICE *dptr)
{
return "KL CI interface";
}
#endif
