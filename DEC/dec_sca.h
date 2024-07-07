
#ifndef _DEC_SCA_H_
#define _DEC_SCA_H_

#define sca_ctx         up8

#define MAX_CONN        (CI_MAX_NODES)                  /* max connections */
#define MAX_BUFF        (CI_MAX_NODES)                  /* max buffer descriptors */

/* SCA process - should be part of connection block? */

typedef struct {
    char   *name;                                       /* SYSAP name */
    uint32 inicr;                                       /* initial credits */
    uint32 mincr;                                       /* min credits */
    t_stat (*msgrec)(UNIT *uptr, uint32 conid, CI_PKT *pkt);
} SYSAP;

/* Buffer descriptor */

#define BD_OFF          0                               /* buffer offset */
#define BD_NAM          1                               /* buffer name */
#define BD_CON          2                               /* connection ID */

typedef struct {
    size_t xfrsz;                                       /* buffer length */
    uint8  *buf;                                        /* buffer pointer */
    uint32 boff;                                        /* buffer offset */
    uint32 *bd;
    uint32 node;
    UNIT   *uptr;                                       /* buffer owner */
    uint32 rspid;                                       /* response ID */
} SCA_BDT;

/* Virtual circuit states */

#define VC_CLOSED       0                               /* VC closed */
#define START_SENT      1                               /* START sent */
#define START_REC       2                               /* START received */
#define VC_OPEN         3                               /* VC open */

/* Path states */

#define PTH_BAD         0
#define PTH_CROSSED     1
#define PTH_GOOD        2
#define PTH_MASK        0x7F
#define PTH_TEST        0x80

/* SCA path block */

typedef struct {
    uint32 vc_state;                                    /* virtual circuit state */
    uint32 path_state[2];
} SCA_PB;

/* SCA system block */

typedef struct {
    uint32 sysid;                                       /* SCS system ID */
    char * nodename;                                    /* SCS node name */
    SCA_PB pb;                                          /* path block */
} SCA_SB;

/* Connection states */

#define CON_CLOSED      0
#define CON_LISTEN      1
#define CON_SENT        2
#define CON_ACK         3
#define CON_REC         4
#define ACC_SENT        5
#define REJ_SENT        6
#define CON_OPEN        7
#define DIS_SENT        8
#define DIS_REC         9
#define DIS_ACK         10
#define DIS_MATCH       11

/* SCA connection block */

typedef struct {
    uint32 cid;                                         /* remote conid */
    uint32 credits;                                     /* current credits */
    SYSAP  *sysap;                                      /* SYSAP descriptor */
    uint32 node;
    SCA_PB *pb;                                         /* path block */
} SCA_CB;

/* SCA unit context */

typedef struct {
    SCA_SB  sb[CI_MAX_NODES];                           /* system blocks */
    SCA_CB  cb[MAX_CONN];
    SCA_BDT bdt[MAX_BUFF];
    SYSAP  *sysap;                                      /* SYSAP list */
    uint32 rspid;
    uint32 poll_path;
    uint32 lb_state[2];
} SCA_CTX;

#endif
