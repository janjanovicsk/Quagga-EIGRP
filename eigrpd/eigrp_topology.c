/*
 * EIGRP Topology table support.
 * Copyright (C) 1999, 2000 Alex Zinin, Kunihiro Ishiguro, Toshiaki Takada
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

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_topology.h"

static int eigrp_topology_node_cmp(struct eigrp_topology_node *, struct eigrp_topology_node *);
static void eigrp_topology_node_del(struct eigrp_topology_node *);
static int eigrp_topology_entry_cmp(struct eigrp_topology_entry *, struct eigrp_topology_entry *);

/*
 * Returns linkedlist used as topology table
 * cmp - assigned function for comparing topology nodes
 * del - assigned function executed before deleting topology node by list function
 */
struct list *
eigrp_topology_new ()
{
        struct list* new = list_new();
        new->cmp = (int (*)(void *, void *)) eigrp_topology_node_cmp;
        new->del = (void (*) (void *)) eigrp_topology_node_del;

        return new;

}

/*
 * Topology node comparison
 */

static int
eigrp_topology_node_cmp(struct eigrp_topology_node *node1, struct eigrp_topology_node *node2)
{
        if (node1->destination->prefix.s_addr < node2->destination->prefix.s_addr)      // parameter used in list_add_sort()
                return -1;                                                                                                                              // actually set to destination
        if (node1->destination->prefix.s_addr > node2->destination->prefix.s_addr)      // IPv4 address
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
eigrp_topology_node_new ()
{
        struct eigrp_topology_node *new;
        new = XCALLOC (MTYPE_EIGRP_TOPOLOGY_NODE, sizeof(struct eigrp_topology_node));
        new->entries = list_new();
        new->entries->cmp = (int (*)(void *, void *)) eigrp_topology_entry_cmp;

        return new;
}

/*
 * Topology entry comparison
 */

static int
eigrp_topology_entry_cmp(struct eigrp_topology_entry *entry1, struct eigrp_topology_entry *entry2)
{
        if (entry1->distance < entry2->distance)                                // parameter used in list_add_sort()
                return -1;                                                                                      // actually set to sort by distance
        if (entry1->distance > entry2->distance)                                //
                return 1;
        return 0;
}

/*
 * Returns new topology entry
 */

struct eigrp_topology_entry *
eigrp_topology_entry_new ()
{
        struct eigrp_topology_entry *new;

        new = XCALLOC (MTYPE_EIGRP_TOPOLOGY_ENTRY, sizeof(struct eigrp_topology_entry));

        return new;
}

/*
 * Freeing topology table list
 */

void
eigrp_topology_free (struct list *list)
{
        list_free(list);
}

/*
 * Deleting all topology nodes in table
 */

void
eigrp_topology_cleanup (struct list *topology)
{
        assert(topology);

        eigrp_topology_delete_all(topology);

}

/*
 * Adding topology node to topology table
 */

void
eigrp_topology_node_add (struct list *topology , struct eigrp_topology_node *node)
{
        if(listnode_lookup(topology, node) != NULL)
                listnode_add_sort(topology, node);
}

/*
 * Adding topology entry to topology node
 */

void
eigrp_topology_entry_add (struct eigrp_topology_node *node, struct eigrp_topology_entry *entry)
{
        if(listnode_lookup(node->entries, entry) != NULL)
                listnode_add_sort(node->entries, entry);
}

/*
 * Deleting topology node from topology table
 */

void
eigrp_topology_node_delete (struct list *topology, struct eigrp_topology_node *node)
{
        if(listnode_lookup(topology, node)!= NULL){
                list_delete_all_node(node->entries);
                list_free(node->entries);
                listnode_delete(topology, node);
        }
}

/*
 * Deleting topology entry from topology node
 */

void
eigrp_topology_entry_delete (struct eigrp_topology_node *node, struct eigrp_topology_entry *entry)
{
        if(listnode_lookup(node->entries, node)!= NULL){
                        listnode_delete(node->entries, entry);
                }
}

/*
 * Deleting all nodes from topology table
 */

void
eigrp_topology_delete_all (struct list *topology)
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
        if(topology->count)
                return 1;
        else
                return 0;
}
