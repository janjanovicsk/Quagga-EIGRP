/*
 * EIGRPd Daemon program.
 * Copyright (C) 1998, 99, 2000 Kunihiro Ishiguro, Toshiaki Takada
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

#include "thread.h"
#include "vty.h"
#include "command.h"
#include "linklist.h"
#include "prefix.h"
#include "table.h"
#include "if.h"
#include "memory.h"
#include "stream.h"
#include "log.h"
#include "sockunion.h"          /* for inet_aton () */
#include "zclient.h"
#include "plist.h"
#include "sockopt.h"

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_network.h"


static struct eigrp_master eigrp_master;

struct eigrp_master *eigrp_om;

static void eigrp_finish_final (struct eigrp *);
static void eigrp_delete (struct eigrp *);
static struct eigrp *eigrp_new (const char *);
static void eigrp_add (struct eigrp *);

extern struct zclient *zclient;
extern struct in_addr router_id_zebra;


void
eigrp_router_id_update (struct eigrp *eigrp)
{
  struct in_addr router_id, router_id_old;
  struct interface *ifp;
  struct listnode *node;

  router_id_old = eigrp->router_id;

  /* Select the router ID based on these priorities:
       1. Statically assigned router ID is always the first choice.
       2. If there is no statically assigned router ID, then try to stick
          with the most recent value, since changing router ID's is very
          disruptive.
       3. Last choice: just go with whatever the zebra daemon recommends.
  */
  if (eigrp->router_id_static.s_addr != 0)
    router_id = eigrp->router_id_static;
  else if (eigrp->router_id.s_addr != 0)
    router_id = eigrp->router_id;
  else
    router_id = router_id_zebra;

  eigrp->router_id = router_id;


  if (!IPV4_ADDR_SAME (&router_id_old, &router_id))
    {
      /* update eigrp_interface's */
      for (ALL_LIST_ELEMENTS_RO (eigrp_om->iflist, node, ifp))
        eigrp_if_update (eigrp, ifp);
    }
}

void
eigrp_master_init ()
{
  memset (&eigrp_master, 0, sizeof (struct eigrp_master));

  eigrp_om = &eigrp_master;
  eigrp_om->eigrp = list_new ();
  eigrp_om->master = thread_master_create ();
  eigrp_om->start_time = quagga_time (NULL);
}


/* Allocate new eigrp structure. */
static struct eigrp *
eigrp_new (const char *AS)
{

  struct eigrp *new = XCALLOC (MTYPE_EIGRP_TOP, sizeof (struct eigrp));

  new->eiflist = list_new ();
  new->passive_interface_default = EIGRP_IF_ACTIVE;
  new->AS = atoi(AS);

  new->networks = route_table_init();

  new->router_id.s_addr = htonl (0);
  new->router_id_static.s_addr = htonl (0);

  if ((new->fd = eigrp_sock_init()) < 0)
    {
      zlog_err("eigrp_new: fatal error: eigrp_sock_init was unable to open "
               "a socket");
      exit(1);
    }

  new->maxsndbuflen = getsockopt_so_sendbuf (new->fd);

  if ((new->ibuf = stream_new(EIGRP_MAX_PACKET_SIZE+1)) == NULL)
      {
        zlog_err("eigrp_new: fatal error: stream_new(%u) failed allocating ibuf",
                 EIGRP_MAX_PACKET_SIZE+1);
        exit(1);
      }

  new->t_read = thread_add_read (master, eigrp_read, new, new->fd);
  new->oi_write_q = list_new ();

  new->sequence_number=0;

  return new;
}

static void
eigrp_add (struct eigrp *eigrp)
{
  listnode_add (eigrp_om->eigrp, eigrp);
}

static void
eigrp_delete (struct eigrp *eigrp)
{
  listnode_delete (eigrp_om->eigrp, eigrp);
}

struct eigrp *
eigrp_get (const char *AS)
{
  struct eigrp *eigrp;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      eigrp = eigrp_new (AS);
      eigrp_add (eigrp);
    }
  return eigrp;
}

/* Shut down the entire process */
void
eigrp_terminate (void)
{
  struct eigrp *eigrp;
  struct listnode *node, *nnode;

  /* shutdown already in progress */
    if (CHECK_FLAG (eigrp_om->options, EIGRP_MASTER_SHUTDOWN))
      return;

    SET_FLAG(eigrp_om->options, EIGRP_MASTER_SHUTDOWN);

  /* exit immediately if EIGRP not actually running */
  if (listcount(eigrp_om->eigrp) == 0)
      exit(0);

  for (ALL_LIST_ELEMENTS (eigrp_om->eigrp, node, nnode, eigrp))
      eigrp_finish (eigrp);
}

void
eigrp_finish (struct eigrp *eigrp)
{

  eigrp_finish_final (eigrp);

  /* eigrp being shut-down? If so, was this the last eigrp instance? */
    if (CHECK_FLAG (eigrp_om->options, EIGRP_MASTER_SHUTDOWN)
        && (listcount (eigrp_om->eigrp) == 0))
      exit (0);
  return;

}

/* Final cleanup of eigrp instance */
static void
eigrp_finish_final (struct eigrp *eigrp)
{

  close (eigrp->fd);


  if(zclient)
    zclient_free(zclient);

  list_free (eigrp->eiflist);
  list_free (eigrp->oi_write_q);
  eigrp_delete (eigrp);

  XFREE (MTYPE_EIGRP_TOP,eigrp);

}

/*Look for existing eigrp process*/
struct eigrp *
eigrp_lookup (void)
{
  if (listcount (eigrp_om->eigrp) == 0)
    return NULL;

  return listgetdata (listhead (eigrp_om->eigrp));
}
