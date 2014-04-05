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
 *
 *
 * This file contains functions for executing logic of finite state machine
 *
 *                                +------------ +
 *                                |     (7)     |
 *                                |             v
 *                    +=====================================+
 *                    |                                     |
 *                    |              Passive                |
 *                    |                                     |
 *                    +=====================================+
 *                        ^     |     ^     ^     ^    |
 *                     (3)|     |  (1)|     |  (1)|    |
 *                        |  (0)|     |  (3)|     | (2)|
 *                        |     |     |     |     |    +---------------+
 *                        |     |     |     |     |                     \
 *              +--------+      |     |     |     +-----------------+    \
 *            /                /     /      |                        \    \
 *          /                /     /        +----+                    \    \
 *         |                |     |               |                    |    |
 *         |                v     |               |                    |    v
 *    +===========+   (6)  +===========+       +===========+   (6)   +===========+
 *    |           |------->|           |  (5)  |           |-------->|           |
 *    |           |   (4)  |           |------>|           |   (4)   |           |
 *    | ACTIVE 0  |<-------| ACTIVE 1  |       | ACTIVE 2  |<--------| ACTIVE 3  |
 * +--|           |     +--|           |    +--|           |      +--|           |
 * |  +===========+     |  +===========+    |  +===========+      |  +===========+
 * |       ^  |(5)      |      ^            |    ^    ^           |         ^
 * |       |  +---------|------|------------|----+    |           |         |
 * +-------+            +------+            +---------+           +---------+
 *    (7)                 (7)                  (7)                   (7)
 *
 * 0- input event other than query from successor, FC not satisfied
 * 1- last reply, FD is reset
 * 2- query from successor, FC not satisfied
 * 3- last reply, FC satisfied with current value of FDij
 * 4- distance increase while in active state
 * 5- query from successor while in active state
 * 6- last reply, FC not satisfied with current value of FDij
 * 7- state not changed, usually by receiving not last reply
 *
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

/*
 * Prototypes
 */
int
eigrp_fsm_event_test(struct eigrp_fsm_action_message *);
int
eigrp_fsm_event_nq_fcn(struct eigrp_fsm_action_message *);
int
eigrp_fsm_event_q_fcn(struct eigrp_fsm_action_message *);
int
eigrp_fsm_event_lr(struct eigrp_fsm_action_message *);

//---------------------------------------------------------------------

/*
 * NSM - field of fields of struct containing one function each.
 * Which function is used depends on actual state of FSM and occurred
 * event(arrow in diagram). Usage:
 * NSM[actual/starting state][occurred event].func
 * Functions are should be executed within separate thread.
 *
 * Events which doesn't belong to state are replaced by "do nothing"
 * function.
 */
struct
{
  int
  (*func)(struct eigrp_fsm_action_message *);
} NSM[EIGRP_FSM_STATE_MAX][EIGRP_FSM_EVENT_MAX] =
  {
    {
    //PASSIVE STATE
          { eigrp_fsm_event_nq_fcn }, /* Event 0 */
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_q_fcn }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
    },
    {
    //Active 0 state
          { eigrp_fsm_event_test }, /* Event 0 */
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */

    },
    {
    //Active 1 state
          { eigrp_fsm_event_test }, /* Event 0 */
          { eigrp_fsm_event_lr }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
    },
    {
    //Active 2 state
          { eigrp_fsm_event_test }, /* Event 0 */
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
    },
    {
    //Active 3 state
          { eigrp_fsm_event_test }, /* Event 0 */
          { eigrp_fsm_event_lr }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
    }, };

/*
 * Main function in which are make decisions which event occurred.
 * msg - argument of type struct eigrp_fsm_action_message contain
 * details about what happen
 *
 * Return number of occurred event (arrow in diagram).
 *
 */
int
eigrp_get_fsm_event(struct eigrp_fsm_action_message *msg)
{
  // Loading base information from message
  struct eigrp_prefix_entry *prefix = msg->prefix;
  struct eigrp_neighbor_entry *entry = msg->entry;
  u_char actual_state = prefix->state;

  // Dividing by actual state of prefix's FSM
  switch (actual_state)
    {
  case EIGRP_FSM_STATE_PASSIVE:
    {
      //If entry doesn't exists yet
      if (entry == NULL)
        {
          /*
           * If it's a update packet then new neighbor_entry is created
           * with values from received TLV.
           *
           * In case of different packet type do nothing (ignore)
           */
          if (msg->packet_type == EIGRP_MSG_UPDATE)
            {
              entry = eigrp_neighbor_entry_new();
              entry->adv_router = msg->adv_router;
              entry->ei = msg->adv_router->ei;
              entry->prefix = prefix;
              msg->entry = entry;
              //Calculate resultant metrics and insert to correct position in entries list
              eigrp_topology_update_distance(msg);

              //If new entry is closer to destination then current successor
              if (entry->distance < prefix->distance)
                {
                  //TO DO: remove current successor/s from route table
                  eigrp_topology_update_node(prefix);
                  //TO DO: insert new successor route to route table
                  eigrp_update_send_all(prefix, msg->adv_router->ei);
                }
              //If not just set correct flags
              else
                {
                  entry->flags =
                      entry->distance == prefix->distance ?
                          EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG :
                          (entry->reported_distance < prefix->fdistance ?
                              EIGRP_NEIGHBOR_ENTRY_FSUCCESSOR_FLAG : 0);
                }
              return EIGRP_FSM_KEEP_STATE;
            }
          //TO DO: send infinity
          return EIGRP_FSM_KEEP_STATE;
        }
      //If neighbor entry already exists
      else
        {
          //Calculate resultant metrics and insert to correct position in entries list
          eigrp_topology_update_distance(msg);
          prefix->fdistance = //Update feasible distance
              prefix->fdistance > entry->distance ?
                  entry->distance : prefix->fdistance;

          struct eigrp_neighbor_entry * head =
              (struct eigrp_neighbor_entry *) entry->prefix->entries->head->data;
          /*
           * After distance change check if first entry in list (with best metric)
           * has successor flag and satisfy feasible condition
           */
          if ((head->flags & EIGRP_NEIGHBOR_ENTRY_SUCCESSOR_FLAG) == 1
              && head->reported_distance < prefix->fdistance)
            {
              //TO DO: remove successors with increased distance from route table
              //If there is distance change update prefix and send update
              if (head->distance != prefix->distance)
                {
                  eigrp_topology_update_node(prefix);
                  eigrp_update_send_all(prefix, msg->adv_router->ei);
                }
              //TO DO: insert possible new successor to route table

              return EIGRP_FSM_KEEP_STATE;
            }
          /*
           * If on top of list isn't entry with successor satisfying feasible condition
           * check if current top entry satisfy feasible condition and if yes use it
           * as new successor, if not it means moving to active state based on packet type
           */
          else
            {
              if ((head->flags & EIGRP_NEIGHBOR_ENTRY_FSUCCESSOR_FLAG) == 1)
                {
                  //TO DO: remove successors with increased distance from route table
                  eigrp_topology_update_node(prefix);
                  //TO DO: insert possible new successor/s to route table
                  eigrp_update_send_all(prefix, msg->adv_router->ei);

                  return EIGRP_FSM_KEEP_STATE;
                }
              else
                {
                  if (msg->packet_type == EIGRP_MSG_QUERY)
                    {
                      return EIGRP_FSM_EVENT_Q_FCN;
                    }
                  else
                    {
                      return EIGRP_FSM_EVENT_NQ_FCN;
                    }
                }
            }
        }
    }

  case EIGRP_FSM_STATE_ACTIVE_0:
    {
      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_1:
    {
      /*
       * When reply is received remove neighbor from whom was reply received from
       * rij list and update distance according to reply TLV. If list is still not empty
       * "do nothing" (wait for rest replies), otherwise indicate "last reply" event
       */
      if (msg->packet_type == EIGRP_MSG_REPLY)
        {
          listnode_delete(prefix->rij, entry->adv_router);
          eigrp_topology_update_distance(msg);

          if (prefix->rij->count)
            {
              return EIGRP_FSM_KEEP_STATE;
            }
          else
            {
              zlog_info("All reply received\n");
              return EIGRP_FSM_EVENT_LR;
            }
        }
      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_2:
    {
      break;
    }
  case EIGRP_FSM_STATE_ACTIVE_3:
    {
      /*
       * When reply is received remove neighbor from whom was reply received from
       * rij list and update distance according to reply TLV. If list is still not empty
       * "do nothing" (wait for rest replies), otherwise indicate "last reply" event
       */
      if (msg->packet_type == EIGRP_MSG_REPLY)
        {
          listnode_delete(prefix->rij, entry->adv_router);
          eigrp_topology_update_distance(msg);

          if (prefix->rij->count)
            {
              return EIGRP_FSM_KEEP_STATE;
            }
          else
            {
              zlog_info("All reply received\n");
              return EIGRP_FSM_EVENT_LR;
            }
        }
      break;
    }
    }

  return 7;
}

/*
 * Function made to execute in separate thread.
 * Load argument from thread and execute proper NSM function
 */
int
eigrp_fsm_event(struct thread *thread)
{
  int event;
  struct eigrp_fsm_action_message *msg;
  msg = (struct eigrp_fsm_action_message *) THREAD_ARG(thread);
  event = THREAD_VAL(thread);
  zlog_info("State: %d  Event: %d Network: %s\n", msg->prefix->state, event,
      eigrp_topology_ip_string(msg->prefix));
  (*(NSM[msg->prefix->state][event].func))(msg);

  return 1;
}
/*
 * Function of event 0.
 *
 */
int
eigrp_fsm_event_nq_fcn(struct eigrp_fsm_action_message *msg)
{

  struct eigrp_prefix_entry *prefix = msg->prefix;
  prefix->state = EIGRP_FSM_STATE_ACTIVE_1;
  prefix->rdistance = prefix->distance =
      eigrp_topology_get_successor(prefix)->distance;
  eigrp_query_send_all(msg->entry);

  return 1;
}

int
eigrp_fsm_event_q_fcn(struct eigrp_fsm_action_message *msg)
{

  struct eigrp_prefix_entry *prefix = msg->prefix;
  prefix->state = EIGRP_FSM_STATE_ACTIVE_3;
  prefix->rdistance = prefix->distance =
      eigrp_topology_get_successor(prefix)->distance;
  eigrp_query_send_all(msg->entry);

  return 1;
}

int
eigrp_fsm_event_test(struct eigrp_fsm_action_message *msg)
{
  return 1;
}

int
eigrp_fsm_event_lr(struct eigrp_fsm_action_message *msg)
{

  struct eigrp_prefix_entry *prefix = msg->prefix;
  prefix->state = EIGRP_FSM_STATE_PASSIVE;

  //TO DO: remove current successor route from route table
  eigrp_topology_update_node(prefix);
  //TO DO: insert new successor route to route table
  eigrp_update_send_all(prefix, msg->adv_router->ei);

  return 1;
}
