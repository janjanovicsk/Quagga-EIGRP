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

static struct eigrp_master eigrp_master;

struct eigrp_master *eigrp_om;

static void eigrp_finish_final (struct eigrp *);
static void eigrp_delete (struct eigrp *);

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
eigrp_new (void)
{
  int i;

  struct eigrp *new = XCALLOC (MTYPE_EIGRP_TOP, sizeof (struct eigrp));

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
eigrp_get (void)
{
  struct eigrp *eigrp;

  eigrp = eigrp_lookup ();
  if (eigrp == NULL)
    {
      eigrp = eigrp_new ();
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
