/* vax_ci.h: Computer Interconnect adapter (VAX Common)

   Copyright (c) 2019, Matt Burke

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

#ifndef _VAX_CI_H_
#define _VAX_CI_H_

/* CI Registers */

#define CI_PSR          0                               /* port status */
#define CI_PQBBR        1                               /* port queue block base */
#define CI_PCQ0CR       2                               /* port cmd queue 0 control */
#define CI_PCQ1CR       3                               /* port cmd queue 1 control */
#define CI_PCQ2CR       4                               /* port cmd queue 2 control */
#define CI_PCQ3CR       5                               /* port cmd queue 3 control */
#define CI_PSRCR        6                               /* FIXME - description */
#define CI_PECR         7                               /* FIXME - description */
#define CI_PDCR         8                               /* FIXME - description */
#define CI_PICR         9                               /* FIXME - description */
#define CI_PDFQCR       10                              /* port datagram free queue control */
#define CI_PMFQCR       11                              /* port message free queue control */
#define CI_PMTCR        12                              /* FIXME - description */
#define CI_PMTECR       13                              /* FIXME - description */
#define CI_PFAR         14                              /* port failing address */
#define CI_PESR         15                              /* FIXME - description */
#define CI_PPR          16                              /* port parameter */

/* Datastructure types */

#define DYN_CIDG        0x3B                            /* CI datagram */
#define DYN_CIMSG       0x3C                            /* CI message */

/* PPD Offsets */

#define PPD_FLINK       0x00                            /* forward link */
#define PPD_BLINK       0x04                            /* backward link */
#define PPD_SIZE        0x08                            /* structure size */
#define  PPD_DG         0                               /* datagram */
#define  PPD_MSG        1                               /* message */
#define PPD_TYPE        0x0A                            /* structure type */
#define PPD_SWFLAG      0x0B                            /* software flags */
#define PPD_PORT        0x0C                            /* src/dest port */
#define PPD_STATUS      0x0D                            /* status */
#define PPD_OPC         0x0E                            /* opcode */
#define PPD_FLAGS       0x0F                            /* flags */

#define PPD_HDR         0x10                            /* PPD header size */

#define PPD_LENGTH      0x10                            /* message length */
#define PPD_TEXT        0x12                            /* datagram/message text */

#define PPD_VCMMSK      0x10                            /* VCD modify mask */
#define PPD_VCMVAL      0x14                            /* VCD modify value */
#define PPD_VCPVAL      0x18                            /* VCD previous value */

#define PPD_XCTID       0x10                            /* transaction ID */
#define PPD_LCONID      0x10                            /* local connection id */
#define PPD_RSPID       0x14                            /* local response id */
#define PPD_XFRSZ       0x18                            /* transfer size */
#define PPD_SBNAM       0x1C                            /* send buffer name */
#define PPD_SBOFF       0x20                            /* send buffer offset */
#define PPD_RBNAM       0x24                            /* receive buffer name */
#define PPD_RBOFF       0x28                            /* receive buffer offset */
#define PPD_DATA        0x2C                            /* block data */

/* Transmit Opcodes */

#define OPC_SNDDG       0x01                            /* send datagram */
#define OPC_SNDMSG      0x02                            /* send message */
#define OPC_RETCNF      0x03                            /* confirm return */
#define OPC_REQID       0x05                            /* request ID */
#define OPC_SNDRST      0x06                            /* send reset */
#define OPC_SNDSTRT     0x07                            /* send start */
#define OPC_REQDAT      0x08                            /* request data (at priority 0) */
#define OPC_REQDAT1     0x09                            /* request data (at priority 1) */
#define OPC_REQDAT2     0x0A                            /* request data (at priority 2) */
#define OPC_RETID       0x0B                            /* send ID */
#define OPC_SNDLB       0x0D                            /* send loopback */
#define OPC_REQMDAT     0x0E                            /* request maintenance data */
#define OPC_SNDDAT      0x10                            /* send data */
#define OPC_RETDAT      0x11                            /* return data */
#define OPC_SNDMDAT     0x12                            /* send maintenance data */

/* Local Opcodes */

#define OPC_INVTC       0x18                            /* invalidate translation cache */
#define OPC_SETCKT      0x19                            /* set circuit */
#define OPC_RDCNT       0x1A                            /* read counters */

/* Packet Lengths */

#define LEN_SETCKT      0x28
#define LEN_RDCNT       0x2C

/* Receive Opcodes */

#define OPC_M_RECV      0x20                            /* opcode receive flag */
#define OPC_DGREC       (OPC_M_RECV | OPC_SNDDG)        /* datagram received */
#define OPC_MSGREC      (OPC_M_RECV | OPC_SNDMSG)       /* message received */
#define OPC_CNFREC      (OPC_M_RECV | OPC_RETCNF)       /* confirm received */
#define OPC_REQREC      (OPC_M_RECV | OPC_REQID)        /* request ID recevied */
#define OPC_RSTREC      (OPC_M_RECV | OPC_SNDRST)       /* reset received */
#define OPC_STRTREC     (OPC_M_RECV | OPC_SNDSTRT)      /* start received */
#define OPC_REQDATREC   (OPC_M_RECV | OPC_REQDAT)       /* request data received */
#define OPC_MCNFREC     0x29                            /* maintenance confirm received */
#define OPC_IDREC       (OPC_M_RECV | OPC_RETID)        /* ID received */
#define OPC_LBREC       (OPC_M_RECV | OPC_SNDLB)        /* loopback received */
#define OPC_SNDDATREC   (OPC_M_RECV | OPC_SNDDAT)       /* send data received */
#define OPC_DATREC      (OPC_M_RECV | OPC_RETDAT)       /* data recieved */
#define OPC_MDATREC     0x33                            /* maintenance data recieved */

/* PPD Status */

#define STS_OK          0x00                            /* OK status */
#define STS_ERR         0x01                            /* error flag */
#define STS_P0ACK       0x00                            /* success or not used */
#define STS_P0NAK       0x02                            /* negative ack */
#define STS_P0RSP       0x04                            /* no response */
#define STS_P0ARB       0x06                            /* arbitration timeout */
#define STS_P1NAK       0x08                            /* negative ack */
#define STS_P1RSP       0x10                            /* no response */
#define STS_P1ARB       0x18                            /* arbitration timeout */
#define STS_VCC         (STS_ERR | 0x20)                /* VC closed */
#define STS_INVBN       (STS_ERR | 0x40)                /* invalid buffer name */
#define STS_BLV         (STS_ERR | 0x60)                /* buffer length violation */
#define STS_ACCV        (STS_ERR | 0x80)                /* access violation */
#define STS_NP          (STS_ERR | 0xA0)                /* no path */
#define STS_BMSE        (STS_ERR | 0xC0)                /* buffer memory system error */
#define STS_OTHER       (STS_ERR | 0xE0)                /* other (see below) */
#define STS_PSV         (STS_OTHER)                     /* pkt size violation */
#define STS_URP         (STS_OTHER | 0x02)              /* unrecognized pkt */
#define STS_INVDP       (STS_OTHER | 0x04)              /* invalid destination port */
#define STS_URC         (STS_OTHER | 0x06)              /* unrecognized command */
#define STS_ABO         (STS_OTHER | 0x08)              /* abort (port disabled) */
#define STS_INVSN       (STS_OTHER | 0x0C)              /* invalid sequence number */
#define STS_UNIMCMD     (STS_OTHER | 0x0E)              /* Unimplemented command (BVP) */
#define STS_INVFS       (STS_OTHER | 0x10)              /* Invalid flags or status (BVP) */

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
#define PPD_FORCE       0x80                            /* force reset */
#define PPD_DSTART      0x80                            /* default start addr */
#define PPD_EXTCNT      0x80                            /* extended counters */

typedef struct {                                        /* register mappings */
    uint32      offset;                                 /* memory offset */
    uint32      rg;                                     /* register index */
} REGMAP;

typedef struct {
    uint32 addr;                                        /* associated memory address */
    size_t length;                                      /* packet length */
    uint8 data[CI_MAXFR];                               /* packet data */
} PPD_PKT;

t_stat ci_vax_rd (UNIT *uptr, int32 *val, int32 rg, int32 lnt);
t_stat ci_vax_wr (UNIT *uptr, int32 val, int32 rg, int32 lnt);
t_stat ci_vax_svc (UNIT *uptr);
t_stat ci_vax_reset (DEVICE *dptr);

#endif
