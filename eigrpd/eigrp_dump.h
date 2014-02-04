/*
 * EIGRPd dump routine.
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


#ifndef _ZEBRA_EIGRPD_DUMP_H_
#define _ZEBRA_EIGRPD_DUMP_H_

#define EIGRP_TIME_DUMP_SIZE	16


extern const char *eigrp_if_name_string (struct eigrp_interface *);
extern const char *eigrp_if_ip_string (struct eigrp_interface *);
extern const char *eigrp_neigh_ip_string (struct eigrp_neighbor *);
extern const char *eigrp_topology_ip_string (struct eigrp_topology_node *);
extern void eigrp_ip_header_dump (struct ip *);
extern void show_ip_eigrp_interface_header (struct vty *);
extern void show_ip_eigrp_neighbor_header (struct vty *);
extern void show_ip_eigrp_topology_header (struct vty *);
extern void show_ip_eigrp_interface_sub (struct vty *, struct eigrp *,
					 struct eigrp_interface *);
extern void show_ip_eigrp_neighbor_sub (struct vty *, struct eigrp_neighbor *);
extern void show_ip_eigrp_topology_node (struct vty *, struct eigrp_topology_node *);
extern void show_ip_eigrp_topology_entry (struct vty *, struct eigrp_topology_entry *);


#endif /* _ZEBRA_EIGRPD_DUMP_H_ */
