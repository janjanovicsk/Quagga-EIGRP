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

  eigrp_nbr_state_set (nbr, EIGRP_NEIGHBOR_DOWN);
//  nbr->state = EIGRP_NEIGHBOR_DOWN;
//  if (ei!=NULL)
//    nbr->v_holddown = EIGRP_IF_PARAM (ei,v_wait);
//  else
//    nbr->v_holddown = EIGRP_HOLD_INTERVAL_DEFAULT;
//
//  nbr->retrans_queue = eigrp_fifo_new ();
//  nbr->multicast_queue = eigrp_fifo_new ();

  return nbr;
}

/**
 *@fn void dissect_eigrp_sw_version (tvbuff_t *tvb, proto_tree *tree,
 *                                   proto_item *ti)
 *
 * @par
 * Create a new neighbor structure and initalize it.
 */
static struct eigrp_neighbor *
eigrp_nbr_add (struct eigrp_interface *ei, struct eigrp_header *eigrph,
              struct ip *iph)
{
  struct eigrp_neighbor *nbr;

  nbr = eigrp_nbr_new (ei);
  nbr->src = iph->ip_src;

//  if (IS_DEBUG_EIGRP_EVENT)
//    zlog_debug("NSM[%s:%s]: start", IF_NAME (nbr->oi),
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
      if (iph->ip_src.s_addr == nbr->src.s_addr)
        {
          return nbr;
        }
    }

  nbr = eigrp_nbr_add (ei, eigrph, iph);
  listnode_add (ei->nbrs, nbr);

  return nbr;
}

/* Delete specified EIGRP neighbor from interface. */
void
eigrp_nbr_delete (struct eigrp_neighbor *nbr)
{
  /* Cancel all events. *//* Thread lookup cost would be negligible. */
  thread_cancel_event (master, nbr);
  eigrp_fifo_free (nbr->multicast_queue);
  eigrp_fifo_free (nbr->retrans_queue);
  THREAD_OFF (nbr->t_holddown);

  listnode_delete (nbr->ei->nbrs,nbr);
  XFREE (MTYPE_EIGRP_NEIGHBOR, nbr);
}

int
holddown_timer_expired (struct thread *thread)
{
  struct eigrp_neighbor *nbr;

  nbr = THREAD_ARG (thread);

  zlog_info ("Neighbor %s (%s) is down: holding time expired",
	     inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));
  eigrp_nbr_delete (nbr);

  return 0;
}

u_char
eigrp_nbr_state_get (struct eigrp_neighbor *nbr)
{
  return(nbr->state);
}

void
eigrp_nbr_state_set (struct eigrp_neighbor *nbr, u_char state)
{

  nbr->state = state;

  if (eigrp_nbr_state_get(nbr) == EIGRP_NEIGHBOR_DOWN)
    {
      // reset all the seq/ack counters
      nbr->recv_sequence_number = 0;
      nbr->init_sequence_number = 0;
      nbr->retrans_counter = 0;

      // Kvalues
      nbr->K1 = EIGRP_K1_DEFAULT;
      nbr->K2 = EIGRP_K2_DEFAULT;
      nbr->K3 = EIGRP_K3_DEFAULT;
      nbr->K4 = EIGRP_K4_DEFAULT;
      nbr->K5 = EIGRP_K5_DEFAULT;
      nbr->K6 = EIGRP_K6_DEFAULT;

      // hold time..
      nbr->v_holddown = EIGRP_HOLD_INTERVAL_DEFAULT;

      /* out with the old */
      if (nbr->multicast_queue)
        eigrp_fifo_free (nbr->multicast_queue);
      if (nbr->retrans_queue)
      eigrp_fifo_free (nbr->retrans_queue);

      /* in with the new */
      nbr->retrans_queue = eigrp_fifo_new ();
      nbr->multicast_queue = eigrp_fifo_new ();

    }
}

const char *
eigrp_nbr_state_str (struct eigrp_neighbor *nbr)
{
  const char *state;
  switch (nbr->state)
    {
    case EIGRP_NEIGHBOR_DOWN:
      {
	state = "Down";
	break;
      }
    case EIGRP_NEIGHBOR_PENDING:
      {
	state = "Waiting for Init";
	break;
      }
    case EIGRP_NEIGHBOR_PENDING_INIT:
      {
	state = "Waiting for Init Ack";
	break;
      }
    case EIGRP_NEIGHBOR_UP:
      {
	state = "Up";
	break;
      }
    }

  return(state);
}

void
eigrp_nbr_state_update (struct eigrp_neighbor *nbr)
{
  switch (nbr->state)
    {
    case EIGRP_NEIGHBOR_DOWN:
      {
	/*Start Hold Down Timer for neighbor*/
	THREAD_OFF(nbr->t_holddown);
	THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired,
			nbr, nbr->v_holddown);
	break;
      }
    case EIGRP_NEIGHBOR_PENDING:
      {
	/*Reset Hold Down Timer for neighbor*/
	THREAD_OFF(nbr->t_holddown);
	THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired, nbr,
			nbr->v_holddown);
	break;
      }
    case EIGRP_NEIGHBOR_PENDING_INIT:
      {
	/*Reset Hold Down Timer for neighbor*/
	THREAD_OFF(nbr->t_holddown);
	THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired, nbr,
			nbr->v_holddown);
	break;
      }
    case EIGRP_NEIGHBOR_UP:
      {
	/*Reset Hold Down Timer for neighbor*/
	THREAD_OFF(nbr->t_holddown);
	THREAD_TIMER_ON(master, nbr->t_holddown, holddown_timer_expired, nbr,
			nbr->v_holddown);
	break;
      }
    }
}
