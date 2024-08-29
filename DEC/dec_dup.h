/* dec_dup.h: Diagnostics & Utilities Protocol

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

#ifndef _DEC_DUP_H_
#define _DEC_DUP_H_

/* DUP offsets */

#define DUP_CMDREF      (SCS_APPL + 0x00)               /* command refrence */
#define DUP_UNUSED      (SCS_APPL + 0x04)

/* Command */

#define DUP_OPCODE      (SCS_APPL + 0x08)               /* opcode */
#define  DUP_GET_STAT   0x01                            /* get DUST status */
#define  DUP_EXE_SUPLD  0x02                            /* execute supplied program */
#define  DUP_EXE_LOCAL  0x03                            /* execute local program */
#define  DUP_SEND_DATA  0x04                            /* send data */
#define  DUP_RECV_DATA  0x05                            /* receive data */
#define  DUP_ABORT      0x06                            /* abort program */
#define  DUP_SEND_IMED  0x07                            /* send data immediate */
#define  DUP_ENDFLAG    0x80                            /* end code flag */
#define DUP_RESERVED1   (SCS_APPL + 0x09)
#define DUP_MODIFIER    (SCS_APPL + 0x0A)               /* command modifiers */
#define  DUP_STNDALONE  0x01                            /* allow standalone */
#define  DUP_TOBECONT   0x08                            /* to be continuted */

/* End message */

#define DUP_ENDCODE     (SCS_APPL + 0x08)               /* end code */
#define DUP_RESERVED2   (SCS_APPL + 0x09)
#define DUP_STATUS      (SCS_APPL + 0x0A)               /* end status */
#define  DUP_SUCCESS    0x0                             /* successful */
#define  DUP_INVCMD     0x1                             /* invalid command */
#define  DUP_NOR_AVIL   0x2                             /* no region available */
#define  DUP_NOR_SUIT   0x3                             /* no region suitable */
#define  DUP_PGM_UNKN   0x4                             /* program unknown */
#define  DUP_LOAD_FAIL  0x5                             /* load failure */
#define  DUP_PGM_STAL   0x6                             /* program is standalone */
#define  DUP_HOST_BUF_ACC 0x9                           /* host buffer access error */

/* Get DUST status end message */

#define DUP_PGM_EXT     (SCS_APPL + 0x0C)               /* program extension */
#define DUP_FLAGS       (SCS_APPL + 0x0F)               /* DUST flags */
#define  DUP_SLSERVER   0x01                            /* controller supports only 1 concurrent server */
#define  DUP_LCL_MEDA   0x02                            /* controller has local storage media */
#define  DUP_NOXSUPLD   0x04                            /* controller doesn't support execute supplied program */
#define  DUP_ACTIVE     0x08                            /* controller is active */
#define  DUP_DO_SDI     0x10                            /* controller supports send data immediate */
#define DUP_PROGRESS    (SCS_APPL + 0x10)               /* progress indicator */
#define DUP_TIMEOUT     (SCS_APPL + 0x14)               /* timeout interval for execute local program */
#define DUP_ABORT_TMO   (SCS_APPL + 0x16)               /* timeout interval for abort */

/* Execute supplied program command */

#define DUP_BYTCNT      (SCS_APPL + 0x0C)               /* initial segment byte count */
#define DUP_INTHNDL     (SCS_APPL + 0x10)               /* initial segment buffer handle */
#define DUP_OVRHNDL     (SCS_APPL + 0x1C)               /* overlay segment buffer handle */

/* Execute local program command */

#define DUP_PGM_NAME    (SCS_APPL + 0x0C)               /* program name */

/* Execute local program end */

#define DUP_VERSION     (SCS_APPL + 0x0C)               /* program version number */
#define DUP_LCL_TMOUT   (SCS_APPL + 0x0E)               /* timeout interval for local program */
#define DUP_LCL_FLAGS   (SCS_APPL + 0x0F)               /* flags for local program */
#define DUP_PGF_STALN   0x01                            /* program is standalone */
#define DUP_PGF_OVERLY  0x02                            /* program requires overlays */
#define DUP_PGF_OVWRT   0x04                            /* overlay buffer must be writable */
#define DUP_PGF_DUPDLG  0x08                            /* program uses DUP send/receive data dialogue */
#define DUP_PFG_PASTR   0x10                            /* pass-through mode */

/* Send and receive data commands/endmessages */

//#define DUP_BYTCNT
#define DUP_DATAHNDL    (SCS_APPL + 0x10)               /* data buffer handle */

/* Send data immediate command */

#define DUP_SDI_BYTCNT  (SCS_APPL + 0x0C)               /* word byte count */
#define DUP_SDI_DATA    (SCS_APPL + 0x0E)               /* message area */

#define DUP_SDI_MAX_DATA  22                            /* maimum amount of data */

/* Standard DUP dialogue */

#define DUP_DLG_RTY     (DUP_SDI_DATA + 0x0)            /* reference and type */
#define DUP_DLG_V_REF   0                               /* message number */
#define DUP_DLG_M_REF   0xFFF
#define DUP_DLG_V_TYPE  12                              /* message type */
#define DUP_DLG_M_TYPE  0xF
#define  DUP_DLG_QUE     0                              /* question */
#define  DUP_DLG_DQUE    1                              /* default question */
#define  DUP_DLG_INF     2                              /* information */
#define  DUP_DLG_TRM     3                              /* termination */
#define  DUP_DLG_FTL     4                              /* fatal error */
#define  DUP_DLG_SPE     5                              /* special */
#define DUP_DLG_TEXT    (DUP_SDI_DATA + 0x2)            /* message text */

/* Packet lengths */

#define LEN_EXE_SUPLD   (DUP_OVRHNDL + 0xC)
#define LEN_SEND_RECV   (DUP_DATAHNDL + 0xC)

/* Packet lengths - end message */

#define LEN_GET_STAT    (DUP_ABORT_TMO + 0x1)           /* get status */
#define LEN_EXE_LOCAL   (DUP_LCL_FLAGS + 0x1)           /* execute local */
#define LEN_STATUS      (DUP_STATUS + 0x2)              /* status only */

/* Constants */

#define DUP_STAT_TIM    3                               /* get DUST status */
#define DUP_EXES_TIM    30                              /* execute supplied program */
#define DUP_ABRT_TIM    10                              /* abort program */

#endif
