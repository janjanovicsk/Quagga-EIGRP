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


#define DECLARE_IF_PARAM(T, P) T P; u_char P##__config:1
#define IF_EIGRP_IF_INFO(I) ((struct eigrp_if_info *)((I)->info))
#define IF_OIFS(I)  (IF_EIGRP_IF_INFO (I)->eifs)
#define IF_OIFS_PARAMS(I) (IF_EIGRP_IF_INFO (I)->params)

#define SET_IF_PARAM(S, P) ((S)->P##__config) = 1
#define IF_DEF_PARAMS(I) (IF_EIGRP_IF_INFO (I)->def_params)

#define UNSET_IF_PARAM(S, P) ((S)->P##__config) = 0

#define EIGRP_IF_PARAM_CONFIGURED(S, P) ((S) && (S)->P##__config)
#define EIGRP_IF_PARAM(O, P) \
        (EIGRP_IF_PARAM_CONFIGURED ((O)->params, P)?\
                        (O)->params->P:IF_DEF_PARAMS((O)->ifp)->P)

#define EIGRP_IF_PASSIVE_STATUS(O) \
       (EIGRP_IF_PARAM_CONFIGURED((O)->params, passive_interface) ? \
         (O)->params->passive_interface : \
         (EIGRP_IF_PARAM_CONFIGURED(IF_DEF_PARAMS((O)->ifp), passive_interface) ? \
           IF_DEF_PARAMS((O)->ifp)->passive_interface : \
           (O)->eigrp->passive_interface_default))

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
#define EI_MEMBER_FLAG(M) (1 << (M))
#define EI_MEMBER_COUNT(O,M) (IF_EIGRP_IF_INFO(ei->ifp)->membership_counts[(M)])
#define EI_MEMBER_CHECK(O,M) \
    (CHECK_FLAG((O)->multicast_memberships, EI_MEMBER_FLAG(M)))
#define EI_MEMBER_JOINED(O,M) \
  do { \
    SET_FLAG ((O)->multicast_memberships, EI_MEMBER_FLAG(M)); \
    IF_EIGRP_IF_INFO((O)->ifp)->membership_counts[(M)]++; \
  } while (0)
#define EI_MEMBER_LEFT(O,M) \
  do { \
    UNSET_FLAG ((O)->multicast_memberships, EI_MEMBER_FLAG(M)); \
    IF_EIGRP_IF_INFO((O)->ifp)->membership_counts[(M)]--; \
  } while (0)


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
      struct route_table *nbrs;             /* EIGRP Neighbor List */

    /* Threads. */
    struct thread *t_hello;               /* timer */

    int on_write_q;

    /* Statistics fields. */
      u_int32_t hello_in;           /* Hello message input count. */
      u_int32_t update_in;           /* Update message input count. */
};

struct eigrp_if_params
{
  DECLARE_IF_PARAM (u_char, passive_interface);      /* EIGRP Interface is passive: no sending or receiving (no need to join multicast groups) */
  DECLARE_IF_PARAM (u_int32_t, v_hello);             /* Hello Interval */
  DECLARE_IF_PARAM (u_int16_t, v_wait);              /* Router Hold Time Interval */
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
extern struct eigrp_interface * eigrp_if_table_lookup (struct interface *,
                                                       struct prefix *);
extern struct eigrp_if_params *eigrp_lookup_if_params (struct interface *,
                                                struct in_addr);
extern int eigrp_if_up (struct eigrp_interface *);
extern void eigrp_if_stream_set (struct eigrp_interface *);
extern void eigrp_if_set_multicast(struct eigrp_interface *);
extern u_char eigrp_default_iftype(struct interface *);
extern void eigrp_if_free (struct eigrp_interface *);
extern int eigrp_if_down (struct eigrp_interface *);
extern void eigrp_if_stream_unset (struct eigrp_interface *);
extern struct eigrp_interface *eigrp_if_lookup_by_local_addr (struct eigrp *,
                                                              struct interface *,
                                                              struct in_addr);
struct eigrp_interface * eigrp_if_lookup_recv_if (struct eigrp *, struct in_addr,
                                                  struct interface *);

/* Simulate down/up on the interface. */
extern void eigrp_if_reset (struct interface *);

#endif /* ZEBRA_EIGRP_INTERFACE_H_ */
