/* vax780_ci.c: VAX 11/780 Computer Interconnect adapter

   Copyright (c) 2017-2024, Matt Burke

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

   ci             CI780/CI750 CI adapter
*/

#include "vax_defs.h"
#include "dec_ci.h"
#include "vax_ci.h"

/* CI780 Hardware Registers */

#define CNFGR_OF        0x000                           /* 0x000 - configuration register */
#define PMCSR_OF        0x001                           /* 0x004 - port maintenance control/status */
#define PMCSR_OF1       0x004                           /* 0x010 - same as above */
#define MADR_OF         0x005                           /* 0x014 - maintenance address */
#define MDATR_OF        0x006                           /* 0x018 - maintenance data */

/* CI780 Software Registers */

#define LSTOR_OF        0x200                           /* 0x800 - local store */
#define PPR_MAINT_OF    0x205                           /* 0x814 - port parameter (expected by EVCGD) */
#define PSR_OF          0x240                           /* 0x900 - port status */
#define PQBBR_OF        0x241                           /* 0x904 - port queue block base */
#define PCQ0CR_OF       0x242                           /* 0x908 - port command queue 0 control */
#define PCQ1CR_OF       0x243                           /* 0x90C - port command queue 1 control */
#define PCQ2CR_OF       0x244                           /* 0x910 - port command queue 2 contro */
#define PCQ3CR_OF       0x245                           /* 0x914 - port command queue 3 control  */
#define PSRCR_OF        0x246                           /* 0x918 - port status release control */
#define PECR_OF         0x247                           /* 0x91C - port enable control */
#define PDCR_OF         0x248                           /* 0x920 - port disable control  */
#define PICR_OF         0x249                           /* 0x924 - port initialise control */
#define PDFQCR_OF       0x24A                           /* 0x928 - port datagram free queue control */
#define PMFQCR_OF       0x24B                           /* 0x92C - port message free queue control */
#define PMTCR_OF        0x24C                           /* 0x930 - port maintenance timer control */
#define PMTECR_OF       0x24D                           /* 0x934 - port maintenance timer expiration control */
#define PFAR_OF         0x24E                           /* 0x938 - port failing address */
#define PESR_OF         0x24F                           /* 0x93C - port error status */
#define PPR_OF          0x250                           /* 0x940 - port parameter */
#define VCD_OF          0x300                           /* 0xC00 - virtual circuit descriptors */

/* Configuration Register */

#define CNFGR_CODE      0x38                            /* adapter code */
#define CNFGR_PFD       0x00000100                      /* power fail disable */
#define CNFGR_TD        0x00000200                      /* transmit dead */
#define CNFGR_TFL       0x00000400                      /* transmit fail */
#define CNFGR_CRD       0x00010000                      /* corrected read data */
#define CNFGR_RDS       0x00020000                      /* read data substitute */
#define CNFGR_CTE       0x00040000                      /* command transmit error */
#define CNFGR_RDT       0x00080000                      /* read data timeout */
#define CNFGR_CTT       0x00100000                      /* command transmit timeout */
#define CNFGR_PUP       0x00400000                      /* power up */
#define CNFGR_PDN       0x00800000                      /* power down */
#define CNFGR_TFLT      0x04000000                      /* transmit fault */
#define CNFGR_MTF       0x08000000                      /* multiple transmitter fault */
#define CNFGR_URDF      0x20000000                      /* unexpected read data fault */
#define CNFGR_RSF       0x40000000                      /* write sequence fault */
#define CNFGR_PF        0x80000000                      /* parity fault */

/* Port Maintenance Control/Status Register */

#define PMCSR_MI        0x00000001                      /* maintenance initialise */
#define PMCSR_MTD       0x00000002                      /* maintenance timer disable */
#define PMCSR_MIE       0x00000004                      /* maintenance interrupt enable */
#define PMCSR_MIF       0x00000008                      /* maintenance interrupt flag */
#define PMCSR_WP        0x00000010                      /* wrong parity */
#define PMCSR_RSVD      0x00000020                      /* reserved */
#define PMCSR_PSA       0x00000040                      /* programmable starting addr */
#define PMCSR_UI        0x00000080                      /* uninitialised state */
#define PMCSR_TBPE      0x00000100                      /* transmit buffer parity error */
#define PMCSR_OPE       0x00000200                      /* output parity error */
#define PMCSR_IPE       0x00000400                      /* input parity error */
#define PMCSR_TMPE      0x00000800                      /* transmit multiple parity error */
#define PMCSR_RBPE      0x00001000                      /* receive buffer parity error */
#define PMCSR_LSPE      0x00002000                      /* local store parity error */
#define PMCSR_CSPE      0x00004000                      /* control store parity error */
#define PMCSR_PE        0x00008000                      /* parity error */
#define PMCSR_RD        0x0000FFFE
#define PMCSR_RW        (PMCSR_PSA | PMCSR_WP | PMCSR_MIE | \
                         PMCSR_MTD | PMCSR_RSVD)
#define PMCSR_W1C       (PMCSR_PE | PMCSR_CSPE | PMCSR_LSPE | \
                         PMCSR_RBPE | PMCSR_TMPE | PMCSR_IPE | \
                         PMCSR_OPE | PMCSR_TBPE | PMCSR_MIF)

/* Maintenance Address Register */

#define MADR_ADDR       0x1FFF

/* Port Status Register */

#define PSR_MTE         0x80000000                      /* maintenance error */

/* CI780 Port Parameters */

#define LPORT_TYPE      (ID_DUAL | ID_CI780)            /* local port type */
#define CODE_REV        0x00700040                      /* code revision */
#define FUNC_MASK       0xFFFFFF00                      /* function mask */
#define INT_BUF_LEN     0x3F8                           /* internal buffer length */

uint32 ci_cnfgr;                                        /* configuration reg */
uint32 ci_pmcsr;                                        /* port maintenance csr */
uint32 ci_madr;                                         /* mainteneance addr reg */
uint32 ci_mdatr[8192];                                  /* maintenanace data reg */
uint32 ci_lstor[1024];                                  /* local store */

extern uint32 nexus_req[NEXUS_HLVL];

t_stat ci_reset (DEVICE *dptr);
t_stat ci_rdreg (int32 *val, int32 pa, int32 mode);
t_stat ci_wrreg (int32 val, int32 pa, int32 lnt);
void ci_set_int (void);
void ci_clr_int (void);

/* CI780 adapter data structures

   ci_dev     CI device descriptors
   ci_unit    CI unit
   ci_reg     CI register list
*/

DIB ci_dib = { TR_CI, 0, &ci_rdreg, &ci_wrreg, 0, NVCL (CI) };

UNIT ci_unit = { UDATA (&ci_vax_svc, UNIT_IDLE|UNIT_ATTABLE, 0) };

REG ci_reg[] = {
    { HRDATA (CNFGR, ci_cnfgr, 32) },
    { HRDATA (PMCSR, ci_pmcsr, 32) },
    { HRDATA (MADR, ci_madr, 32) },
    { HRDATA (MDATR, ci_mdatr, 32) },
    { FLDATA (NEXINT, nexus_req[IPL_CI], TR_CI) },
    { NULL }
    };

MTAB ci_mod[] = {
    { MTAB_XTD|MTAB_VDV, TR_CI, "NEXUS", NULL,
      NULL, &show_nexus },
    { MTAB_XTD|MTAB_VDV, 0, "NODE", "NODE",
      &ci_set_node, &ci_show_node },
    { MTAB_XTD|MTAB_VDV, 0, "GROUP", "GROUP",
      &ci_set_group, &ci_show_group },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "LINKS", NULL,
      NULL, &ci_show_links, NULL },
    { 0 }
    };

DEBTAB ci_debug[] = {
    { "REG",   DBG_REG },
    { "WARN",  DBG_WRN },
    { "DIAG",  DBG_DIAG },
    { "CONF",  DBG_CONF },
    { "PPD",   DBG_PPD },
    { "SCS",   DBG_SCS },
    { "BLK",   DBG_BLK },
    { "LCMD",  DBG_LCMD },
    { "CONN",  DBG_CONN },
    { "TRACE", DBG_TRC },
    { "PKT",   DBG_PKT },
    { 0 }
    };

DEVICE ci_dev = {
    "CI", &ci_unit, ci_reg, ci_mod,
    1, 0, 0, 0, 0, 0,
    NULL, NULL, &ci_reset,
    NULL, &ci_attach, &ci_detach,
    &ci_dib, DEV_NEXUS | DEV_DEBUG | DEV_DISABLE | DEV_DIS | DEV_CI, 0,
    ci_debug, 0, 0
    };

REGMAP ci_regmap[] = {
    { PPR_MAINT_OF, CI_PPR },
    { PSR_OF, CI_PSR },
    { PQBBR_OF, CI_PQBBR },
    { PCQ0CR_OF, CI_PCQ0CR },
    { PCQ1CR_OF, CI_PCQ1CR },
    { PCQ2CR_OF, CI_PCQ2CR },
    { PCQ3CR_OF, CI_PCQ3CR },
    { PSRCR_OF, CI_PSRCR },
    { PECR_OF, CI_PECR },
    { PDCR_OF, CI_PDCR },
    { PICR_OF, CI_PICR },
    { PDFQCR_OF, CI_PDFQCR },
    { PMFQCR_OF, CI_PMFQCR },
    { PMTCR_OF, CI_PMTCR },
    { PMTCR_OF, CI_PMTCR },
    { PMTECR_OF, CI_PMTECR },
    { PFAR_OF, CI_PFAR },
    { PESR_OF, CI_PESR },
    { PPR_OF, CI_PPR },
    { 0, 0 }
    };

CI_PORT ci_port = {
    LPORT_TYPE,
    CODE_REV,
    FUNC_MASK,
    INT_BUF_LEN,
    &ci_set_int,
    &ci_clr_int
    };

/* Read CI780 adapter register */

t_stat ci_rdreg (int32 *val, int32 pa, int32 lnt)
{
UNIT *uptr = &ci_unit;
REGMAP *p;
int32 ofs;

ofs = NEXUS_GETOFS (pa);                                /* get offset */
if (ci_pmcsr & PMCSR_PSA) {                             /* microcode running? */
    for (p = &ci_regmap[0]; p->offset != 0; p++) {      /* check for port register */
        if (p->offset == ofs)                           /* mapped? */
            return ci_vax_rd (uptr, val, p->rg, lnt);
        }
    }
if (ofs >= VCD_OF) {                                    /* virtual circuit descriptors */
    ofs = ofs & 0xFF;                                   /* get node number */
    *val = ci_vcd[ofs] & 0xFFFF;
    sim_debug (DBG_REG, &ci_dev, "VCD[%d] rd: %08X\n", ofs, *val);
    return SCPE_OK;
    }
if (ofs >= LSTOR_OF) {                                  /* local store */
    *val = ci_lstor[(ofs - LSTOR_OF)];
    sim_debug (DBG_REG, &ci_dev, "LSTOR[%03X] rd: %08X\n", ofs, *val);
    return SCPE_OK;
    }
switch (ofs) {                                          /* CI780 hardware register */

    case CNFGR_OF:
        *val = ci_cnfgr | CNFGR_CODE;
        sim_debug (DBG_REG, &ci_dev, "CNFGR rd: %08X\n", *val);
        break;

    case PMCSR_OF:
    case PMCSR_OF1:
        *val = (ci_pmcsr & PMCSR_RD);
        if (uptr->ci_state <= PORT_UIM)                 /* uninitialised? */
            *val |= PMCSR_UI;
        if ((ci_pmcsr & 0x7F00) > 0)                    /* any error bits set? */
            *val |= PMCSR_PE;                           /* yes, set parity error */
        sim_debug (DBG_REG, &ci_dev, "PMCSR rd: %08X\n", *val);
        break;

    case MADR_OF:
        *val = (ci_madr & MADR_ADDR);
        sim_debug (DBG_REG, &ci_dev, "MADR rd: %08X\n", *val);
        break;

    case MDATR_OF:
        *val = ci_mdatr[ci_madr];
        if (ci_pmcsr & PMCSR_PSA) {                     /* microcode running? */
            if (ci_madr == 0xBFF)                       /* WCS revision */
                *val = 0x04000000;
            else if (ci_madr == 0x3FA)                  /* ROM revision */
                *val = 0x03000000;
            else if (ci_madr == 0x3FB)                  /* control store ROM */
                *val = 0x44332211;
            else if (ci_madr == 0x13FB)
                *val = 0x00006655;
            else if (ci_madr == 0x3FC)
                *val = 0x00000000;
            else if (ci_madr == 0x13FC)
                *val = 0x00000000;
            else if (ci_madr == 0x3FD)
                *val = 0x0004080C;
            else if (ci_madr == 0x13FD)
                *val = 0x0000080C;
            else if (ci_madr == 0x3FE)
                *val = 0x0305090C;
            else if (ci_madr == 0x13FE)
                *val = 0x00000A0F;
            else if (ci_madr == 0x3FF)
                *val = 0x03010100;
            else if (ci_madr == 0x13FF)
                *val = 0x00000203;
            }
        sim_debug (DBG_REG, &ci_dev, "MDATR rd: %08X\n", *val);
        break;

    default:
        *val = 0;
        sim_debug (DBG_WRN, &ci_dev, "defaulting on read: %X\n", ofs);
        }

return SCPE_OK;
}

/* Write CI780 adapter register */

t_stat ci_wrreg (int32 val, int32 pa, int32 lnt)
{
UNIT *uptr = &ci_unit;
REGMAP *p;
int32 ofs;

#if !defined (VAX_750)
if ((pa & 3) || (lnt != L_LONG)) {                      /* unaligned or not lw? */
    sim_printf (">>CI: invalid adapter write mask, pa = 0x%X, lnt = %d\r\n", pa, lnt);
    sbi_set_errcnf ();                                  /* err confirmation */
    return SCPE_OK;
    }
#endif
ofs = NEXUS_GETOFS (pa);                                /* get offset */
if (ci_pmcsr & PMCSR_PSA) {                             /* microcode running? */
    for (p = &ci_regmap[0]; p->offset != 0; p++) {      /* check for port register */
        if (p->offset == ofs)                           /* mapped? */
            return ci_vax_wr (uptr, val, p->rg, lnt);
        }
    }
if (ofs >= VCD_OF) {                                    /* virtual circuit descriptors */
    ofs = ofs & 0xFF;                                   /* get node number */
    ci_vcd[ofs] = val & 0xFFFF;
    sim_debug (DBG_REG, &ci_dev, "VCD[%d] wr: %08X\n", ofs, val);
    return SCPE_OK;
    }
if (ofs >= LSTOR_OF) {                                  /* local store */
    sim_debug (DBG_REG, &ci_dev, "LSTOR[%03X] wr: %08X\n", ofs, val);
    if (ofs == PSR_OF)
        val = val & 0xFFFF;                             /* expected by EVCGA */
    ci_lstor[(ofs - LSTOR_OF)] = val;
    return SCPE_OK;
    }
switch (ofs) {                                          /* CI780 hardware register */

    case CNFGR_OF:
        sim_debug (DBG_REG, &ci_dev, "CNFGR wr: %08X\n", val);
        ci_cnfgr = val & CNFGR_PFD;
        break;

    case PMCSR_OF:
    case PMCSR_OF1:
        sim_debug (DBG_REG, &ci_dev, "PMCSR wr: %08X\n", val);
        if (val & PMCSR_MI) {                           /* Maintenance Initialise */
            ci_reset (&ci_dev);
            break;
            }
        ci_pmcsr &= ~(val & PMCSR_W1C);                 /* Clear W1C bits */
        ci_pmcsr = (ci_pmcsr & ~PMCSR_RW) | (val & PMCSR_RW); /* Set RW bits */

        if (val & PMCSR_MIF)                            /* interrupt W1C */
            ci_clr_int();
        break;

    case MADR_OF:
        sim_debug (DBG_REG, &ci_dev, "MADR wr: %08X\n", val);
        ci_madr = (val & MADR_ADDR);
        break;

    case MDATR_OF:
        sim_debug (DBG_REG, &ci_dev, "MDATR wr: %08X\n", val);
        ci_mdatr[ci_madr] = val;
        break;

    default:
        sim_debug (DBG_WRN, &ci_dev, "defaulting on write: %X\n", ofs);
        }
return SCPE_OK;
}

/* Set CI780 interrupt */

void ci_set_int ()
{
ci_pmcsr |= PMCSR_MIF;
if (ci_pmcsr & PMCSR_MIE)
    SET_NEXUS_INT (CI);
return;
}

/* Clear CI780 interrupt */

void ci_clr_int ()
{
ci_pmcsr &= ~PMCSR_MIF;
CLR_NEXUS_INT (CI);
return;
}

/* Reset CI780 adapter */

t_stat ci_reset (DEVICE *dptr)
{
REGMAP *p;
int32 val, i;
t_stat r;

ci_cnfgr = 0;
ci_pmcsr = 0;
ci_madr = 0;
for (p = &ci_regmap[0]; p->offset != 0; p++) {          /* cache port registers in local store */
    if (p->offset >= LSTOR_OF) {
        r = ci_vax_rd (&ci_unit, &val, p->rg, L_LONG);  /* read port register */
        if (r == SCPE_OK)                               /* success? */
            ci_lstor[p->offset - LSTOR_OF] = val;       /* cache value */
        }
    }
for (i = 0; i < 8192; i++)
    ci_mdatr[i] = 0;
ci_vax_reset (dptr, &ci_port);
return SCPE_OK;
}
