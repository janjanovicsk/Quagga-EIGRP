/*
 * EIGRP Interface functions.
 * Copyright (C) 1999 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
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

#ifndef _ZEBRA_EIGRP_INTERFACE_H_
#define _ZEBRA_EIGRP_INTERFACE_H_

#include "eigrp_packet.h"

#define DECLARE_IF_PARAM(T, P) T P; u_char P##__config:1
#define IF_EIGRP_IF_INFO(I) ((struct eigrp_if_info *)((I)->info))
#define IF_OIFS(I)  (IF_EIGRP_IF_INFO (I)->eifs)
#define IF_OIFS_PARAMS(I) (IF_EIGRP_IF_INFO (I)->params)

#define SET_IF_PARAM(S, P) ((S)->P##__config) = 1
#define IF_DEF_PARAMS(I) (IF_EIGRP_IF_INFO (I)->def_params)

#define UNSET_IF_PARAM(S, P) ((S)->P##__config) = 0


/*EIGRP interface structure*/
struct eigrp_interface
{
  /* This interface's parent eigrp instance. */
    struct eigrp *eigrp;

    /* Interface data from zebra. */
    struct interface *ifp;

    /* Packet send buffer. */
    struct eigrp_fifo *obuf;               /* Output queue */

    /* To which multicast groups do we currently belong? */

    /* Configured varables. */
      struct eigrp_if_params *params;

    u_char multicast_memberships;

    /* EIGRP Network Type. */
    u_char type;
 #define EIGRP_IFTYPE_NONE                0
 #define EIGRP_IFTYPE_POINTOPOINT         1
 #define EIGRP_IFTYPE_BROADCAST           2
 #define EIGRP_IFTYPE_NBMA                3
 #define EIGRP_IFTYPE_POINTOMULTIPOINT    4
 #define EIGRP_IFTYPE_LOOPBACK            5
 #define EIGRP_IFTYPE_MAX                 6

    struct prefix *address;             /* Interface prefix */
    struct connected *connected;          /* Pointer to connected */

    /* Neighbor information. */
      struct route_table *nbrs;             /* OSPF Neighbor List */

    /* Threads. */
    struct thread *t_hello;               /* timer */
    struct thread *t_wait;                /* timer */

    int on_write_q;
};

struct eigrp_if_params
{
  DECLARE_IF_PARAM (u_char, passive_interface);      /* EIGRP Interface is passive: no sending or receiving (no need to join multicast groups) */
  DECLARE_IF_PARAM (u_int32_t, v_hello);             /* Hello Interval */
  DECLARE_IF_PARAM (u_int32_t, v_wait);              /* Router Dead Interval */
  DECLARE_IF_PARAM (u_char, type);                   /* type of interface */

#define EIGRP_IF_ACTIVE                  0
#define EIGRP_IF_PASSIVE                 1

};

enum
{
  MEMBER_ALLROUTERS = 0,
  MEMBER_MAX,
};

struct eigrp_if_info
{
  struct eigrp_if_params *def_params;
  struct route_table *params;
  struct route_table *eifs;
  unsigned int membership_counts[MEMBER_MAX];   /* multicast group refcnts */
};

/*Prototypes*/
extern void eigrp_if_init (void);
extern int eigrp_if_new_hook (struct interface *);
extern int eigrp_if_delete_hook (struct interface *);

extern void eigrp_del_if_params (struct eigrp_if_params *);
extern struct eigrp_if_params *eigrp_new_if_params (void);
extern struct eigrp_interface * eigrp_if_new (struct eigrp *, struct interface *,
                                              struct prefix *);
extern struct eigrp_interface * eigrp_if_table_lookup (struct interface *ifp,
                                                       struct prefix *prefix);
struct eigrp_if_params *eigrp_lookup_if_params (struct interface *ifp,
                                                struct in_addr addr);
int eigrp_if_up (struct eigrp_interface *);
void eigrp_if_stream_set (struct eigrp_interface *);

#endif /* ZEBRA_EIGRP_INTERFACE_H_ */
