
#ifndef _CI_H_
#define _CI_H_

/* CI Opcodes */

#define OPC_DG          0x01                            /* datagram */
#define OPC_MSG         0x02                            /* message */
#define OPC_CNF         0x03                            /* confirm */
#define OPC_MCNF        0x04                            /* maintenance confirm */
#define OPC_IDREQ       0x05                            /* identification request */
#define OPC_RST         0x06                            /* reset */
#define OPC_STRT        0x07                            /* start */
#define OPC_DATREQ0     0x08                            /* data request (priority 0) */
#define OPC_DATREQ1     0x09                            /* data request (priority 1) */
#define OPC_DATREQ2     0x0A                            /* data request (priority 2) */
#define OPC_ID          0x0B                            /* identification */
#define OPC_LB          0x0D                            /* loopback datagram */
#define OPC_MDATREQ     0x0E                            /* maintenance data request */
#define OPC_SNTDAT      0x10                            /* sent data */
#define OPC_RETDAT      0x11                            /* returned data */
#define OPC_SNTMDAT     0x12                            /* sent maintenance data */
#define OPC_RETMDAT     0x13                            /* returned maintenance data */

#define LEN_HEADER      0x05
#define LEN_DG          (0x02 + LEN_HEADER)             /* excludes TEXT_LEN */
#define LEN_MSG         (0x02 + LEN_HEADER)             /* excludes TEXT_LEN */
#define LEN_CNF         (0x0A + LEN_HEADER)
#define LEN_MCNF        (0x0A + LEN_HEADER)
#define LEN_IDREQ       (0x0A + LEN_HEADER)
#define LEN_RST         (0x0A + LEN_HEADER)
#define LEN_STRT        (0x0E + LEN_HEADER)
#define LEN_DATREQ      (0x1E + LEN_HEADER)
#define LEN_ID          (0x32 + LEN_HEADER)
#define LEN_LB          (0x02 + LEN_HEADER)             /* excludes TEXT_LEN */
#define LEN_DAT         (0x12 + LEN_HEADER)             /* excludes DATA_LEN */

/* CI Packet Header */

#define CI_LEN_HI       0x00                            /* packet size high byte */
#define  CI_V_PATH      4
#define  CI_M_PATH      0x30
#define CI_LEN_LO       0x01                            /* packet size low byte */
#define CI_DEST         0x02                            /* destination node */
#define CI_NOT_DEST     0x03                            /* destination node (complement) */
#define CI_SRC          0x04                            /* source node */

/* CI Packet Body - Generic */

#define CI_OPC          0x05                            /* opcode */
#define CI_FLAGS        0x06                            /* flags */
#define  CI_PF          0x80                            /* packing format, DG, LB, MSG packets only */
#define  CI_F           0x80                            /* force reset, RST packets only */
#define  CI_DSA         0x80                            /* default start address, STRT packets only */
#define  CI_P           0x80                            /* packet size, data packets only */
#define  CI_PATH_0      0x10                            /* sent on path 0, ID packets only */
#define  CI_PATH_1      0x20                            /* sent on path 1, ID packets only */
#define  CI_V_M         4                               /* packet size multiple, data packets only */
#define  CI_M_M         0x7
#define  CI_NS          0x02                            /* sending sequence number */
#define  CI_LP          0x01                            /* last packet */

/* CI Packet Body - Datagrams (8.5), Messages (8.6) */

#define CI_TEXT         0x07                            /* datagram/message text */

/* CI Packet Body - Data Transfer (8.7) */

#define CI_XCTID        0x07                            /* transaction ID */
#define CI_RBNAM        0x0F                            /* receive buffer name */
#define CI_RBOFF        0x13                            /* receive buffer offset */
#define CI_DATA         0x17                            /* data */

#define CI_REQ_XFRSZ    0x0F                            /* transfer size */
#define CI_REQ_SBNAM    0x13                            /* send buffer name */
#define CI_REQ_SBOFF    0x17                            /* send buffer offset */
#define CI_REQ_RBNAM    0x1B                            /* receive buffer name */
#define CI_REQ_RBOFF    0x1F                            /* receive buffer offset */

/* CI Packet Body - Configuration (8.8) */

#define CI_MAINT_ID     0x0F                            /* port identification */
#define  ID_CI780       2                               /* SBI bus */
#define  ID_CI750       2                               /* CMI bus */
#define  ID_HSC         4                               /* HSC50/HSC70 */
#define  ID_KL10        6                               /* PDP10 */
#define  ID_BCA         11                              /* BI bus */
#define  ID_CIXCD       14                              /* XMI bus */
#define  ID_CITCA       17                              /* TC bus */
#define  ID_CIPCA       18                              /* PCI bus */
#define  ID_SHAC        34
#define  ID_RF70        48                              /* DSSI disks */
#define  ID_RF71        48
#define  ID_RF30        49
#define  ID_RF31        50
#define  ID_RF72        51
#define  ID_RF32        52
#define  ID_RF73        53
#define  ID_RF31F       54
#define  ID_RF35        55
#define  ID_RF36        58
#define  ID_RF37        59
#define  ID_RF74        60
#define  ID_RF75        61
#define  ID_TF70        64                              /* DSSI tapes */
#define  ID_TF30        65
#define  ID_TF85        65
#define  ID_TF86        66
#define  ID_HSJ         80
#define  ID_HSD         81
#define  ID_DUAL        0x80000000                      /* dual path */
#define CI_CODE_REV     0x13                            /* code revision */
#define CI_PORT_FCN     0x17                            /* port functionality */
#define CI_RST_PORT     0x1B                            /* resetting port */
#define CI_SYS_STATE    0x1C                            /* system state */
#define  CI_MNT         0x01                            /* maintenance */
#define  CI_PTST_UNI    0x00                            /* port state - uninitialised */
#define  CI_PTST_DIS    0x02                            /* port state - disabled */
#define  CI_PTST_ENA    0x04                            /* port state - enabled */
#define CI_ID_MBZ       0x1D

/* CI Packet Body - Diagnostic and Booting (8.9) */

#define CI_ADDR         0x0F                            /* start address */

/* Limits */

#define CI_MAXFR        1024                            /* max transfer */
#define CI_MAXDAT       512                             /* max block data */
#define CI_MAX_NODES    32

#endif
