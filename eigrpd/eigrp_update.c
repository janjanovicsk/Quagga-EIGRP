/*
 * EIGRP Sending and Receiving EIGRP Update Packets.
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
#include "eigrpd/eigrp_macros.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"


/*
 * EIGRP UPDATE read function
 */
void
eigrp_update_receive (struct eigrp *eigrp, struct ip *iph, struct eigrp_header *eigrph,
                      struct stream * s, struct eigrp_interface *ei, int size)
{
  struct eigrp_neighbor *nbr;
  struct TLV_IPv4_Internal_type *tlv;
  struct eigrp_prefix_entry *tnode;
  struct eigrp_neighbor_entry *tentry;
  u_int32_t flags;
  u_int16_t type;
  uint16_t  length;

  /* increment statistics. */
  ei->update_in++;

  /* get neighbor struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph);

  /* neighbor must be valid, eigrp_nbr_get creates if none existed */
  assert(nbr);

  flags = ntohl(eigrph->flags);

  if (flags & EIGRP_CR_FLAG)
    {
      return;
    }

  nbr->recv_sequence_number = ntohl(eigrph->sequence);

  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Processing Update size[%u] int(%s) nbr(%s) seq [%u] flags [%0x]",
               size, ifindex2ifname(nbr->ei->ifp->ifindex),
               inet_ntoa(nbr->src),
               nbr->recv_sequence_number, flags);


  /*
   * We don't need to send separate Ack for INIT Update. INIT will be acked in EOT Update.
   */
  if ((nbr->state == EIGRP_NEIGHBOR_UP) && !(flags & EIGRP_INIT_FLAG))
    {
      eigrp_hello_send_ack(nbr);
    }

    if((flags & EIGRP_INIT_FLAG) && (nbr->state == EIGRP_NEIGHBOR_UP))
    {
        /*
         * TODO: RESET NEIGHBOR, HIS TOPOLOGY INFORMATION
         */

        eigrp_update_send_init(nbr);
    }

  /*If there is topology information*/
  while (s->endp > s->getp)
    {
      type = stream_getw(s);
      if (type == EIGRP_TLV_IPv4_INT)
        {
          stream_set_getp(s, s->getp - sizeof(u_int16_t));

          tlv = eigrp_read_ipv4_tlv(s);

          /*searching if destination exists */
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

              msg->packet_type = EIGRP_OPC_UPDATE;
              msg->eigrp = eigrp;
              msg->data_type = EIGRP_TLV_IPv4_INT;
              msg->adv_router = nbr;
              msg->data.ipv4_int_type = tlv;
              msg->entry = entry;
              msg->prefix = dest;
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
              tentry->reported_distance = eigrp_calculate_metrics(eigrp,
                  &tlv->metric);

              tentry->distance = eigrp_calculate_total_metrics(eigrp, tentry);

              tnode->fdistance = tnode->distance = tnode->rdistance =
                  tentry->distance;
              tentry->prefix = tnode;
              tentry->flags = EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG;

              eigrp_prefix_entry_add(eigrp->topology_table, tnode);
              eigrp_neighbor_entry_add(tnode, tentry);
              tnode->distance = tnode->fdistance = tnode->rdistance =
                  tentry->distance;
              tnode->reported_metric = tentry->total_metric;
              eigrp_topology_update_node_flags(tnode);
              eigrp_update_send_all(eigrp, tnode, ei);
            }
          XFREE(MTYPE_EIGRP_IPV4_INT_TLV, tlv);
        }
    }
}

/*send EIGRP Update packet*/
void
eigrp_update_send_init (struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_LEN;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Enqueuing Update Init Seq [%u] Ack [%u]",
               nbr->ei->eigrp->sequence_number,
               nbr->recv_sequence_number);

  eigrp_packet_header_init(EIGRP_OPC_UPDATE, nbr->ei, ep->s, EIGRP_INIT_FLAG,
                           nbr->ei->eigrp->sequence_number,
                           nbr->recv_sequence_number);

  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;
  ep->dst.s_addr = nbr->src.s_addr;

  /*This ack number we await from neighbor*/
  nbr->init_sequence_number = nbr->ei->eigrp->sequence_number;
  ep->sequence_number = nbr->ei->eigrp->sequence_number;
  nbr->ei->eigrp->sequence_number++;

  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Enqueuing Update Init Len [%u] Seq [%u] Dest [%s]",
               ep->length, ep->sequence_number, inet_ntoa(ep->dst));

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }
}

void
eigrp_update_send_EOT (struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep;
//  struct eigrp_packet *ep_multicast;
  u_int16_t length = EIGRP_HEADER_LEN;
  struct eigrp_neighbor_entry *te;
  struct eigrp_prefix_entry *tn;
  struct listnode *node, *node2, *nnode, *nnode2;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP EOT UPDATE header */
  eigrp_packet_header_init(EIGRP_OPC_UPDATE, nbr->ei, ep->s, EIGRP_EOT_FLAG,
                           nbr->ei->eigrp->sequence_number,
                           nbr->recv_sequence_number);

  for (ALL_LIST_ELEMENTS(nbr->ei->eigrp->topology_table, node, nnode, tn))
    {
      for (ALL_LIST_ELEMENTS(tn->entries, node2, nnode2, te))
        {
          if ((te->ei == nbr->ei)
              && (te->prefix->dest_type == EIGRP_TOPOLOGY_TYPE_REMOTE))
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

  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Enqueuing Update Init Len [%u] Seq [%u] Dest [%s]",
               ep->length, ep->sequence_number, inet_ntoa(ep->dst));

//  ep_multicast = eigrp_packet_duplicate(ep, nbr);
//  ep_multicast->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);
//  eigrp_fifo_push_head(nbr->ei->obuf, ep_multicast);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }

}

void
eigrp_update_send (struct eigrp_interface *ei, struct eigrp_prefix_entry *pe)
{
  struct eigrp_packet *ep, *duplicate;
  struct listnode *node, *nnode, *node2, *nnode2;
  struct eigrp_neighbor *nbr;
  struct eigrp_neighbor_entry *entry;

  u_int16_t length = EIGRP_HEADER_LEN;

  ep = eigrp_packet_new(ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_packet_header_init(EIGRP_OPC_UPDATE, ei, ep->s, 0,
                           ei->eigrp->sequence_number, 0);

  for (ALL_LIST_ELEMENTS(pe->entries, node2, nnode2, entry))
    {
      if ((entry->flags & EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG) == EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG)
        length += eigrp_add_internalTLV_to_stream(ep->s, entry);
    }

  /* EIGRP Checksum */
  eigrp_packet_checksum(ei, ep->s, length);
  ep->length = length;

  ep->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

  /*This ack number we await from neighbor*/
  ep->sequence_number = ei->eigrp->sequence_number;

  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Enqueuing Update length[%u] Seq [%u]",
               length, ep->sequence_number);

  for (ALL_LIST_ELEMENTS(ei->nbrs, node, nnode, nbr))
    {
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

  eigrp_fifo_push_head(ei->obuf, ep);

  /* Hook thread to write packet. */
  if (ei->on_write_q == 0)
    {
      listnode_add(ei->eigrp->oi_write_q, ei);
      ei->on_write_q = 1;
    }
  if (ei->eigrp->t_write == NULL)
    ei->eigrp->t_write =
        thread_add_write(master, eigrp_write, ei->eigrp, ei->eigrp->fd);

  ei->eigrp->sequence_number++;
}

void
eigrp_update_send_all (struct eigrp *eigrp, struct eigrp_prefix_entry *pe,
                       struct eigrp_interface *exception)
{
  /* TODO: Split horizont with poison reverse*/


  struct eigrp_interface *iface;
  struct listnode *node;

  for (ALL_LIST_ELEMENTS_RO(eigrp->eiflist, node, iface))
    {
      if (iface != exception)
        eigrp_update_send(iface, pe);
    }
}



