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

static int
eigrp_neighbor_packet_queue_sum(struct eigrp_interface *ei)
{
  struct route_node *rn;
  struct eigrp_neighbor *nbr;
  int sum;
  sum = 0;

  for (rn = route_top (ei->nbrs); rn; rn = route_next (rn))
    {
      nbr = rn->info;
      sum += nbr->retrans_queue->count;
    }

  return sum;
}

/* Expects header to be in host order */
void
eigrp_ip_header_dump (struct ip *iph)
{
  /* IP Header dump. */
  zlog_debug ("ip_v %d", iph->ip_v);
  zlog_debug ("ip_hl %d", iph->ip_hl);
  zlog_debug ("ip_tos %d", iph->ip_tos);
  zlog_debug ("ip_len %d", iph->ip_len);
  zlog_debug ("ip_id %u", (u_int32_t) iph->ip_id);
  zlog_debug ("ip_off %u", (u_int32_t) iph->ip_off);
  zlog_debug ("ip_ttl %d", iph->ip_ttl);
  zlog_debug ("ip_p %d", iph->ip_p);
  zlog_debug ("ip_sum 0x%x", (u_int32_t) iph->ip_sum);
  zlog_debug ("ip_src %s",  inet_ntoa (iph->ip_src));
  zlog_debug ("ip_dst %s", inet_ntoa (iph->ip_dst));
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
eigrp_topology_ip_string (struct eigrp_topology_node *tn)
{
  static char buf[EIGRP_IF_STRING_MAXLEN] = "";
  u_int32_t ifaddr;

  ifaddr = ntohl (tn->destination->prefix.s_addr);
  snprintf (buf, EIGRP_IF_STRING_MAXLEN,
            "%d.%d.%d.%d",
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
            "%d.%d.%d.%d",
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
            "%d.%d.%d.%d",
            (ifaddr >> 24) & 0xff, (ifaddr >> 16) & 0xff,
            (ifaddr >> 8) & 0xff, ifaddr & 0xff);
  return buf;
}

void
show_ip_eigrp_interface_header (struct vty *vty)
{
  vty_out (vty, "%s%-20s %-7s %-12s %-7s %-14s %-12s %-10s%s %-26s %-13s %-7s %-14s %-12s %-8s%s",
           VTY_NEWLINE,
           "Interface", "Peers", "Xmit Queue", "Mean",
           "Pacing Time", "Multicast", "Pending",
           VTY_NEWLINE,"","Un/Reliable","SRTT","Un/Reliable","Flow Timer","Routes"
           ,VTY_NEWLINE);
}

void
show_ip_eigrp_interface_sub (struct vty *vty, struct eigrp *eigrp,
struct eigrp_interface *ei)
{
  vty_out (vty, "%-20s ", eigrp_if_name_string(ei));
  vty_out (vty, "%-7lu", route_table_count(ei->nbrs));
  vty_out (vty, "%d %c %-10d",0,'/',eigrp_neighbor_packet_queue_sum(ei));
  vty_out (vty, "%-7d %-14d %-12d %d%s",0,0,0,0,VTY_NEWLINE);
}

void
show_ip_eigrp_neighbor_header (struct vty *vty)
{
  vty_out (vty, "%s%-3s %-17s %-20s %-6s %-8s %-6s %-5s %-5s %-5s%s %-41s %-6s %-8s %-6s %-4s %-6s %-5s %s",
           VTY_NEWLINE,
           "H", "Address", "Interface", "Hold", "Uptime",
           "SRTT", "RTO", "Q", "Seq", VTY_NEWLINE
           ,"","(sec)","","(ms)","","Cnt","Num", VTY_NEWLINE);
}

void
show_ip_eigrp_neighbor_sub (struct vty *vty, struct eigrp_neighbor *nbr)
{

  vty_out (vty, "%-3d %-17s %-21s",0,eigrp_neigh_ip_string(nbr),eigrp_if_name_string(nbr->ei));
  vty_out (vty,"%-7lu",thread_timer_remain_second(nbr->t_holddown));
  vty_out (vty,"%-8d %-6d %-5d",0,0,EIGRP_PACKET_RETRANS_TIME);
  vty_out (vty,"%-7lu",nbr->retrans_queue->count);
  vty_out (vty,"%d%s",nbr->recv_sequence_number,VTY_NEWLINE);
}

void
show_ip_eigrp_topology_header (struct vty *vty)
{
	vty_out (vty, "%s%s%s%s%s%s%s",
	           VTY_NEWLINE,
	           "Codes: P - Passive, A - Active, U - Update, Q - Query, "
	           "R - Reply", VTY_NEWLINE ,"       ","r - reply Status, s - sia Status",VTY_NEWLINE,VTY_NEWLINE);
}

void
show_ip_eigrp_topology_node (struct vty *vty, struct eigrp_topology_node *tn)
{
    vty_out (vty, "%-3c",(tn->state > 0) ? 'A' : 'P');
    vty_out (vty, "%s/%d, ",inet_ntoa(tn->destination->prefix),tn->destination->prefixlen);
    vty_out (vty, "%d successors, ",1);
    vty_out (vty, "FD is %d%s",tn->fdistance, VTY_NEWLINE);

}

void
show_ip_eigrp_topology_entry (struct vty *vty, struct eigrp_topology_entry *te)
{
  if (te->parent->dest_type == EIGRP_TOPOLOGY_TYPE_CONNECTED)
    vty_out (vty, "%-7s%s, %s%s"," ","via Connected",eigrp_if_name_string(te->ei), VTY_NEWLINE);
  if (te->parent->dest_type == EIGRP_TOPOLOGY_TYPE_REMOTE)
    {
      vty_out (vty, "%-7s%s%s (%d/%d), %s%s"," ","via ",inet_ntoa(te->adv_router->src),te->distance, te->reported_distance, eigrp_if_name_string(te->ei), VTY_NEWLINE);
    }
}

