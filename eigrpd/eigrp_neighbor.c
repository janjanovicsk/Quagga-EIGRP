/*
 * EIGRP neighbor related functions.
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
  if(ei!=NULL)
    nbr->v_holddown = EIGRP_IF_PARAM(ei,v_wait);
  else
    nbr->v_holddown = EIGRP_HOLD_INTERVAL_DEFAULT;

  nbr->retrans_queue = eigrp_fifo_new();
  nbr->multicast_queue = eigrp_fifo_new();

  nbr->init_sequence_number = 0;

  return nbr;
}

static struct eigrp_neighbor *
eigrp_nbr_add (struct eigrp_interface *ei, struct eigrp_header *eigrph,
              struct ip *iph)
{
  struct eigrp_neighbor *nbr;

  nbr = eigrp_nbr_new (ei);

  nbr->src = iph->ip_src;

//
//  if (IS_DEBUG_OSPF_EVENT)
//    zlog_debug ("NSM[%s:%s]: start", IF_NAME (nbr->oi),
//               inet_ntoa (nbr->router_id));

  return nbr;
}

struct eigrp_neighbor *
eigrp_nbr_get (struct eigrp_interface *ei, struct eigrp_header *eigrph,
              struct ip *iph)
{
  struct eigrp_neighbor *nbr;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (ei->nbrs, node, nnode, nbr))
    {
      if(iph->ip_src.s_addr == nbr->src.s_addr)
        {
          return nbr;
        }
    }

  nbr = eigrp_nbr_add (ei, eigrph, iph);
  listnode_add(ei->nbrs, nbr);

  return nbr;
}

/* Delete specified EIGRP neighbor from interface. */
void
eigrp_nbr_delete (struct eigrp_neighbor *nbr)
{
  /* Cancel all events. *//* Thread lookup cost would be negligible. */
  thread_cancel_event (master, nbr);
  eigrp_fifo_free(nbr->multicast_queue);
  eigrp_fifo_free(nbr->retrans_queue);
  THREAD_OFF(nbr->t_holddown);

  listnode_delete(nbr->ei->nbrs,nbr);
  XFREE (MTYPE_EIGRP_NEIGHBOR, nbr);
}

int
holddown_timer_expired (struct thread *thread)
{
  struct eigrp_neighbor *nbr;

  nbr = THREAD_ARG(thread);

  zlog_info("Neighbor %s (%s) is down: holding time expired",inet_ntoa(nbr->src),ifindex2ifname(nbr->ei->ifp->ifindex));
  eigrp_nbr_delete(nbr);

  return 0;
}

int
eigrp_neighborship_check(struct eigrp_neighbor *nbr,struct TLV_Parameter_Type *param)
{
  struct eigrp *eigrp = nbr->ei->eigrp;
  if(eigrp->k_values[0]!=param->K1)
    {
      return -1;

    }

  return 1;
}
