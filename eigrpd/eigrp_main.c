/*
 * EIGRPd main routine.
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

#include <lib/version.h>
#include "getopt.h"
#include "thread.h"
#include "prefix.h"
#include "linklist.h"
#include "if.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "filter.h"
#include "plist.h"
#include "stream.h"
#include "log.h"
#include "memory.h"
#include "privs.h"
#include "sigevent.h"
#include "zclient.h"

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_neighbor.h"


/* eigprd privileges */
zebra_capabilities_t _caps_p [] = 
{
  ZCAP_NET_RAW,
  ZCAP_BIND,
  ZCAP_NET_ADMIN,
};

struct zebra_privs_t eigrpd_privs =
{
#if defined(QUAGGA_USER) && defined(QUAGGA_GROUP)
  .user = QUAGGA_USER,
  .group = QUAGGA_GROUP,
#endif
#if defined(VTY_GROUP)
  .vty_group = VTY_GROUP,
#endif
  .caps_p = _caps_p,
  .cap_num_p = array_size(_caps_p),
  .cap_num_i = 0
};

/* Master of threads. */
struct thread_master *master;

/* Process ID saved for use by init system */
const char *pid_file = PATH_EIGRPD_PID;

/* Configuration filename and directory. */
char *config_default = SYSCONFDIR EIGRP_DEFAULT_CONFIG;

/* SIGHUP handler. */
static void 
sighup (void)
{
  zlog (NULL, LOG_INFO, "SIGHUP received");
}

/* SIGINT / SIGTERM handler. */
static void
sigint (void)
{
  zlog_notice ("Terminating on signal");
  eigrp_terminate ();
}

/* SIGUSR1 handler. */
static void
sigusr1 (void)
{
  zlog_rotate (NULL);
}

struct quagga_signal_t eigrp_signals[] =
{
  {
    .signal = SIGHUP,
    .handler = &sighup,
  },
  {
    .signal = SIGUSR1,
    .handler = &sigusr1,
  },  
  {
    .signal = SIGINT,
    .handler = &sigint,
  },
  {
    .signal = SIGTERM,
    .handler = &sigint,
  },
};

/* EIGRPd main routine. */
int
main (int argc, char **argv)
{
  char *p;
  char *vty_addr = NULL;
  int vty_port = EIGRP_VTY_PORT;
  int daemon_mode = 0;
  char *config_file = NULL;
  char *progname;
  struct thread thread;
  int dryrun = 0;
  
  /* Set umask before anything for security */
  umask (0027);
  
  /* get program name */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);
  
    /* Invoked by a priviledged user? -- endo. */
  if (geteuid () != 0)
    {
      errno = EPERM;
      perror (progname);
      exit (1);
    }
  
  zlog_default = openzlog (progname, ZLOG_EIGRP,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  /* EIGRP master init. */
  eigrp_master_init ();
  
  /* Initializations. */
  master = eigrp_om->master;
  
  /* Library inits. */
  zprivs_init (&eigrpd_privs);
  signal_init (master, array_size(eigrp_signals), eigrp_signals);
  cmd_init (1);
  vty_init (master);
  memory_init ();

  /*EIGRPd init*/
  eigrp_if_init ();
  eigrp_zebra_init ();


  sort_node ();
  /* Get configuration file. */
  vty_read_config (config_file, config_default);

  /* EIGRP VTY inits */
  eigrp_vty_init ();
  eigrp_vty_show_init ();

  /* Start execution only if not in dry-run mode */
  if (dryrun)
    return(0);
  
  /* Change to the daemon program. */
  if (daemon_mode && daemon (0, 0) < 0)
    {
      zlog_err("EIGRPd daemon failed: %s", strerror(errno));
      exit (1);
    }

  /* Process id file create. */
  pid_output (pid_file);

  /* Create VTY socket */
  vty_serv_sock (vty_addr, vty_port, EIGRP_VTYSH_PATH);

//  /* Print banner. */
  zlog_notice ("EIGRPd %s starting: vty@%d", QUAGGA_VERSION, vty_port);

  /* Fetch next active thread. */
  while (thread_fetch (master, &thread))
    thread_call (&thread);

  /* Not reached. */
  return (0);
  
}
