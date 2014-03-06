/*
 * EIGRP Sending and Receiving EIGRP Packets.
 * Copyright (C) 2013-2014
 * Authors:
 * Jan Janovic
 * Matej Perina
 * Peter Orsag
 * Peter Paluch
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

#include <zebra.h>

#include "thread.h"
#include "memory.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "sockunion.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"
#include "checksum.h"
#include "md5.h"

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"

static void
eigrp_hello_send_sub(struct eigrp_interface *, in_addr_t);
static void
eigrp_make_header(int, struct eigrp_interface *, struct stream *, u_int32_t,
    u_int32_t, u_int32_t);
static int
eigrp_make_hello(struct eigrp_interface *, struct stream *);
static void
eigrp_packet_add_top(struct eigrp_interface *, struct eigrp_packet *);
static void
eigrp_fifo_push_head(struct eigrp_fifo *fifo, struct eigrp_packet *ep);
static int
eigrp_write(struct thread *);
static void
eigrp_packet_checksum(struct eigrp_interface *, struct stream *, u_int16_t);
static struct stream *
eigrp_recv_packet(int, struct interface **, struct stream *);
static unsigned
eigrp_packet_examin(struct eigrp_header *, const unsigned);
static int
eigrp_verify_header(struct stream *, struct eigrp_interface *, struct ip *,
    struct eigrp_header *);
static int
eigrp_check_network_mask(struct eigrp_interface *, struct in_addr);

/*
 * Converts a 24-bit integer represented as an unsigned char[3] *value
 * in network byte order into uint32_t in host byte order
 */
//static uint32_t u24_32 (const unsigned char *value)
//{
//  return (value[0] << 16) + (value[1] << 8) + value[2];
//}
//
///*
// * Converts an uint32_t value in host byte order into a 24-bit integer
// * in network byte order represented by unsigned char[3] *result
// */
//static unsigned char * u32_24 (uint32_t value, unsigned char *result)
//{
//  value = htonl(value & 0x00FFFFFF);
//  memcpy(result, (unsigned char *) &value + 1, 3);
//
//  return result;
//}
/*EIGRP UPDATE read function*/
static void
eigrp_update(struct ip *iph, struct eigrp_header *eigrph, struct stream * s,
    struct eigrp_interface *ei, int size)
{
  struct eigrp_neighbor *nbr;
  struct prefix p;
  struct TLV_IPv4_Internal_type *tlv;
  struct eigrp_prefix_entry *tnode;
  struct eigrp_neighbor_entry *tentry;
  struct eigrp *eigrp;
  u_int16_t type;

  /* increment statistics. */
  ei->update_in++;

  eigrp = eigrp_lookup();
  /* If Hello is myself, silently discard. */
  if (IPV4_ADDR_SAME (&iph->ip_src.s_addr, &ei->address->u.prefix4))
    {
      return;
    }

  /* get neighbour struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph, &p);

  /* neighbour must be valid, eigrp_nbr_get creates if none existed */
  assert(nbr);

  nbr->recv_sequence_number = ntohl(eigrph->sequence);

  if ((ntohl(eigrph->flags) & EIGRP_HEADER_FLAG_CR) == EIGRP_HEADER_FLAG_CR)
    {
      if (nbr->state >= EIGRP_NEIGHBOR_PENDING_INIT)
        eigrp_ack_send(nbr);
      return;
    }

  /*If it is INIT update*/
  if ((ntohl(eigrph->flags) & EIGRP_HEADER_FLAG_INIT) == EIGRP_HEADER_FLAG_INIT)
    {
      struct eigrp_packet *ep;
      ep = eigrp_fifo_tail(nbr->retrans_queue);
      if (ep != NULL)
        {
          if (ntohl(eigrph->ack) == ep->sequence_number)
            {
              if (ntohl(eigrph->ack) == nbr->init_sequence_number)
                {
                  nbr->state = EIGRP_NEIGHBOR_PENDING_INIT;
                  nbr->init_sequence_number = 0;
                  ep = eigrp_fifo_pop_tail(nbr->retrans_queue);
                  eigrp_packet_free(ep);
                  eigrp_send_EOT_update(nbr);
                  return;
                }
              ep = eigrp_fifo_pop_tail(nbr->retrans_queue);
              eigrp_packet_free(ep);
              eigrp_send_init_update(nbr);
              return;
            }
        }
      else
        {
          eigrp_send_init_update(nbr);
          return;
        }
    } /*If it is END OF TABLE update*/
  else if ((ntohl(eigrph->flags) & EIGRP_HEADER_FLAG_EOT)
      == EIGRP_HEADER_FLAG_EOT)
    {
      struct eigrp_packet *ep;
      ep = eigrp_fifo_tail(nbr->retrans_queue);
      if (ep != NULL)
        {
          if (ntohl(eigrph->ack) == ep->sequence_number)
            {
              ep = eigrp_fifo_pop_tail(nbr->retrans_queue);
              eigrp_packet_free(ep);
              if (nbr->retrans_queue->count > 0)
                {
                  eigrp_send_packet_reliably(nbr);
                }
              if (ntohl(eigrph->ack) == nbr->init_sequence_number)
                {
                  nbr->state = EIGRP_NEIGHBOR_PENDING_INIT;
                  nbr->init_sequence_number = 0;
                  eigrp_send_EOT_update(nbr);
                }
            }
        }
      /*If there is topology information*/
      while (s->endp > s->getp)
        {
          type = stream_getw(s);
          if (type == TLV_INTERNAL_TYPE)
            {
              stream_set_getp(s, s->getp - sizeof(u_int16_t));

              tlv = eigrp_read_ipv4_tlv(s);

              /*seraching if destination exists */
              struct prefix_ipv4 *dest_addr;
              dest_addr = prefix_ipv4_new();
              dest_addr->prefix = tlv->destination;
              dest_addr->prefixlen = tlv->prefix_length;
              struct eigrp_prefix_entry *dest = eigrp_topology_table_lookup(
                  eigrp->topology_table, dest_addr);

              /*if exists it comes to DUAL*/
              if (dest != NULL)
                {
                  struct eigrp_fsm_action_message *msg;
                  msg = XCALLOC(MTYPE_EIGRP_FSM_MSG,
                      sizeof(struct eigrp_fsm_action_message));
                  struct eigrp_neighbor_entry *entry =
                      eigrp_prefix_entry_lookup(dest->entries, nbr);

                  msg->packet_type = EIGRP_MSG_UPDATE;
                  msg->data_type = TLV_INTERNAL_TYPE;
                  msg->adv_router = nbr;
                  msg->data.ipv4_int_type = tlv;
                  msg->entry = entry;
                  int event = eigrp_get_fsm_event(msg);
                  EIGRP_FSM_EVENT_SCHEDULE(msg, event);
                }
              else
                {
                  /*Here comes topology information save*/
                  tnode = eigrp_prefix_entry_new();
                  tnode->destination->family = AF_INET;
                  tnode->destination->prefix = tlv->destination;
                  tnode->destination->prefixlen = tlv->prefix_length;
                  tnode->state = EIGRP_FSM_STATE_PASSIVE;
                  tnode->dest_type = EIGRP_TOPOLOGY_TYPE_REMOTE;

                  tentry = eigrp_neighbor_entry_new();
                  tentry->ei = ei;
                  tentry->adv_router = nbr;
                  tentry->reported_metric = tlv->metric;
                  tentry->total_metric = tentry->reported_metric;
                  tentry->reported_distance = eigrp_calculate_metrics(
                      &tlv->metric);

                  u_int32_t bw = EIGRP_IF_PARAM(tentry->ei,bandwidth);
                  tentry->total_metric.bandwith =
                      tentry->total_metric.bandwith > bw ?
                          bw : tentry->total_metric.bandwith;
                  tentry->total_metric.delay +=
                      EIGRP_IF_PARAM(tentry->ei, delay);
                  tentry->distance = eigrp_calculate_metrics(
                      &tentry->total_metric);
                  tnode->fdistance = tnode->distance = tnode->rdistance =
                      tentry->distance;
                  tentry->node = tnode;

                  eigrp_prefix_entry_add(eigrp->topology_table, tnode);
                  eigrp_neighbor_entry_add(tnode, tentry);
                  eigrp_topology_update_node(tnode);
                  eigrp_update_send_all(tentry, ei);
                }
              XFREE(MTYPE_EIGRP_IPV4_INT_TLV, tlv);
            }
        }
      if (nbr->state != EIGRP_NEIGHBOR_UP)
        {
          eigrp_ack_send(nbr);
          nbr->state = EIGRP_NEIGHBOR_UP;
          zlog_info("Neighbor adjacency became full");
          return;
        }
    }/*regular update*/
  else
    {
      /*If there is topology information*/
      while (s->endp > s->getp)
        {
          type = stream_getw(s);
          if (type == TLV_INTERNAL_TYPE)
            {
              stream_set_getp(s, s->getp - sizeof(u_int16_t));

              tlv = eigrp_read_ipv4_tlv(s);

              /*seraching if destination exists */
              struct prefix_ipv4 *dest_addr;
              dest_addr = prefix_ipv4_new();
              dest_addr->prefix = tlv->destination;
              dest_addr->prefixlen = tlv->prefix_length;
              struct eigrp_prefix_entry *dest = eigrp_topology_table_lookup(
                  eigrp->topology_table, dest_addr);
              /*if exists it comes to DUAL*/
              if (dest != NULL)
                {
                  struct eigrp_fsm_action_message *msg;
                  msg = XCALLOC(MTYPE_EIGRP_FSM_MSG,
                      sizeof(struct eigrp_fsm_action_message));
                  struct eigrp_neighbor_entry *entry =
                      eigrp_prefix_entry_lookup(dest->entries, nbr);

                  msg->packet_type = EIGRP_MSG_UPDATE;
                  msg->data_type = TLV_INTERNAL_TYPE;
                  msg->adv_router = nbr;
                  msg->data.ipv4_int_type = tlv;
                  msg->entry = entry;
                  int event = eigrp_get_fsm_event(msg);
                  EIGRP_FSM_EVENT_SCHEDULE(msg, event);
                }
              else
                {
                  /*Here comes topology information save*/
                  tnode = eigrp_prefix_entry_new();
                  tnode->destination->family = AF_INET;
                  tnode->destination->prefix = tlv->destination;
                  tnode->destination->prefixlen = tlv->prefix_length;
                  tnode->state = EIGRP_FSM_STATE_PASSIVE;
                  tnode->dest_type = EIGRP_TOPOLOGY_TYPE_REMOTE;

                  tentry = eigrp_neighbor_entry_new();
                  tentry->adv_router = nbr;
                  tentry->reported_metric = tlv->metric;
                  tentry->total_metric = tentry->reported_metric;
                  tentry->reported_distance = eigrp_calculate_metrics(
                      &tlv->metric);
                  tentry->distance = tentry->reported_distance;
                  tentry->ei = ei;
                  tentry->node = tnode;

                  eigrp_prefix_entry_add(eigrp->topology_table, tnode);
                  eigrp_neighbor_entry_add(tnode, tentry);
                  eigrp_topology_update_node(tnode);

                }
              XFREE(MTYPE_EIGRP_IPV4_INT_TLV, tlv);
            }
        }
    }

  if (nbr->state >= EIGRP_NEIGHBOR_PENDING_INIT)
    eigrp_ack_send(nbr);
}

/*EIGRP hello read function*/
static void
eigrp_hello(struct ip *iph, struct eigrp_header *eigrph, struct stream * s,
    struct eigrp_interface *ei, int size)
{
  struct TLV_Parameter_Type *hello;
  struct eigrp_neighbor *nbr;
  struct prefix p;

  /* get neighbor prefix. */
  p.family = AF_INET;
  p.u.prefix4 = iph->ip_src;

  /* If Hello is myself, silently discard. */
  if (IPV4_ADDR_SAME (&iph->ip_src.s_addr, &ei->address->u.prefix4))
    {
      return;
    }

  /* get neighbour struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph, &p);

  /* neighbour must be valid, eigrp_nbr_get creates if none existed */
  assert(nbr);

  hello = (struct TLV_Parameter_Type *) STREAM_PNT (s);

  /*If received packet is hello with Parameter TLV*/
  if (eigrph->ack == 0)
    {
      /* increment statistics. */
      ei->hello_in++;

      hello = (struct TLV_Parameter_Type *) STREAM_PNT (s);

      switch (nbr->state)
        {
      case EIGRP_NEIGHBOR_DOWN:
        {
          nbr->v_holddown = ntohs(hello->hold_time);
          /*Check for correct values to be able to become neighbors*/
          if (eigrp_neighborship_check(nbr, hello))
            {
              /*Start Hold Down Timer for neighbor*/
              THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired,
                  nbr, nbr->v_holddown);
              nbr->state = EIGRP_NEIGHBOR_PENDING;
              eigrp_send_init_update(nbr);
              zlog_info("Neighbor %s (%s) is up: new adjacency",
                  inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
            }
          else
            return;
          break;
        }
      case EIGRP_NEIGHBOR_PENDING:
        {
          /*Reset Hold Down Timer for neighbor*/
          THREAD_OFF(nbr->t_holddown);
          THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired, nbr,
              nbr->v_holddown);
          break;
        }
      case EIGRP_NEIGHBOR_PENDING_INIT:
        {
          /*Reset Hold Down Timer for neighbor*/
          THREAD_OFF(nbr->t_holddown);
          THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired, nbr,
              nbr->v_holddown);
          break;
        }
      case EIGRP_NEIGHBOR_UP:
        {
          /*Reset Hold Down Timer for neighbor*/
          THREAD_OFF(nbr->t_holddown);
          THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired, nbr,
              nbr->v_holddown);
          break;
        }
        }
    } /*If packet is only ack*/
  else
    {
      struct eigrp_packet *ep;

      ep = eigrp_fifo_tail(nbr->retrans_queue);
      if (ep != NULL)
        {
          if (ntohl(eigrph->ack) == ep->sequence_number)
            {
              ep = eigrp_fifo_pop_tail(nbr->retrans_queue);
              eigrp_packet_free(ep);
              if (nbr->retrans_queue->count > 0)
                {
                  eigrp_send_packet_reliably(nbr);
                }
              return;
            }
        }
      ep = eigrp_fifo_tail(nbr->multicast_queue);
      if (ep != NULL)
        {
          if (ntohl(eigrph->ack) == ep->sequence_number)
            {
              ep = eigrp_fifo_pop_tail(nbr->multicast_queue);
              eigrp_packet_free(ep);
              if (nbr->multicast_queue->count > 0)
                {
                  eigrp_send_packet_reliably(nbr);
                }
              return;
            }
        }
    }

//  if (IS_DEBUG_OSPF_EVENT)
//    zlog_debug ("Packet %s [Hello:RECV]: Options %s",
//               inet_ntoa (ospfh->router_id),
//               ospf_options_dump (hello->options));
}

/*EIGRP QUERY read function*/
static void
eigrp_query(struct ip *iph, struct eigrp_header *eigrph, struct stream * s,
    struct eigrp_interface *ei, int size)
{
  struct eigrp_neighbor *nbr;
  struct prefix p;
  struct TLV_IPv4_Internal_type *tlv;
  struct eigrp *eigrp;
  struct listnode *node, *nnode;

  struct eigrp_prefix_entry *temp_tn;
  struct eigrp_neighbor_entry *temp_te;

  u_int16_t type;

  /* increment statistics. */
  ei->query_in++;

  eigrp = eigrp_lookup();
  /* If Hello is myself, silently discard. */
  if (IPV4_ADDR_SAME (&iph->ip_src.s_addr, &ei->address->u.prefix4))
    {
      return;
    }

  /* get neighbour struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph, &p);

  /* neighbour must be valid, eigrp_nbr_get creates if none existed */
  assert(nbr);

  nbr->recv_sequence_number = ntohl(eigrph->sequence);

  while (s->endp > s->getp)
    {
      type = stream_getw(s);
      if (type == TLV_INTERNAL_TYPE)
        {
          stream_set_getp(s, s->getp - sizeof(u_int16_t));

          tlv = eigrp_read_ipv4_tlv(s);

          struct prefix_ipv4 *dest_addr;
          dest_addr = prefix_ipv4_new();
          dest_addr->prefix = tlv->destination;
          dest_addr->prefixlen = tlv->prefix_length;
          struct eigrp_prefix_entry *dest = eigrp_topology_table_lookup(
              eigrp->topology_table, dest_addr);

          temp_te = XCALLOC(MTYPE_EIGRP_NEIGHBOR_ENTRY,
              sizeof(struct eigrp_neighbor_entry));
          temp_tn = XCALLOC(MTYPE_EIGRP_PREFIX_ENTRY,
              sizeof(struct eigrp_prefix_entry));
          temp_te->total_metric.delay = 0xFFFFFFFF;
          temp_te->node = temp_tn;
          temp_tn->destination = dest_addr;

          eigrp_send_reply(nbr, temp_te);
          XFREE(MTYPE_EIGRP_NEIGHBOR_ENTRY, temp_te);
          XFREE(MTYPE_EIGRP_PREFIX_ENTRY, temp_tn);

          /* If the destination exists (it should, but one never know)*/
          if (dest != NULL)
            {
              struct eigrp_fsm_action_message *msg;
              msg = XCALLOC(MTYPE_EIGRP_FSM_MSG,
                  sizeof(struct eigrp_fsm_action_message));

              msg->packet_type = EIGRP_MSG_QUERY;
              msg->data_type = TLV_INTERNAL_TYPE;
              msg->adv_router = nbr;
              msg->data.ipv4_int_type = tlv;
              msg->entry->node = dest;
              int event = eigrp_get_fsm_event(msg);
              EIGRP_FSM_EVENT_SCHEDULE(msg, event);
              eigrp_prefix_entry_delete(eigrp->topology_table, dest);
            }

        }
    }
  eigrp_ack_send(nbr);

}

/*EIGRP QUERY read function*/
static void
eigrp_reply(struct ip *iph, struct eigrp_header *eigrph, struct stream * s,
    struct eigrp_interface *ei, int size)
{
  struct eigrp_neighbor *nbr;
  struct prefix p;
  struct TLV_IPv4_Internal_type *tlv;
  struct eigrp *eigrp;
  struct listnode *node, *nnode;

  struct eigrp_prefix_entry *temp_tn;
  struct eigrp_neighbor_entry *temp_te;

  u_int16_t type;

  /* increment statistics. */
  ei->reply_in++;

  eigrp = eigrp_lookup();
  /* If Hello is myself, silently discard. */
  if (IPV4_ADDR_SAME (&iph->ip_src.s_addr, &ei->address->u.prefix4))
    {
      return;
    }

  /* get neighbour struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph, &p);

  /* neighbour must be valid, eigrp_nbr_get creates if none existed */
  assert(nbr);

  nbr->recv_sequence_number = ntohl(eigrph->sequence);

  while (s->endp > s->getp)
    {
      struct eigrp_packet *ep;
      ep = eigrp_fifo_tail(nbr->retrans_queue);
      if (ep != NULL)
        {
          if (ntohl(eigrph->ack) == ep->sequence_number)
            {
              ep = eigrp_fifo_pop_tail(nbr->retrans_queue);
              eigrp_packet_free(ep);
              if (nbr->retrans_queue->count > 0)
                {
                  eigrp_send_packet_reliably(nbr);
                }
            }
        }

      type = stream_getw(s);
      if (type == TLV_INTERNAL_TYPE)
        {
          stream_set_getp(s, s->getp - sizeof(u_int16_t));

          tlv = eigrp_read_ipv4_tlv(s);

          //TU TREBA MSG!!!!!!!!!!!!!
          struct prefix_ipv4 *dest_addr;
          dest_addr = prefix_ipv4_new();
          dest_addr->prefix = tlv->destination;
          dest_addr->prefixlen = tlv->prefix_length;
          struct eigrp_prefix_entry *dest = eigrp_topology_table_lookup(
              eigrp->topology_table, dest_addr);

          struct eigrp_fsm_action_message *msg;
          msg = XCALLOC(MTYPE_EIGRP_FSM_MSG,
              sizeof(struct eigrp_fsm_action_message));
          struct eigrp_neighbor_entry *entry = eigrp_prefix_entry_lookup(
              dest->entries, nbr);

          msg->packet_type = EIGRP_MSG_REPLY;
          msg->data_type = TLV_INTERNAL_TYPE;
          msg->adv_router = nbr;
          msg->data.ipv4_int_type = tlv;
          msg->entry = entry;
          int event = eigrp_get_fsm_event(msg);
          EIGRP_FSM_EVENT_SCHEDULE(msg, event);

        }
    }
  eigrp_ack_send(nbr);
}

static int
eigrp_write(struct thread *thread)
{
  struct eigrp *eigrp = THREAD_ARG (thread);
  struct eigrp_interface *ei;
  struct eigrp_packet *ep;
  struct sockaddr_in sa_dst;
  struct ip iph;
  struct msghdr msg;
  struct iovec iov[2];

  int ret;
  int flags = 0;
  struct listnode *node;
#ifdef WANT_OSPF_WRITE_FRAGMENT
  static u_int16_t ipid = 0;
#endif /* WANT_OSPF_WRITE_FRAGMENT */
#define EIGRP_WRITE_IPHL_SHIFT 2

  eigrp->t_write = NULL;

  node = listhead (eigrp->oi_write_q);
  assert(node);
  ei = listgetdata (node);
  assert(ei);

#ifdef WANT_OSPF_WRITE_FRAGMENT
  /* seed ipid static with low order bits of time */
  if (ipid == 0)
  ipid = (time(NULL) & 0xffff);
#endif /* WANT_OSPF_WRITE_FRAGMENT */

  /* Get one packet from queue. */
  ep = eigrp_fifo_head(ei->obuf);
  assert(ep);
  assert(ep->length >= EIGRP_HEADER_SIZE);

  if (ep->dst.s_addr == htonl(EIGRP_MULTICAST_ADDRESS))
    eigrp_if_ipmulticast(eigrp, ei->address, ei->ifp->ifindex);

  memset(&iph, 0, sizeof(struct ip));
  memset(&sa_dst, 0, sizeof(sa_dst));

  sa_dst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sa_dst.sin_len = sizeof(sa_dst);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
  sa_dst.sin_addr = ep->dst;
  sa_dst.sin_port = htons(0);

  /* Set DONTROUTE flag if dst is unicast. */
  if (!IN_MULTICAST (htonl (ep->dst.s_addr)))
    flags = MSG_DONTROUTE;

  iph.ip_hl = sizeof(struct ip) >> EIGRP_WRITE_IPHL_SHIFT;
  /* it'd be very strange for header to not be 4byte-word aligned but.. */
  if (sizeof(struct ip) > (unsigned int) (iph.ip_hl << EIGRP_WRITE_IPHL_SHIFT))
    iph.ip_hl++; /* we presume sizeof struct ip cant overflow ip_hl.. */

  iph.ip_v = IPVERSION;
  iph.ip_tos = IPTOS_PREC_INTERNETCONTROL;
  iph.ip_len = (iph.ip_hl << EIGRP_WRITE_IPHL_SHIFT) + ep->length;

#if defined(__DragonFly__)
  /*
   * DragonFly's raw socket expects ip_len/ip_off in network byte order.
   */
  iph.ip_len = htons(iph.ip_len);
#endif

//#ifdef WANT_OSPF_WRITE_FRAGMENT
//  /* XXX-MT: not thread-safe at all..
//   * XXX: this presumes this is only programme sending OSPF packets
//   * otherwise, no guarantee ipid will be unique
//   */
//  iph.ip_id = ++ipid;
//#endif /* WANT_OSPF_WRITE_FRAGMENT */

  iph.ip_off = 0;

  iph.ip_ttl = EIGRP_IP_TTL;
  iph.ip_p = IPPROTO_EIGRPIGP;
  iph.ip_sum = 0;
  iph.ip_src.s_addr = ei->address->u.prefix4.s_addr;
  iph.ip_dst.s_addr = ep->dst.s_addr;

  memset(&msg, 0, sizeof(msg));
  msg.msg_name = (caddr_t) &sa_dst;
  msg.msg_namelen = sizeof(sa_dst);
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  iov[0].iov_base = (char*) &iph;
  iov[0].iov_len = iph.ip_hl << EIGRP_WRITE_IPHL_SHIFT;
  iov[1].iov_base = STREAM_PNT (ep->s);
  iov[1].iov_len = ep->length;

  /* Sadly we can not rely on kernels to fragment packets because of either
   * IP_HDRINCL and/or multicast destination being set.
   */
//#ifdef WANT_OSPF_WRITE_FRAGMENT
//  if ( ep->length > maxdatasize )
//    ospf_write_frags (ospf->fd, op, &iph, &msg, maxdatasize,
//                      oi->ifp->mtu, flags, type);
//#endif /* WANT_OSPF_WRITE_FRAGMENT */
  /* send final fragment (could be first) */
  sockopt_iphdrincl_swab_htosys(&iph);
  ret = sendmsg(eigrp->fd, &msg, flags);
  sockopt_iphdrincl_swab_systoh(&iph);

  if (ret < 0)
    zlog_warn("*** sendmsg in eigrp_write failed to %s, "
        "id %d, off %d, len %d, interface %s, mtu %u: %s",
        inet_ntoa(iph.ip_dst), iph.ip_id, iph.ip_off, iph.ip_len, ei->ifp->name,
        ei->ifp->mtu, safe_strerror(errno));

//  /* Show debug sending packet. */
//  if (IS_DEBUG_OSPF_PACKET (type - 1, SEND))
//    {
//      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
//        {
//          zlog_debug ("-----------------------------------------------------");
//          ospf_ip_header_dump (&iph);
//          stream_set_getp (op->s, 0);
//          ospf_packet_dump (op->s);
//        }
//
//      zlog_debug ("%s sent to [%s] via [%s].",
//                 LOOKUP (ospf_packet_type_str, type), inet_ntoa (op->dst),
//                 IF_NAME (oi));
//
//      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
//        zlog_debug ("-----------------------------------------------------");
//    }

  /* Now delete packet from queue. */
  eigrp_packet_delete(ei);

  if (eigrp_fifo_head(ei->obuf) == NULL)
    {
      ei->on_write_q = 0;
      list_delete_node(eigrp->oi_write_q, node);
    }

  /* If packets still remain in queue, call write thread. */
  if (!list_isempty (eigrp->oi_write_q))
    eigrp->t_write = thread_add_write (master, eigrp_write, eigrp, eigrp->fd);

  return 0;
}

/* Starting point of packet process function. */
int
eigrp_read(struct thread *thread)
{
  int ret;
  struct stream *ibuf;
  struct eigrp *eigrp;
  struct eigrp_interface *ei;
  struct ip *iph;
  struct eigrp_header *eigrph;
  u_int16_t length;
  struct interface *ifp;

  /* first of all get interface pointer. */
  eigrp = THREAD_ARG (thread);

  /* prepare for next packet. */
  eigrp->t_read = thread_add_read (master, eigrp_read, eigrp, eigrp->fd);

  stream_reset(eigrp->ibuf);
  if (!(ibuf = eigrp_recv_packet(eigrp->fd, &ifp, eigrp->ibuf)))
    return -1;
  /* This raw packet is known to be at least as big as its IP header. */

  /* Note that there should not be alignment problems with this assignment
   because this is at the beginning of the stream data buffer. */
  iph = (struct ip *) STREAM_DATA (ibuf);

  /* Note that sockopt_iphdrincl_swab_systoh was called in ospf_recv_packet. */

  if (ifp == NULL)
    /* Handle cases where the platform does not support retrieving the ifindex,
     and also platforms (such as Solaris 8) that claim to support ifindex
     retrieval but do not. */
    ifp = if_lookup_address(iph->ip_src);

  if (ifp == NULL)
    return 0;

  /* IP Header dump. */
//    if (IS_DEBUG_OSPF_PACKET(0, RECV))
//            eigrp_ip_header_dump (iph);
  /* Self-originated packet should be discarded silently. */
  if (eigrp_if_lookup_by_local_addr(eigrp, NULL, iph->ip_src))
    {
//      if (IS_DEBUG_OSPF_PACKET (0, RECV))
//        {
//          zlog_debug ("ospf_read[%s]: Dropping self-originated packet",
//                     inet_ntoa (iph->ip_src));
//        }
      return 0;
    }

  /* Advance from IP header to EIGRP header (iph->ip_hl has been verified
   by ospf_recv_packet() to be correct). */
  stream_forward_getp(ibuf, iph->ip_hl * 4);

  eigrph = (struct eigrp_header *) STREAM_PNT (ibuf);
//  if (MSG_OK != eigrp_packet_examin (eigrph, stream_get_endp (ibuf) - stream_get_getp (ibuf)))
//    return -1;
  /* Now it is safe to access all fields of OSPF packet header. */

  /* associate packet with eigrp interface */
  ei = eigrp_if_lookup_recv_if(eigrp, iph->ip_src, ifp);

  /* eigrp_verify_header() relies on a valid "ei" and thus can be called only
   after the hecks below are passed. These checks
   in turn access the fields of unverified "eigrph" structure for their own
   purposes and must remain very accurate in doing this. */
  if (!ei)
    return 0;

  /* If incoming interface is passive one, ignore it. */
  if (ei && EIGRP_IF_PASSIVE_STATUS (ei) == EIGRP_IF_PASSIVE)
    {
//      char buf[3][INET_ADDRSTRLEN];

//      if (IS_DEBUG_OSPF_EVENT)
//        zlog_debug ("ignoring packet from router %s sent to %s, "
//                    "received on a passive interface, %s",
//                    inet_ntop(AF_INET, &ospfh->router_id, buf[0], sizeof(buf[0])),
//                    inet_ntop(AF_INET, &iph->ip_dst, buf[1], sizeof(buf[1])),
//                    inet_ntop(AF_INET, &oi->address->u.prefix4,
//                              buf[2], sizeof(buf[2])));

      if (iph->ip_dst.s_addr == htonl(EIGRP_MULTICAST_ADDRESS))
        {
          /* Try to fix multicast membership.
           * Some OS:es may have problems in this area,
           * make sure it is removed.
           */
          EI_MEMBER_JOINED(ei, MEMBER_ALLROUTERS);
          eigrp_if_set_multicast(ei);
        }
      return 0;
    }

  /* else it must be a local eigrp interface, check it was received on
   * correct link
   */
  else if (ei->ifp != ifp)
    {
//      if (IS_DEBUG_OSPF_EVENT)
//        zlog_warn ("Packet from [%s] received on wrong link %s",
//                   inet_ntoa (iph->ip_src), ifp->name);
      return 0;
    }

  /* Verify more EIGRP header fields. */
  ret = eigrp_verify_header(ibuf, ei, iph, eigrph);
  if (ret < 0)
    {
//    if (IS_DEBUG_OSPF_PACKET (0, RECV))
//      zlog_debug ("ospf_read[%s]: Header check failed, "
//                  "dropping.",
//                  inet_ntoa (iph->ip_src));
      return ret;
    }

//      zlog_debug ("received from [%s] via [%s]",
//                 inet_ntoa (iph->ip_src), IF_NAME (ei));
//      zlog_debug (" src [%s],", inet_ntoa (iph->ip_src));
//      zlog_debug (" dst [%s]", inet_ntoa (iph->ip_dst));
//
//        zlog_debug ("-----------------------------------------------------");

  stream_forward_getp(ibuf, EIGRP_HEADER_SIZE);

  /* Read rest of the packet and call each sort of packet routine. */
  switch (eigrph->opcode)
    {
  case EIGRP_MSG_HELLO:
    eigrp_hello(iph, eigrph, ibuf, ei, length);
    break;
  case EIGRP_MSG_PROBE:
//      ospf_db_desc (iph, ospfh, ibuf, oi, length);
    break;
  case EIGRP_MSG_QUERY:
    eigrp_query(iph, eigrph, ibuf, ei, length);
    break;
  case EIGRP_MSG_REPLY:
    eigrp_reply(iph, eigrph, ibuf, ei, length);
    break;
  case EIGRP_MSG_REQUEST:
//      ospf_ls_ack (iph, ospfh, ibuf, oi, length);
    break;
  case EIGRP_MSG_SIAQUERY:
//          ospf_ls_ack (iph, ospfh, ibuf, oi, length);
    break;
  case EIGRP_MSG_SIAREPLY:
//          ospf_ls_ack (iph, ospfh, ibuf, oi, length);
    break;
  case EIGRP_MSG_UPDATE:
    eigrp_update(iph, eigrph, ibuf, ei, length);
    break;
  default:
    zlog(NULL, LOG_WARNING,
        "interface %s: EIGRP packet header type %d is illegal", IF_NAME (ei),
        eigrph->opcode);
    break;
    }

  return 0;
}

static struct stream *
eigrp_recv_packet(int fd, struct interface **ifp, struct stream *ibuf)
{
  int ret;
  struct ip *iph;
  u_int16_t ip_len;
  unsigned int ifindex = 0;
  struct iovec iov;
  /* Header and data both require alignment. */
  char buff[CMSG_SPACE(SOPT_SIZE_CMSG_IFINDEX_IPV4())];
  struct msghdr msgh;

  memset(&msgh, 0, sizeof(struct msghdr));
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;
  msgh.msg_control = (caddr_t) buff;
  msgh.msg_controllen = sizeof(buff);

  ret = stream_recvmsg(ibuf, fd, &msgh, 0, EIGRP_MAX_PACKET_SIZE + 1);
  if (ret < 0)
    {
      zlog_warn("stream_recvmsg failed: %s", safe_strerror(errno));
      return NULL;
    }
  if ((unsigned int) ret < sizeof(iph)) /* ret must be > 0 now */
    {
      zlog_warn("ospf_recv_packet: discarding runt packet of length %d "
          "(ip header size is %u)", ret, (u_int) sizeof(iph));
      return NULL;
    }

  /* Note that there should not be alignment problems with this assignment
   because this is at the beginning of the stream data buffer. */
  iph = (struct ip *) STREAM_DATA(ibuf);
  sockopt_iphdrincl_swab_systoh(iph);

  ip_len = iph->ip_len;

#if !defined(GNU_LINUX) && (OpenBSD < 200311) && (__FreeBSD_version < 1000000)
  /*
   * Kernel network code touches incoming IP header parameters,
   * before protocol specific processing.
   *
   *   1) Convert byteorder to host representation.
   *      --> ip_len, ip_id, ip_off
   *
   *   2) Adjust ip_len to strip IP header size!
   *      --> If user process receives entire IP packet via RAW
   *          socket, it must consider adding IP header size to
   *          the "ip_len" field of "ip" structure.
   *
   * For more details, see <netinet/ip_input.c>.
   */
  ip_len = ip_len + (iph->ip_hl << 2);
#endif

#if defined(__DragonFly__)
  /*
   * in DragonFly's raw socket, ip_len/ip_off are read
   * in network byte order.
   * As OpenBSD < 200311 adjust ip_len to strip IP header size!
   */
  ip_len = ntohs(iph->ip_len) + (iph->ip_hl << 2);
#endif

  ifindex = getsockopt_ifindex(AF_INET, &msgh);

  *ifp = if_lookup_by_index(ifindex);

  if (ret != ip_len)
    {
      zlog_warn("eigrp_recv_packet read length mismatch: ip_len is %d, "
          "but recvmsg returned %d", ip_len, ret);
      return NULL;
    }

  return ibuf;
}

///* Verify a complete OSPF packet for proper sizing/alignment. */
//static unsigned
//eigrp_packet_examin (struct eigrp_header * eh, const unsigned bytesonwire)
//{
////  unsigned ret;
//
//  /* Length, 1st approximation. */
//  if (bytesonwire < EIGRP_HEADER_SIZE)
//  {
////    if (IS_DEBUG_OSPF_PACKET (0, RECV))
////      zlog_debug ("%s: undersized (%u B) packet", __func__, bytesonwire);
//    return MSG_NG;
//  }
//  /* Now it is safe to access header fields. Performing length check, allow
//   * for possible extra bytes of crypto auth/padding, which are not counted
//   * in the OSPF header "length" field. */
//  if (eh->version != EIGRP_HEADER_VERSION)
//  {
////    if (IS_DEBUG_OSPF_PACKET (0, RECV))
////      zlog_debug ("%s: invalid (%u) protocol version", __func__, oh->version);
//    return MSG_NG;
//  }
//
//  switch (eh->opcode)
//  {
//  case EIGRP_MSG_HELLO:
//    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
//       by N>=0 router-IDs. */
//    break;
//  case EIGRP_MSG_PROBE:
//    /* RFC2328 A.3.3, packet header + OSPF_DB_DESC_MIN_SIZE bytes followed
//       by N>=0 header-only LSAs. */
//
//    break;
//  case EIGRP_MSG_QUERY:
//    /* RFC2328 A.3.4, packet header followed by N>=0 12-bytes request blocks. */
//    break;
//  case EIGRP_MSG_REPLY:
//    /* RFC2328 A.3.5, packet header + OSPF_LS_UPD_MIN_SIZE bytes followed
//       by N>=0 full LSAs (with N declared beforehand). */
//    break;
//  case EIGRP_MSG_REQUEST:
//    /* RFC2328 A.3.6, packet header followed by N>=0 header-only LSAs. */
//    break;
//  case EIGRP_MSG_SIAQUERY:
//    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
//       by N>=0 router-IDs. */
//
//    break;
//  case EIGRP_MSG_SIAREPLY:
//    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
//       by N>=0 router-IDs. */
//
//    break;
//  case EIGRP_MSG_UPDATE:
//    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
//       by N>=0 router-IDs. */
//
//    break;
//  default:
////    if (IS_DEBUG_OSPF_PACKET (0, RECV))
////      zlog_debug ("%s: invalid packet type 0x%02x", __func__, eh->opcode);
//    return MSG_NG;
//  }
////  if (ret != MSG_OK && IS_DEBUG_OSPF_PACKET (0, RECV))
////    zlog_debug ("%s: malformed %s packet", __func__, LOOKUP (ospf_packet_type_str, eh->opcode));
//  return MSG_OK;
//}

struct eigrp_fifo *
eigrp_fifo_new(void)
{
  struct eigrp_fifo *new;

  new = XCALLOC(MTYPE_EIGRP_FIFO, sizeof(struct eigrp_fifo));
  return new;
}

/* Free eigrp packet fifo. */
void
eigrp_fifo_free(struct eigrp_fifo *fifo)
{
  struct eigrp_packet *ep;
  struct eigrp_packet *next;

  for (ep = fifo->head; ep; ep = next)
    {
      next = ep->next;
      eigrp_packet_free(ep);
    }
  fifo->head = fifo->tail = NULL;
  fifo->count = 0;

  XFREE(MTYPE_EIGRP_FIFO, fifo);
}

/* Free eigrp fifo entries without destroying fifo itself*/
void
eigrp_fifo_reset(struct eigrp_fifo *fifo)
{
  struct eigrp_packet *ep;
  struct eigrp_packet *next;

  for (ep = fifo->head; ep; ep = next)
    {
      next = ep->next;
      eigrp_packet_free(ep);
    }
  fifo->head = fifo->tail = NULL;
  fifo->count = 0;
}

struct eigrp_packet *
eigrp_packet_new(size_t size)
{
  struct eigrp_packet *new;

  new = XCALLOC(MTYPE_EIGRP_PACKET, sizeof(struct eigrp_packet));
  new->s = stream_new(size);
  new->retrans_counter = 0;

  return new;
}

/* Send EIGRP Hello. */
void
eigrp_hello_send(struct eigrp_interface *ei)
{
//  /* If this is passive interface, do not send OSPF Hello. */
//  if (OSPF_IF_PASSIVE_STATUS (ei) == OSPF_IF_PASSIVE)
//    return;

  if (ei->type == EIGRP_IFTYPE_NBMA)
    {

    }
  else
    {
      eigrp_hello_send_sub(ei, htonl(EIGRP_MULTICAST_ADDRESS));
    }
}

void
eigrp_ack_send(struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep;

  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);
  /* Prepare EIGRP common header. */
  eigrp_make_header(EIGRP_MSG_HELLO, nbr->ei, ep->s, 0, 0,
      nbr->recv_sequence_number);

  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;

  ep->dst.s_addr = nbr->src.s_addr;

  /* Add packet to the top of the interface output queue*/
  eigrp_packet_add_top(nbr->ei, ep);

  /* Hook thread to write packet. */
  if (nbr->ei->on_write_q == 0)
    {
      listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
      nbr->ei->on_write_q = 1;
    }
  if (nbr->ei->eigrp->t_write == NULL)
    nbr->ei->eigrp->t_write =
        thread_add_write (master, eigrp_write, nbr->ei->eigrp, nbr->ei->eigrp->fd);
}

static void
eigrp_hello_send_sub(struct eigrp_interface *ei, in_addr_t addr)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new(ei->ifp->mtu);

  /* Prepare EIGRP common header. */
  eigrp_make_header(EIGRP_MSG_HELLO, ei, ep->s, 0, 0, 0);

  /* Prepare EIGRP Hello body. */
  length += eigrp_make_hello(ei, ep->s);

  /* EIGRP Checksum */
  eigrp_packet_checksum(ei, ep->s, length);

  /* Set packet length. */
  ep->length = length;

  ep->dst.s_addr = addr;

  /* Add packet to the top of the interface output queue*/
  eigrp_packet_add_top(ei, ep);

  /* Hook thread to write packet. */
  if (ei->on_write_q == 0)
    {
      listnode_add(ei->eigrp->oi_write_q, ei);
      ei->on_write_q = 1;
    }
  if (ei->eigrp->t_write == NULL)
    ei->eigrp->t_write =
        thread_add_write (master, eigrp_write, ei->eigrp, ei->eigrp->fd);
}

/*send EIGRP Update packet*/
void
eigrp_send_init_update(struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_make_header(EIGRP_MSG_UPDATE, nbr->ei, ep->s, EIGRP_HEADER_FLAG_INIT,
      nbr->ei->eigrp->sequence_number, nbr->recv_sequence_number);

  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;

  ep->dst.s_addr = nbr->src.s_addr;

  nbr->init_sequence_number = nbr->ei->eigrp->sequence_number;

  /*This ack number we await from neighbor*/
  ep->sequence_number = nbr->ei->eigrp->sequence_number;

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }
}

void
eigrp_send_EOT_update(struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep, *ep_multicast;
  u_int16_t length = EIGRP_HEADER_SIZE;
  struct eigrp_neighbor_entry *te;
  struct eigrp_prefix_entry *tn;
  struct listnode *node, *node2, *nnode, *nnode2;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_make_header(EIGRP_MSG_UPDATE, nbr->ei, ep->s, EIGRP_HEADER_FLAG_EOT,
      nbr->ei->eigrp->sequence_number, nbr->recv_sequence_number);

  for (ALL_LIST_ELEMENTS(nbr->ei->eigrp->topology_table, node, nnode, tn))
    {
      for (ALL_LIST_ELEMENTS(tn->entries, node2, nnode2, te))
        {
          if ((te->ei == nbr->ei)
              && (te->node->dest_type == EIGRP_TOPOLOGY_TYPE_REMOTE))
            continue;

          length += eigrp_add_internalTLV_to_stream(ep->s, te);
        }
    }

  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;

  ep->dst.s_addr = nbr->src.s_addr;

  /*This ack number we await from neighbor*/
  ep->sequence_number = nbr->ei->eigrp->sequence_number;

  ep_multicast = eigrp_packet_duplicate(ep, nbr);
  ep_multicast->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);
  eigrp_packet_add_top(nbr->ei, ep_multicast);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }

}

void
eigrp_send_packet_reliably(struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep;

  ep = eigrp_fifo_tail(nbr->retrans_queue);

  if (ep)
    {
      struct eigrp_packet *duplicate;
      duplicate = eigrp_packet_duplicate(ep, nbr);
      /* Add packet to the top of the interface output queue*/
      if (ep->dst.s_addr != htonl(EIGRP_MULTICAST_ADDRESS))
        eigrp_packet_add_top(nbr->ei, duplicate);

      /*Start retransmission timer*/
      THREAD_TIMER_ON(master, ep->t_retrans_timer, eigrp_unack_packet_retrans,
          nbr, EIGRP_PACKET_RETRANS_TIME);

      /*Increment sequence number counter*/
      nbr->ei->eigrp->sequence_number++;

      /* Hook thread to write packet. */
      if (nbr->ei->on_write_q == 0)
        {
          listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
          nbr->ei->on_write_q = 1;
        }
      if (nbr->ei->eigrp->t_write == NULL)
        nbr->ei->eigrp->t_write =
            thread_add_write (master, eigrp_write, nbr->ei->eigrp, nbr->ei->eigrp->fd);
    }
}

void
eigrp_send_reply(struct eigrp_neighbor *nbr, struct eigrp_neighbor_entry *te)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_make_header(EIGRP_MSG_REPLY, nbr->ei, ep->s, 0,
      nbr->ei->eigrp->sequence_number, 0);

  length += eigrp_add_internalTLV_to_stream(ep->s, te);

  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;

  ep->dst.s_addr = nbr->src.s_addr;

  /*This ack number we await from neighbor*/
  ep->sequence_number = nbr->ei->eigrp->sequence_number;

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }
}

void
eigrp_send_query(struct eigrp_neighbor *nbr, struct eigrp_neighbor_entry *te)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_make_header(EIGRP_MSG_QUERY, nbr->ei, ep->s, 0,
      nbr->ei->eigrp->sequence_number, 0);

  length += eigrp_add_internalTLV_to_stream(ep->s, te);

  listnode_add(te->node->rij, nbr);
  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;

  ep->dst.s_addr = nbr->src.s_addr;

  /*This ack number we await from neighbor*/
  ep->sequence_number = nbr->ei->eigrp->sequence_number;

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }
}

/* Calculate EIGRP checksum */
static void
eigrp_packet_checksum(struct eigrp_interface *ei, struct stream *s,
    u_int16_t length)
{
  struct eigrp_header *eigrph;

  eigrph = (struct eigrp_header *) STREAM_DATA (s);

  /* Calculate checksum. */
  eigrph->checksum = in_cksum(eigrph, length);
}

/* Make EIGRP header. */
static void
eigrp_make_header(int type, struct eigrp_interface *ei, struct stream *s,
    u_int32_t flags, u_int32_t sequence, u_int32_t ack)
{
  struct eigrp_header *eigrph;

  eigrph = (struct eigrp_header *) STREAM_DATA (s);

  eigrph->version = (u_char) EIGRP_HEADER_VERSION;
  eigrph->opcode = (u_char) type;

  eigrph->routerID = 0;

  eigrph->checksum = 0;

  eigrph->ASNumber = htons(ei->eigrp->AS);
  eigrph->ack = htonl(ack);
  eigrph->sequence = htonl(sequence);
  eigrph->flags = htonl(flags);

  stream_forward_endp(s, EIGRP_HEADER_SIZE);
}

/*Make EIGRP Hello body*/
static int
eigrp_make_hello(struct eigrp_interface *ei, struct stream *s)
{

  u_int16_t length = EIGRP_HELLO_MIN_SIZE;

  stream_putw(s, TLV_PARAMETER_TYPE);
  stream_putw(s, EIGRP_HELLO_MIN_SIZE);
  stream_putc(s, ei->eigrp->k_values[0]); /* K1 */
  stream_putc(s, ei->eigrp->k_values[1]); /* K2 */
  stream_putc(s, ei->eigrp->k_values[2]); /* K3 */
  stream_putc(s, ei->eigrp->k_values[3]); /* K4 */
  stream_putc(s, ei->eigrp->k_values[4]); /* K5 */
  stream_putc(s, ei->eigrp->k_values[5]); /* K6 */
  stream_putw(s, (u_int16_t) 15);

  return length;
}

static void
eigrp_packet_add_top(struct eigrp_interface *ei, struct eigrp_packet *ep)
{
  if (!ei->obuf)
    {
      zlog_err("eigrp_packet_add_top ERROR");
      return;
    }

  /* Add packet to head of queue. */
  eigrp_fifo_push_head(ei->obuf, ep);

  /* Debug of packet fifo*/
  /* ospf_fifo_debug (oi->obuf); */
}

/* Add new packet to head of fifo. */
static void
eigrp_fifo_push_head(struct eigrp_fifo *fifo, struct eigrp_packet *ep)
{
  ep->next = fifo->head;
  ep->previous = NULL;

  if (fifo->tail == NULL)
    fifo->tail = ep;

  if (fifo->count != 0)
    fifo->head->previous = ep;

  fifo->head = ep;

  fifo->count++;
}

/* Return first fifo entry. */
struct eigrp_packet *
eigrp_fifo_head(struct eigrp_fifo *fifo)
{
  return fifo->head;
}

/* Return last fifo entry. */
struct eigrp_packet *
eigrp_fifo_tail(struct eigrp_fifo *fifo)
{
  return fifo->tail;
}

void
eigrp_packet_delete(struct eigrp_interface *ei)
{
  struct eigrp_packet *ep;

  ep = eigrp_fifo_pop(ei->obuf);

  if (ep)
    eigrp_packet_free(ep);
}

/* Delete first packet from fifo. */
struct eigrp_packet *
eigrp_fifo_pop(struct eigrp_fifo *fifo)
{
  struct eigrp_packet *ep;

  ep = fifo->head;

  if (ep)
    {
      fifo->head = ep->next;

      if (fifo->head == NULL)
        fifo->tail = NULL;
      else
        fifo->head->previous = NULL;

      fifo->count--;
    }

  return ep;
}

void
eigrp_packet_free(struct eigrp_packet *ep)
{
  if (ep->s)
    stream_free(ep->s);

  THREAD_OFF(ep->t_retrans_timer);

  XFREE(MTYPE_EIGRP_PACKET, ep);

  ep = NULL;
}

/* EIGRP Header verification. */
static int
eigrp_verify_header(struct stream *ibuf, struct eigrp_interface *ei,
    struct ip *iph, struct eigrp_header *eigrph)
{

  /* Check network mask, Silently discarded. */
  if (!eigrp_check_network_mask(ei, iph->ip_src))
    {
      zlog_warn("interface %s: eigrp_read network address is not same [%s]",
          IF_NAME (ei), inet_ntoa(iph->ip_src));
      return -1;
    }
//
//  /* Check authentication. The function handles logging actions, where required. */
//  if (! ospf_check_auth (oi, ospfh))
//    return -1;

  return 0;
}

/* Unbound socket will accept any Raw IP packets if proto is matched.
 To prevent it, compare src IP address and i/f address with masking
 i/f network mask. */
static int
eigrp_check_network_mask(struct eigrp_interface *ei, struct in_addr ip_src)
{
  struct in_addr mask, me, him;

  if (ei->type == EIGRP_IFTYPE_POINTOPOINT)
    return 1;

  masklen2ip(ei->address->prefixlen, &mask);

  me.s_addr = ei->address->u.prefix4.s_addr & mask.s_addr;
  him.s_addr = ip_src.s_addr & mask.s_addr;

  if (IPV4_ADDR_SAME (&me, &him))
    return 1;

  return 0;
}

int
eigrp_unack_packet_retrans(struct thread *thread)
{
  struct eigrp_neighbor *nbr;
  nbr = (struct eigrp_neighbor *) THREAD_ARG(thread);

  struct eigrp_packet *ep;
  ep = eigrp_fifo_tail(nbr->retrans_queue);

  if (ep)
    {
      struct eigrp_packet *duplicate;
      duplicate = eigrp_packet_duplicate(ep, nbr);
      /* Add packet to the top of the interface output queue*/
      eigrp_packet_add_top(nbr->ei, duplicate);

      /*Start retransmission timer*/
      ep->t_retrans_timer =
          thread_add_timer(master, eigrp_unack_packet_retrans, nbr,EIGRP_PACKET_RETRANS_TIME);

      /*This ack number we await from neighbor*/
      ep->sequence_number = nbr->ei->eigrp->sequence_number;

      /*Increment sequence number counter*/
      nbr->ei->eigrp->sequence_number++;

      /* Hook thread to write packet. */
      if (nbr->ei->on_write_q == 0)
        {
          listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
          nbr->ei->on_write_q = 1;
        }
      if (nbr->ei->eigrp->t_write == NULL)
        nbr->ei->eigrp->t_write =
            thread_add_write (master, eigrp_write, nbr->ei->eigrp, nbr->ei->eigrp->fd);
    }

  return 0;
}

int
eigrp_unack_multicast_packet_retrans(struct thread *thread)
{
  struct eigrp_neighbor *nbr;
  nbr = (struct eigrp_neighbor *) THREAD_ARG(thread);

  struct eigrp_packet *ep;
  ep = eigrp_fifo_tail(nbr->multicast_queue);

  if (ep)
    {
      struct eigrp_packet *duplicate;
      duplicate = eigrp_packet_duplicate(ep, nbr);
      /* Add packet to the top of the interface output queue*/
      eigrp_packet_add_top(nbr->ei, duplicate);

      /*Start retransmission timer*/
      ep->t_retrans_timer =
          thread_add_timer(master, eigrp_unack_multicast_packet_retrans, nbr,EIGRP_PACKET_RETRANS_TIME);

      /*This ack number we await from neighbor*/
      ep->sequence_number = nbr->ei->eigrp->sequence_number;

      /*Increment sequence number counter*/
      nbr->ei->eigrp->sequence_number++;

      /* Hook thread to write packet. */
      if (nbr->ei->on_write_q == 0)
        {
          listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
          nbr->ei->on_write_q = 1;
        }
      if (nbr->ei->eigrp->t_write == NULL)
        nbr->ei->eigrp->t_write =
            thread_add_write (master, eigrp_write, nbr->ei->eigrp, nbr->ei->eigrp->fd);
    }

  return 0;
}

/* Get packet from tail of fifo. */
struct eigrp_packet *
eigrp_fifo_pop_tail(struct eigrp_fifo *fifo)
{
  struct eigrp_packet *ep;

  ep = fifo->tail;

  if (ep)
    {
      fifo->tail = ep->previous;

      if (fifo->tail == NULL)
        fifo->head = NULL;
      else
        fifo->tail->next = NULL;

      fifo->count--;
    }

  return ep;
}

struct eigrp_packet *
eigrp_packet_duplicate(struct eigrp_packet *old, struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *new;

  new = eigrp_packet_new(nbr->ei->ifp->mtu);
  new->length = old->length;
  new->retrans_counter = old->retrans_counter;
  new->dst = old->dst;
  new->sequence_number = old->sequence_number;
  stream_copy(new->s, old->s);

  return new;
}

struct TLV_IPv4_Internal_type *
eigrp_read_ipv4_tlv(struct stream *s)
{
  struct TLV_IPv4_Internal_type *tlv;

  tlv = XCALLOC(MTYPE_EIGRP_IPV4_INT_TLV,
      sizeof(struct TLV_IPv4_Internal_type));

  tlv->type = stream_getw(s);
  tlv->length = stream_getw(s);
  tlv->forward.s_addr = stream_getl(s);
  tlv->metric.delay = stream_getl(s);
  tlv->metric.bandwith = stream_getl(s);
  tlv->metric.mtu[0] = stream_getc(s);
  tlv->metric.mtu[1] = stream_getc(s);
  tlv->metric.mtu[2] = stream_getc(s);
  tlv->metric.hop_count = stream_getc(s);
  tlv->metric.reliability = stream_getc(s);
  tlv->metric.load = stream_getc(s);
  tlv->metric.tag = stream_getc(s);
  tlv->metric.flags = stream_getc(s);

  tlv->prefix_length = stream_getc(s);

  if (tlv->prefix_length <= 8)
    {
      tlv->destination_part[0] = stream_getc(s);
      tlv->destination.s_addr = (tlv->destination_part[0]);

    }
  if (tlv->prefix_length > 8 && tlv->prefix_length <= 16)
    {
      tlv->destination_part[0] = stream_getc(s);
      tlv->destination_part[1] = stream_getc(s);
      tlv->destination.s_addr = ((tlv->destination_part[1] << 8)
          + tlv->destination_part[0]);
    }
  if (tlv->prefix_length > 16 && tlv->prefix_length <= 24)
    {
      tlv->destination_part[0] = stream_getc(s);
      tlv->destination_part[1] = stream_getc(s);
      tlv->destination_part[2] = stream_getc(s);
      tlv->destination.s_addr = ((tlv->destination_part[2] << 16)
          + (tlv->destination_part[1] << 8) + tlv->destination_part[0]);
    }
  if (tlv->prefix_length > 24 && tlv->prefix_length <= 32)
    {
      tlv->destination_part[0] = stream_getc(s);
      tlv->destination_part[1] = stream_getc(s);
      tlv->destination_part[2] = stream_getc(s);
      tlv->destination_part[3] = stream_getc(s);
      tlv->destination.s_addr = ((tlv->destination_part[3] << 24)
          + (tlv->destination_part[2] << 16) + (tlv->destination_part[1] << 8)
          + tlv->destination_part[0]);
    }
  return tlv;
}

u_int16_t
eigrp_add_internalTLV_to_stream(struct stream *s,
    struct eigrp_neighbor_entry *te)
{
  u_int16_t length;

  stream_putw(s, TLV_INTERNAL_TYPE);
  if (te->node->destination->prefixlen <= 8)
    {
      stream_putw(s, 0x001A);
      length = 0x001A;
    }
  if ((te->node->destination->prefixlen > 8)
      && (te->node->destination->prefixlen <= 16))
    {
      stream_putw(s, 0x001B);
      length = 0x001B;
    }
  if ((te->node->destination->prefixlen > 16)
      && (te->node->destination->prefixlen <= 24))
    {
      stream_putw(s, 0x001C);
      length = 0x001C;
    }
  if (te->node->destination->prefixlen > 24)
    {
      stream_putw(s, 0x001D);
      length = 0x001D;
    }

  stream_putl(s, 0x00000000);

  /*Metric*/
  stream_putl(s, te->total_metric.delay);
  stream_putl(s, te->total_metric.bandwith);
  stream_putc(s, te->total_metric.mtu[2]);
  stream_putc(s, te->total_metric.mtu[1]);
  stream_putc(s, te->total_metric.mtu[0]);
  stream_putc(s, te->total_metric.hop_count);
  stream_putc(s, te->total_metric.reliability);
  stream_putc(s, te->total_metric.load);
  stream_putc(s, te->total_metric.tag);
  stream_putc(s, te->total_metric.flags);

  stream_putc(s, te->node->destination->prefixlen);

  if (te->node->destination->prefixlen <= 8)
    {
      stream_putc(s, te->node->destination->prefix.s_addr & 0xFF);
    }
  if ((te->node->destination->prefixlen > 8)
      && (te->node->destination->prefixlen <= 16))
    {
      stream_putc(s, te->node->destination->prefix.s_addr & 0xFF);
      stream_putc(s, (te->node->destination->prefix.s_addr >> 8) & 0xFF);
    }
  if ((te->node->destination->prefixlen > 16)
      && (te->node->destination->prefixlen <= 24))
    {
      stream_putc(s, te->node->destination->prefix.s_addr & 0xFF);
      stream_putc(s, (te->node->destination->prefix.s_addr >> 8) & 0xFF);
      stream_putc(s, (te->node->destination->prefix.s_addr >> 16) & 0xFF);
    }
  if (te->node->destination->prefixlen > 24)
    {
      stream_putc(s, te->node->destination->prefix.s_addr & 0xFF);
      stream_putc(s, (te->node->destination->prefix.s_addr >> 8) & 0xFF);
      stream_putc(s, (te->node->destination->prefix.s_addr >> 16) & 0xFF);
      stream_putc(s, (te->node->destination->prefix.s_addr >> 24) & 0xFF);
    }

  return length;

}

void
eigrp_update_send(struct eigrp_interface *ei, struct eigrp_neighbor_entry *te)
{
  struct eigrp_packet *ep, *duplicate;
  struct route_node *rn;
  struct eigrp_neighbor *nbr;

  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new(ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_make_header(EIGRP_MSG_UPDATE, ei, ep->s, 0, ei->eigrp->sequence_number,
      0);

  length += eigrp_add_internalTLV_to_stream(ep->s, te);

  /* EIGRP Checksum */
  eigrp_packet_checksum(ei, ep->s, length);
  ep->length = length;

  ep->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

  /*This ack number we await from neighbor*/
  ep->sequence_number = ei->eigrp->sequence_number;

  for (rn = route_top(ei->nbrs); rn; rn = route_next(rn))
    {
      nbr = rn->info;
      if (nbr->state == EIGRP_NEIGHBOR_UP)
        {
          duplicate = eigrp_packet_duplicate(ep, nbr);
          duplicate->dst.s_addr = nbr->src.s_addr;
          /*Put packet to retransmission queue*/
          eigrp_fifo_push_head(nbr->multicast_queue, duplicate);

          if (nbr->multicast_queue->count == 1)
            {
              /*Start retransmission timer*/
              THREAD_TIMER_ON(master, duplicate->t_retrans_timer,
                  eigrp_unack_multicast_packet_retrans, nbr,
                  EIGRP_PACKET_RETRANS_TIME);
            }
        }
    }

  eigrp_packet_add_top(ei, ep);

  /* Hook thread to write packet. */
  if (ei->on_write_q == 0)
    {
      listnode_add(ei->eigrp->oi_write_q, ei);
      ei->on_write_q = 1;
    }
  if (ei->eigrp->t_write == NULL)
    ei->eigrp->t_write =
        thread_add_write (master, eigrp_write, ei->eigrp, ei->eigrp->fd);

  ei->eigrp->sequence_number++;

}

void
eigrp_update_send_all(struct eigrp_neighbor_entry *te,
    struct eigrp_interface *exception)
{
  struct eigrp_interface *iface;
  struct listnode *node;

  for (ALL_LIST_ELEMENTS_RO(eigrp_lookup()->eiflist, node, iface))
    {
      if (iface != exception)
        eigrp_update_send(iface, te);
    }
}

void
eigrp_query_send_all(struct eigrp_neighbor_entry *te)
{
  struct eigrp_interface *iface;
  struct listnode *node;
  struct route_node *rn;
  struct eigrp_neighbor *nbr;

  for (ALL_LIST_ELEMENTS_RO(eigrp_lookup()->eiflist, node, iface))
    {
      for (rn = route_top(iface->nbrs); rn; rn = route_next(rn))
        {
          nbr = rn->info;
          eigrp_send_query(nbr, te);
        }
    }
}
