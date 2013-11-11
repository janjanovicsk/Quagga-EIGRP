/*
 * EIGRP neighbor related functions.
 *   Copyright (C) 1999 Toshiaki Takada
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
#include "prefix.h"
#include "memory.h"
#include "command.h"
#include "thread.h"
#include "stream.h"
#include "table.h"
#include "log.h"

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_network.h"


static void
eigrp_nbr_key (struct eigrp_interface *ei, struct eigrp_neighbor *nbr,
              struct prefix *key)
{
  key->family = AF_INET;
  key->prefixlen = IPV4_MAX_BITLEN;
  key->u.prefix4 = nbr->src;
  return;
}

struct eigrp_neighbor *
eigrp_nbr_new (struct eigrp_interface *ei)
{
  struct eigrp_neighbor *nbr;

  /* Allcate new neighbor. */
  nbr = XCALLOC (MTYPE_EIGRP_NEIGHBOR, sizeof (struct eigrp_neighbor));

  /* Relate neighbor to the interface. */
  nbr->ei = ei;

  /* Set default values. */
  nbr->state = EIGRP_NEIGHBOR_DOWN;

  nbr->retrans_queue = eigrp_fifo_new();

  return nbr;
}

static struct eigrp_neighbor *
eigrp_nbr_add (struct eigrp_interface *ei, struct eigrp_header *eigrph,
              struct prefix *p)
{
  struct eigrp_neighbor *nbr;

  nbr = eigrp_nbr_new (ei);

  nbr->src = p->u.prefix4;
  nbr->ack = 0;

//  if (ei->type == OSPF_IFTYPE_NBMA)
//    {
//      struct ospf_nbr_nbma *nbr_nbma;
//      struct listnode *node;
//
//      for (ALL_LIST_ELEMENTS_RO (oi->nbr_nbma, node, nbr_nbma))
//        {
//          if (IPV4_ADDR_SAME(&nbr_nbma->addr, &nbr->src))
//            {
//              nbr_nbma->nbr = nbr;
//              nbr->nbr_nbma = nbr_nbma;
//
//              if (nbr_nbma->t_poll)
//                OSPF_POLL_TIMER_OFF (nbr_nbma->t_poll);
//
//              nbr->state_change = nbr_nbma->state_change + 1;
//            }
//        }
//    }

//
//  if (IS_DEBUG_OSPF_EVENT)
//    zlog_debug ("NSM[%s:%s]: start", IF_NAME (nbr->oi),
//               inet_ntoa (nbr->router_id));

  return nbr;
}

struct eigrp_neighbor *
eigrp_nbr_get (struct eigrp_interface *ei, struct eigrp_header *eigrph,
              struct ip *iph, struct prefix *p)
{
  struct route_node *rn;
  struct prefix key;
  struct eigrp_neighbor *nbr;

  key.family = AF_INET;
  key.prefixlen = IPV4_MAX_BITLEN;
  key.u.prefix4 = iph->ip_src;

  rn = route_node_get (ei->nbrs, &key);
  if (rn->info)
    {
      route_unlock_node (rn);
      nbr = rn->info;

//      if (ei->type == OSPF_IFTYPE_NBMA)
//        {
//          nbr->src = iph->ip_src;
//          memcpy (&nbr->address, p, sizeof (struct prefix));
//        }
    }
  else
    {
      rn->info = nbr = eigrp_nbr_add (ei, eigrph, p);
    }

  return nbr;
}

void
eigrp_nbr_free (struct eigrp_neighbor *nbr)
{

  /* Cancel all events. *//* Thread lookup cost would be negligible. */
  thread_cancel_event (master, nbr);

  XFREE (MTYPE_EIGRP_NEIGHBOR, nbr);
}

/* Delete specified EIGRP neighbor from interface. */
void
eigrp_nbr_delete (struct eigrp_neighbor *nbr)
{
  struct eigrp_interface *ei;
  struct route_node *rn;
  struct prefix p;

  ei = nbr->ei;

  /* get appropriate prefix 'key' */
  eigrp_nbr_key (ei, nbr, &p);

  rn = route_node_lookup (ei->nbrs, &p);
  if (rn)
    {
      /* If lookup for a NBR succeeds, the leaf route_node could
       * only exist because there is (or was) a nbr there.
       * If the nbr was deleted, the leaf route_node should have
       * lost its last refcount too, and be deleted.
       * Therefore a looked-up leaf route_node in nbrs table
       * should never have NULL info.
       */
      assert (rn->info);

      if (rn->info)
        {
          rn->info = NULL;
          route_unlock_node (rn);
        }
      else
        zlog_info ("Can't find neighbor %s in the interface %s",
                   inet_ntoa (nbr->src), IF_NAME (ei));

      route_unlock_node (rn);
    }

  /* Free ospf_neighbor structure. */
  eigrp_nbr_free (nbr);
}

int
holddown_timer_expired (struct thread *thread)
{
  struct eigrp_neighbor *nbr;

  nbr = THREAD_ARG(thread);

  zlog_debug("VYPRSAL HOLDDOWN TIMER u suseda \n");
  nbr->state = EIGRP_NEIGHBOR_DOWN;
  thread_cancel(nbr->t_holddown);

  return 0;
}
