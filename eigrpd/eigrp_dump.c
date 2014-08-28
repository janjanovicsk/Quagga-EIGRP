/*
 * EIGRPd dump routine.
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <zebra.h>

#include "linklist.h"
#include "thread.h"
#include "prefix.h"
#include "command.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"
#include "table.h"

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_topology.h"

/* Enable debug option variables -- valid only session. */
unsigned long term_debug_eigrp = 0;
unsigned long term_debug_eigrp_nei = 0;
unsigned long term_debug_eigrp_packet[5] = {0, 0, 0, 0, 0};
unsigned long term_debug_eigrp_zebra = 0;

static int
eigrp_neighbor_packet_queue_sum (struct eigrp_interface *ei)
{
  struct eigrp_neighbor *nbr;
  struct listnode *node, *nnode;
  int sum;
  sum = 0;

  for (ALL_LIST_ELEMENTS (ei->nbrs, node, nnode, nbr))
    {
      sum += nbr->retrans_queue->count;
    }

  return sum;
}

/*
 * Expects header to be in host order
 */
void
eigrp_ip_header_dump (struct ip *iph)
{
  /* IP Header dump. */
  zlog_debug ("ip_v %u", iph->ip_v);
  zlog_debug ("ip_hl %u", iph->ip_hl);
  zlog_debug ("ip_tos %u", iph->ip_tos);
  zlog_debug ("ip_len %u", iph->ip_len);
  zlog_debug ("ip_id %u", (u_int32_t) iph->ip_id);
  zlog_debug ("ip_off %u", (u_int32_t) iph->ip_off);
  zlog_debug ("ip_ttl %u", iph->ip_ttl);
  zlog_debug ("ip_p %u", iph->ip_p);
  zlog_debug ("ip_sum 0x%x", (u_int32_t) iph->ip_sum);
  zlog_debug ("ip_src %s",  inet_ntoa (iph->ip_src));
  zlog_debug ("ip_dst %s", inet_ntoa (iph->ip_dst));
}

/*
 * Expects header to be in host order
 */
void
eigrp_header_dump (struct eigrp_header *eigrph)
{
  /* EIGRP Header dump. */
  zlog_debug ("eigrp_version %u",	eigrph->version);
  zlog_debug ("eigrp_opcode %u",	eigrph->opcode);
  zlog_debug ("eigrp_checksum 0x%x",	ntohs(eigrph->checksum));
  zlog_debug ("eigrp_flags 0x%x",	ntohl(eigrph->flags));
  zlog_debug ("eigrp_sequence %u",	ntohl(eigrph->sequence));
  zlog_debug ("eigrp_ack %u",		ntohl(eigrph->ack));
  zlog_debug ("eigrp_vrid %u"	,	ntohs(eigrph->vrid));
  zlog_debug ("eigrp_AS %u",		ntohs(eigrph->ASNumber));
}

const char *
eigrp_if_name_string (struct eigrp_interface *ei)
{
  static char buf[EIGRP_IF_STRING_MAXLEN] = "";

  if (!ei)
    return "inactive";

  snprintf (buf, EIGRP_IF_STRING_MAXLEN,
            "%s", ei->ifp->name);
  return buf;
}

const char *
eigrp_topology_ip_string (struct eigrp_prefix_entry *tn)
{
  static char buf[EIGRP_IF_STRING_MAXLEN] = "";
  u_int32_t ifaddr;

  ifaddr = ntohl (tn->destination->prefix.s_addr);
  snprintf (buf, EIGRP_IF_STRING_MAXLEN,
            "%u.%u.%u.%u",
            (ifaddr >> 24) & 0xff, (ifaddr >> 16) & 0xff,
            (ifaddr >> 8) & 0xff, ifaddr & 0xff);
  return buf;
}


const char *
eigrp_if_ip_string (struct eigrp_interface *ei)
{
  static char buf[EIGRP_IF_STRING_MAXLEN] = "";
  u_int32_t ifaddr;

  if (!ei)
    return "inactive";

  ifaddr = ntohl (ei->address->u.prefix4.s_addr);
  snprintf (buf, EIGRP_IF_STRING_MAXLEN,
            "%u.%u.%u.%u",
            (ifaddr >> 24) & 0xff, (ifaddr >> 16) & 0xff,
            (ifaddr >> 8) & 0xff, ifaddr & 0xff);

  return buf;
}

const char *
eigrp_neigh_ip_string (struct eigrp_neighbor *nbr)
{
  static char buf[EIGRP_IF_STRING_MAXLEN] = "";
  u_int32_t ifaddr;

  ifaddr = ntohl (nbr->src.s_addr);
  snprintf (buf, EIGRP_IF_STRING_MAXLEN,
            "%u.%u.%u.%u",
            (ifaddr >> 24) & 0xff, (ifaddr >> 16) & 0xff,
            (ifaddr >> 8) & 0xff, ifaddr & 0xff);

  return buf;
}

void
show_ip_eigrp_interface_header (struct vty *vty, struct eigrp *eigrp)
{

  vty_out (vty, "%s%s%d%s%s%s %-10s %-10s %-10s %-6s %-12s %-7s %-14s %-12s %-8s %-8s %-8s%s %-39s %-12s %-7s %-14s %-12s %-8s%s",
           VTY_NEWLINE,
           "EIGRP interfaces for AS(",eigrp->AS,")",VTY_NEWLINE,VTY_NEWLINE,
           "Interface", "Bandwidth", "Delay", "Peers", "Xmit Queue", "Mean",
           "Pacing Time", "Multicast", "Pending", "Hello", "Holdtime",
           VTY_NEWLINE,"","Un/Reliable","SRTT","Un/Reliable","Flow Timer","Routes",
           VTY_NEWLINE);
}

void
show_ip_eigrp_interface_sub (struct vty *vty, struct eigrp *eigrp,
			     struct eigrp_interface *ei)
{
  vty_out (vty, "%-11s ", eigrp_if_name_string (ei));
  vty_out (vty, "%-11u",IF_DEF_PARAMS (ei->ifp)->bandwidth);
  vty_out (vty, "%-11u",IF_DEF_PARAMS (ei->ifp)->delay);
  vty_out (vty, "%-7u", ei->nbrs->count);
  vty_out (vty, "%u %c %-10u",0,'/',eigrp_neighbor_packet_queue_sum (ei));
  vty_out (vty, "%-7u %-14u %-12u %-8u",0,0,0,0);
  vty_out (vty, "%-8u %-8u %s",IF_DEF_PARAMS (ei->ifp)->v_hello,IF_DEF_PARAMS (ei->ifp)->v_wait,VTY_NEWLINE);
}

void
show_ip_eigrp_interface_detail (struct vty *vty, struct eigrp *eigrp,
				struct eigrp_interface *ei)
{
  vty_out (vty, "%-2s %s %d %-3s %s","","Hello interval is ",0," sec",VTY_NEWLINE);
  vty_out (vty, "%-2s %s %s %s","","Next xmit serial","<none>",VTY_NEWLINE);
  vty_out (vty, "%-2s %s %d %s %d %s %d %s %d %s","","Un/reliable mcasts: ",0,"/",0,"Un/reliable ucasts: ",0,"/",0,VTY_NEWLINE);
  vty_out (vty, "%-2s %s %d %s %d %s %d %s","","Mcast exceptions: ",0,"  CR packets: ",0,"  ACKs supressed: ",0,VTY_NEWLINE);
  vty_out (vty, "%-2s %s %d %s %d %s","","Retransmissions sent: ",0,"Out-of-sequence rcvd: ",0,VTY_NEWLINE);
  vty_out (vty, "%-2s %s %s %s %s","","Authentication mode is ","not","set",VTY_NEWLINE);
  vty_out (vty, "%-2s %s %s","","Use multicast",VTY_NEWLINE);
}

void
show_ip_eigrp_neighbor_header (struct vty *vty, struct eigrp *eigrp)
{
  vty_out (vty, "%s%s%d%s%s%s%-3s %-17s %-20s %-6s %-8s %-6s %-5s %-5s %-5s%s %-41s %-6s %-8s %-6s %-4s %-6s %-5s %s",
           VTY_NEWLINE,
           "EIGRP neighbors for AS(",eigrp->AS,")",VTY_NEWLINE,VTY_NEWLINE,
           "H", "Address", "Interface", "Hold", "Uptime",
           "SRTT", "RTO", "Q", "Seq", VTY_NEWLINE
           ,"","(sec)","","(ms)","","Cnt","Num", VTY_NEWLINE);
}

void
show_ip_eigrp_neighbor_sub (struct vty *vty, struct eigrp_neighbor *nbr,
			    int detail)
{

  vty_out (vty, "%-3u %-17s %-21s",0,eigrp_neigh_ip_string (nbr),eigrp_if_name_string (nbr->ei));
  vty_out (vty,"%-7lu",thread_timer_remain_second (nbr->t_holddown));
  vty_out (vty,"%-8u %-6u %-5u",0,0,EIGRP_PACKET_RETRANS_TIME);
  vty_out (vty,"%-7lu",nbr->retrans_queue->count);
  vty_out (vty,"%u%s",nbr->recv_sequence_number,VTY_NEWLINE);


  if (detail)
    {
      vty_out(vty,"    Version %u.%u/%u.%u",
	      nbr->os_rel_major, nbr->os_rel_minor,
	      nbr->tlv_rel_major, nbr->tlv_rel_minor);
      vty_out(vty,", Retrans: %lu, Retries: %lu",
	      nbr->retrans_queue->count, 0UL);
      vty_out(vty,", %s%s", eigrp_nbr_state_str(nbr), VTY_NEWLINE);
    }
}

/*
 * Print standard header for show EIGRP topology output
 */
void
show_ip_eigrp_topology_header (struct vty *vty, struct eigrp *eigrp)
{
  struct in_addr router_id;
  router_id.s_addr = htonl(eigrp->router_id);

  vty_out (vty, "%s%s%d%s%s%s%s%s%s%s%s%s%s%s",
	     VTY_NEWLINE,
	     "EIGRP Topology Table for AS(", eigrp->AS, ")/ID(", inet_ntoa(router_id), ")", VTY_NEWLINE,VTY_NEWLINE,
	     "Codes: P - Passive, A - Active, U - Update, Q - Query, "
	     "R - Reply", VTY_NEWLINE ,"       ","r - reply Status, s - sia Status",VTY_NEWLINE,VTY_NEWLINE);
}

void
show_ip_eigrp_prefix_entry (struct vty *vty, struct eigrp_prefix_entry *tn)
{
  vty_out (vty, "%-3c",(tn->state > 0) ? 'A' : 'P');
  vty_out (vty, "%s/%u, ",inet_ntoa (tn->destination->prefix),tn->destination->prefixlen);
  vty_out (vty, "%u successors, ",eigrp_topology_get_successor_count (tn));
  vty_out (vty, "FD is %u%s",tn->fdistance, VTY_NEWLINE);

}

void
show_ip_eigrp_neighbor_entry (struct vty *vty, struct eigrp *eigrp, struct eigrp_neighbor_entry *te)
{
  if (te->adv_router == eigrp->neighbor_self)
    vty_out (vty, "%-7s%s, %s%s"," ","via Connected",eigrp_if_name_string (te->ei), VTY_NEWLINE);
  else
    {
      vty_out (vty, "%-7s%s%s (%u/%u), %s%s"," ","via ",inet_ntoa (te->adv_router->src),te->distance, te->reported_distance, eigrp_if_name_string (te->ei), VTY_NEWLINE);
    }
}

