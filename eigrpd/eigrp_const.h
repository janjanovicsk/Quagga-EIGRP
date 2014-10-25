/*
 * EIGRP Definition of Constants.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _ZEBRA_EIGRP_CONST_H_
#define _ZEBRA_EIGRP_CONST_H_

#define FALSE 0

#define EIGRP_NEIGHBOR_DOWN           0
#define EIGRP_NEIGHBOR_PENDING        1
#define EIGRP_NEIGHBOR_PENDING_INIT   2
#define EIGRP_NEIGHBOR_UP             3
#define EIGRP_NEIGHBOR_STATE_MAX      4

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
#define EIGRP_BANDWIDTH_DEFAULT             10000000
#define EIGRP_DELAY_DEFAULT                 1000
#define EIGRP_RELIABILITY_DEFAULT           255
#define EIGRP_LOAD_DEFAULT                  1

#define EIGRP_MULTICAST_ADDRESS            0xe000000A /*224.0.0.10*/

#define EIGRP_MAX_METRIC                   0xffffffffU    /*8589934591*/

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

/* EIGRP TT destination type */
#define EIGRP_TOPOLOGY_TYPE_CONNECTED           0
#define EIGRP_TOPOLOGY_TYPE_REMOTE              1

/*EIGRP TT entry flags*/
#define EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG     1
#define EIGRP_NEIGHBOR_ENTRY_FSUCCESSOR_FLAG    2

/*EIGRP FSM state count, event count*/
#define EIGRP_FSM_STATE_MAX                  5
#define EIGRP_FSM_EVENT_MAX                  16

/*EGRP FSM states*/
#define EIGRP_FSM_STATE_PASSIVE              0
#define EIGRP_FSM_STATE_ACTIVE_0             1
#define EIGRP_FSM_STATE_ACTIVE_1             2
#define EIGRP_FSM_STATE_ACTIVE_2             3
#define EIGRP_FSM_STATE_ACTIVE_3             4

/*EIGRP FSM events*/
#define EIGRP_FSM_EVENT_NQ_FCN                  0 /*input event other than query from succ, FC not satisfied*/
#define EIGRP_FSM_EVENT_LR                      1 /*last reply, FD is reset*/
#define EIGRP_FSM_EVENT_Q_FCN                   2 /*query from succ, FC not satisfied*/
#define EIGRP_FSM_EVENT_LR_FCS                  3 /*last reply, FC satisfied with current value of FDij*/
#define EIGRP_FSM_EVENT_DINC                    4 /*distance increase while in active state*/
#define EIGRP_FSM_EVENT_QACT                    5 /*query from succ while in active state*/
#define EIGRP_FSM_EVENT_LR_FCN                  6 /*last reply, FC not satisfied with current value of FDij*/
#define EIGRP_FSM_KEEP_STATE                    7 /*state not changed, usually by receiving not last reply */

#define INT_TYPES_CMD_STR						 \
	"detail|fastethernet|loopback|static"

#define INT_TYPES_DESC							\
	"Show detailed peer information\n"				\
	"FastEthernet IEEE 802.3\n"					\
	"Loopback interface\n"						\
	"Show static peer information\n"

/**
 * External routes originate from some other protocol - these are them
 */
#define NULL_PROTID		0		/*!< unknown protocol */
#define IGRP_PROTID		1		/*!< IGRP.. whos your daddy! */
#define EIGRP_PROTID		2		/*!< EIGRP - Just flat out the best */
#define STATIC_PROTID		3		/*!< Staticly configured source */
#define RIP_PROTID		4		/*!< Routing Information Protocol */
#define HELLO_PROTID		5		/*!< Hello? RFC-891 you there? */
#define OSPF_PROTID		6		/*!< OSPF - Open Shortest Path First */
#define ISIS_PROTID		7		/*!< Intermediate System To Intermediate System */
#define EGP_PROTID		8		/*!< Exterior Gateway Protocol */
#define BGP_PROTID		9		/*!< Border Gateway Protocol */
#define IDRP_PROTID		10		/*!< InterDomain Routing Protocol */
#define CONN_PROTID		11		/*!< Connected source */

/* 
 * metric k-value defaults
 */
#define EIGRP_K1_DEFAULT	1		//!< unweighed inverse bandwidth
#define EIGRP_K2_DEFAULT	0		//!< no loading term
#define EIGRP_K3_DEFAULT	1		//!< unweighted delay
#define EIGRP_K4_DEFAULT	0		//!< no reliability term
#define EIGRP_K5_DEFAULT	0		//!< no reliability term
#define EIGRP_K6_DEFAULT	0		//!< do not add in extended metrics

#endif /* _ZEBRA_EIGRP_CONST_H_ */
