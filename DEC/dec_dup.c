/* dec_dup.c: Diagnostics & Utilities Protocol

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

#include <sys/types.h>
#include <unistd.h>
#include "sim_defs.h"
#include "dec_ci.h"
#include "dec_ppd.h"
#include "dec_scs.h"
#include "dec_sca.h"
#include "dec_dup.h"

/* SYSAP - MSCP$DUP */

const char* dup_types[] = {                             /* DUP debug definitions */
    "(invalid)",
    "GET_STAT",
    "EXE_SUPLD",
    "EXE_LOCAL",
    "SEND_DATA",
    "RECV_DATA",
    "ABORT",
    "SEND_IMED",
    };

static CTAB dup_cmds[] = {
    { "EXAMINE",  &exdep_cmd,    EX_E },
    { "DEPOSIT",  &exdep_cmd,    EX_D },
    { "EVALUATE", &eval_cmd,        0 },
    { "ATTACH",   &attach_cmd,      0 },
    { "DETACH",   &detach_cmd,      0 },
    { "ASSIGN",   &assign_cmd,      0 },
    { "DEASSIGN", &deassign_cmd,    0 },
    { "PWD",      &pwd_cmd,         0 },
    { "SAVE",     &save_cmd,        0 },
    { "DIR",      &dir_cmd,         0 },
    { "LS",       &dir_cmd,         0 },
    { "SET",      &set_cmd,         0 },
    { "SHOW",     &show_cmd,        0 },
    { NULL,       NULL }
    };

t_stat dup_svc (UNIT *uptr);

char dup_buf[4*CBUFSIZE];
uint32 dup_buf_cnt = 0;
uint32 dup_buf_off = 0;
t_bool dup_active = FALSE;
char dup_log[PATH_MAX+1];
t_bool dup_log_temp = FALSE;
t_offset dup_log_start = 0;                      /* Log File saved position */
t_bool dup_read_pending = FALSE;

UNIT dup_unit = {
    UDATA (&dup_svc, UNIT_IDLE, 0)
    };

void dup_debug_pkt (UNIT *uptr, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, DUP_OPCODE);
uint8 rsp = opcode & DUP_ENDFLAG;
opcode = opcode & ~DUP_ENDFLAG;

if (opcode <= DUP_SEND_IMED) {
    if (rsp)
        sim_debug_unit (DBG_SCS, uptr, "==> %s\n", dup_types[opcode]);
    else
        sim_debug_unit (DBG_SCS, uptr, "<== %s\n", dup_types[opcode]);
    }
else
    sim_debug_unit (DBG_SCS, uptr, "??? DUP\n");
}

void dup_get_log ()
{
char cbuf[4*CBUFSIZE];

if (sim_log) {
    fflush (sim_log);
    (void)sim_fseeko (sim_log, dup_log_start, SEEK_SET);
    cbuf[sizeof(cbuf)-1] = '\0';
    dup_buf[0] = '\0';
    while (fgets (cbuf, sizeof(cbuf)-1, sim_log))
        strcat (dup_buf, cbuf);
//        strcpy (dup_buf, cbuf);
    }
}

void dup_process_cmd (void)
{
char cbuf[4*CBUFSIZE], gbuf[CBUFSIZE], *argv[1] = {NULL};
CONST char *cptr;
int32 saved_switches = sim_switches;
CTAB *cmdp = NULL;
t_stat r;

strlcpy (cbuf, dup_buf, sizeof (cbuf));
while (isspace(cbuf[0]))
    memmove (cbuf, cbuf+1, strlen(cbuf+1)+1);   /* skip leading whitespace */
sim_sub_args (cbuf, sizeof(cbuf), argv);
cptr = cbuf;
cptr = get_glyph (cptr, gbuf, 0);               /* get command glyph */
if ((gbuf[0] == 3) || (gbuf[0] == '\r')) {      /* ctrl-c or CR */
    sprintf (dup_buf, "\r\nsim> ");
    dup_buf_cnt = strlen (dup_buf);
    if (dup_read_pending)
        sim_activate (&dup_unit, 1000);
    return;
    }

if (!sim_log) {                                 /* Not currently logging? */
    int32 save_quiet = sim_quiet;
    sim_quiet = 1;
    sprintf (dup_log, "dup_console_%d.temporary_log", (int)getpid());
    sim_set_logon (0, dup_log);
    sim_quiet = save_quiet;
    dup_log_temp = TRUE;
    }

fflush (sim_log);
dup_log_start = sim_ftell (sim_log);

if (cmdp = find_ctab (dup_cmds, gbuf))          /* find command */
    r = cmdp->action (cmdp->arg, cptr);         /* execute command */
else
    r = SCPE_UNK;
if (r != SCPE_OK) {                             /* error? */
    if (r >= SCPE_BASE)
        sprintf (dup_buf, "%s\r\n", sim_error_text (r));
    }
else
    dup_get_log ();
if (sim_vm_post != NULL)                        /* optionally let the simulator know */
    (*sim_vm_post) (TRUE);                      /* something might have changed */
sim_switches = saved_switches;                  /* restore original switches */

if (dup_log_temp) {                             /* If we setup a temporary log, clean it now  */
    int32 save_quiet = sim_quiet;
    sim_quiet = 1;
    sim_set_logoff (0, NULL);
    sim_quiet = save_quiet;
    (void)remove (dup_log);
    dup_log_temp = FALSE;
    }
dup_buf_off = 0;
dup_buf_cnt = strlen (dup_buf);
if (dup_read_pending)
    sim_activate (&dup_unit, 1000);
}

t_stat dup_send_rsp (UNIT *uptr, int32 cid, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, DUP_OPCODE);
CI_PUTB (pkt->data, DUP_OPCODE, (opcode | DUP_ENDFLAG));
dup_debug_pkt (uptr, pkt);
return scs_send_msg (uptr, cid, pkt);                   /* send DUP response */
}

/* get DUST status */

t_stat dup_get_stat (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint8 flags;

flags = (DUP_DO_SDI | DUP_SLSERVER | DUP_LCL_MEDA | DUP_NOXSUPLD);
if (dup_active)
    flags = flags | DUP_ACTIVE;
CI_PUTW (pkt->data, DUP_STATUS, DUP_SUCCESS);
sprintf (&pkt->data[DUP_PGM_EXT], "UTL");
CI_PUTB (pkt->data, DUP_FLAGS, flags);
CI_PUTL (pkt->data, DUP_PROGRESS, 0);
CI_PUTW (pkt->data, DUP_TIMEOUT, 10);
CI_PUTB (pkt->data, DUP_ABORT_TMO, DUP_ABRT_TIM);
pkt->length = LEN_GET_STAT;
return dup_send_rsp (uptr, cid, pkt);
}

/* execute supplied program */

t_stat dup_exe_supld (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
return SCPE_OK;
}

/* execute local program */

t_stat dup_exe_local (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
dup_active = TRUE;
CI_PUTB (pkt->data, DUP_STATUS, DUP_SUCCESS);
CI_PUTW (pkt->data, DUP_VERSION, 1);
CI_PUTB (pkt->data, DUP_LCL_TMOUT, 0);
CI_PUTB (pkt->data, DUP_LCL_FLAGS, 0);
pkt->length = LEN_EXE_LOCAL;
return dup_send_rsp (uptr, cid, pkt);
}

/* send data */

t_stat dup_send_data (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
size_t xfrsz = CI_GETL (pkt->data, DUP_BYTCNT);
uint32 bd[3];
int32 bcnt;

bd[BD_CON] = CI_GETL (pkt->data, DUP_DATAHNDL + (BD_CON << 2));
bd[BD_NAM] = CI_GETL (pkt->data, DUP_DATAHNDL + (BD_NAM << 2));
bd[BD_OFF] = CI_GETL (pkt->data, DUP_DATAHNDL + (BD_OFF << 2));

bcnt = scs_read (uptr, bd, dup_buf, xfrsz, &dup_unit);
if (bcnt < 0) {
    dup_unit.iostarttime = CI_GETL (pkt->data, DUP_CMDREF);
    dup_unit.u3 = DUP_SEND_DATA;
    dup_unit.u4 = cid;
    dup_unit.u5 = bd[BD_NAM];
    dup_unit.u6 = bd[BD_OFF];
    dup_unit.up7 = uptr;
    dup_unit.hwmark = xfrsz;
    }
else if (bcnt == 0) {
    dup_process_cmd ();
    CI_PUTB (pkt->data, DUP_STATUS, DUP_SUCCESS);
    return dup_send_rsp (uptr, cid, pkt);
    }
return SCPE_OK;
}

/* receive data */

t_stat dup_recv_data (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
size_t xfrsz = CI_GETL (pkt->data, DUP_BYTCNT);
uint32 bd[3];
int32 bcnt;

bd[BD_CON] = CI_GETL (pkt->data, DUP_DATAHNDL + (BD_CON << 2));
bd[BD_NAM] = CI_GETL (pkt->data, DUP_DATAHNDL + (BD_NAM << 2));
bd[BD_OFF] = CI_GETL (pkt->data, DUP_DATAHNDL + (BD_OFF << 2));

dup_unit.iostarttime = CI_GETL (pkt->data, DUP_CMDREF);
dup_unit.u3 = DUP_RECV_DATA;
dup_unit.u4 = cid;
dup_unit.u5 = bd[BD_NAM];
dup_unit.u6 = bd[BD_OFF];
dup_unit.up7 = uptr;
dup_unit.hwmark = xfrsz;

if (dup_buf_cnt == 0) {
    dup_read_pending = TRUE;
//    sim_activate_after (&dup_unit, 5000000);            /* delay */
    return SCPE_OK;
    }
//else {
//    sim_activate (&dup_unit, 5000);            /* delay */
//    sim_activate_after (&dup_unit, 5000000);            /* delay */
//    }
sim_printf ("%d bytes requested, %d bytes returned\n", xfrsz, dup_buf_cnt);
xfrsz = dup_buf_cnt;
bcnt = scs_write (uptr, bd, dup_buf, xfrsz, &dup_unit);
if (bcnt == 0) {
    dup_buf_cnt = 0;
    dup_read_pending = FALSE;
    CI_PUTB (pkt->data, DUP_STATUS, DUP_SUCCESS);
    CI_PUTL (pkt->data, DUP_BYTCNT, xfrsz);
    return dup_send_rsp (uptr, cid, pkt);
    }
return SCPE_OK;
}

/* abort program */

t_stat dup_abort (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
dup_active = FALSE;
CI_PUTB (pkt->data, DUP_STATUS, DUP_SUCCESS);
return dup_send_rsp (uptr, cid, pkt);
}

/* send data immediate */

t_stat dup_sdi_end (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
size_t xfrsz = (dup_buf_cnt > DUP_SDI_MAX_DATA) ? DUP_SDI_MAX_DATA : dup_buf_cnt;

if (xfrsz) {
    CI_PUTB (pkt->data, DUP_OPCODE, DUP_SEND_IMED);
    CI_PUTW (pkt->data, DUP_SDI_BYTCNT, xfrsz);
    memcpy (&pkt->data[DUP_SDI_DATA], &dup_buf[dup_buf_off], xfrsz);
    pkt->length = DUP_SDI_DATA + xfrsz;
    dup_buf_off += xfrsz;
    dup_buf_cnt -= xfrsz;
    dup_debug_pkt (uptr, pkt);
    return scs_send_msg (uptr, cid, pkt);               /* send DUP response */
    }
return SCPE_OK;
}

t_stat dup_send_imed (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
size_t xfrsz = CI_GETW (pkt->data, DUP_SDI_BYTCNT);
t_stat r;
uint32 i;

memcpy (&dup_buf[dup_buf_cnt], &pkt->data[DUP_SDI_DATA], xfrsz);
dup_buf_cnt = dup_buf_cnt + xfrsz;
for (i = 0; i < dup_buf_cnt; i++) {
    if (dup_buf[i] == '\r') {
        if (i != 0)
            dup_buf[i] = '\0';
        dup_process_cmd ();
        break;
        }
    else if (dup_buf[i] == 3) {                         /* ctrl-c */
        dup_process_cmd ();
        break;
        }
    }
CI_PUTB (pkt->data, DUP_STATUS, DUP_SUCCESS);
pkt->length = LEN_SDI;
r = dup_send_rsp (uptr, cid, pkt);
if ((r == SCPE_OK) && dup_buf_cnt)
    r = dup_sdi_end (uptr, cid, pkt);
return r;
}

t_stat dup_svc (UNIT *uptr)
{
uint8 opcode = dup_unit.u3;
uint32 cid = dup_unit.u4;
UNIT *cptr = dup_unit.up7;
CI_PKT pkt;
t_stat r;

CI_PUTL (pkt.data, DUP_CMDREF, dup_unit.iostarttime);
CI_PUTL (pkt.data, DUP_UNUSED, 0);
CI_PUTB (pkt.data, DUP_OPCODE, opcode);
CI_PUTL (pkt.data, DUP_RESERVED1, 0);
CI_PUTW (pkt.data, DUP_MODIFIER, 0);
switch (opcode) {
    case DUP_SEND_DATA:                                 /* send data */
        CI_PUTL (pkt.data, DUP_BYTCNT, dup_unit.hwmark);
        CI_PUTL (pkt.data, DUP_DATAHNDL + (BD_CON << 2), cid);
        CI_PUTL (pkt.data, DUP_DATAHNDL + (BD_NAM << 2), dup_unit.u5);
        CI_PUTL (pkt.data, DUP_DATAHNDL + (BD_OFF << 2), dup_unit.u6);
        pkt.length = LEN_SEND_RECV;
        r = dup_send_data (cptr, cid, &pkt);
        break;

    case DUP_RECV_DATA:                                 /* receive data */
        CI_PUTL (pkt.data, DUP_BYTCNT, dup_unit.hwmark);
        CI_PUTL (pkt.data, DUP_DATAHNDL + (BD_CON << 2), cid);
        CI_PUTL (pkt.data, DUP_DATAHNDL + (BD_NAM << 2), dup_unit.u5);
        CI_PUTL (pkt.data, DUP_DATAHNDL + (BD_OFF << 2), dup_unit.u6);
        pkt.length = LEN_SEND_RECV;
//        if (dup_buf_cnt == 0) {
//            sprintf (dup_buf, "sim> ");
//            sprintf (dup_buf, "Test message\r\n");
//            dup_buf_cnt = strlen (dup_buf);
//            }
        r = dup_recv_data (cptr, cid, &pkt);
        break;

    default:
        r = SCPE_IERR;
        break;
    }
return r;
}

t_stat dup_endcmd (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, DUP_OPCODE);

opcode = opcode & ~DUP_ENDFLAG;

switch (opcode) {
    case DUP_SEND_IMED:                                 /* send data immediate */
        return dup_sdi_end (uptr, cid, pkt);

    default:
        CI_PUTW (pkt->data, DUP_STATUS, DUP_INVCMD);
        break;
    }

return dup_send_rsp (uptr, cid, pkt);                   /* send DUP response */
}

t_stat app_dup (UNIT *uptr, uint32 cid, CI_PKT *pkt)
{
uint8 opcode = CI_GETB (pkt->data, DUP_OPCODE);

dup_debug_pkt (uptr, pkt);

if (opcode & DUP_ENDFLAG)
    return dup_endcmd (uptr, cid, pkt);

switch (opcode) {
    case DUP_GET_STAT:                                  /* get DUST status */
        return dup_get_stat (uptr, cid, pkt);

    case DUP_EXE_SUPLD:                                 /* execute supplied program */
        return dup_exe_supld (uptr, cid, pkt);

    case DUP_EXE_LOCAL:                                 /* execute local program */
        return dup_exe_local (uptr, cid, pkt);

    case DUP_SEND_DATA:                                 /* send data */
        return dup_send_data (uptr, cid, pkt);

    case DUP_RECV_DATA:                                 /* receive data */
        return dup_recv_data (uptr, cid, pkt);

    case DUP_ABORT:                                     /* abort program */
        return dup_abort (uptr, cid, pkt);

    case DUP_SEND_IMED:                                 /* send data immediate */
        return dup_send_imed (uptr, cid, pkt);

    default:
        CI_PUTW (pkt->data, DUP_STATUS, DUP_INVCMD);
        break;
    }

return dup_send_rsp (uptr, cid, pkt);                   /* send DUP response */
}
