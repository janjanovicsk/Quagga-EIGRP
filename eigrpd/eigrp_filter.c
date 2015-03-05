/*
 * EIGRP Interface Functions.
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

//#include "filter.h"
//#include "plist.h"
//#include "distribute.h"

#include "if.h"
#include "command.h"
#include "prefix.h"
#include "table.h"
#include "thread.h"
#include "memory.h"
#include "log.h"
#include "stream.h"
#include "filter.h"
#include "sockunion.h"
#include "sockopt.h"
#include "routemap.h"
#include "if_rmap.h"
#include "plist.h"
#include "distribute.h"
#include "md5.h"
#include "keychain.h"
#include "privs.h"

#include "eigrpd/eigrp_const.h"
#include "eigrpd/eigrp_filter.h"

/* Distribute-list update functions. */
void
eigrp_distribute_update (struct distribute *dist)
{
  struct interface *ifp;
  struct eigrp_interface *ei;
  struct access_list *alist;
  struct prefix_list *plist;

  if (! dist->ifname)
    return;

  ifp = if_lookup_by_name (dist->ifname);
  if (ifp == NULL)
    return;

  ei = ifp->info;

  if (dist->list[DISTRIBUTE_IN])
    {
      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_IN]);
      if (alist)
	ei->list[EIGRP_FILTER_IN] = alist;
      else
	ei->list[EIGRP_FILTER_IN] = NULL;
    }
  else
    ei->list[EIGRP_FILTER_IN] = NULL;

  if (dist->list[DISTRIBUTE_OUT])
    {
      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_OUT]);
      if (alist)
	ei->list[EIGRP_FILTER_OUT] = alist;
      else
	ei->list[EIGRP_FILTER_OUT] = NULL;
    }
  else
    ei->list[EIGRP_FILTER_OUT] = NULL;

  if (dist->prefix[DISTRIBUTE_IN])
    {
      plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_IN]);
      if (plist)
	ei->prefix[EIGRP_FILTER_IN] = plist;
      else
	ei->prefix[EIGRP_FILTER_IN] = NULL;
    }
  else
    ei->prefix[EIGRP_FILTER_IN] = NULL;

  if (dist->prefix[DISTRIBUTE_OUT])
    {
      plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_OUT]);
      if (plist)
	ei->prefix[EIGRP_FILTER_OUT] = plist;
      else
	ei->prefix[EIGRP_FILTER_OUT] = NULL;
    }
  else
    ei->prefix[EIGRP_FILTER_OUT] = NULL;
}

void
eigrp_distribute_update_interface (struct interface *ifp)
{
  struct distribute *dist;

  dist = distribute_lookup (ifp->name);
  if (dist)
    eigrp_distribute_update (dist);
}

/* Update all interface's distribute list. */
/* ARGSUSED */
void
eigrp_distribute_update_all (struct prefix_list *notused)
{
  struct interface *ifp;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (iflist, node, nnode, ifp))
    eigrp_distribute_update_interface (ifp);
}

/* ARGSUSED */
void
eigrp_distribute_update_all_wrapper(struct access_list *notused)
{
        eigrp_distribute_update_all(NULL);
}

