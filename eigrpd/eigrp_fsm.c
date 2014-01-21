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
eigrp_fsm_event_test(struct eigrp_fsm_action_message *, int);

struct
{
  int
  (*func)(struct eigrp_fsm_action_message *,int);
} NSM[EIGRP_FSM_STATE_MAX][EIGRP_FSM_EVENT_MAX] =
  {
    {
    //PASSIVE STATE
          { eigrp_fsm_event_test }, /* Event 1 */
          { eigrp_fsm_event_test }, /* Event 2 */
          { eigrp_fsm_event_test }, /* Event 3 */
          { eigrp_fsm_event_test }, /* Event 4 */
          { eigrp_fsm_event_test }, /* Event 5 */
          { eigrp_fsm_event_test }, /* Event 6 */
          { eigrp_fsm_event_test }, /* Event 7 */
          { eigrp_fsm_event_test }, /* Event 8 */
          { eigrp_fsm_event_test }, /* Event 9 */
          { eigrp_fsm_event_test }, /* Event 10 */
          { eigrp_fsm_event_test }, /* Event 11 */
          { eigrp_fsm_event_test }, /* Event 12 */
          { eigrp_fsm_event_test }, /* Event 13 */
          { eigrp_fsm_event_test }, /* Event 14 */
          { eigrp_fsm_event_test }, /* Event 15 */
          { eigrp_fsm_event_test }  /* Event 16 */

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
          { eigrp_fsm_event_test }, /* Event 9 */
          { eigrp_fsm_event_test }, /* Event 10 */
          { eigrp_fsm_event_test }, /* Event 11 */
          { eigrp_fsm_event_test }, /* Event 12 */
          { eigrp_fsm_event_test }, /* Event 13 */
          { eigrp_fsm_event_test }, /* Event 14 */
          { eigrp_fsm_event_test }, /* Event 15 */
          { eigrp_fsm_event_test }  /* Event 16 */

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
          { eigrp_fsm_event_test }, /* Event 9 */
          { eigrp_fsm_event_test }, /* Event 10 */
          { eigrp_fsm_event_test }, /* Event 11 */
          { eigrp_fsm_event_test }, /* Event 12 */
          { eigrp_fsm_event_test }, /* Event 13 */
          { eigrp_fsm_event_test }, /* Event 14 */
          { eigrp_fsm_event_test }, /* Event 15 */
          { eigrp_fsm_event_test } /* Event 16 */

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
          { eigrp_fsm_event_test }, /* Event 9 */
          { eigrp_fsm_event_test }, /* Event 10 */
          { eigrp_fsm_event_test }, /* Event 11 */
          { eigrp_fsm_event_test }, /* Event 12 */
          { eigrp_fsm_event_test }, /* Event 13 */
          { eigrp_fsm_event_test }, /* Event 14 */
          { eigrp_fsm_event_test }, /* Event 15 */
          { eigrp_fsm_event_test }  /* Event 16 */

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
          { eigrp_fsm_event_test }, /* Event 9 */
          { eigrp_fsm_event_test }, /* Event 10 */
          { eigrp_fsm_event_test }, /* Event 11 */
          { eigrp_fsm_event_test }, /* Event 12 */
          { eigrp_fsm_event_test }, /* Event 13 */
          { eigrp_fsm_event_test }, /* Event 14 */
          { eigrp_fsm_event_test }, /* Event 15 */
          { eigrp_fsm_event_test }  /* Event 16 */

    }, };

int
eigrp_get_fsm_event(struct eigrp_fsm_action_message *msg)
{

  struct eigrp_topology_node *node = msg->dest;
  u_char actual_state = node->state;

  switch (actual_state)
    {
  case EIGRP_FSM_STATE_PASSIVE:
    {
      switch (msg->packet_type)
        {
      case EIGRP_MSG_QUERY:
        {
          if (msg->adv_router != eigrp_topology_get_successor(node)->adv_router)
            {
              return EIGRP_FSM_EVENT_1;
            }
          else
            {
              return EIGRP_FSM_EVENT_3;
            }
          break;
        }
      case EIGRP_MSG_UPDATE:
        {
          if (node->dest_type == EIGRP_TOPOLOGY_TYPE_CONNECTED)
            {
              return EIGRP_FSM_EVENT_2;
            }
          if (msg->adv_router != eigrp_topology_get_successor(node)->adv_router)
            {
              return EIGRP_FSM_EVENT_2;
            }
          else if (eigrp_topology_get_fsuccessor(node) != NULL)
            {
              return EIGRP_FSM_EVENT_2;
            }
          else
            {
              return EIGRP_FSM_EVENT_3;
            }
          break;
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

  return 15;
}

void
eigrp_fsm_update_all_nodes()
{
  struct list *table = eigrp_lookup()->topology_table;
  struct eigrp_topology_node *data;
  struct listnode *node, *nnode;
  for (ALL_LIST_ELEMENTS(table, node, nnode, data))
    {
      eigrp_fsm_update_node(data);
    }
}

void
eigrp_fsm_update_node(struct eigrp_topology_node *dest)
{
  struct listnode *node, *nnode;
  struct eigrp_topology_entry *data, *successor;

  successor = NULL;

  u_int32_t best_metric = EIGRP_MAX_METRIC;

  for (ALL_LIST_ELEMENTS(dest->entries, node, nnode, data))
    {
      if (data->distance < best_metric)
        {
          best_metric = data->distance;
          successor = data;
        }
    }
  successor->flags = EIGRP_TOPOLOGY_ENTRY_SUCCESSOR_FLAG;
}

int
eigrp_fsm_event(struct thread *thread)
{
  int event;
  struct eigrp_fsm_action_message *msg;
  msg = (struct eigrp_fsm_action_message *) THREAD_ARG(thread);
  event = THREAD_VAL(thread);
  zlog_info("State: %d\nEvent: %d",msg->dest->state,event);
  (*(NSM[msg->dest->state][event].func))(msg,event);

  return 1;
}

int
eigrp_fsm_event_test(struct eigrp_fsm_action_message *msg, int cislo)
{
  zlog_info("Triggerred action %d\n", cislo);
  return 1;
}
