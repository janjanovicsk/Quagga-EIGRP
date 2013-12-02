/* EIGRP VTY interface.
 * Copyright (C) 2000 Toshiaki Takada
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

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_dump.h"

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

DEFUN (show_ip_eigrp_interfaces,
       show_ip_eigrp_interfaces_cmd,
       "show ip eigrp interfaces",
       SHOW_STR
       IP_STR
       "IP-EIGRP show commands\n"
       "IP-EIGRP interfaces\n")
{
  struct interface *ifp;
  struct eigrp *eigrp;
  struct listnode *node;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
  {
    vty_out (vty, "EIGRP Routing Process not enabled%s", VTY_NEWLINE);
    return CMD_SUCCESS;
  }

  show_ip_eigrp_interface_header (vty);

    if (argc == 0)
      for (ALL_LIST_ELEMENTS_RO (iflist, node, ifp))
        show_ip_eigrp_interface_sub (vty, eigrp, ifp);
    else
    {
      if ((ifp = if_lookup_by_name (argv[0])) == NULL)
        vty_out (vty, "No such interface name%s", VTY_NEWLINE);
      else
        show_ip_eigrp_interface_sub (vty, eigrp, ifp);
    }

    return CMD_SUCCESS;
}

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
  struct listnode *node;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
  {
    vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
    return CMD_SUCCESS;
  }

  show_ip_eigrp_neighbor_header (vty);

  for (ALL_LIST_ELEMENTS_RO (eigrp->eiflist, node, ei))
    show_ip_eigrp_neighbor_sub (vty, ei);

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
  struct eigrp_interface *ei;
  struct listnode *node;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
  {
	vty_out (vty, " EIGRP Routing Process not enabled%s", VTY_NEWLINE);
    return CMD_SUCCESS;
  }

  show_ip_eigrp_topology_header (vty);

  show_ip_eigrp_topology_sub (vty);
  return CMD_SUCCESS;
}


static struct cmd_node eigrp_node =
{
  EIGRP_NODE,
  "%s(config-router)# ",
  1
};

/* Save EIGRP configuration */
static int
eigrp_config_write (struct vty *vty)
{
  int write = 0;

  return write;
}

void
eigrp_vty_show_init (void)
{
  install_element(ENABLE_NODE, &show_ip_eigrp_interfaces_cmd);
  install_element(VIEW_NODE, &show_ip_eigrp_interfaces_cmd);
  install_element(ENABLE_NODE, &show_ip_eigrp_neighbors_cmd);
  install_element(VIEW_NODE, &show_ip_eigrp_neighbors_cmd);
  install_element(ENABLE_NODE, &show_ip_eigrp_topology_cmd);
  install_element(VIEW_NODE, &show_ip_eigrp_topology_cmd);
}

/* eigrpd's interface node. */
static struct cmd_node eigrp_interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
  1
};

static int
eigrp_write_interface (struct vty *vty)
{
  int write=0;

  return write;
}

static void
eigrp_vty_if_init (void)
{
  install_node (&eigrp_interface_node, eigrp_write_interface);

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

  install_element (CONFIG_NODE, &router_eigrp_cmd);

  install_default(EIGRP_NODE);

  install_element(EIGRP_NODE, &eigrp_network_cmd);
  install_element(EIGRP_NODE, &no_eigrp_network_cmd);



}
