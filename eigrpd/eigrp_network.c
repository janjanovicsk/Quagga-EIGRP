/*
 * EIGRP network related functions.
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


#include <zebra.h>

#include "thread.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "sockunion.h"
#include "log.h"
#include "sockopt.h"
#include "privs.h"

extern struct zebra_privs_t eigrpd_privs;

#include "eigrpd.h"
#include "eigrp_packet.h"



int
eigrp_sock_init (void)
{
  int eigrp_sock;
  int ret, hincl = 1;

  if ( eigrpd_privs.change (ZPRIVS_RAISE) )
    zlog_err ("eigrp_sock_init: could not raise privs, %s",
               safe_strerror (errno) );

  ospf_sock = socket (AF_INET, SOCK_RAW, IPPROTO_OSPFIGP);
  if (ospf_sock < 0)
    {
      int save_errno = errno;
      if ( ospfd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("ospf_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_err ("ospf_read_sock_init: socket: %s", safe_strerror (save_errno));
      exit(1);
    }

#ifdef IP_HDRINCL
  /* we will include IP header with packet */
  ret = setsockopt (ospf_sock, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof (hincl));
  if (ret < 0)
    {
      int save_errno = errno;
      if ( ospfd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("ospf_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_warn ("Can't set IP_HDRINCL option for fd %d: %s",
                 ospf_sock, safe_strerror(save_errno));
    }
#elif defined (IPTOS_PREC_INTERNETCONTROL)
#warning "IP_HDRINCL not available on this system"
#warning "using IPTOS_PREC_INTERNETCONTROL"
  ret = setsockopt_ipv4_tos(ospf_sock, IPTOS_PREC_INTERNETCONTROL);
  if (ret < 0)
    {
      int save_errno = errno;
      if ( ospfd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("ospf_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_warn ("can't set sockopt IP_TOS %d to socket %d: %s",
                 tos, ospf_sock, safe_strerror(save_errno));
      close (ospf_sock);        /* Prevent sd leak. */
      return ret;
    }
#else /* !IPTOS_PREC_INTERNETCONTROL */
#warning "IP_HDRINCL not available, nor is IPTOS_PREC_INTERNETCONTROL"
  zlog_warn ("IP_HDRINCL option not available");
#endif /* IP_HDRINCL */

  ret = setsockopt_ifindex (AF_INET, ospf_sock, 1);

  if (ret < 0)
     zlog_warn ("Can't set pktinfo option for fd %d", ospf_sock);

  if (ospfd_privs.change (ZPRIVS_LOWER))
    {
      zlog_err ("ospf_sock_init: could not lower privs, %s",
               safe_strerror (errno) );
    }

  return ospf_sock;
}
