/* EIGRP VTY interface.
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

#include "memory.h"
#include "thread.h"
#include "prefix.h"
#include "table.h"
#include "vty.h"
#include "command.h"
#include "plist.h"
#include "log.h"
#include "zclient.h"
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
#include "eigrpd/eigrp_const.h"


static int
config_write_network (struct vty *vty, struct eigrp *eigrp)
{
  struct route_node *rn;

  /* `network area' print. */
  for (rn = route_top (eigrp->networks); rn; rn = route_next (rn))
    if (rn->info)
      {
        /* Network print. */
        vty_out (vty, " network %s/%d %s",
                 inet_ntoa (rn->p.u.prefix4), rn->p.prefixlen, VTY_NEWLINE);
      }

  return 0;
}

static int
eigrp_write_interface (struct vty *vty)
{
  int write=0;

  return write;
}

static int
eigrp_write_keychain (struct vty *vty)
{
  int write=0;

  return write;
}
static int
eigrp_write_keychain_key (struct vty *vty)
{
  int write=0;

  return write;
}

DEFUN (router_eigrp,
       router_eigrp_cmd,
       "router eigrp <1-65535>",
       "Enable a routing process\n"
       "Start EIGRP configuration\n")
{
  vty->node = EIGRP_NODE;
  vty->index = eigrp_get (argv[0]);

  return CMD_SUCCESS;
}

DEFUN (eigrp_network,
       eigrp_network_cmd,
       "network A.B.C.D/M",
       "Enable routing on an IP network\n"
       "EIGRP network prefix\n")
{
  struct eigrp *eigrp = vty->index;
  struct prefix_ipv4 p;
  int ret;

  VTY_GET_IPV4_PREFIX ("network prefix", p, argv[0]);

  ret = eigrp_network_set (eigrp, &p);

  if (ret == 0)
    {
      vty_out (vty, "There is already same network statement.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

DEFUN (no_eigrp_network,
       no_eigrp_network_cmd,
       "no network A.B.C.D/M",
       "Disable routing on an IP network\n"
       "EIGRP network prefix\n")
{
  struct eigrp *eigrp = vty->index;
  struct prefix_ipv4 p;
  int ret;

  VTY_GET_IPV4_PREFIX ("network prefix", p, argv[0]);

  ret = eigrp_network_unset (eigrp, &p);

  if (ret == 0)
  {
    vty_out (vty,"Can't find specified network area configuration.%s", VTY_NEWLINE);
    return CMD_WARNING;
  }

  return CMD_SUCCESS;
}

DEFUN (show_ip_eigrp_topology,
       show_ip_eigrp_topology_cmd,
       "show ip eigrp topology",
       SHOW_STR
       IP_STR
       "IP-EIGRP show commands\n"
       "IP-EIGRP topology\n")
{
  struct eigrp *eigrp;
  struct listnode *node, *nnode, *node2, *nnode2;
  struct eigrp_prefix_entry *tn;
  struct eigrp_neighbor_entry *te;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
  {
    vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
    return CMD_SUCCESS;
  }

  show_ip_eigrp_topology_header (vty, eigrp);

  for (ALL_LIST_ELEMENTS (eigrp->topology_table, node, nnode, tn))
  {
    show_ip_eigrp_prefix_entry (vty,tn);
    for (ALL_LIST_ELEMENTS (tn->entries, node2, nnode2, te))
      {
        if (((te->flags & EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG) == EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG)||
            ((te->flags & EIGRP_NEIGHBOR_ENTRY_FSUCCESSOR_FLAG) == EIGRP_NEIGHBOR_ENTRY_FSUCCESSOR_FLAG))
          show_ip_eigrp_neighbor_entry (vty, eigrp, te);
      }
    }

  return CMD_SUCCESS;
}

DEFUN (show_ip_eigrp_topology_all_links,
       show_ip_eigrp_topology_all_links_cmd,
       "show ip eigrp topology all-links",
       SHOW_STR
       IP_STR
       "IP-EIGRP show commands\n"
       "IP-EIGRP topology\n"
       "Show all links in topology table\n")
{
  struct eigrp *eigrp;
  struct listnode *node, *nnode, *node2, *nnode2;
  struct eigrp_prefix_entry *tn;
  struct eigrp_neighbor_entry *te;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  show_ip_eigrp_topology_header (vty, eigrp);

  for (ALL_LIST_ELEMENTS (eigrp->topology_table, node, nnode, tn))
    {
      show_ip_eigrp_prefix_entry (vty,tn);
      for (ALL_LIST_ELEMENTS (tn->entries, node2, nnode2, te))
        {
          show_ip_eigrp_neighbor_entry (vty, eigrp, te);
        }
    }

  return CMD_SUCCESS;
}

ALIAS (show_ip_eigrp_topology,
       show_ip_eigrp_topology_detail_cmd,
       "show ip eigrp topology (A.B.C.D|A.B.C.D/nn|detail|summary)",
       SHOW_STR
       IP_STR
       "IP-EIGRP show commands\n"
       "IP-EIGRP topology\n"
       "Netwok to display information about\n"
       "IP prefix <network>/<length>, e.g., 192.168.0.0/16\n"
       "Show all links in topology table\n"
       "Show a summary of the topology table\n")

DEFUN (show_ip_eigrp_interfaces,
       show_ip_eigrp_interfaces_cmd,
       "show ip eigrp interfaces",
       SHOW_STR
       IP_STR
       "IP-EIGRP show commands\n"
       "IP-EIGRP interfaces\n")
{
  struct eigrp_interface *ei;
  struct eigrp *eigrp;
  struct listnode *node;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, "EIGRP Routing Process not enabled%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  if (!argc)
    {
        show_ip_eigrp_interface_header (vty, eigrp);
    }

  for (ALL_LIST_ELEMENTS_RO (eigrp->eiflist, node, ei))
  {
    if ((argc > 0) && ( strncmp (argv[0], "d", 1) == 0))
      {
        show_ip_eigrp_interface_header (vty, eigrp);
      }

  //	if ((strncmp (argv[1], "f", 1) == 0 && strncmp (eigrp_if_name_string (ei), "F",1) == 0) ||
  //		(strncmp (argv[1], "l", 1) == 0 && strncmp (eigrp_if_name_string (ei), "L",1) == 0) ||
  //		(strncmp (argv[1], "s", 1) == 0 && strncmp (eigrp_if_name_string (ei), "S",1) == 0))
  //	{
        show_ip_eigrp_interface_sub (vty, eigrp, ei);
    //}

    if ((argc > 0) && ( strncmp (argv[0], "d", 1) == 0))
      {
        show_ip_eigrp_interface_detail (vty, eigrp, ei);
      }
  }

  return CMD_SUCCESS;
}

ALIAS (show_ip_eigrp_interfaces,
	   show_ip_eigrp_interfaces_detail_cmd,
	   "show ip eigrp interfaces (" INT_TYPES_CMD_STR ")",
	   SHOW_STR
	   IP_STR
	   "IP-EIGRP show commands\n"
	   "IP-EIGRP interfaces\n"
	   INT_TYPES_DESC)

DEFUN (show_ip_eigrp_neighbors,
       show_ip_eigrp_neighbors_cmd,
       "show ip eigrp neighbors",
       SHOW_STR
       IP_STR
       "IP-EIGRP show commands\n"
       "IP-EIGRP neighbors\n")
{
  struct eigrp *eigrp;
  struct eigrp_interface *ei;
  struct listnode *node, *node2, *nnode2;
  struct eigrp_neighbor *nbr;
  int detail = FALSE;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  detail = ((argc > 0) && (strncmp(argv[0], "d", 1) == 0));
  show_ip_eigrp_neighbor_header (vty, eigrp);

  for (ALL_LIST_ELEMENTS_RO (eigrp->eiflist, node, ei))
    {
      for (ALL_LIST_ELEMENTS (ei->nbrs, node2, nnode2, nbr))
        {
	  if (detail || (nbr->state == EIGRP_NEIGHBOR_UP))
	    show_ip_eigrp_neighbor_sub (vty, nbr, detail);
        }
    }

  return CMD_SUCCESS;
}

ALIAS (show_ip_eigrp_neighbors,
	   show_ip_eigrp_neighbors_detail_cmd,
	   "show ip eigrp neighbors (" INT_TYPES_CMD_STR ")",
	   SHOW_STR
	   IP_STR
	   "IP-EIGRP show commands\n"
	   "IP-EIGRP neighbors\n"
	   INT_TYPES_DESC)

DEFUN (eigrp_if_delay,
       eigrp_if_delay_cmd,
       "delay <1-16777215>",
       "Specify interface throughput delay\n"
       "Throughput delay (tens of microseconds)\n")
{
  struct eigrp *eigrp;
  u_int32_t delay;
  struct listnode *node, *nnode, *node2, *nnode2;
  struct eigrp_interface *ei;
  struct interface *ifp;
  struct eigrp_prefix_entry *pe;
  struct eigrp_neighbor_entry *ne;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);

      return CMD_SUCCESS;
    }

  delay = atoi (argv[0]);

  /* delay range is <1-16777215>. */
  if ((delay < 1 )|| (delay > 16777215))
    {
      vty_out (vty, "Interface delay is invalid%s", VTY_NEWLINE);

      return CMD_WARNING;
    }

  ifp = vty->index;
  IF_DEF_PARAMS (ifp)->delay = delay;

  for (ALL_LIST_ELEMENTS (eigrp->eiflist, node, nnode, ei))
    {
      if (ei->ifp == ifp)
        break;
    }

  for (ALL_LIST_ELEMENTS (eigrp->topology_table, node, nnode, pe))
    {
      for (ALL_LIST_ELEMENTS (pe->entries, node2, nnode2, ne))
        {

        }
    }

  return CMD_SUCCESS;
}

DEFUN (eigrp_if_bandwidth,
       eigrp_if_bandwidth_cmd,
       "bandwidth <1-10000000>",
       "Set bandwidth informational parameter\n"
       "Bandwidth in kilobits\n")
{
  u_int32_t bandwidth;
  struct eigrp *eigrp;
  struct eigrp_interface *ei;
  struct listnode *node, *nnode, *node2, *nnode2;
  struct interface *ifp;
  struct eigrp_prefix_entry *pe;
  struct eigrp_neighbor_entry *ne;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  bandwidth = atoi (argv[0]);

  /* bandwidth range is <1-10000000>. */
  if ((bandwidth < 1) || (bandwidth > 10000000))
    {
      vty_out (vty, "Interface bandwidth is invalid%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ifp = vty->index;
  IF_DEF_PARAMS (ifp)->bandwidth = bandwidth;

  for (ALL_LIST_ELEMENTS (eigrp->eiflist, node, nnode, ei))
    {
      if (ei->ifp == ifp)
        break;
    }

  for (ALL_LIST_ELEMENTS (eigrp->topology_table, node, nnode, pe))
    {
      for (ALL_LIST_ELEMENTS (pe->entries, node2, nnode2, ne))
        {

        }
    }

  return CMD_SUCCESS;
}

DEFUN (eigrp_if_ip_hellointerval,
       eigrp_if_ip_hellointerval_cmd,
       "ip hello-interval eigrp <1-65535>",
       "Interface Internet Protocol config commands\n"
       "Configures EIGRP hello interval\n"
       "Enhanced Interior Gateway Routing Protocol (EIGRP)\n"
       "Seconds between hello transmissions\n")
{
  u_int32_t hello;
  struct eigrp *eigrp;
  struct eigrp_interface *ei;
  struct interface *ifp;
  struct listnode *node, *nnode;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  hello = atoi (argv[0]);

  /* hello range is <1-65535> */
  if ((hello < 1) || (hello > 65535))
    {
      vty_out (vty, "Hello-interval value is invalid%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ifp = vty->index;
  IF_DEF_PARAMS (ifp)->v_hello = hello;

  for (ALL_LIST_ELEMENTS (eigrp->eiflist, node, nnode, ei))
    {
      if (ei->ifp == ifp)
        {
          THREAD_TIMER_OFF (ei->t_hello);
          THREAD_TIMER_ON (master,ei->t_hello,eigrp_hello_timer,ei,1);
          break;
        }
    }

  return CMD_SUCCESS;
}

DEFUN (eigrp_if_ip_holdinterval,
       eigrp_if_ip_holdinterval_cmd,
       "ip hold-time eigrp <1-65535>",
       "Interface Internet Protocol config commands\n"
       "Configures EIGRP hello interval\n"
       "Enhanced Interior Gateway Routing Protocol (EIGRP)\n"
       "Seconds before neighbor is considered down\n")
{
  u_int32_t hold;
  struct eigrp *eigrp;
  struct interface *ifp;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  hold = atoi (argv[0]);

  /* hello range is <1-65535> */
  if ((hold < 1) || (hold > 65535))
    {
      vty_out (vty, "Hello-interval value is invalid%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ifp = vty->index;
  IF_DEF_PARAMS (ifp)->v_wait = hold;

  return CMD_SUCCESS;
}

static struct cmd_node eigrp_node =
{
  EIGRP_NODE,
  "%s (config-router)# ",
  1
};

/* Save EIGRP configuration */
static int
eigrp_config_write (struct vty *vty)
{
  struct eigrp *eigrp;

  int write = 0;

  eigrp = eigrp_lookup ();
  if (eigrp != NULL)
    {
      /* `router eigrp' print. */
      vty_out (vty, "router eigrp %d%s", eigrp->AS, VTY_NEWLINE);

      write++;

      if (!eigrp->networks)
        return write;

      /* Router ID print. */
      if (eigrp->router_id_static != 0)
      {
	struct in_addr router_id_static;
	router_id_static.s_addr = htonl(eigrp->router_id_static);
        vty_out (vty, " eigrp router-id %s%s",
                 inet_ntoa (router_id_static), VTY_NEWLINE);
      }

//      /* log-adjacency-changes flag print. */
//      if (CHECK_FLAG (eigrp->config, EIGRP_LOG_ADJACENCY_CHANGES))
//        {
//          vty_out (vty, " log-adjacency-changes");
//          if (CHECK_FLAG (eigrp->config, EIGRP_LOG_ADJACENCY_DETAIL))
//            vty_out (vty, " detail");
//          vty_out (vty, "%s", VTY_NEWLINE);
//        }

      /* SPF timers print. */
//      if (eigrp->spf_delay != EIGRP_SPF_DELAY_DEFAULT ||
//          eigrp->spf_holdtime != EIGRP_SPF_HOLDTIME_DEFAULT ||
//          eigrp->spf_max_holdtime != EIGRP_SPF_MAX_HOLDTIME_DEFAULT)
//        vty_out (vty, " timers throttle spf %d %d %d%s",
//                 eigrp->spf_delay, eigrp->spf_holdtime,
//                 eigrp->spf_max_holdtime, VTY_NEWLINE);

//      /* Max-metric router-lsa print */
//      config_write_stub_router (vty, eigrp);

//      /* SPF refresh parameters print. */
//      if (eigrp->lsa_refresh_interval != EIGRP_LSA_REFRESH_INTERVAL_DEFAULT)
//        vty_out (vty, " refresh timer %d%s",
//                 eigrp->lsa_refresh_interval, VTY_NEWLINE);
//
//      /* Redistribute information print. */
//      config_write_eigrp_redistribute (vty, eigrp);
//
//      /* passive-interface print. */
//      if (eigrp->passive_interface_default == EIGRP_IF_PASSIVE)
//        vty_out (vty, " passive-interface default%s", VTY_NEWLINE);
//
//      for (ALL_LIST_ELEMENTS_RO (om->iflist, node, ifp))
//        if (EIGRP_IF_PARAM_CONFIGURED (IF_DEF_PARAMS (ifp), passive_interface)
//            && IF_DEF_PARAMS (ifp)->passive_interface !=
//                              eigrp->passive_interface_default)
//          {
//            vty_out (vty, " %spassive-interface %s%s",
//                     IF_DEF_PARAMS (ifp)->passive_interface ? "" : "no ",
//                     ifp->name, VTY_NEWLINE);
//          }
//      for (ALL_LIST_ELEMENTS_RO (eigrp->oiflist, node, oi))
//        {
//          if (!EIGRP_IF_PARAM_CONFIGURED (oi->params, passive_interface))
//            continue;
//          if (EIGRP_IF_PARAM_CONFIGURED (IF_DEF_PARAMS (oi->ifp),
//                                        passive_interface))
//            {
//              if (oi->params->passive_interface == IF_DEF_PARAMS (oi->ifp)->passive_interface)
//                continue;
//            }
//          else if (oi->params->passive_interface == eigrp->passive_interface_default)
//            continue;
//
//          vty_out (vty, " %spassive-interface %s %s%s",
//                   oi->params->passive_interface ? "" : "no ",
//                   oi->ifp->name,
//                   inet_ntoa (oi->address->u.prefix4), VTY_NEWLINE);
//        }

      /* Network area print. */
      config_write_network (vty, eigrp);

//      /* Area config print. */
//      config_write_eigrp_area (vty, eigrp);
//
//      /* static neighbor print. */
//      config_write_eigrp_nbr_nbma (vty, eigrp);
//
//      /* Virtual-Link print. */
//      config_write_virtual_link (vty, eigrp);
//
//      /* Default metric configuration.  */
//      config_write_eigrp_default_metric (vty, eigrp);
//
//      /* Distribute-list and default-information print. */
//      config_write_eigrp_distribute (vty, eigrp);
//
//      /* Distance configuration. */
//      config_write_eigrp_distance (vty, eigrp)
    }

  return write;
}

void
eigrp_vty_show_init (void)
{
  install_element (ENABLE_NODE, &show_ip_eigrp_interfaces_cmd);
  install_element (VIEW_NODE, &show_ip_eigrp_interfaces_cmd);

  install_element (ENABLE_NODE, &show_ip_eigrp_neighbors_cmd);
  install_element (VIEW_NODE, &show_ip_eigrp_neighbors_cmd);

  install_element (ENABLE_NODE, &show_ip_eigrp_topology_cmd);
  install_element (VIEW_NODE, &show_ip_eigrp_topology_cmd);

  install_element (VIEW_NODE, &show_ip_eigrp_neighbors_detail_cmd);
  install_element (ENABLE_NODE, &show_ip_eigrp_neighbors_detail_cmd);

  install_element (VIEW_NODE, &show_ip_eigrp_interfaces_detail_cmd);
  install_element (ENABLE_NODE, &show_ip_eigrp_interfaces_detail_cmd);

  install_element (ENABLE_NODE, &show_ip_eigrp_topology_all_links_cmd);
  install_element (VIEW_NODE, &show_ip_eigrp_topology_all_links_cmd);

  install_element (ENABLE_NODE, &show_ip_eigrp_topology_detail_cmd);
  install_element (VIEW_NODE, &show_ip_eigrp_topology_detail_cmd);

}

/* eigrpd's interface node. */
static struct cmd_node eigrp_interface_node =
{
  INTERFACE_NODE,
  "%s (config-if)# ",
  1
};

/* eigrpd's interface node. */
static struct cmd_node eigrp_keychain_node =
{
  KEYCHAIN_NODE,
  "%s (config-keychain)# ",
  1
};

/* eigrpd's interface node. */
static struct cmd_node eigrp_keychain_key_node =
{
  KEYCHAIN_KEY_NODE,
  "%s (config-keychain-key)# ",
  1
};


void
eigrp_vty_if_init (void)
{
  install_node (&eigrp_interface_node, eigrp_write_interface);
  install_default (INTERFACE_NODE);
  install_element (CONFIG_NODE, &interface_cmd);
  install_element (CONFIG_NODE, &no_interface_cmd);

  /* Delay and bandwidth configuration commands*/
  install_element (INTERFACE_NODE, &eigrp_if_delay_cmd);
  install_element (INTERFACE_NODE, &eigrp_if_bandwidth_cmd);

  /*Hello-interval and hold-time interval configuration commands*/
  install_element (INTERFACE_NODE, &eigrp_if_ip_holdinterval_cmd);
  install_element (INTERFACE_NODE, &eigrp_if_ip_hellointerval_cmd);

  /* "description" commands. */
  install_element (INTERFACE_NODE, &interface_desc_cmd);
  install_element (INTERFACE_NODE, &no_interface_desc_cmd);

//  /* "ip eigrp dead-interval" commands. */
//  install_element (INTERFACE_NODE, &ip_eigrp_dead_interval_cmd);
//
//  /* "ip eigrp hello-interval" commands. */
//  install_element (INTERFACE_NODE, &ip_eigrp_hello_interval_cmd);
}

static void
eigrp_vty_zebra_init (void)
{

}

/* Install EIGRP related vty commands. */
void
eigrp_vty_init (void)
{
  install_node (&eigrp_node, eigrp_config_write);
  install_node (&eigrp_keychain_node, eigrp_write_keychain);
  install_node (&eigrp_keychain_key_node, eigrp_write_keychain_key);

  install_element (CONFIG_NODE, &router_eigrp_cmd);

  install_default (EIGRP_NODE);

  install_element (EIGRP_NODE, &eigrp_network_cmd);
  install_element (EIGRP_NODE, &no_eigrp_network_cmd);
}
