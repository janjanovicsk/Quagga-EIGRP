/*
 * EIGRP Interface functions.
 * Copyright (C) 1999, 2000 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
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

#include "thread.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "memory.h"
#include "command.h"
#include "stream.h"
#include "log.h"

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_neighbor.h"


static void
eigrp_add_to_if (struct interface *ifp, struct eigrp_interface *ei)
{
  struct route_node *rn;
  struct prefix p;

  p = *ei->address;
  p.prefixlen = IPV4_MAX_PREFIXLEN;

  rn = route_node_get (IF_OIFS (ifp), &p);
  /* rn->info should either be NULL or equal to this ei
   * as route_node_get may return an existing node
   */
  assert (!rn->info || rn->info == ei);
  rn->info = ei;
}

struct eigrp_interface *
eigrp_if_new (struct eigrp *eigrp, struct interface *ifp, struct prefix *p)
{
  struct eigrp_interface *ei;

  if ((ei = eigrp_if_table_lookup (ifp, p)) == NULL)
    {
      ei = XCALLOC (MTYPE_EIGRP_IF, sizeof (struct eigrp_interface));
      memset (ei, 0, sizeof (struct eigrp_interface));
    }
  else
    return ei;

  /* Set zebra interface pointer. */
  ei->ifp = ifp;
  ei->address = p;

  eigrp_add_to_if (ifp, ei);
  listnode_add (eigrp->eiflist, ei);

  ei->type = EIGRP_IFTYPE_BROADCAST;

  /* Initialize neighbor list. */
  ei->nbrs = route_table_init ();

  return ei;
}

/* lookup ei for specified prefix/ifp */
struct eigrp_interface *
eigrp_if_table_lookup (struct interface *ifp, struct prefix *prefix)
{
  struct prefix p;
  struct route_node *rn;
  struct eigrp_interface *rninfo = NULL;

  p = *prefix;
  p.prefixlen = IPV4_MAX_PREFIXLEN;

  /* route_node_get implicitly locks */
  if ((rn = route_node_lookup (IF_OIFS (ifp), &p)))
    {
      rninfo = (struct eigrp_interface *) rn->info;
      route_unlock_node (rn);
    }

  return rninfo;
}


int
eigrp_if_delete_hook (struct interface *ifp)
{
  int rc = 0;

  struct route_node *rn;

  route_table_finish (IF_OIFS (ifp));

  for (rn = route_top (IF_OIFS_PARAMS (ifp)); rn; rn = route_next (rn))
      if (rn->info)
        eigrp_del_if_params (rn->info);
    route_table_finish (IF_OIFS_PARAMS (ifp));

  return rc;
}

void
eigrp_if_init ()
{
  /* Initialize Zebra interface data structure. */
  if_init ();
  eigrp_om->iflist = iflist;
  if_add_hook (IF_NEW_HOOK, eigrp_if_new_hook);
  if_add_hook (IF_DELETE_HOOK, eigrp_if_delete_hook);
}

int
eigrp_if_new_hook (struct interface *ifp)
{
  int rc = 0;

  ifp->info = XCALLOC (MTYPE_EIGRP_IF_INFO, sizeof (struct eigrp_if_info));

  IF_OIFS (ifp) = route_table_init ();
  IF_OIFS_PARAMS (ifp) = route_table_init ();

  IF_DEF_PARAMS (ifp) = eigrp_new_if_params ();

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), v_hello);
  IF_DEF_PARAMS (ifp)->v_hello = EIGRP_HELLO_INTERVAL_DEFAULT;

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), v_wait);
  IF_DEF_PARAMS (ifp)->v_wait = EIGRP_HOLD_INTERVAL_DEFAULT;

  return rc;
}

struct eigrp_if_params *
eigrp_new_if_params (void)
{
  struct eigrp_if_params *eip;

  eip = XCALLOC (MTYPE_EIGRP_IF_PARAMS, sizeof (struct eigrp_if_params));
  if (!eip)
      return NULL;

  UNSET_IF_PARAM (eip, passive_interface);
  UNSET_IF_PARAM (eip, v_hello);
  UNSET_IF_PARAM (eip, v_wait);

  return eip;
}

void
eigrp_del_if_params (struct eigrp_if_params *eip)
{
  XFREE (MTYPE_EIGRP_IF_PARAMS, eip);
}

struct eigrp_if_params *
eigrp_lookup_if_params (struct interface *ifp, struct in_addr addr)
{
  struct prefix_ipv4 p;
  struct route_node *rn;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = addr;

  rn = route_node_lookup (IF_OIFS_PARAMS (ifp), (struct prefix*)&p);

  if (rn)
    {
      route_unlock_node (rn);
      return rn->info;
    }

  return NULL;
}

int
eigrp_if_up (struct eigrp_interface *ei)
{
  if (ei == NULL)
    return 0;

      struct eigrp *eigrp= eigrp_lookup ();
      if (eigrp != NULL)
        eigrp_adjust_sndbuflen (eigrp, ei->ifp->mtu);
      else
        zlog_warn ("%s: eigrp_lookup() returned NULL", __func__);
      eigrp_if_stream_set (ei);
      //OSPF_ISM_EVENT_SCHEDULE (oi, ISM_InterfaceUp);

  return 1;
}

void
eigrp_if_stream_set (struct eigrp_interface *ei)
{
  /* set output fifo queue. */
  if (ei->obuf == NULL)
    ei->obuf = eigrp_fifo_new ();
}
