/*
 * EIGRPd Finite State Machine (DUAL).
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

#include <thread.h>
#include <zebra.h>

#include "prefix.h"
#include "table.h"
#include "memory.h"
#include "log.h"
#include "linklist.h"

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
#include "eigrpd/eigrp_fsm.h"

int
eigrp_fsm_event_test(struct eigrp_fsm_action_message *);
int
eigrp_fsm_event_nq_fcn(struct eigrp_fsm_action_message *);

struct
{
  int
  (*func)(struct eigrp_fsm_action_message *);
} NSM[EIGRP_FSM_STATE_MAX][EIGRP_FSM_EVENT_MAX] =
  {
    {
    //PASSIVE STATE
          { eigrp_fsm_event_nq_fcn }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
          { eigrp_fsm_event_test }, /* Event 8 */
    },
    {
    //Active 0 state
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
          { eigrp_fsm_event_test }, /* Event 8 */

    },
    {
    //Active 1 state
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
          { eigrp_fsm_event_test }, /* Event 8 */
    },
    {
    //Active 2 state
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
          { eigrp_fsm_event_test }, /* Event 8 */
    },
    {
    //Active 3 state
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
          { eigrp_fsm_event_test }, /* Event 8 */
    }, };

int
eigrp_get_fsm_event(struct eigrp_fsm_action_message *msg)
{

  struct eigrp_topology_node *node = msg->entry->node;
  struct eigrp_topology_entry *entry = msg->entry;
  u_char actual_state = node->state;

  switch (actual_state)
    {
  case EIGRP_FSM_STATE_PASSIVE:
    {
      if (entry == NULL)
        {
          entry = eigrp_topology_entry_new();
          entry->adv_router = msg->adv_router;
          entry->ei = msg->adv_router->ei;
          entry->node = node;
          eigrp_topology_entry_add(node, entry);
          eigrp_topology_update_distance(msg);

          if (entry->distance < eigrp_topology_get_successor(node)->distance)
            {
              eigrp_topology_get_successor(node)->flags = 0;
              //vyhodit succesora z route table
              eigrp_topology_update_node(node);
              //vlozit noveho sucessora
              eigrp_update_send_all(entry, msg->adv_router->ei);
            }
          else
            {
              entry->flags =
                  entry->reported_distance < node->fdistance ?
                      EIGRP_TOPOLOGY_ENTRY_FSUCCESSOR_FLAG : 0;
            }
          return EIGRP_FSM_EVENT_SNC;
        }
      else
        {
          eigrp_topology_update_distance(msg);
          node->fdistance =
              node->fdistance > entry->distance ?
                  entry->distance : node->fdistance;
          /*
           * If FC not satisfied
           */
          if (eigrp_topology_get_successor(node)->distance > node->fdistance)
            {
              struct eigrp_topology_entry *best = eigrp_topology_get_best_entry(
                  node);
              if (best->reported_distance < node->fdistance)
                {
                  eigrp_topology_get_successor(node)->flags = 0;
                  //vyhodit succesora z route table
                  eigrp_topology_update_node(node);
                  //vlozit noveho sucessora
                  eigrp_update_send_all(entry, msg->adv_router->ei);

                  return EIGRP_FSM_EVENT_SNC;
                }

              if (msg->packet_type == EIGRP_MSG_QUERY)
                {
                  return EIGRP_FSM_EVENT_Q_FCN;
                }
              else
                {
                  return EIGRP_FSM_EVENT_NQ_FCN;
                }
            }
          /*
           * If FC satisfied
           * send update with change to all neighbors
           */
          else
            {
              if (entry->flags
                  & EIGRP_TOPOLOGY_ENTRY_SUCCESSOR_FLAG
                      == EIGRP_TOPOLOGY_ENTRY_SUCCESSOR_FLAG)
                {
                  eigrp_update_send_all(entry, msg->adv_router->ei);
                }
              return EIGRP_FSM_EVENT_SNC;
            }
        }
      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_0:
    {
      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_1:
    {

      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_2:
    {
      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_3:
    {
      break;
    }
    }

  return 7;
}

int
eigrp_fsm_event(struct thread *thread)
{
  int event;
  struct eigrp_fsm_action_message *msg;
  msg = (struct eigrp_fsm_action_message *) THREAD_ARG(thread);
  event = THREAD_VAL(thread);
  zlog_info("State: %d\nEvent: %d\n", msg->entry->node->state, event);
  (*(NSM[msg->entry->node->state][event].func))(msg);

  return 1;
}

int
eigrp_fsm_event_nq_fcn(struct eigrp_fsm_action_message *msg)
{

  struct eigrp_topology_node *node = msg->entry->node;
  node->state = EIGRP_FSM_STATE_ACTIVE_1;
  node->rdistance = node->distance = eigrp_topology_get_successor(node)->distance;
  //send query to all


  return 1;
}


int
eigrp_fsm_event_test(struct eigrp_fsm_action_message *msg)
{
  return 1;
}
