/* dec_ctp.h: CI Test Protocol

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
 
#ifndef _DEC_CTP_H_
#define _DEC_CTP_H_

/* Connect data */

#define CTP_CTP         0x00                            /* protocol type */
#define CTP_VERSION     0x03                            /* protocol version */
#define CTP_REVISION    0x00                            /* protocol revision */

/* CTP offsets */

#define CTP_OPCODE      (SCS_APPL + 0x00)               /* opcode */
#define CTP_REFERENCE   (SCS_APPL + 0x01)               /* command refrence */
#define CTP_DELAY       (SCS_APPL + 0x05)               /* delay count */
#define CTP_STATUS      (SCS_APPL + 0x05)               /* return status */
#define CTP_BUFTYPE     (SCS_APPL + 0x05)               /* buffer type */
#define CTP_RESERV5     (SCS_APPL + 0x05)               /* MBZ */
#define CTP_FMASK       (SCS_APPL + 0x06)               /* function mask */
#define CTP_ACTCOUNT    (SCS_APPL + 0x06)               /* actual count */
#define CTP_REPCOUNT    (SCS_APPL + 0x06)               /* repeat count */
#define CTP_RESERV6     (SCS_APPL + 0x06)               /* MBZ */
#define CTP_RESERV7     (SCS_APPL + 0x07)               /* MBZ */
#define CTP_EXTEND      (SCS_APPL + 0x08)
#define  CTP_UNCONDRST  0x00                            /* unconditional reset */
#define  CTP_CONDRST    0x01                            /* conditional reset */
#define  CTP_DEFALTADR  0x00                            /* default start address */
#define  CTP_SPECIFADR  0x01                            /* specific start address */
#define CTP_GENFUNCT    (SCS_APPL + 0x08)               /* generate function */
#define  CTP_GENFILL    0x00                            /* fill buffer with constant */
#define  CTP_GENFBPAIR  0x01                            /* fill buffer with byte pairs */
#define  CTP_GENFNODE   0x02                            /* fill buffer with local port number */
#define  CTP_GENFIMAGE  0x03                            /* fill buffer with image data */
#define CTP_MOVETYPE    (SCS_APPL + 0x08)               /* block transfer type */
#define  CTP_MOVEFROM   0x00                            /* buffer read */
#define  CTP_MOVETO     0x01                            /* buffer write */
#define CTP_GENCONST    (SCS_APPL + 0x09)               /* fill constant */
#define CTP_OTHERNODE   (SCS_APPL + 0x09)               /* remote node number */
#define CTP_RESERV9     (SCS_APPL + 0x09)               /* MBZ */
#define CTP_NOACTFLAG   (SCS_APPL + 0x0A)               /* poller activity */
#define  CTP_NOACTON    0x00                            /* turn activity on */
#define  CTP_NOACTOFF   0x00                            /* turn activity off */
#define CTP_CNTFLG      (SCS_APPL + 0x0A)               /* read counter flag */
#define  CTP_RELCNT     0x00                            /* relative count */
#define  CTP_KEEPCNT    0x01                            /* absolute count */
#define CTP_PKTSIZ      (SCS_APPL + 0x0A)               /* packet size */
#define  CTP_PS512      0x00                            /* 512 bytes */
#define  CTP_PS576      0x01                            /* 576 bytes */
#define CTP_RESERV10    (SCS_APPL + 0x0A)               /* MBZ */
#define CTP_CFGP0STS    (SCS_APPL + 0x0B)               /* path 0 status */
#define CTP_PKTMULT     (SCS_APPL + 0x0B)               /* packet multiple */
#define CTP_RESERV11    (SCS_APPL + 0x0B)               /* MBZ */
#define CTP_CFGP1STS    (SCS_APPL + 0x0C)               /* path 1 status */
#define CTP_STARTADR    (SCS_APPL + 0x0C)               /* start address */
#define CTP_GENLENGTH   (SCS_APPL + 0x0C)               /* generate length */
#define CTP_BUFLENGTH   (SCS_APPL + 0x0C)               /* mapped buffer length */
#define CTP_CNTRP0ACK   (SCS_APPL + 0x0C)               /* path 0 ACKs */
#define CTP_RESERV12    (SCS_APPL + 0x0C)               /* MBZ */
#define CTP_GENDATA     (SCS_APPL + 0x0E)               /* generated data */
#define CTP_IMAGEDATA   (SCS_APPL + 0x0E)               /* image data */
#define CTP_BUFLNAME    (SCS_APPL + 0x10)               /* local buffer name */
#define CTP_CNTRP0NAK   (SCS_APPL + 0x10)               /* path 0 NAKs */
#define CTP_BUFLOFSET   (SCS_APPL + 0x14)               /* local buffer offset */
#define CTP_CNTRP0NRSP  (SCS_APPL + 0x14)               /* path 0 no responses */
#define CTP_RESERV20    (SCS_APPL + 0x14)               /* MBZ */
#define CTP_BUFRNAME    (SCS_APPL + 0x18)               /* remote buffer name */
#define CTP_CNTRP1ACK   (SCS_APPL + 0x18)               /* path 1 ACKs */
#define CTP_BUFROFSET   (SCS_APPL + 0x1C)               /* remote buffer offset */
#define CTP_CNTRP1NAK   (SCS_APPL + 0x1C)               /* path 1 NAKs */
#define CTP_CNTRP1NRSP  (SCS_APPL + 0x20)               /* path 1 no responses */
#define CTP_CNTRDISCDG  (SCS_APPL + 0x24)               /* discarded datagrams */

/* Status returns */

#define CTP_SUCCESS     0x00                            /* success */
#define CTP_CONNTRANS   0xF5                            /* connection in transition state */
#define CTP_NOCONNECT   0xF6                            /* no connection */
#define CTP_NODEUNKNOWN 0xF8                            /* remote node unknown */
#define CTP_NOBUFMAPPED 0xF9                            /* no buffer mapped */
#define CTP_BUFLIMIT    0xFA                            /* buffer map limit exceeded */
#define CTP_FAILURE     0xFB                            /* general failure */
#define CTP_REFUSE      0xFC                            /* connection refused */
#define CTP_NORESOURCE  0xFD                            /* resources exceeded */
#define CTP_NONSENSE    0xFE                            /* invalid request */

/* CTP opcodes */

#define CTP_FUNCTREQ    0x00                            /* function set */
#define CTP_BUFMAPREQ   0x01                            /* buffer map */
#define CTP_BUFUNMREQ   0x02                            /* buffer unmap */
#define CTP_MOVBUFREQ   0x03                            /* buffer move */
#define CTP_GENMSGREQ   0x04                            /* generate message */
#define CTP_GENDGRREQ   0x05                            /* generate datagram */
#define CTP_GENRSTREQ   0x06                            /* generate reset */
#define CTP_GENSTRREQ   0x07                            /* generate start */
#define CTP_NOACTREQ    0x08                            /* start/stop poller */
#define CTP_CONFIGREQ   0x09                            /* configuration */
#define CTP_COUNTSREQ   0x0A                            /* read counters */
#define CTP_CONNECTREQ  0x0B                            /* third party connect */
#define CTP_FINISHREQ   0x0C                            /* listener disconnect */
#define CTP_MBUFMAPREQ  0x0D                            /* maintenance buffer map */
#define CTP_MBUFUNMREQ  0x0E                            /* maintenance buffer unmap */
#define CTP_MOVMBUFREQ  0x0F                            /* maintenance buffer move */
#define CTP_MSTATEREQ   0x10                            /* set maintenance state */

#define CTP_RESPONSE    0x40

#define CTP_FUNCTRSP    (CTP_RESPONSE | CTP_FUNCTREQ)
#define CTP_BUFMAPRSP   (CTP_RESPONSE | CTP_BUFMAPREQ)
#define CTP_BUFUNMRSP   (CTP_RESPONSE | CTP_BUFUNMREQ)
#define CTP_MOVBUFRSP   (CTP_RESPONSE | CTP_MOVBUFREQ)
#define CTP_GENMSGRSP   (CTP_RESPONSE | CTP_GENMSGREQ)
#define CTP_GENDGRRSP   (CTP_RESPONSE | CTP_GENDGRREQ)
#define CTP_GENRSTRSP   (CTP_RESPONSE | CTP_GENRSTREQ)
#define CTP_GENSTRRSP   (CTP_RESPONSE | CTP_GENSTRREQ)
#define CTP_NOACTRSP    (CTP_RESPONSE | CTP_NOACTREQ)
#define CTP_CONFIGRSP   (CTP_RESPONSE | CTP_CONFIGREQ)
#define CTP_COUNTSRSP   (CTP_RESPONSE | CTP_COUNTSREQ)
#define CTP_CONNECTRSP  (CTP_RESPONSE | CTP_CONNECTREQ)
#define CTP_FINISHRSP   (CTP_RESPONSE | CTP_FINISHREQ)
#define CTP_MBUFMAPRSP  (CTP_RESPONSE | CTP_MBUFMAPREQ)
#define CTP_MBUFUNMRSP  (CTP_RESPONSE | CTP_MBUFUNMREQ)
#define CTP_MOVMBUFRSP  (CTP_RESPONSE | CTP_MOVMBUFREQ)
#define CTP_MSTATERSP   (CTP_RESPONSE | CTP_MSTATEREQ)

/* Limits */

#define MAX_BUF_LEN     1024                            /* max read/write buffer length */
#define MAX_BUF_MAP     2                               /* max mapped buffers per connection */

#endif
