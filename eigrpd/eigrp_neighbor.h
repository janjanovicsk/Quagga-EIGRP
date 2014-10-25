/*
 * EIGRP Neighbor Handling Functions.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
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

#ifndef _ZEBRA_EIGRP_NEIGHBOR_H
#define _ZEBRA_EIGRP_NEIGHBOR_H

/* Prototypes */
extern struct eigrp_neighbor *eigrp_nbr_get(struct eigrp_interface *,
					    struct eigrp_header *,
					    struct ip *);
extern struct eigrp_neighbor *eigrp_nbr_new (struct eigrp_interface *);
extern void eigrp_nbr_delete(struct eigrp_neighbor *);

extern int holddown_timer_expired(struct thread *);

extern int eigrp_neighborship_check(struct eigrp_neighbor *,struct TLV_Parameter_Type *);
extern void eigrp_nbr_state_update(struct eigrp_neighbor *);
extern void eigrp_nbr_state_set(struct eigrp_neighbor *, u_char state);
extern u_char eigrp_nbr_state_get(struct eigrp_neighbor *);
extern const char *eigrp_nbr_state_str(struct eigrp_neighbor *);

#endif /* _ZEBRA_EIGRP_NEIGHBOR_H */
