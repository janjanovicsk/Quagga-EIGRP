/*
 * eigrp_const.h
 *
 *  Created on: Nov 7, 2013
 *      Author: janovic
 */

#ifndef _ZEBRA_EIGRP_CONST_H_
#define _ZEBRA_EIGRP_CONST_H_

#define EIGRP_NEIGHBOR_DOWN           0
#define EIGRP_NEIGHBOR_PENDING        1
#define EIGRP_NEIGHBOR_UP             2
#define EIGRP_NEIGHBOR_STATE_MAX      3

#define EIGRP_MAX_PACKET_SIZE  65535U   /* includes IP Header size. */
#define EIGRP_HEADER_SIZE         20U
#define EIGRP_HELLO_MIN_SIZE      12U

#define EIGRP_HEADER_FLAG_INIT          0x00000001
#define EIGRP_HEADER_FLAG_CR            0x00000002
#define EIGRP_HEADER_FLAG_RESET         0x00000004
#define EIGRP_HEADER_FLAG_EOT           0x00000008

#define EIGRP_MSG_UPDATE        1  /* EIGRP Hello Message. */
#define EIGRP_MSG_REQUEST       2  /* EIGRP Database Descriptoin Message. */
#define EIGRP_MSG_QUERY         3  /* EIGRP Link State Request Message. */
#define EIGRP_MSG_REPLY         4  /* EIGRP Link State Update Message. */
#define EIGRP_MSG_HELLO         5  /* EIGRP Link State Acknoledgement Message. */
#define EIGRP_MSG_PROBE         7  /* EIGRP Probe Message. */
#define EIGRP_MSG_SIAQUERY     10  /* EIGRP SIAQUERY. */
#define EIGRP_MSG_SIAREPLY     11  /* EIGRP SIAREPLY. */

/*EIGRP TLV Type definitions*/
#define TLV_PARAMETER_TYPE              0x0001       /*K types*/
#define TLV_AUTHENTICATION_TYPE         0x0002
#define TLV_SEQUENCE_TYPE               0x0003
#define TLV_SOFTWARE_VERSION_TYPE       0x0004
#define TLV_MULTICAST_SEQUENCE_TYPE     0x0005
#define TLV_PEER_INFORMATION_TYPE       0x0006
#define TLV_PEER_TERMINATION_TYPE       0x0007
#define TLV_PEER_TID_LIST_TYPE          0x0008

/*Packet requiring ack will be retransmitted again after this time*/
#define EIGRP_PACKET_RETRANS_TIME        5 /* in seconds */

/* Return values of functions involved in packet verification */
#define MSG_OK    0
#define MSG_NG    1

#define EIGRP_HEADER_VERSION            2


/* Default protocol, port number. */
#ifndef IPPROTO_EIGRPIGP
#define IPPROTO_EIGRPIGP         88
#endif /* IPPROTO_EIGRPIGP */

/* IP TTL for EIGRP protocol. */
#define EIGRP_IP_TTL             1

/* VTY port number. */
#define EIGRP_VTY_PORT          2609

/* Default configuration file name for eigrp. */
#define EIGRP_DEFAULT_CONFIG   "eigrpd.conf"

#define EIGRP_HELLO_INTERVAL_DEFAULT        5
#define EIGRP_HOLD_INTERVAL_DEFAULT         15

#define EIGRP_MULTICAST_ADDRESS                0xe000000A /*224.0.0.10*/

    /* EIGRP Network Type. */
 #define EIGRP_IFTYPE_NONE                0
 #define EIGRP_IFTYPE_POINTOPOINT         1
 #define EIGRP_IFTYPE_BROADCAST           2
 #define EIGRP_IFTYPE_NBMA                3
 #define EIGRP_IFTYPE_POINTOMULTIPOINT    4
 #define EIGRP_IFTYPE_LOOPBACK            5
 #define EIGRP_IFTYPE_MAX                 6

#define EIGRP_IF_ACTIVE                  0
#define EIGRP_IF_PASSIVE                 1

#endif /* _ZEBRA_EIGRP_CONST_H_ */
