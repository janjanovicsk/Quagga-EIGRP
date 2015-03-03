/*
 * EIGRP Dump Functions and Debugging.
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

#include "linklist.h"
#include "thread.h"
#include "prefix.h"
#include "command.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"
#include "table.h"
#include "keychain.h"

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
unsigned long term_debug_eigrp_packet[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long term_debug_eigrp_zebra = 0;

/* Configuration debug option variables. */
unsigned long conf_debug_eigrp = 0;
unsigned long conf_debug_eigrp_nei = 0;
unsigned long conf_debug_eigrp_packet[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long conf_debug_eigrp_zebra = 0;


static int
config_write_debug (struct vty *vty)
{
  int write = 0;
  int i, r;

  const char *type_str[] = {"SIA-Query", "SIA-Reply", "ack", "hello", "probe", "query", "reply", "request",
		  	  	  	  	    "retransmission", "stub", "terse", "update", "all"};
  const char *detail_str[] = {"", " send", " recv", "", " detail",
							  " send detail", " recv detail", " detail"};


  /* debug eigrp event. */
//  if (IS_CONF_DEBUG_EIGRP (event, EVENT) == EIGRP_DEBUG_EVENT)
//  {
//      vty_out (vty, "debug eigrp event%s", VTY_NEWLINE);
//      write = 1;
//  }

  /* debug eigrp packet all detail. */
  r = EIGRP_DEBUG_SEND_RECV|EIGRP_DEBUG_DETAIL;
  for (i = 0; i < 12; i++)
      r &= conf_debug_eigrp_packet[i] & (EIGRP_DEBUG_SEND_RECV|EIGRP_DEBUG_DETAIL);
  if (r == (EIGRP_DEBUG_SEND_RECV|EIGRP_DEBUG_DETAIL))
  {
      vty_out (vty, "debug eigrp packet all detail%s", VTY_NEWLINE);
      return 1;
  }

  /* debug eigrp packet all. */
  r = EIGRP_DEBUG_SEND_RECV;
  for (i = 0; i < 12; i++)
      r &= conf_debug_eigrp_packet[i] & EIGRP_DEBUG_SEND_RECV;
  if (r == EIGRP_DEBUG_SEND_RECV)
  {
      vty_out (vty, "debug eigrp packet all%s", VTY_NEWLINE);
      for (i = 0; i < 12; i++)
    	  if (conf_debug_eigrp_packet[i] & EIGRP_DEBUG_DETAIL)
    	  vty_out (vty, "debug eigrp packet %s detail%s",
		  type_str[i],
		  VTY_NEWLINE);
      return 1;
  }

  /* debug eigrp packet */
  for (i = 0; i < 12; i++)
  {
      if (conf_debug_eigrp_packet[i] == 0)
    	  continue;

      	  vty_out (vty, "debug eigrp packet %s%s%s",
	      type_str[i], detail_str[conf_debug_eigrp_packet[i]],
	      VTY_NEWLINE);
      write = 1;
  }

	//  int write = 0;
	//  int i, r;
	//
	//  const char *type_str[] = {"hello", "dd", "ls-request", "ls-update", "ls-ack"};
	//  const char *detail_str[] = {"", " send", " recv", "", " detail",
	//			" send detail", " recv detail", " detail"};
	//
	//  /* debug ospf ism (status|events|timers). */
	//  if (IS_CONF_DEBUG_OSPF (ism, ISM) == OSPF_DEBUG_ISM)
	//    vty_out (vty, "debug ospf ism%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_CONF_DEBUG_OSPF (ism, ISM_STATUS))
	//	vty_out (vty, "debug ospf ism status%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (ism, ISM_EVENTS))
	//	vty_out (vty, "debug ospf ism event%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (ism, ISM_TIMERS))
	//	vty_out (vty, "debug ospf ism timer%s", VTY_NEWLINE);
	//    }
	//
	//  /* debug ospf nsm (status|events|timers). */
	//  if (IS_CONF_DEBUG_OSPF (nsm, NSM) == OSPF_DEBUG_NSM)
	//    vty_out (vty, "debug ospf nsm%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_CONF_DEBUG_OSPF (nsm, NSM_STATUS))
	//	vty_out (vty, "debug ospf nsm status%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (nsm, NSM_EVENTS))
	//	vty_out (vty, "debug ospf nsm event%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (nsm, NSM_TIMERS))
	//	vty_out (vty, "debug ospf nsm timer%s", VTY_NEWLINE);
	//    }
	//
	//  /* debug ospf lsa (generate|flooding|install|refresh). */
	//  if (IS_CONF_DEBUG_OSPF (lsa, LSA) == OSPF_DEBUG_LSA)
	//    vty_out (vty, "debug ospf lsa%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_CONF_DEBUG_OSPF (lsa, LSA_GENERATE))
	//	vty_out (vty, "debug ospf lsa generate%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (lsa, LSA_FLOODING))
	//	vty_out (vty, "debug ospf lsa flooding%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (lsa, LSA_INSTALL))
	//	vty_out (vty, "debug ospf lsa install%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (lsa, LSA_REFRESH))
	//	vty_out (vty, "debug ospf lsa refresh%s", VTY_NEWLINE);
	//
	//      write = 1;
	//    }
	//
	//  /* debug ospf zebra (interface|redistribute). */
	//  if (IS_CONF_DEBUG_OSPF (zebra, ZEBRA) == OSPF_DEBUG_ZEBRA)
	//    vty_out (vty, "debug ospf zebra%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_CONF_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
	//	vty_out (vty, "debug ospf zebra interface%s", VTY_NEWLINE);
	//      if (IS_CONF_DEBUG_OSPF (zebra, ZEBRA_REDISTRIBUTE))
	//	vty_out (vty, "debug ospf zebra redistribute%s", VTY_NEWLINE);
	//
	//      write = 1;
	//    }
	//
	//  /* debug ospf event. */
	//  if (IS_CONF_DEBUG_OSPF (event, EVENT) == OSPF_DEBUG_EVENT)
	//    {
	//      vty_out (vty, "debug ospf event%s", VTY_NEWLINE);
	//      write = 1;
	//    }
	//
	//  /* debug ospf nssa. */
	//  if (IS_CONF_DEBUG_OSPF (nssa, NSSA) == OSPF_DEBUG_NSSA)
	//    {
	//      vty_out (vty, "debug ospf nssa%s", VTY_NEWLINE);
	//      write = 1;
	//    }
	//
	//  /* debug ospf packet all detail. */
	//  r = OSPF_DEBUG_SEND_RECV|OSPF_DEBUG_DETAIL;
	//  for (i = 0; i < 5; i++)
	//    r &= conf_debug_ospf_packet[i] & (OSPF_DEBUG_SEND_RECV|OSPF_DEBUG_DETAIL);
	//  if (r == (OSPF_DEBUG_SEND_RECV|OSPF_DEBUG_DETAIL))
	//    {
	//      vty_out (vty, "debug ospf packet all detail%s", VTY_NEWLINE);
	//      return 1;
	//    }
	//
	//  /* debug ospf packet all. */
	//  r = OSPF_DEBUG_SEND_RECV;
	//  for (i = 0; i < 5; i++)
	//    r &= conf_debug_ospf_packet[i] & OSPF_DEBUG_SEND_RECV;
	//  if (r == OSPF_DEBUG_SEND_RECV)
	//    {
	//      vty_out (vty, "debug ospf packet all%s", VTY_NEWLINE);
	//      for (i = 0; i < 5; i++)
	//	if (conf_debug_ospf_packet[i] & OSPF_DEBUG_DETAIL)
	//	  vty_out (vty, "debug ospf packet %s detail%s",
	//		   type_str[i],
	//		   VTY_NEWLINE);
	//      return 1;
	//    }
	//
	//  /* debug ospf packet (hello|dd|ls-request|ls-update|ls-ack)
	//     (send|recv) (detail). */
	//  for (i = 0; i < 5; i++)
	//    {
	//      if (conf_debug_ospf_packet[i] == 0)
	//	continue;
	//
	//      vty_out (vty, "debug ospf packet %s%s%s",
	//	       type_str[i], detail_str[conf_debug_ospf_packet[i]],
	//	       VTY_NEWLINE);
	//      write = 1;
	//    }

  return write;
}


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

  ifaddr = ntohl (tn->destination_ipv4->prefix.s_addr);
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
  vty_out (vty, "%s/%u, ",inet_ntoa (tn->destination_ipv4->prefix),tn->destination_ipv4->prefixlen);
  vty_out (vty, "%u successors, ",eigrp_topology_get_successor(tn)->count);
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


DEFUN (show_debugging_eigrp,
       show_debugging_eigrp_cmd,
       "show debugging eigrp",
       SHOW_STR
       DEBUG_STR
       EIGRP_STR)
{
  int i;

  vty_out (vty, "EIGRP debugging status:%s", VTY_NEWLINE);

  /* Show debug status for events. */
  if (IS_DEBUG_EIGRP(event,EVENT))
    vty_out (vty, "  EIGRP event debugging is on%s", VTY_NEWLINE);


  /* Show debug status for EIGRP Packets. */
  for (i = 0; i < 12; i++)
  {
	if (IS_DEBUG_EIGRP_PACKET (i, SEND) && IS_DEBUG_EIGRP_PACKET (i, RECV))
	{
		vty_out (vty, "  EIGRP packet %s%s debugging is on%s",
		LOOKUP (eigrp_packet_type_str, i + 1),
		IS_DEBUG_EIGRP_PACKET (i, DETAIL) ? " detail" : "",
		VTY_NEWLINE);
	}
	else
	{
		if (IS_DEBUG_EIGRP_PACKET (i, SEND))
			vty_out (vty, "  EIGRP packet %s send%s debugging is on%s",
			LOOKUP (eigrp_packet_type_str, i + 1),
			IS_DEBUG_EIGRP_PACKET (i, DETAIL) ? " detail" : "",
			VTY_NEWLINE);
		if (IS_DEBUG_EIGRP_PACKET (i, RECV))
			vty_out (vty, "  EIGRP packet %s receive%s debugging is on%s",
			LOOKUP (eigrp_packet_type_str, i + 1),
			IS_DEBUG_EIGRP_PACKET (i, DETAIL) ? " detail" : "",
			VTY_NEWLINE);
	}
  }

	//  int i;
	//
	//  vty_out (vty, "EIGRP debugging status:%s", VTY_NEWLINE);
	//
	//  /* Show debug status for events. */
	//  if (IS_DEBUG_EIGRP(event,EVENT))
	//    vty_out (vty, "  OSPF event debugging is on%s", VTY_NEWLINE);
	//
	//  /* Show debug status for ISM. */
	//  if (IS_DEBUG_OSPF (ism, ISM) == OSPF_DEBUG_ISM)
	//    vty_out (vty, "  OSPF ISM debugging is on%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_DEBUG_OSPF (ism, ISM_STATUS))
	//	vty_out (vty, "  OSPF ISM status debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (ism, ISM_EVENTS))
	//	vty_out (vty, "  OSPF ISM event debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (ism, ISM_TIMERS))
	//	vty_out (vty, "  OSPF ISM timer debugging is on%s", VTY_NEWLINE);
	//    }
	//
	//  /* Show debug status for NSM. */
	//  if (IS_DEBUG_OSPF (nsm, NSM) == OSPF_DEBUG_NSM)
	//    vty_out (vty, "  OSPF NSM debugging is on%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_DEBUG_OSPF (nsm, NSM_STATUS))
	//	vty_out (vty, "  OSPF NSM status debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (nsm, NSM_EVENTS))
	//	vty_out (vty, "  OSPF NSM event debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
	//	vty_out (vty, "  OSPF NSM timer debugging is on%s", VTY_NEWLINE);
	//    }
	//
	//  /* Show debug status for OSPF Packets. */
	//  for (i = 0; i < 5; i++)
	//    if (IS_DEBUG_OSPF_PACKET (i, SEND) && IS_DEBUG_OSPF_PACKET (i, RECV))
	//      {
	//	vty_out (vty, "  OSPF packet %s%s debugging is on%s",
	//		 LOOKUP (ospf_packet_type_str, i + 1),
	//		 IS_DEBUG_OSPF_PACKET (i, DETAIL) ? " detail" : "",
	//		 VTY_NEWLINE);
	//      }
	//    else
	//      {
	//	if (IS_DEBUG_OSPF_PACKET (i, SEND))
	//	  vty_out (vty, "  OSPF packet %s send%s debugging is on%s",
	//		   LOOKUP (ospf_packet_type_str, i + 1),
	//		   IS_DEBUG_OSPF_PACKET (i, DETAIL) ? " detail" : "",
	//		   VTY_NEWLINE);
	//	if (IS_DEBUG_OSPF_PACKET (i, RECV))
	//	  vty_out (vty, "  OSPF packet %s receive%s debugging is on%s",
	//		   LOOKUP (ospf_packet_type_str, i + 1),
	//		   IS_DEBUG_OSPF_PACKET (i, DETAIL) ? " detail" : "",
	//		   VTY_NEWLINE);
	//      }
	//
	//  /* Show debug status for OSPF LSAs. */
	//  if (IS_DEBUG_OSPF (lsa, LSA) == OSPF_DEBUG_LSA)
	//    vty_out (vty, "  OSPF LSA debugging is on%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
	//	vty_out (vty, "  OSPF LSA generation debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (lsa, LSA_FLOODING))
	//	vty_out (vty, "  OSPF LSA flooding debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (lsa, LSA_INSTALL))
	//	vty_out (vty, "  OSPF LSA install debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (lsa, LSA_REFRESH))
	//	vty_out (vty, "  OSPF LSA refresh debugging is on%s", VTY_NEWLINE);
	//    }
	//
	//  /* Show debug status for Zebra. */
	//  if (IS_DEBUG_OSPF (zebra, ZEBRA) == OSPF_DEBUG_ZEBRA)
	//    vty_out (vty, "  OSPF Zebra debugging is on%s", VTY_NEWLINE);
	//  else
	//    {
	//      if (IS_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
	//	vty_out (vty, "  OSPF Zebra interface debugging is on%s", VTY_NEWLINE);
	//      if (IS_DEBUG_OSPF (zebra, ZEBRA_REDISTRIBUTE))
	//	vty_out (vty, "  OSPF Zebra redistribute debugging is on%s", VTY_NEWLINE);
	//    }
	//
	//  /* Show debug status for NSSA. */
	//  if (IS_DEBUG_OSPF (nssa, NSSA) == OSPF_DEBUG_NSSA)
	//    vty_out (vty, "  OSPF NSSA debugging is on%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}


/*
   [no] debug eigrp packet (hello|dd|ls-request|ls-update|ls-ack|all)
                          [send|recv [detail]]
*/
DEFUN (debug_eigrp_packet,
       debug_eigrp_packet_all_cmd,
       "debug eigrp packet (SIAquery|SIAreply|ack|hello|probe|query|reply|request|retry|stub|terse|update|all)",
       DEBUG_STR
       EIGRP_STR
       "EIGRP SIA-Query packets\n"
       "EIGRP SIA-Reply packets\n"
       "EIGRP ack packets\n"
       "EIGRP hello packets\n"
       "EIGRP probe packets\n"
       "EIGRP query packets\n"
       "EIGRP reply packets\n"
       "EIGRP request packets\n"
       "EIGRP retransmissions\n"
       "EIGRP stub packets\n"
       "Display all EIGRP packets except Hellos\n"
       "EIGRP update packets\n"
       "Display all EIGRP packets\n")
{
  int type = 0;
  int flag = 0;
  int i;

  assert (argc > 0);

  /* Check packet type. */
  if (strncmp (argv[0], "h", 1) == 0)
    type = EIGRP_DEBUG_HELLO;
  if (strncmp (argv[0], "u", 1) == 0)
    type = EIGRP_DEBUG_UPDATE;
  if (strncmp (argv[0], "q", 1) == 0)
    type = EIGRP_DEBUG_QUERY;
  if (strncmp (argv[0], "a", 1) == 0)
    type = EIGRP_DEBUG_ACK;
  if (strncmp (argv[0], "p", 1) == 0)
    type = EIGRP_DEBUG_PROBE;
  if (strncmp (argv[0], "st", 2) == 0)
    type = EIGRP_DEBUG_STUB;
  if (strncmp (argv[0], "rep", 3) == 0)
    type = EIGRP_DEBUG_REPLY;
  if (strncmp (argv[0], "req", 3) == 0)
    type = EIGRP_DEBUG_REQUEST;
  if (strncmp (argv[0], "siaq", 4) == 0)
    type = EIGRP_DEBUG_SIAQUERY;
  if (strncmp (argv[0], "siar", 4) == 0)
    type = EIGRP_DEBUG_SIAREPLY;


  /* Default, both send and recv. */
  if (argc == 1)
    flag = EIGRP_DEBUG_SEND_RECV;

  /* send or recv. */
  if (argc >= 2)
  {
    if (strncmp (argv[1], "s", 1) == 0)
	  flag = EIGRP_DEBUG_SEND;
    else if (strncmp (argv[1], "r", 1) == 0)
	  flag = EIGRP_DEBUG_RECV;
    else if (strncmp (argv[1], "d", 1) == 0)
	  flag = EIGRP_DEBUG_SEND_RECV | EIGRP_DEBUG_PACKET_DETAIL;
  }

  /* detail. */
  if (argc == 3)
    if (strncmp (argv[2], "d", 1) == 0)
      flag |= EIGRP_DEBUG_PACKET_DETAIL;

  for (i = 0; i < 10; i++)
    if (type & (0x01 << i))
    {
    	if (vty->node == CONFIG_NODE)
		  DEBUG_PACKET_ON (i, flag);
		else
		  TERM_DEBUG_PACKET_ON (i, flag);
    }

  return CMD_SUCCESS;
}

ALIAS (debug_eigrp_packet,
       debug_ospf_packet_send_recv_cmd,
       "debug eigrp packet (SIAquery|SIAreply|ack|hello|probe|query|reply|request|retry|stub|terse|update|all) (send|recv|detail)",
       DEBUG_STR
       EIGRP_STR
       "EIGRP SIA-Query packets\n"
       "EIGRP SIA-Reply packets\n"
       "EIGRP ack packets\n"
       "EIGRP hello packets\n"
       "EIGRP probe packets\n"
       "EIGRP query packets\n"
       "EIGRP reply packets\n"
       "EIGRP request packets\n"
       "EIGRP retransmissions\n"
       "EIGRP stub packets\n"
       "Display all EIGRP packets except Hellos\n"
       "EIGRP update packets\n"
       "Display all EIGRP packets\n"
       "Packet sent\n"
       "Packet received\n"
       "Detail information\n")

ALIAS (debug_eigrp_packet,
       debug_eigrp_packet_send_recv_detail_cmd,
       "debug eigrp packet (SIAquery|SIAreply|ack|hello|probe|query|reply|request|retry|stub|terse|update|all) (send|recv) (detail|)",
       DEBUG_STR
       EIGRP_STR
       "EIGRP SIA-Query packets\n"
       "EIGRP SIA-Reply packets\n"
       "EIGRP ack packets\n"
       "EIGRP hello packets\n"
       "EIGRP probe packets\n"
       "EIGRP query packets\n"
       "EIGRP reply packets\n"
       "EIGRP request packets\n"
       "EIGRP retransmissions\n"
       "EIGRP stub packets\n"
       "Display all EIGRP packets except Hellos\n"
       "EIGRP update packets\n"
       "Display all EIGRP packets\n"
       "Packet sent\n"
       "Packet received\n"
       "Detail Information\n")


DEFUN (no_debug_eigrp_packet,
	  no_debug_eigrp_packet_all_cmd,
	  "debug eigrp packet (SIAquery|SIAreply|ack|hello|probe|query|reply|request|retry|stub|terse|update|all)",
	  DEBUG_STR
	  EIGRP_STR
	  "EIGRP SIA-Query packets\n"
	  "EIGRP SIA-Reply packets\n"
	  "EIGRP ack packets\n"
	  "EIGRP hello packets\n"
	  "EIGRP probe packets\n"
	  "EIGRP query packets\n"
	  "EIGRP reply packets\n"
	  "EIGRP request packets\n"
	  "EIGRP retransmissions\n"
	  "EIGRP stub packets\n"
	  "Display all EIGRP packets except Hellos\n"
	  "EIGRP update packets\n"
	  "Display all EIGRP packets\n")
{
 int type = 0;
 int flag = 0;
 int i;

 assert (argc > 0);

 /* Check packet type. */
 if (strncmp (argv[0], "h", 1) == 0)
   type = EIGRP_DEBUG_HELLO;
 if (strncmp (argv[0], "u", 1) == 0)
   type = EIGRP_DEBUG_UPDATE;
 if (strncmp (argv[0], "q", 1) == 0)
   type = EIGRP_DEBUG_QUERY;
 if (strncmp (argv[0], "a", 1) == 0)
   type = EIGRP_DEBUG_ACK;
 if (strncmp (argv[0], "p", 1) == 0)
   type = EIGRP_DEBUG_PROBE;
 if (strncmp (argv[0], "st", 2) == 0)
   type = EIGRP_DEBUG_STUB;
 if (strncmp (argv[0], "rep", 3) == 0)
   type = EIGRP_DEBUG_REPLY;
 if (strncmp (argv[0], "req", 3) == 0)
   type = EIGRP_DEBUG_REQUEST;
 if (strncmp (argv[0], "siaq", 4) == 0)
   type = EIGRP_DEBUG_SIAQUERY;
 if (strncmp (argv[0], "siar", 4) == 0)
   type = EIGRP_DEBUG_SIAREPLY;


 /* Default, both send and recv. */
 if (argc == 1)
   flag = EIGRP_DEBUG_SEND_RECV;

 /* send or recv. */
 if (argc >= 2)
 {
   if (strncmp (argv[1], "s", 1) == 0)
  flag = EIGRP_DEBUG_SEND;
   else if (strncmp (argv[1], "r", 1) == 0)
  flag = EIGRP_DEBUG_RECV;
   else if (strncmp (argv[1], "d", 1) == 0)
  flag = EIGRP_DEBUG_SEND_RECV | EIGRP_DEBUG_PACKET_DETAIL;
 }

 /* detail. */
 if (argc == 3)
   if (strncmp (argv[2], "d", 1) == 0)
	 flag |= EIGRP_DEBUG_PACKET_DETAIL;

 for (i = 0; i < 10; i++)
   if (type & (0x01 << i))
   {
	if (vty->node == CONFIG_NODE)
	  DEBUG_PACKET_OFF (i, flag);
	else
	  TERM_DEBUG_PACKET_OFF (i, flag);
   }

 return CMD_SUCCESS;
}

ALIAS (no_debug_eigrp_packet,
	  no_debug_ospf_packet_send_recv_cmd,
	  "debug eigrp packet (SIAquery|SIAreply|ack|hello|probe|query|reply|request|retry|stub|terse|update|all) (send|recv|detail)",
	  DEBUG_STR
	  EIGRP_STR
	  "EIGRP SIA-Query packets\n"
	  "EIGRP SIA-Reply packets\n"
	  "EIGRP ack packets\n"
	  "EIGRP hello packets\n"
	  "EIGRP probe packets\n"
	  "EIGRP query packets\n"
	  "EIGRP reply packets\n"
	  "EIGRP request packets\n"
	  "EIGRP retransmissions\n"
	  "EIGRP stub packets\n"
	  "Display all EIGRP packets except Hellos\n"
	  "EIGRP update packets\n"
	  "Display all EIGRP packets\n"
	  "Packet sent\n"
	  "Packet received\n"
	  "Detail information\n")

ALIAS (no_debug_eigrp_packet,
	  no_debug_eigrp_packet_send_recv_detail_cmd,
	  "debug eigrp packet (SIAquery|SIAreply|ack|hello|probe|query|reply|request|retry|stub|terse|update|all) (send|recv) (detail|)",
	  DEBUG_STR
	  EIGRP_STR
	  "EIGRP SIA-Query packets\n"
	  "EIGRP SIA-Reply packets\n"
	  "EIGRP ack packets\n"
	  "EIGRP hello packets\n"
	  "EIGRP probe packets\n"
	  "EIGRP query packets\n"
	  "EIGRP reply packets\n"
	  "EIGRP request packets\n"
	  "EIGRP retransmissions\n"
	  "EIGRP stub packets\n"
	  "Display all EIGRP packets except Hellos\n"
	  "EIGRP update packets\n"
	  "Display all EIGRP packets\n"
	  "Packet sent\n"
	  "Packet received\n"
	  "Detail Information\n")



/* Debug node. */
static struct cmd_node eigrp_debug_node =
{
  DEBUG_NODE,
  "",
  1 /* VTYSH */
};

/* Initialize debug commands. */
void
eigrp_debug_init ()
{
  install_node (&eigrp_debug_node, config_write_debug);

  install_element (ENABLE_NODE, &show_debugging_eigrp_cmd);
  install_element (ENABLE_NODE, &debug_eigrp_packet_all_cmd);
  install_element (ENABLE_NODE, &no_debug_eigrp_packet_all_cmd);
  install_element (ENABLE_NODE, &debug_ospf_packet_send_recv_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_packet_send_recv_cmd);
  install_element (ENABLE_NODE, &debug_eigrp_packet_send_recv_detail_cmd);
  install_element (ENABLE_NODE, &no_debug_eigrp_packet_send_recv_detail_cmd);

  install_element (CONFIG_NODE, &show_debugging_eigrp_cmd);
  install_element (CONFIG_NODE, &debug_eigrp_packet_all_cmd);
  install_element (CONFIG_NODE, &no_debug_eigrp_packet_all_cmd);
  install_element (CONFIG_NODE, &debug_ospf_packet_send_recv_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_packet_send_recv_cmd);
  install_element (CONFIG_NODE, &debug_eigrp_packet_send_recv_detail_cmd);
  install_element (CONFIG_NODE, &no_debug_eigrp_packet_send_recv_detail_cmd);
}


