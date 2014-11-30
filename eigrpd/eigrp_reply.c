/*
 * EIGRP Sending and Receiving EIGRP Reply Packets.
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
#include "keychain.h"

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

void
eigrp_send_reply (struct eigrp_neighbor *nbr, struct eigrp_neighbor_entry *te)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_LEN;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_packet_header_init(EIGRP_OPC_REPLY, nbr->ei, ep->s, 0,
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

/*EIGRP REPLY read function*/
void
eigrp_reply_receive (struct eigrp *eigrp, struct ip *iph, struct eigrp_header *eigrph,
                     struct stream * s, struct eigrp_interface *ei, int size)
{
  struct eigrp_neighbor *nbr;
  struct TLV_IPv4_Internal_type *tlv;

  u_int16_t type;

  /* increment statistics. */
  ei->reply_in++;

  /* get neighbor struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph);

  /* neighbor must be valid, eigrp_nbr_get creates if none existed */
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
      if (type == EIGRP_TLV_IPv4_INT)
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
          /*
           * Destination must exists
           */
          assert(dest);

          struct eigrp_fsm_action_message *msg;
          msg = XCALLOC(MTYPE_EIGRP_FSM_MSG,
              sizeof(struct eigrp_fsm_action_message));
          struct eigrp_neighbor_entry *entry = eigrp_prefix_entry_lookup(
              dest->entries, nbr);

          assert(entry); //testing

          msg->packet_type = EIGRP_OPC_REPLY;
          msg->eigrp = eigrp;
          msg->data_type = EIGRP_TLV_IPv4_INT;
          msg->adv_router = nbr;
          msg->data.ipv4_int_type = tlv;
          msg->entry = entry;
          msg->prefix = dest;
          int event = eigrp_get_fsm_event(msg);
          EIGRP_FSM_EVENT_SCHEDULE(msg, event);

          eigrp_IPv4_InternalTLV_free (tlv);
        }
    }
  eigrp_hello_send_ack(nbr);
}

