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

#ifndef _ZEBRA_EIGRP_TOPOLOGY_H
#define _ZEBRA_EIGRP_TOPOLOGY_H


/* EIGRP Topology table related functions. */
extern struct list *eigrp_topology_new (void);
extern void eigrp_topology_init(struct list*);
extern struct eigrp_topology_node *eigrp_topology_node_new (void);
extern void eigrp_topology_node_init(struct eigrp_topology_node*);
extern struct eigrp_topology_entry *eigrp_topology_entry_new (void);
extern void eigrp_topology_free (struct list *);
extern void eigrp_topology_cleanup (struct list *);
extern void eigrp_topology_node_add (struct list *, struct eigrp_topology_node *);
extern void eigrp_topology_entry_add (struct eigrp_topology_node *, struct eigrp_topology_entry *);
extern void eigrp_topology_node_delete (struct list *, struct eigrp_topology_node *);
extern void eigrp_topology_entry_delete (struct eigrp_topology_node *, struct eigrp_topology_entry *);
extern void eigrp_topology_delete_all (struct list *);
extern unsigned int eigrp_topology_table_isempty(struct list *);
/* Set all stats to -1 (LSA_SPF_NOT_EXPLORED). */
/*extern void ospf_lsdb_clean_stat (struct ospf_lsdb *lsdb);
extern struct ospf_lsa *ospf_lsdb_lookup (struct ospf_lsdb *, struct ospf_lsa *);
extern struct ospf_lsa *ospf_lsdb_lookup_by_id (struct ospf_lsdb *, u_char,
                                        struct in_addr, struct in_addr);
extern struct ospf_lsa *ospf_lsdb_lookup_by_id_next (struct ospf_lsdb *, u_char,
                                             struct in_addr, struct in_addr,
                                             int);
extern unsigned long ospf_lsdb_count_all (struct ospf_lsdb *);
extern unsigned long ospf_lsdb_count (struct ospf_lsdb *, int);
extern unsigned long ospf_lsdb_count_self (struct ospf_lsdb *, int);
extern unsigned int ospf_lsdb_checksum (struct ospf_lsdb *, int);
extern unsigned long ospf_lsdb_isempty (struct ospf_lsdb *);
*/
#endif
