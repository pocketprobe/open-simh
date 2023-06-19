/* hsc_kg.h: HSC common K requestor functions

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

#ifndef _HSC_KG_H
#define _HSC_KG_H   0

/* status codes */

#define K_PIO           0000
#define K_CI            0001                            /* K.ci host interface */
#define K_CI2           0010                            /* K.ci2 host interface - no microcode loaded */
#define K_CI2_MICRO     0211                            /* K.ci2 host interface - microcode loaded */
#define K_SDI           0002                            /* K.sdi disk data channel */
#define K_STI           0203                            /* K.sti tape data channel */
#define K_SI4           0004                            /* K.si 4-port data channel - no microcode loaded */
#define K_SI4_DISK      0205                            /* K.si 4-port data channel - disk microcode loaded */
#define K_SI4_TAPE      0206                            /* K.si 4-port data channel - tape microcode loaded */
#define K_SI8           0212                            /* K.si 8-port data channel - no microcode loaded */
#define K_SI8_MICRO     0007                            /* K.si 8-port data channel - microcode loaded */
#define K_SCSI          0015                            /* K.scsi data channel module */
#define K_NONE          0377                            /* No requestor present */

/* actions */

#define K_NOACTION      0000
#define K_RUN           0001
#define K_HALT          0002

#define GETB(off)       RdxMemB (CM, off)
#define GETW(off)       RdxMemW (CM, off)
#define GETL(off)       ((GETW (off + 2) << 16) | GETW (off))
#define PUTB(off, val)  WrxMemB (CM, off, val)
#define PUTW(off, val)  WrxMemW (CM, off, val)
#define PUTL(off, val)  PUTW (off + 2, (val >> 16)); PUTW (off, (val & 0177777))
#define GETP(off)       ((GETW (off) & 0077776) ? (GETW (off) << 2) : GETW (off))
#define PUTP(off, val)  PUTW (off, (val >> 2))

#define GETDB(off)      RdxMemB (DM, off)
#define GETDW(off)      RdxMemW (DM, off)
#define PUTDB(off, val) WrxMemB (DM, off, val)
#define PUTDW(off, val) WrxMemW (DM, off, val)

#define Q_LOCKED(q)     (GETW (q) == 0100000)
#define Q_EMPTY(q)      (GETW (q) == 0)

/* transmission queue */

#define Q_SRVC          0004                            /* service queue pointer */
#define Q_PROC          0002
#define Q_FLGS          0001
#define Q_HEAD          0000                            /* resource queue head */
#define Q_TAIL          0002                            /* resource queue tail */
#define Q_WTHD          0004                            /* waiter queue head */
#define Q_WTTL          0006                            /* waiter queue tail */

/* counter queue */

#define C_SRVC          0000                            /* service queue pointer */
#define C_CNT           0002                            /* counter */

/* queue flags */

#define FLGS_IQ         0100                            /* interrupt queue */
#define FLGS_SVC        0200                            /* service pending */

/* route vector */

#define V_FLGS          0000                            /* control flags */
#define  V_FB           0001                            /* free buffers */
#define  V_DC           0002                            /* decrement counter */
#define  V_NC           0010                            /* prevent down count in DRAT */
#define V_OPCD          0001                            /* opcode */
#define  V_SD           0001                            /* send data to host */
#define  V_RD           0002                            /* request data from host */
#define  V_DR           0027                            /* disk read */
#define  V_DW           0245                            /* disk write */
#define V_SUCC          0002                            /* success destination address */
#define V_CTX           0004                            /* context */
#define V_AERR          0006                            /* error destination address */

/* K initialisation structure */

#define KI_C1           0000                            /* check value 1 - 125252 */
#define KI_C2           0002                            /* check value 2 - 052525 */
#define KI_KIA          0004                            /* K init address */
#define KI_CHK          0006                            /* check value */

/* K generic control area */

#define KG_TLK          0040
#define KG_SLT          0042
#define KG_TYP          0043
#define KG_VRS          0044                            /* microcode version */
#define KG_FCB          0046                            /* free control memory buffers */
#define KG_FBU          0050                            /* free data memory buffers */
#define KG_BWS          0052
#define KG_TMR          0054
#define KG_DMP          0056
#define KG_DCA          0060
#define KG_TST          0062
#define KG_FRS          0064
#define KG_LAS          0066
#define KG_MDL          0070
#define KG_MTY          0070
#define KG_PFL          0072
#define KG_RSL          0074                            /* microdiagnostic result */
#define KG_LTR          0076
#define KG_PSA          0100
#define KG_PSB          0102
#define KG_DSA          0104
#define KG_MC0          0130
#define KG_KSA          0140

/* drive state area */

#define KSA_SRV         0000                            /* service queue pointer */
#define KSA_STA         0003                            /* state */
#define KSA_DHD         0004                            /* DCB queue head */
#define KSA_DTL         0006                            /* DCB queue tail */
#define KSA_CSP         0010
#define KSA_SFW         0012                            /* software context */
#define KSA_FLG         0014
#define KSA_CKS         0014
#define KSA_IND         0016                            /* drive state area lock */
#define KSA_LEN         0020                            /* size of drive state area */

/* data buffer */

#define B_OERR          000000                          /* original error bits */
#define B_LLBN          000002                          /* LBN (low) */
#define B_HLBN          000004                          /* LBN (high) */
#define B_EFLG          000006                          /* error recovery flags */
#define B_ECCN          000007                          /* ECC symbols corrected */
#define B_LARC          000010                          /* error retry count A */
#define B_LBRC          000011                          /* error retry count B */
#define B_LCRC          000012                          /* error retry count C */
#define B_ERCL          000013                          /* error recovery command */
#define B_CMP           000014
#define B_HDRS          000020                          /* sector headers */
#define B_DATA          000040                          /* sector data */
#define B_6EDC          001040                          /* EDC (16-bit) */
#define B_6ECC          001042                          /* ECC (16-bit) */
#define B_6EWC          001072
#define B_6PED          001074

/* fragment request block */

#define F_LINK          0000                            /* link to next */
#define F_PCTR          0002                            /* parent counter pointer */
#define F_RTE           0004                            /* route pointer */
#define F_MOD           0006
#define F_CTRL          0010
#define F_OFF           0012
#define F_LLBN          0016                            /* low LBN */
#define F_RT1           0020                            /* route 1 */
#define F_HLBN          0022                            /* high LBN */
#define F_SCT           0024                            /* sector */
#define F_RT2           0024                            /* route 2 */
#define F_TRK           0026                            /* track */
#define F_RT3           0030                            /* route 3 */
#define F_GRP           0030                            /* group */
#define F_RSB           0032
#define F_SCNT          0034                            /* sector count */
#define F_SRTE          0036                            /* saved route pointer */
#define F_BFHD          0040                            /* buffer queue head */
#define F_BFTL          0042                            /* buffer queue tail */
#define F_RT4           0044                            /* route 4 */
#define F_SLLB          0050                            /* saved low LBN */
#define F_SHLB          0052                            /* saved high LBN */
#define F_IQA           0054
#define F_DSTR          0074
#define F_ESUM          0076
#define F_CRUN          0110
#define F_CFLG          0111

/* deallocation context? */

#define DX_CTR          0000                            /* outstanding operation counter */
#define DX_SMQ          0004                            /* server message queue */
#define DX_SHQ          0010
#define DX_DU           0014                            /* disk context structure pointer */
#define DX_NDS          0016
#define DX_DSL          0020
#define DX_STM          0022
#define DX_TEF          0024

/* debugging bitmaps */

#define KG_DBG_TRC      0x1000                          /* trace routine calls */
#define KG_DBG_STRUCT   0x2000                          /* dump structures */

/* structure definitions */

#define ST_INT          0                               /* integer */
#define ST_STR          1                               /* string */
#define ST_LNK          2                               /* link to another structure */

typedef struct stdef {
    char *name;                                         /* element name */
    uint16 off;                                         /* offset */
    uint16 len;                                         /* length */
    uint8 typ;                                          /* type */
    struct stdef *lnk;                                  /* link to embedded structure */
} STDEF;

extern uint32 kg_init (t_bool *run);
extern t_stat kg_show_addr (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern t_stat kg_show_req (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
extern void kg_print_struct (FILE *st, UNIT *uptr, uint32 kg, STDEF *stc);
extern void kg_dec_ctr (UNIT *uptr, uint32 ctr);
extern void kg_route_frb (UNIT *uptr, uint32 kg, uint32 frb, t_bool sts);
extern uint32 kg_receive (uint32 q);
extern void kg_send (uint32 q, uint32 itm);

extern int16 inst_pc;
extern STDEF kg_stc[];

#endif
