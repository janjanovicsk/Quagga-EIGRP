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

  switch (action_type)
    {
      case EIGRP_FSM_QUERY:
        {
          if (actual_state == EIGRP_TOPOLOGY_NODE_PASSIVE)
            {
              struct eigrp_topology_entry *entry = eigrp_topology_get_successor(
                  node);
              struct eigrp_fsm_query_event *query_data =
                  (struct eigrp_fsm_query_event *) data;
              if (query_data->adv_router == entry->adv_router)
                {

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
