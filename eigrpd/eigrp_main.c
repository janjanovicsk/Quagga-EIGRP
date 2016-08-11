/*
 * EIGRP Main Routine.
 * Copyright (C) 2013-2015
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 *   Frantisek Gazo
 *   Tomas Hvorkovy
 *   Martin Kontsek
 *   Lukas Koribsky
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
#include "keychain.h"

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_snmp.h"
#include "eigrpd/eigrp_filter.h"

/* eigprd privileges */
zebra_capabilities_t _caps_p [] = 
{
  ZCAP_NET_RAW,
  ZCAP_BIND,
  ZCAP_NET_ADMIN,
};

struct zebra_privs_t eigrpd_privs =
{
#if defined (QUAGGA_USER) && defined (QUAGGA_GROUP)
  .user = QUAGGA_USER,
  .group = QUAGGA_GROUP,
#endif
#if defined (VTY_GROUP)
  .vty_group = VTY_GROUP,
#endif
  .caps_p = _caps_p,
  .cap_num_p = array_size (_caps_p),
  .cap_num_i = 0
};

/* EIGRPd options. */
struct option longopts[] =
{
  { "daemon",      no_argument,       NULL, 'd'},
  { "config_file", required_argument, NULL, 'f'},
  { "pid_file",    required_argument, NULL, 'i'},
  { "socket",      required_argument, NULL, 'z'},
  { "dryrun",      no_argument,       NULL, 'C'},
  { "help",        no_argument,       NULL, 'h'},
  { "vty_addr",    required_argument, NULL, 'A'},
  { "vty_port",    required_argument, NULL, 'P'},
  { "user",        required_argument, NULL, 'u'},
  { "group",       required_argument, NULL, 'g'},
  { "version",     no_argument,       NULL, 'v'},
  { 0 }
};

/* Help information display. */
static void __attribute__ ((noreturn))
usage (char *progname, int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {
      printf ("Usage : %s [OPTION...]\n\
Daemon which manages EIGRP.\n\n\
-d, --daemon       Runs in daemon mode\n\
-f, --config_file  Set configuration file name\n\
-i, --pid_file     Set process identifier file name\n\
-z, --socket       Set path of zebra socket\n\
-A, --vty_addr     Set vty's bind address\n\
-P, --vty_port     Set vty's port number\n\
-u, --user         User to run as\n\
-g, --group        Group to run as\n\
-v, --version      Print program version\n\
-C, --dryrun       Check configuration for validity and exit\n\
-h, --help         Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }
  exit (status);
}

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
  
  while (1)
    {
      int opt;

      opt = getopt_long (argc, argv, "df:i:z:hA:P:u:g:vC", longopts, 0);

      if (opt == EOF)
        break;

      switch (opt)
        {
        case 0:
          break;
        case 'd':
          daemon_mode = 1;
          break;
        case 'f':
          config_file = optarg;
          break;
        case 'A':
          vty_addr = optarg;
          break;
        case 'i':
          pid_file = optarg;
          break;
        case 'z':
          zclient_serv_path_set (optarg);
          break;
        case 'P':
          /* Deal with atoi() returning 0 on failure, and eigrpd not
             listening on eigrpd port... */
          if (strcmp(optarg, "0") == 0)
            {
              vty_port = 0;
              break;
            }
          vty_port = atoi (optarg);
          if (vty_port <= 0 || vty_port > 0xffff)
            vty_port = EIGRP_VTY_PORT;
          break;
        case 'u':
          eigrpd_privs.user = optarg;
          break;
        case 'g':
          eigrpd_privs.group = optarg;
          break;
        case 'v':
          print_version (progname);
          exit (0);
          break;
        case 'C':
          dryrun = 1;
          break;
        case 'h':
          usage (progname, 0);
          break;
        default:
          usage (progname, 1);
          break;
        }
    }

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
  signal_init (master, array_size (eigrp_signals), eigrp_signals);
  cmd_init (1);
  vty_init (master);
  memory_init ();

  /*EIGRPd init*/
  eigrp_if_init ();
  eigrp_zebra_init ();
  eigrp_debug_init ();

  /* Get configuration file. */
  /* EIGRP VTY inits */
  eigrp_vty_init ();
  keychain_init();
  eigrp_vty_show_init ();
  eigrp_vty_if_init ();

#ifdef HAVE_SNMP
  eigrp_snmp_init ();
#endif /* HAVE_SNMP */

  /* Access list install. */
  access_list_init ();
  access_list_add_hook (eigrp_distribute_update_all_wrapper);
  access_list_delete_hook (eigrp_distribute_update_all_wrapper);

  /* Prefix list initialize.*/
  prefix_list_init ();
  prefix_list_add_hook (eigrp_distribute_update_all);
  prefix_list_delete_hook (eigrp_distribute_update_all);

  /* Distribute list install. */
  distribute_list_init (EIGRP_NODE);
  distribute_list_add_hook (eigrp_distribute_update);
  distribute_list_delete_hook (eigrp_distribute_update);

  vty_read_config (config_file, config_default);


  /* Start execution only if not in dry-run mode */
  if (dryrun)
    return (0);
  
  /* Change to the daemon program. */
  if (daemon_mode && daemon (0, 0) < 0)
    {
      zlog_err ("EIGRPd daemon failed: %s", strerror (errno));
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
