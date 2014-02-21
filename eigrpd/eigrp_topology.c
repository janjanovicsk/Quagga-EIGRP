/*
 * EIGRP Topology table support.
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

static int
eigrp_topology_node_cmp(struct eigrp_topology_node *,
    struct eigrp_topology_node *);
static void
eigrp_topology_node_del(struct eigrp_topology_node *);
static int
eigrp_topology_entry_cmp(struct eigrp_topology_entry *,
    struct eigrp_topology_entry *);

/*
 * asdf;laksdjf;lajsdf;kasdjf;asdjf;
 * asdfaskdjfa;sdkjf;adlskj
 * Returns linkedlist used as topology table
 * cmp - assigned function for comparing topology nodes
 * del - assigned function executed before deleting topology node by list function
 */
struct list *
eigrp_topology_new()
{
  struct list* new = list_new();
  new->cmp = (int
  (*)(void *, void *)) eigrp_topology_node_cmp;
  new->del = (void
  (*)(void *)) eigrp_topology_node_del;

  return new;

}

/*
 * Topology node comparison
 */

static int
eigrp_topology_node_cmp(struct eigrp_topology_node *node1,
    struct eigrp_topology_node *node2)
{
  if (node1->destination->prefix.s_addr < node2->destination->prefix.s_addr) // parameter used in list_add_sort()
    return -1; // actually set to destination
  if (node1->destination->prefix.s_addr > node2->destination->prefix.s_addr) // IPv4 address
    return 1;
  return 0;
}

/*
 * Topology node delete
 */

static void
eigrp_topology_node_del(struct eigrp_topology_node *node)
{
  list_delete_all_node(node->entries);
  list_free(node->entries);
}

/*
 * Returns new created toplogy node
 * cmp - assigned function for comparing topology entry
 */

struct eigrp_topology_node *
eigrp_topology_node_new()
{
  struct eigrp_topology_node *new;
  new = XCALLOC(MTYPE_EIGRP_TOPOLOGY_NODE, sizeof(struct eigrp_topology_node));
  new->destination = XCALLOC(MTYPE_PREFIX_IPV4, sizeof(struct prefix_ipv4));
  new->entries = list_new();
  new->entries->cmp = (int
  (*)(void *, void *)) eigrp_topology_entry_cmp;

  return new;
}

/*
 * Topology entry comparison
 */

static int
eigrp_topology_entry_cmp(struct eigrp_topology_entry *entry1,
    struct eigrp_topology_entry *entry2)
{
  if (entry1->distance < entry2->distance) // parameter used in list_add_sort()
    return -1; // actually set to sort by distance
  if (entry1->distance > entry2->distance) //
    return 1;
  return 0;
}

/*
 * Returns new topology entry
 */

struct eigrp_topology_entry *
eigrp_topology_entry_new()
{
  struct eigrp_topology_entry *new;

  new = XCALLOC(MTYPE_EIGRP_TOPOLOGY_ENTRY,
      sizeof(struct eigrp_topology_entry));

  return new;
}

/*
 * Freeing topology table list
 */

void
eigrp_topology_free(struct list *list)
{
  list_free(list);
}

/*
 * Deleting all topology nodes in table
 */

void
eigrp_topology_cleanup(struct list *topology)
{
  assert(topology);

  eigrp_topology_delete_all(topology);

}

/*
 * Adding topology node to topology table
 */

void
eigrp_topology_node_add(struct list *topology, struct eigrp_topology_node *node)
{
  if (listnode_lookup(topology, node) == NULL)
    {
      listnode_add_sort(topology, node);
    }

}

/*
 * Adding topology entry to topology node
 */

void
eigrp_topology_entry_add(struct eigrp_topology_node *node,
    struct eigrp_topology_entry *entry)
{
  if (listnode_lookup(node->entries, entry) == NULL)
    {
      listnode_add_sort(node->entries, entry);
      entry->parent = node;
    }

}

/*
 * Deleting topology node from topology table
 */

void
eigrp_topology_node_delete(struct list *topology,
    struct eigrp_topology_node *node)
{
  if (listnode_lookup(topology, node) != NULL)
    {
      list_delete_all_node(node->entries);
      list_free(node->entries);
      listnode_delete(topology, node);
    }
}

/*
 * Deleting topology entry from topology node
 */

void
eigrp_topology_entry_delete(struct eigrp_topology_node *node,
    struct eigrp_topology_entry *entry)
{
  if (listnode_lookup(node->entries, node) != NULL)
    {
      listnode_delete(node->entries, entry);
    }
}

/*
 * Deleting all nodes from topology table
 */

void
eigrp_topology_delete_all(struct list *topology)
{
  list_delete_all_node(topology);
}

/*
 * Return 0 if topology is not empty
 * otherwise return 1
 */

unsigned int
eigrp_topology_table_isempty(struct list *topology)
{
  if (topology->count)
    return 1;
  else
    return 0;
}

struct eigrp_topology_node *
eigrp_topology_table_lookup(struct list *topology_table,
    struct prefix_ipv4 * address)
{
  struct eigrp_topology_node *data;
  struct listnode *node, *nnode;
  for (ALL_LIST_ELEMENTS(topology_table, node, nnode, data))
    {
      if ((data->destination->prefix.s_addr == address->prefix.s_addr)
          && (data->destination->prefixlen == address->prefixlen))
        return data;
    }
  return NULL;
}

extern struct eigrp_topology_entry *
eigrp_topology_get_successor(struct eigrp_topology_node *table_node)
{
  struct eigrp_topology_entry *data;
  struct listnode *node, *nnode;
  for (ALL_LIST_ELEMENTS(table_node->entries, node, nnode, data))
    {
      if ((data->flags & EIGRP_TOPOLOGY_ENTRY_SUCCESSOR_FLAG) == 1)
        {
          return data;
        }
    }
  return NULL;
}

extern struct eigrp_topology_entry *
eigrp_topology_get_fsuccessor(struct eigrp_topology_node *table_node)
{
  struct eigrp_topology_entry *data;
  struct listnode *node, *nnode;
  for (ALL_LIST_ELEMENTS(table_node->entries, node, nnode, data))
    {
      if ((data->flags & EIGRP_TOPOLOGY_ENTRY_FSUCCESSOR_FLAG) == 1)
        {
          return data;
        }
    }
  return NULL;
}

struct eigrp_topology_entry *
eigrp_topology_node_lookup(struct list *entries, struct eigrp_neighbor *nbr)
{
  struct eigrp_topology_entry *data;
  struct listnode *node, *nnode;
  for (ALL_LIST_ELEMENTS(entries, node, nnode, data))
    {
      if (data->adv_router == nbr)
        {
          return data;
        }
    }
  return NULL;
}

void
eigrp_topology_update_distance(struct eigrp_fsm_action_message *msg)
{
  struct eigrp_topology_node *node = msg->dest;
  struct eigrp_topology_entry *entry = eigrp_topology_node_lookup(node->entries,msg->adv_router);

  assert(entry);

  struct TLV_IPv4_External_type *ext_data = NULL;
  struct TLV_IPv4_Internal_type *int_data = NULL;
  if(msg->data_type == TLV_INTERNAL_TYPE)
    {
      int_data = msg->data.ipv4_int_type;
      entry->reported_metric = int_data->metric;
      entry->reported_distance = eigrp_calculate_metrics(&int_data->metric);
      entry->feasible_metric= int_data->metric;
      u_int32_t bw = EIGRP_IF_PARAM(entry->ei,bandwidth);
      entry->feasible_metric.bandwith = entry->feasible_metric.bandwith > bw ? bw : entry->feasible_metric.bandwith;
      entry->feasible_metric.delay += EIGRP_IF_PARAM(entry->ei, delay);
      entry->distance = eigrp_calculate_metrics(&entry->feasible_metric);
      node->fdistance = node->fdistance > entry->distance ? entry->distance : node->fdistance;

    }
  else
    {
      ext_data = msg->data.ipv4_ext_data;
    }


}
