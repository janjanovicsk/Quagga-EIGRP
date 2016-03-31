/*
 * EIGRP Sending and Receiving EIGRP Update Packets.
 * Copyright (C) 2013-2015
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 *   Frantisek Gazo
 *   Tomas Hvorkovy
 *   Martin Kontsek
 *   Lukas Koribsky
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
#include "plist.h"
#include "routemap.h"

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


static void
remove_received_prefix_gr (struct list *nbr_prefixes, struct eigrp_prefix_entry *recv_prefix)
{
	//TODO: Needs implementation
	struct listnode *node1, *node11;
	struct eigrp_prefix_entry *prefix;

	for (ALL_LIST_ELEMENTS(nbr_prefixes, node1, node11, prefix))
	{
		if (prefix == recv_prefix)
		{
			listnode_delete(nbr_prefixes, prefix);
		}
	}
}

/*
 * EIGRP UPDATE read function
 */
void
eigrp_update_receive (struct eigrp *eigrp, struct ip *iph, struct eigrp_header *eigrph,
                      struct stream * s, struct eigrp_interface *ei, int size)
{
  struct eigrp_neighbor *nbr;
  struct TLV_IPv4_Internal_type *tlv;
  struct eigrp_prefix_entry *pe;
  struct eigrp_neighbor_entry *ne;
  u_int32_t flags;
  u_int16_t type;
  uint16_t  length;
  u_char same;
  struct access_list *alist;
  struct prefix_list *plist;
  struct eigrp *e;
  u_char graceful_restart;
  struct list *nbr_prefixes;
  int ret;

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

  same = 0;
  graceful_restart = 0;
  if((nbr->recv_sequence_number) == (ntohl(eigrph->sequence)))
      same = 1;

  nbr->recv_sequence_number = ntohl(eigrph->sequence);
  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Processing Update size[%u] int(%s) nbr(%s) seq [%u] flags [%0x]",
               size, ifindex2ifname(nbr->ei->ifp->ifindex),
               inet_ntoa(nbr->src),
               nbr->recv_sequence_number, flags);

  	/* Graceful restart Update received */
    if((flags == (EIGRP_INIT_FLAG+EIGRP_RS_FLAG+EIGRP_EOT_FLAG)) && (!same))
    {
    	//TODO: Needs implementation
		zlog_info("Neighbor %s (%s) is resync: peer graceful-restart",
				  inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
		/* get all prefixes from neighbor from topology table */
    	nbr_prefixes = eigrp_neighbor_prefixes_lookup(eigrp, nbr);
    	graceful_restart = 1;
    }
    else if((flags & EIGRP_INIT_FLAG) && (!same))
    {   /* When in pending state, send INIT update only if it wasn't
        already sent before (only if init_sequence is 0) */
        if((nbr->state == EIGRP_NEIGHBOR_PENDING) && (nbr->init_sequence_number == 0))
          eigrp_update_send_init(nbr);

        if (nbr->state == EIGRP_NEIGHBOR_UP)
          {
            eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_DOWN);
            eigrp_topology_neighbor_down(nbr->ei->eigrp,nbr);
            nbr->recv_sequence_number = ntohl(eigrph->sequence);
            zlog_info("Neighbor %s (%s) is down: peer restarted",
                      inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
            eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_PENDING);
            zlog_info("Neighbor %s (%s) is pending: new adjacency",
                      inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
            eigrp_update_send_init(nbr);
          }
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
          struct eigrp_prefix_entry *dest = eigrp_topology_table_lookup_ipv4(
              eigrp->topology_table, dest_addr);

          /*if exists it comes to DUAL*/
          if (dest != NULL)
            {
        	  /* remove received prefix from neighbor prefix list if in GR */
        	  if(graceful_restart)
        		  remove_received_prefix_gr(nbr_prefixes, dest);

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
              eigrp_fsm_event(msg, event);
            }
          else
            {
              /*Here comes topology information save*/
              pe = eigrp_prefix_entry_new();
              pe->serno = eigrp->serno;
              pe->destination_ipv4 = dest_addr;
              pe->af = AF_INET;
              pe->state = EIGRP_FSM_STATE_PASSIVE;
              pe->nt = EIGRP_TOPOLOGY_TYPE_REMOTE;

              ne = eigrp_neighbor_entry_new();
              ne->ei = ei;
              ne->adv_router = nbr;
              ne->reported_metric = tlv->metric;
              ne->reported_distance = eigrp_calculate_metrics(eigrp,
                  &tlv->metric);

              //TODO: Work in progress
              /*
               * Filtering
               */
        	  e = eigrp_lookup();
        	  /* get access-list from eigrp process */
        	  alist = e->list[EIGRP_FILTER_IN];
			  zlog_info("PROC IN Prefix: %s", inet_ntoa(dest_addr->prefix));
			  if (alist) {
				  zlog_info ("ALIST PROC IN: %s", alist->name);
			  } else {
				  zlog_info("ALIST PROC IN je prazdny");
			  }

			  /* Check if access-list fits */
			  if (alist && access_list_apply (alist,
						 (struct prefix *) dest_addr) == FILTER_DENY)
			  {
				  /* If yes, set reported metric to Max */
				  zlog_info("PROC alist IN: Skipping");
				  //ne->reported_metric.delay = EIGRP_MAX_METRIC;
				  zlog_info("PROC IN Prefix: %s", inet_ntoa(dest_addr->prefix));
				  eigrp_IPv4_InternalTLV_free (tlv);
				  continue;
			  } else {
				  zlog_info("PROC alist IN: NENastavujem metriku ");
			  }

			  plist = e->prefix[EIGRP_FILTER_IN];

			  if (plist) {
				  zlog_info ("PLIST PROC IN: %s", plist->name);
			  } else {
				  zlog_info("PLIST PROC IN je prazdny");
			  }

			  /* Check if prefix-list fits */
			  if (plist && prefix_list_apply (plist,
						 (struct prefix *) dest_addr) == FILTER_DENY)
			  {
				  /* If yes, set reported metric to Max */
				  zlog_info("PLIST PROC IN: Skipping");
				  //ne->reported_metric.delay = EIGRP_MAX_METRIC;
				  zlog_info("PLIST PROC IN Prefix: %s", inet_ntoa(dest_addr->prefix));
				  eigrp_IPv4_InternalTLV_free (tlv);
				  continue;
			  } else {
				  zlog_info("PLIST PROC IN: NENastavujem metriku ");
			  }

			  //Check route-map

			  /*if (e->routemap[EIGRP_FILTER_IN])
			  {
			  	 ret = route_map_apply (e->routemap[EIGRP_FILTER_IN],
			  				   (struct prefix *)dest_addr, RMAP_EIGRP, NULL);

			  	 if (ret == RMAP_DENYMATCH)
			  	 {
			  	    zlog_debug ("%s is filtered by route-map",inet_ntoa (dest_addr->prefix));
			  	    continue;
			  	 }
			  }*/

			  /*Get access-list from current interface */
			  zlog_info("Checking access_list on interface: %s",ei->ifp->name);
			  alist = ei->list[EIGRP_FILTER_IN];
			  if (alist) {
			  	  zlog_info ("ALIST INT IN: %s", alist->name);
			  } else {
			  	  zlog_info("ALIST INT IN je prazdny");
			  	}

			  /* Check if access-list fits */
			  if (alist && access_list_apply (alist, (struct prefix *) dest_addr) == FILTER_DENY)
			  {
				  /* If yes, set reported metric to Max */
			  	  zlog_info("INT alist IN: Skipping");
			  	  //ne->reported_metric.delay = EIGRP_MAX_METRIC;
			  	  zlog_info("INT IN Prefix: %s", inet_ntoa(dest_addr->prefix));
			  	  eigrp_IPv4_InternalTLV_free (tlv);
			  	  continue;
			  	} else {
			  	  zlog_info("INT IN: NENastavujem metriku ");
			  }

			  plist = ei->prefix[EIGRP_FILTER_IN];

			  if (plist) {
				  zlog_info ("PLIST INT IN: %s", plist->name);
			  } else {
				  zlog_info("PLIST INT IN je prazdny");
			  }

			  /* Check if prefix-list fits */
			  if (plist && prefix_list_apply (plist,
						 (struct prefix *) dest_addr) == FILTER_DENY)
			  {
				  /* If yes, set reported metric to Max */
				  zlog_info("PLIST INT IN: Skipping");
				  //ne->reported_metric.delay = EIGRP_MAX_METRIC;
				  zlog_info("PLIST INT IN Prefix: %s", inet_ntoa(dest_addr->prefix));
				  eigrp_IPv4_InternalTLV_free (tlv);
				  continue;
			  } else {
				  zlog_info("PLIST INT IN: NENastavujem metriku ");
			  }

			  //Check route-map

			  /*if (ei->routemap[EIGRP_FILTER_IN])
			  {
				 ret = route_map_apply (ei->routemap[EIGRP_FILTER_IN],
							   (struct prefix *)dest_addr, RMAP_EIGRP, NULL);

				 if (ret == RMAP_DENYMATCH)
				 {
					zlog_debug ("%s is filtered by route-map",inet_ntoa (dest_addr->prefix));
					continue;
				 }
			  }*/
			  /*
			   * End of filtering
			   */

			  ne->distance = eigrp_calculate_total_metrics(eigrp, ne);

			  zlog_info("<DEBUG PROC IN Distance: %x", ne->distance);
			  zlog_info("<DEBUG PROC IN Delay: %x", ne->total_metric.delay);

              pe->fdistance = pe->distance = pe->rdistance =
                  ne->distance;
              ne->prefix = pe;
              ne->flags = EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG;

              eigrp_prefix_entry_add(eigrp->topology_table, pe);
              eigrp_neighbor_entry_add(pe, ne);
              pe->distance = pe->fdistance = pe->rdistance = ne->distance;
              pe->reported_metric = ne->total_metric;
              eigrp_topology_update_node_flags(pe);

              pe->req_action |= EIGRP_FSM_NEED_UPDATE;
              listnode_add(eigrp->topology_changes_internalIPV4, pe);
            }
          eigrp_IPv4_InternalTLV_free (tlv);
        }
    }

    /* ask about prefixes not present in GR update */
    if(graceful_restart)
    {
    	//TODO: Needs implementation
    	struct listnode *node1;
		struct eigrp_prefix_entry *prefix;

		for (ALL_LIST_ELEMENTS_RO(nbr_prefixes, node1, prefix))
		{
			zlog_debug("***GR: Neighbor not advertised %s/%d",
					inet_ntoa(prefix->destination_ipv4->prefix),
					prefix->destination_ipv4->prefixlen);

			struct eigrp_fsm_action_message *fsm_msg;
			fsm_msg = XCALLOC(MTYPE_EIGRP_FSM_MSG,
			  sizeof(struct eigrp_fsm_action_message));
			struct eigrp_neighbor_entry *entry =
			  eigrp_prefix_entry_lookup(prefix->entries, nbr);

			/* set reported delay to MAX */
			entry->reported_metric.delay = EIGRP_MAX_METRIC;

			fsm_msg->packet_type = EIGRP_OPC_UPDATE;
			fsm_msg->eigrp = eigrp;
			fsm_msg->data_type = EIGRP_TLV_IPv4_INT;
			fsm_msg->adv_router = nbr;
			fsm_msg->data.ipv4_int_type = tlv;
			fsm_msg->entry = entry;
			fsm_msg->prefix = prefix;
			int event = eigrp_get_fsm_event(fsm_msg);
			eigrp_fsm_event(fsm_msg, event);
		}
    }

  /*
   * We don't need to send separate Ack for INIT Update. INIT will be acked in EOT Update.
   */
  if ((nbr->state == EIGRP_NEIGHBOR_UP) && !(flags == EIGRP_INIT_FLAG))
    {
      eigrp_hello_send_ack(nbr);
    }

  eigrp_query_send_all(eigrp);
  eigrp_update_send_all(eigrp, ei);
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

  // encode Authentication TLV, if needed
  if((IF_DEF_PARAMS (nbr->ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (nbr->ei->ifp)->auth_keychain != NULL))
    {
      length += eigrp_add_authTLV_MD5_to_stream(ep->s,nbr->ei);
      eigrp_make_md5_digest(nbr->ei,ep->s, EIGRP_AUTH_UPDATE_INIT_FLAG);
    }

  /* EIGRP Checksum */
  eigrp_packet_checksum(nbr->ei, ep->s, length);

  ep->length = length;
  ep->dst.s_addr = nbr->src.s_addr;

  /*This ack number we await from neighbor*/
  nbr->init_sequence_number = nbr->ei->eigrp->sequence_number;
  ep->sequence_number = nbr->ei->eigrp->sequence_number;
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
  struct eigrp_prefix_entry *pe;
  struct listnode *node, *node2, *nnode, *nnode2;
  struct access_list *alist;
  struct prefix_list *plist;
  struct access_list *alist_i;
  struct prefix_list *plist_i;
  struct eigrp *e;
  struct prefix_ipv4 *dest_addr;

  ep = eigrp_packet_new(nbr->ei->ifp->mtu);

  /* Prepare EIGRP EOT UPDATE header */
  eigrp_packet_header_init(EIGRP_OPC_UPDATE, nbr->ei, ep->s, EIGRP_EOT_FLAG,
                           nbr->ei->eigrp->sequence_number,
                           nbr->recv_sequence_number);

  // encode Authentication TLV, if needed
  if((IF_DEF_PARAMS (nbr->ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (nbr->ei->ifp)->auth_keychain != NULL))
    {
      length += eigrp_add_authTLV_MD5_to_stream(ep->s,nbr->ei);
    }

  for (ALL_LIST_ELEMENTS(nbr->ei->eigrp->topology_table, node, nnode, pe))
    {
      for (ALL_LIST_ELEMENTS(pe->entries, node2, nnode2, te))
        {
          if ((te->ei == nbr->ei)
              && (te->prefix->nt == EIGRP_TOPOLOGY_TYPE_REMOTE))
            continue;


          /* Get destination address from prefix */
		  dest_addr = pe->destination_ipv4;

		  /*
		   * Filtering
		   */
		  //TODO: Work in progress
		  /* get list from eigrp process */
		  e = eigrp_lookup();
		  /* Get access-lists and prefix-lists from process and interface */
		  alist = e->list[EIGRP_FILTER_OUT];
		  plist = e->prefix[EIGRP_FILTER_OUT];
		  alist_i = nbr->ei->list[EIGRP_FILTER_OUT];
		  plist_i = nbr->ei->prefix[EIGRP_FILTER_OUT];

		  /* Check if any list fits */
		  if ((alist && access_list_apply (alist,
					 (struct prefix *) dest_addr) == FILTER_DENY)||
				  (plist && prefix_list_apply (plist,
							(struct prefix *) dest_addr) == FILTER_DENY)||
				  (alist_i && access_list_apply (alist_i,
							(struct prefix *) dest_addr) == FILTER_DENY)||
				  (plist_i && prefix_list_apply (plist_i,
							(struct prefix *) dest_addr) == FILTER_DENY))
		  {
			  zlog_info("PROC OUT EOT: Skipping");
			  //pe->reported_metric.delay = EIGRP_MAX_METRIC;
			  zlog_info("PROC OUT EOT Prefix: %s", inet_ntoa(dest_addr->prefix));
			  continue;
		  } else {
			  zlog_info("PROC OUT EOT: NENastavujem metriku ");
			  length += eigrp_add_internalTLV_to_stream(ep->s, pe);
		  }
		  /*
		   * End of filtering
		   */

		  /* NULL the pointer */
		  dest_addr = NULL;

        }
    }

  if((IF_DEF_PARAMS (nbr->ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (nbr->ei->ifp)->auth_keychain != NULL))
    {
      eigrp_make_md5_digest(nbr->ei,ep->s, EIGRP_AUTH_UPDATE_FLAG);
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

  /*Put packet to retransmission queue*/
  eigrp_fifo_push_head(nbr->retrans_queue, ep);

  if (nbr->retrans_queue->count == 1)
    {
      eigrp_send_packet_reliably(nbr);
    }

}

void
eigrp_update_send (struct eigrp_interface *ei)
{
  struct eigrp_packet *ep;
  struct listnode *node, *nnode, *nnode2;
  struct eigrp_neighbor *nbr;
  struct eigrp_prefix_entry *pe;
  u_char has_tlv;
  struct access_list *alist;
  struct prefix_list *plist;
  struct access_list *alist_i;
  struct prefix_list *plist_i;
  struct eigrp *e;
  struct prefix_ipv4 *dest_addr;

  u_int16_t length = EIGRP_HEADER_LEN;

  ep = eigrp_packet_new(ei->ifp->mtu);

  /* Prepare EIGRP INIT UPDATE header */
  eigrp_packet_header_init(EIGRP_OPC_UPDATE, ei, ep->s, 0,
                           ei->eigrp->sequence_number, 0);

  // encode Authentication TLV, if needed
  if((IF_DEF_PARAMS (ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (ei->ifp)->auth_keychain != NULL))
    {
      length += eigrp_add_authTLV_MD5_to_stream(ep->s,ei);
    }

  has_tlv = 0;
  for (ALL_LIST_ELEMENTS(ei->eigrp->topology_changes_internalIPV4, node, nnode, pe))
    {
      if(pe->req_action & EIGRP_FSM_NEED_UPDATE)
        {

    	  /* Get destination address from prefix */
    	  dest_addr = pe->destination_ipv4;

    	  /*
		   * Filtering
		   */
    	  //TODO: Work in progress
		  /* get list from eigrp process */
		  e = eigrp_lookup();
		  /* Get access-lists and prefix-lists from process and interface */
		  alist = e->list[EIGRP_FILTER_OUT];
		  plist = e->prefix[EIGRP_FILTER_OUT];
		  alist_i = ei->list[EIGRP_FILTER_OUT];
		  plist_i = ei->prefix[EIGRP_FILTER_OUT];

		  /* Check if any list fits */
		  if ((alist && access_list_apply (alist,
					 (struct prefix *) dest_addr) == FILTER_DENY)||
				  (plist && prefix_list_apply (plist,
							 (struct prefix *) dest_addr) == FILTER_DENY)||
				  (alist_i && access_list_apply (alist_i,
							(struct prefix *) dest_addr) == FILTER_DENY)||
				  (plist_i && prefix_list_apply (plist_i,
							(struct prefix *) dest_addr) == FILTER_DENY))
		  {
			  zlog_info("PROC OUT: Skipping");
			  //pe->reported_metric.delay = EIGRP_MAX_METRIC;
			  zlog_info("PROC OUT Prefix: %s", inet_ntoa(dest_addr->prefix));
			  continue;
		  } else {
			  zlog_info("PROC OUT: NENastavujem metriku ");
			  length += eigrp_add_internalTLV_to_stream(ep->s, pe);
			  has_tlv = 1;
		  }
		  /*
		   * End of filtering
		   */

		  /* NULL the pointer */
		  dest_addr = NULL;

        }
    }

  if(!has_tlv)
    {
      eigrp_packet_free(ep);
      return;
    }

  if((IF_DEF_PARAMS (ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (ei->ifp)->auth_keychain != NULL))
    {
      eigrp_make_md5_digest(ei,ep->s, EIGRP_AUTH_UPDATE_FLAG);
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
          /*Put packet to retransmission queue*/
          eigrp_fifo_push_head(nbr->retrans_queue, ep);

          if (nbr->retrans_queue->count == 1)
            {
              eigrp_send_packet_reliably(nbr);
            }
        }
    }
}

void
eigrp_update_send_all (struct eigrp *eigrp, struct eigrp_interface *exception)
{

  struct eigrp_interface *iface;
  struct listnode *node, *node2, *nnode2;
  struct eigrp_prefix_entry *pe;

  for (ALL_LIST_ELEMENTS_RO(eigrp->eiflist, node, iface))
    {
      if (iface != exception)
        {
          eigrp_update_send(iface);
        }
    }

  for (ALL_LIST_ELEMENTS(eigrp->topology_changes_internalIPV4, node2, nnode2, pe))
    {
      if(pe->req_action & EIGRP_FSM_NEED_UPDATE)
        {
          pe->req_action &= ~EIGRP_FSM_NEED_UPDATE;
          listnode_delete(eigrp->topology_changes_internalIPV4, pe);
          zlog_debug("UPDATE COUNT: %d", eigrp->topology_changes_internalIPV4->count);
        }
    }
}

/**
 * @fn eigrp_update_send_GR
 *
 * @param[in]		nbr			Neighbor who would receive Graceful restart
 * @param[in]		is_manual 	True, if executed by manual command
 *
 * @return void
 *
 * @par
 * Function used for sending Graceful restart Update packet:
 * Creates Update packet with INIT, RS, EOT flags and include
 * all route except those filtered
 */
void
eigrp_update_send_GR (struct eigrp_neighbor *nbr, u_char is_manual)
{
	struct eigrp_packet *ep;
	u_int16_t length = EIGRP_HEADER_LEN;
	struct eigrp_neighbor_entry *te;
	struct eigrp_prefix_entry *pe;
	struct listnode *node, *node2, *nnode, *nnode2;
	struct access_list *alist;
	struct prefix_list *plist;
	struct access_list *alist_i;
	struct prefix_list *plist_i;
	struct eigrp *e;
	struct prefix_ipv4 *dest_addr;

	if(!is_manual)
	{
		/* function was called after applying filtration */
		zlog_info("Neighbor %s (%s) is resync: route configuration changed",
				  inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
	} else {
		/* Graceful restart was called manually */
		zlog_info("Neighbor %s (%s) is resync: manually cleared",
				  inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
	}

	ep = eigrp_packet_new(nbr->ei->ifp->mtu);

	/* Prepare EIGRP Graceful restart UPDATE header */
	eigrp_packet_header_init(EIGRP_OPC_UPDATE, nbr->ei, ep->s,
			EIGRP_INIT_FLAG + EIGRP_RS_FLAG + EIGRP_EOT_FLAG,
			nbr->ei->eigrp->sequence_number,
			nbr->recv_sequence_number);

	// encode Authentication TLV, if needed
	if((IF_DEF_PARAMS (nbr->ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (nbr->ei->ifp)->auth_keychain != NULL))
	{
		length += eigrp_add_authTLV_MD5_to_stream(ep->s,nbr->ei);
	}

	for (ALL_LIST_ELEMENTS(nbr->ei->eigrp->topology_table, node, nnode, pe))
	{
		for (ALL_LIST_ELEMENTS(pe->entries, node2, nnode2, te))
		{
			if ((te->ei == nbr->ei)
			  && (te->prefix->nt == EIGRP_TOPOLOGY_TYPE_REMOTE))
				continue;

			/* Get destination address from prefix */
			dest_addr = pe->destination_ipv4;

			/*
			* Filtering
			*/

			/* get list from eigrp process */
			e = eigrp_lookup();
			/* Get access-lists and prefix-lists from process and interface */
			alist = e->list[EIGRP_FILTER_OUT];
			plist = e->prefix[EIGRP_FILTER_OUT];
			alist_i = nbr->ei->list[EIGRP_FILTER_OUT];
			plist_i = nbr->ei->prefix[EIGRP_FILTER_OUT];

			/* Check if any list fits */
			if ((alist && access_list_apply (alist,
					 (struct prefix *) dest_addr) == FILTER_DENY)||
				  (plist && prefix_list_apply (plist,
							(struct prefix *) dest_addr) == FILTER_DENY)||
				  (alist_i && access_list_apply (alist_i,
							(struct prefix *) dest_addr) == FILTER_DENY)||
				  (plist_i && prefix_list_apply (plist_i,
							(struct prefix *) dest_addr) == FILTER_DENY))
			{
			  zlog_info("PROC OUT GR: Nastavujem metriku na MAX");
			  //pe->reported_metric.delay = EIGRP_MAX_METRIC;
			  zlog_info("PROC OUT GR Prefix: %s", inet_ntoa(dest_addr->prefix));
			  continue;
			} else {
			  zlog_info("PROC OUT GR: NENastavujem metriku ");
			  length += eigrp_add_internalTLV_to_stream(ep->s, pe);
			}
			/*
			* End of filtering
			*/

			/* NULL the pointer */
			dest_addr = NULL;

		}
	}

	if((IF_DEF_PARAMS (nbr->ei->ifp)->auth_type == EIGRP_AUTH_TYPE_MD5) && (IF_DEF_PARAMS (nbr->ei->ifp)->auth_keychain != NULL))
	{
		eigrp_make_md5_digest(nbr->ei,ep->s, EIGRP_AUTH_UPDATE_FLAG);
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

	/*Put packet to retransmission queue*/
	eigrp_fifo_push_head(nbr->retrans_queue, ep);

	if (nbr->retrans_queue->count == 1)
	{
		eigrp_send_packet_reliably(nbr);
	}
}
