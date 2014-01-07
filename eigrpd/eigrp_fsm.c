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
eigrp_get_fsm_event(struct eigrp_topology_node *node, struct eigrp_fsm_action_message *msg)
{

  u_char actual_state = node->state;

  switch (actual_state)
  {
      case EIGRP_FSM_STATE_PASSIVE:
        {
          switch (msg->type)
          {
              case EIGRP_FSM_QUERY:
                {
                  if(msg->adv_router != eigrp_topology_get_successor(node))
                    {
                      return EIGRP_FSM_EVENT_1;
                    }
                  break;
                }
              case EIGRP_FSM_UPDATE:
          }
        }
    }


  return -1;
}
