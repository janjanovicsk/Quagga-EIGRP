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

#include "eigrpd/eigrp_topology.h"

static int eigrp_topology_node_cmp(struct eigrp_topology_node *, struct eigrp_topology_node *);
static void eigrp_topology_node_del(struct eigrp_topology_node *);


struct list *
eigrp_topology_new ()
{
	struct list* new = list_new();
	eigrp_topology_init(new);

	return new;

}

void
eigrp_topology_init(struct list *topology)
{
	topology->cmp = (int (*)(void *, void *)) eigrp_topology_node_cmp;
	topology->del = (void (*) (void *)) eigrp_topology_node_del;
}

static int
eigrp_topology_node_cmp(struct eigrp_topology_node *node1, struct eigrp_topology_node *node2)
{
	if (node1->destination->s_addr < node2->destination->s_addr)
		return -1;
	if (node1->destination->s_addr > node2->destination->s_addr)
		return 1;
	return 0;
}

static void
eigrp_topology_node_del(struct eigrp_topology_node *node)
{
	//list_delete_all_nodes(node->records);
}

struct eigrp_topology_node *
eigrp_topology_node_new ()
{
	struct eigrp_topology_node *new;

	new = XCALLOC (MTYPE_EIGRP_TOPOLOGY_NODE, sizeof(struct eigrp_topology_node));

	return new;
}

struct eigrp_topology_entry *
eigrp_topology_entry_new ()
{
	struct eigrp_topology_entry *new;

	new = XCALLOC (MTYPE_EIGRP_TOPOLOGY_ENTRY, sizeof(struct eigrp_topology_entry));

	return new;
}

void
eigrp_topology_free (struct list *list)
{
	list_free(list);
}

void
eigrp_topology_cleanup (struct list *topology)
{
	assert(topology);

	//eigrp_topology_delete_all(topology);

}
void
eigrp_topology_node_add (struct list *topology, struct eigrp_topology_node *node)
{
	listnode_add_sort(topology, node);
}

void
eigrp_topology_entry_add (struct eigrp_topology_node * node, struct eigrp_topology_entry *entry)
{

}
