/*
 * eigrp_structs.h
 *
 *  Created on: Nov 7, 2013
 *      Author: janovic
 */

#ifndef _ZEBRA_EIGRP_STRUCTS_H_
#define _ZEBRA_EIGRP_STRUCTS_H_

#include "eigrpd/eigrp_const.h"
#include "eigrpd/eigrp_macros.h"

/* EIGRP master for system wide configuration and variables. */
struct eigrp_master
{
  /* EIGRP instance. */
  struct list *eigrp;

  /* EIGRP thread master. */
  struct thread_master *master;

  /* Zebra interface list. */
  struct list *iflist;

  /* EIGRP start time. */
  time_t start_time;

  /* Various EIGRP global configuration. */
  u_char options;

#define EIGRP_MASTER_SHUTDOWN (1 << 0) /* deferred-shutdown */
};

struct eigrp
{

  /* EIGRP Router ID. */
  struct in_addr router_id; /* Configured automatically. */
  struct in_addr router_id_static; /* Configured manually. */

  struct list *eiflist; /* eigrp interfaces */
  u_char passive_interface_default; /* passive-interface default */

  int AS; /* Autonomous system number */

  unsigned int fd;
  unsigned int maxsndbuflen;

  u_int32_t sequence_number; /*Global EIGRP sequence number*/

  struct stream *ibuf;
  struct list *oi_write_q;

  /*Threads*/
  struct thread *t_write;
  struct thread *t_read;

  struct route_table *networks; /* EIGRP config networks. */

  u_char k_values[6]; /*Array for K values configuration*/

  struct list *topology_table;

};

//------------------------------------------------------------------------------------------------------------------------------------------

/*EIGRP interface structure*/
struct eigrp_interface
{
  /* This interface's parent eigrp instance. */
  struct eigrp *eigrp;

  /* Interface data from zebra. */
  struct interface *ifp;

  /* Packet send buffer. */
  struct eigrp_fifo *obuf; /* Output queue */

  /* To which multicast groups do we currently belong? */

  /* Configured varables. */
  struct eigrp_if_params *params;

  u_char multicast_memberships;

  /* EIGRP Network Type. */
  u_char type;

  struct prefix *address; /* Interface prefix */
  struct connected *connected; /* Pointer to connected */

  /* Neighbor information. */
  struct route_table *nbrs; /* EIGRP Neighbor List */

  /* Threads. */
  struct thread *t_hello; /* timer */

  int on_write_q;

  /* Statistics fields. */
  u_int32_t hello_in; /* Hello message input count. */
  u_int32_t update_in; /* Update message input count. */
};

struct eigrp_if_params
{
  DECLARE_IF_PARAM (u_char, passive_interface)
  ; /* EIGRP Interface is passive: no sending or receiving (no need to join multicast groups) */
  DECLARE_IF_PARAM (u_int32_t, v_hello)
  ; /* Hello Interval */
  DECLARE_IF_PARAM (u_int16_t, v_wait)
  ; /* Router Hold Time Interval */
  DECLARE_IF_PARAM (u_char, type)
  ; /* type of interface */

};

enum
{
  MEMBER_ALLROUTERS = 0, MEMBER_MAX,
};

struct eigrp_if_info
{
  struct eigrp_if_params *def_params;
  struct route_table *params;
  struct route_table *eifs;
  unsigned int membership_counts[MEMBER_MAX]; /* multicast group refcnts */
};

//------------------------------------------------------------------------------------------------------------------------------------------

/* Neighbor Data Structure */
struct eigrp_neighbor
{
  /* This neighbor's parent eigrp interface. */
  struct eigrp_interface *ei;

  /* OSPF neighbor Information */
  u_char state; /* neigbor status. */
  u_int32_t recv_sequence_number; /* Last received sequence Number. */
  u_int32_t ack; /* Acknowledgement number*/

  /*If packet is unacknowledged, we try to send it again 16 times*/
  u_char retrans_counter;

  struct in_addr src; /* Neighbor Src address. */

  u_char K1;
  u_char K2;
  u_char K3;
  u_char K4;
  u_char K5;
  u_char K6;

  /* Timer values. */
  u_int16_t v_holddown;

  /* Threads. */
  struct thread *t_holddown;

  struct eigrp_fifo *retrans_queue;
};

//---------------------------------------------------------------------------------------------------------------------------------------------

struct eigrp_packet
{
  struct eigrp_packet *next;
  struct eigrp_packet *previous;

  /* Pointer to data stream. */
  struct stream *s;

  /* IP destination address. */
  struct in_addr dst;

  /*Packet retransmission thread*/
  struct thread *t_retrans_timer;

  /*Packet retransmission counter*/
  u_char retrans_counter;

  /* EIGRP packet length. */
  u_int16_t length;
};

struct eigrp_fifo
{
  unsigned long count;

  struct eigrp_packet *head;

  struct eigrp_packet *tail;
};

struct eigrp_header
{
  u_char version;
  u_char opcode;
  u_int16_t checksum;
  u_int32_t flags;
  u_int32_t sequence;
  u_int32_t ack;
  u_int16_t routerID;
  u_int16_t ASNumber;

}__attribute__((packed));

struct TLV_Parameter_Type
{
  u_int16_t type;
  u_int16_t length;
  u_char K1;
  u_char K2;
  u_char K3;
  u_char K4;
  u_char K5;
  u_char K6;
  u_int16_t hold_time;
}__attribute__((packed));

struct TLV_Authentication_Type
{

}__attribute__((packed));

struct TLV_Sequence_Type
{
  u_int16_t type;
  u_int16_t length;
  u_char addr_length;
  struct in_addr address;
}__attribute__((packed));

struct TLV_Software_Type
{
  u_int16_t type;
  u_int16_t length;
  u_char vender_major;
  u_char vender_minor;
  u_char eigrp_major;
  u_char eigrp_minor;
}__attribute__((packed));

struct TLV_IPv4_Internal_type
{
  u_int16_t type;
  u_int16_t length;
  struct in_addr forward;

  /*Metrics*/
  u_int32_t delay;
  u_int32_t bandwith;
  unsigned char mtu[3];
  u_char hop_count;
  u_char reliability;
  u_char load;
  u_char tag;
  u_char flags;

  u_char prefix_length;

  unsigned char destination_part[4];
  struct in_addr destination;
}__attribute__((packed));

//---------------------------------------------------------------------------------------------------------------------------------------------

/* EIGRP Topology table node structure */
struct eigrp_topology_node
{
  struct list *entries;
  struct prefix_ipv4 *destination; //destination address
  u_char state; //route state
};

/* EIGRP Topology table record structure */
struct eigrp_topology_entry
{
  struct prefix *data;
  unsigned long reported_distance; //distance reported by neighbor
  unsigned long distance; //sum of reported distance and link cost to advertised neighbor
  struct eigrp_neighbor *adv_router; //ip address of advertising neighbor
  u_char flags; //used for marking successor and FS
};

//---------------------------------------------------------------------------------------------------------------------------------------------

/* EIGRP Finite State Machine */
struct eigrp_fsm_query_event
{
  struct eigrp_neighbor *adv_router;
  int adv_cost;

};


#endif /* _ZEBRA_EIGRP_STRUCTURES_H_ */
