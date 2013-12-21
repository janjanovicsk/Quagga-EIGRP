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
eigrp_get_fsm_event(struct eigrp_topology_node *node, u_char action_type,
    void *data)
{

  u_char actual_state = node->state;

  switch (actual_state)
  {
      case EIGRP_FSM_STATE_PASSIVE:
        {
          if (action_type == EIGRP_FSM_QUERY)
            {
              struct eigrp_topology_entry *successor = eigrp_topology_get_successor(
                  node);
              struct eigrp_fsm_query_event *query_data =
                  (struct eigrp_fsm_query_event *) data;
              if (query_data->adv_router == successor->adv_router)
                {
                  struct eigrp_topology_entry *fsuccessor = eigrp_topology_get_fsuccessor(
                                    node);
                  if(fsuccessor != NULL)
                    {
                      return EIGRP_FSM_EVENT_2;
                    }
                  else
                    {
                      return EIGRP_FSM_EVENT_3;
                    }
                }
              else
                {
                  return EIGRP_FSM_EVENT_1;
                }

            }
          else if (action_type == EIGRP_FSM_IF_COST)
            {
              struct eigrp_topology_entry *successor = eigrp_topology_get_successor(node);
              struct eigrp_fsm_cost_event *cost_data =(struct eigrp_fsm_cost_event *) data;
              if(cost_data->adv_router == NULL || cost_data->adv_router == successor->adv_router)
                {
                  struct eigrp_topology_entry *fsuccessor = eigrp_topology_get_fsuccessor(
                                                      node);
                  if(fsuccessor != NULL)
                    {
                      return EIGRP_FSM_EVENT_2;
                    }
                  else
                    {
                      return EIGRP_FSM_EVENT_4;
                    }
                }
              else
                {
                  return EIGRP_FSM_EVENT_4;
                }
            }
          break;
        }
      default:
        {
          break;
        }
    }


  return -1;
}
