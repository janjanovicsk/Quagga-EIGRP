/*
 * EIGRP neighbor related functions.
 *   Copyright (C) 1999 Toshiaki Takada
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


#define EIGRP_NEIGHBOR_DOWN           0
#define EIGRP_NEIGHBOR_PENDING        1
#define EIGRP_NEIGHBOR_UP             2
#define EIGRP_NEIGHBOR_STATE_MAX      3

/* Neighbor Data Structure */
struct eigrp_neighbor
{
  /* This neighbor's parent eigrp interface. */
  struct eigrp_interface *ei;

  /* OSPF neighbor Information */
  u_char state;                               /* neigbor status. */
  u_int32_t sequence_number;                  /* Sequence Number. */
  u_int32_t ack;                              /* Acknowledgement number*/

  /* Neighbor Information from Hello. */
  struct prefix address;                /* Neighbor Interface Address. */

  struct in_addr src;                   /* Src address. */

  u_char K1;
  u_char K2;
  u_char K3;
  u_char K4;
  u_char K5;
  u_char K6;

  /* Timer values. */
  u_int16_t v_holddown;

  /* Threads. */
  struct thread *t_holddown;
};


/* Prototypes */
extern struct eigrp_neighbor *eigrp_nbr_get (struct eigrp_interface *,
                                              struct eigrp_header *,
                                              struct ip *, struct prefix *);
extern struct eigrp_neighbor *eigrp_nbr_new (struct eigrp_interface *);
extern void eigrp_nbr_free (struct eigrp_neighbor *);
extern void eigrp_nbr_delete (struct eigrp_neighbor *);

#endif /* _ZEBRA_EIGRP_NEIGHBOR_H */
