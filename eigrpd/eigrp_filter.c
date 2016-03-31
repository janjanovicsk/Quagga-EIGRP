/*
 * EIGRP Filter Functions.
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

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_const.h"
#include "eigrpd/eigrp_filter.h"

/*
 * Distribute-list update functions.
 */
void
eigrp_distribute_update (struct distribute *dist)
{
  struct interface *ifp;
  struct eigrp_interface *ei;
  struct access_list *alist;
  struct prefix_list *plist;
  struct route_map *routemap;
  struct eigrp *e;

  /* if no interface address is present, set list to eigrp process struct */
  e = eigrp_lookup();

  /* Check if distribute-list was set for process or interface */
  if (! dist->ifname)
    {
	  /* access list IN for whole process */
	  if (dist->list[DISTRIBUTE_IN])
	    {
	      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_IN]);
	      zlog_info("<DEBUG DISTRIBUTE ACL IN FOUND: %s",alist->name);
	      if (alist)
	        e->list[EIGRP_FILTER_IN] = alist;
	      else
	        e->list[EIGRP_FILTER_IN] = NULL;
	    }
	  else
	    {
	      e->list[EIGRP_FILTER_IN] = NULL;
	    }

	  /* access list OUT for whole process */
	  if (dist->list[DISTRIBUTE_OUT])
		{
		  zlog_info("<DEBUG DISTRIBUTE ACL OUT FOUND: %s",dist->list[DISTRIBUTE_OUT]);
		  alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_OUT]);
		  if (alist)
			e->list[EIGRP_FILTER_OUT] = alist;
		  else
			e->list[EIGRP_FILTER_OUT] = NULL;
		}
	  else
		{
		  e->list[EIGRP_FILTER_OUT] = NULL;
		}

	  /* PREFIX_LIST IN for process */
	  if (dist->prefix[DISTRIBUTE_IN])
		 {
		   zlog_info("<DEBUG DISTRIBUTE PREFIX IN FOUND: %s",dist->prefix[DISTRIBUTE_IN]);
		   plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_IN]);
		   if (plist)
			{
			  e->prefix[EIGRP_FILTER_IN] = plist;
			}
			else
			  e->prefix[EIGRP_FILTER_IN] = NULL;
		  } else
		      e->prefix[EIGRP_FILTER_IN] = NULL;

	   /* PREFIX_LIST OUT for process */
	   if (dist->prefix[DISTRIBUTE_OUT])
		  {
		    zlog_info("<DEBUG DISTRIBUTE PREFIX OUT FOUND: %s",dist->prefix[DISTRIBUTE_OUT]);
			plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_OUT]);
			if (plist)
			{
			  e->prefix[EIGRP_FILTER_OUT] = plist;

			}
			else
			  e->prefix[EIGRP_FILTER_OUT] = NULL;
		  }
		else
		  e->prefix[EIGRP_FILTER_OUT] = NULL;

	   /* route-map IN for whole process */
	   if (dist->route[DISTRIBUTE_IN])
		 {
		  routemap = route_map_lookup_by_name (dist->route[DISTRIBUTE_IN]);
		  if (routemap)
			e->routemap[EIGRP_FILTER_IN] = routemap;
		  else
			e->routemap[EIGRP_FILTER_IN] = NULL;
		 }
	   else
		{
		  e->routemap[EIGRP_FILTER_IN] = NULL;
		}

	   /* route-map OUT for whole process */
	   if (dist->route[DISTRIBUTE_OUT])
		{
		  routemap = route_map_lookup_by_name (dist->route[DISTRIBUTE_OUT]);
		  if (routemap)
			e->routemap[EIGRP_FILTER_OUT] = routemap;
		  else
			e->routemap[EIGRP_FILTER_OUT] = NULL;
		}
	   else
		{
		  e->routemap[EIGRP_FILTER_OUT] = NULL;
		}

	  return;
    }

  ifp = if_lookup_by_name (dist->ifname);
  if (ifp == NULL)
    return;

  zlog_info("<DEBUG ACL 2");

  /*struct eigrp_if_info * info = ifp->info;
  ei = info->eigrp_interface;*/
  struct listnode *node, *nnode;
  struct eigrp_interface *ei2;
  /* Find proper interface */
  for (ALL_LIST_ELEMENTS (e->eiflist, node, nnode, ei2))
  {
	  if(strcmp(ei2->ifp->name,ifp->name) == 0){
		  ei = ei2;
		  break;
	  }
  }

  if(ei == NULL)
  {
	  zlog_info("Not Found eigrp interface %s",ifp->name);
  }

  /* Access-list for interface in */
  if (dist->list[DISTRIBUTE_IN])
    {
	  zlog_info("<DEBUG ACL in");
      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_IN]);
      if (alist){
        ei->list[EIGRP_FILTER_IN] = alist;
      }
      else
	    ei->list[EIGRP_FILTER_IN] = NULL;
    }
  else
  {
    ei->list[EIGRP_FILTER_IN] = NULL;
  }

  /* Access-list for interface in */
  if (dist->list[DISTRIBUTE_OUT])
    {
      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_OUT]);
      if (alist)
    	ei->list[EIGRP_FILTER_OUT] = alist;
      else
    	ei->list[EIGRP_FILTER_OUT] = NULL;

    }
  else
  {
    ei->list[EIGRP_FILTER_OUT] = NULL;
	  zlog_info("<DEBUG ACL out else");
  }

  /* Prefix-list for interface in */
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

  /* Prefix-list for interface out */
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

  /* route-map IN for whole process */
  if (dist->route[DISTRIBUTE_IN])
	{
	  zlog_info("<DEBUG ACL ALL in");
	  routemap = route_map_lookup_by_name (dist->route[DISTRIBUTE_IN]);
	  if (routemap)
		ei->routemap[EIGRP_FILTER_IN] = routemap;
	  else
		ei->routemap[EIGRP_FILTER_IN] = NULL;
	 }
  else
	{
	  ei->routemap[EIGRP_FILTER_IN] = NULL;
	}

  /* route-map OUT for whole process */
  if (dist->route[DISTRIBUTE_OUT])
	{
	  routemap = route_map_lookup_by_name (dist->route[DISTRIBUTE_OUT]);
	  if (routemap)
		ei->routemap[EIGRP_FILTER_OUT] = routemap;
	  else
		ei->routemap[EIGRP_FILTER_OUT] = NULL;
	}
  else
	{
	  ei->routemap[EIGRP_FILTER_OUT] = NULL;
	}

}

/*
 * Function called by prefix-list and access-list update
 */
void
eigrp_distribute_update_interface (struct interface *ifp)
{
  struct distribute *dist;

  dist = distribute_lookup (ifp->name);
  if (dist)
    eigrp_distribute_update (dist);
}

/* Update all interface's distribute list.
 * Function used in hook for prefix-list
 */
void
eigrp_distribute_update_all (struct prefix_list *notused)
{
  struct interface *ifp;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (eigrp_om->iflist, node, nnode, ifp))
    eigrp_distribute_update_interface (ifp);
}

/*
 * Function used in hook for acces-list
 */
void
eigrp_distribute_update_all_wrapper(struct access_list *notused)
{
        eigrp_distribute_update_all(NULL);
}

