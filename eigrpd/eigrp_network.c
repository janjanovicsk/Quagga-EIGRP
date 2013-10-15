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

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_network.h"

int
eigrp_sock_init (void)
{
  int eigrp_sock;
  int ret, hincl = 1;

  if ( eigrpd_privs.change (ZPRIVS_RAISE) )
    zlog_err ("eigrp_sock_init: could not raise privs, %s",
               safe_strerror (errno) );

  eigrp_sock = socket (AF_INET, SOCK_RAW, IPPROTO_EIGRPIGP);
  if (eigrp_sock < 0)
    {
      int save_errno = errno;
      if ( eigrpd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("eigrp_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_err ("eigrp_read_sock_init: socket: %s", safe_strerror (save_errno));
      exit(1);
    }

#ifdef IP_HDRINCL
  /* we will include IP header with packet */
  ret = setsockopt (eigrp_sock, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof (hincl));
  if (ret < 0)
    {
      int save_errno = errno;
      if ( eigrpd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("eigrp_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_warn ("Can't set IP_HDRINCL option for fd %d: %s",
                 eigrp_sock, safe_strerror(save_errno));

    }
#elif defined (IPTOS_PREC_INTERNETCONTROL)
#warning "IP_HDRINCL not available on this system"
#warning "using IPTOS_PREC_INTERNETCONTROL"
  ret = setsockopt_ipv4_tos(eigrp_sock, IPTOS_PREC_INTERNETCONTROL);
  if (ret < 0)
    {
      int save_errno = errno;
      if ( eigrpd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("eigrpd_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_warn ("can't set sockopt IP_TOS %d to socket %d: %s",
                 tos, eigrp_sock, safe_strerror(save_errno));
      close (eigrp_sock);        /* Prevent sd leak. */
      return ret;
    }
#else /* !IPTOS_PREC_INTERNETCONTROL */
#warning "IP_HDRINCL not available, nor is IPTOS_PREC_INTERNETCONTROL"
  zlog_warn ("IP_HDRINCL option not available");
#endif /* IP_HDRINCL */

  ret = setsockopt_ifindex (AF_INET, eigrp_sock, 1);

  if (ret < 0)
     zlog_warn ("Can't set pktinfo option for fd %d", eigrp_sock);

  if (eigrpd_privs.change (ZPRIVS_LOWER))
    {
      zlog_err ("eigrp_sock_init: could not lower privs, %s",
               safe_strerror (errno) );
    }

  return eigrp_sock;
}

void
eigrp_adjust_sndbuflen (struct eigrp * eigrp, unsigned int buflen)
{
  int ret, newbuflen;
  /* Check if any work has to be done at all. */
  if (eigrp->maxsndbuflen >= buflen)
    return;
  if (eigrpd_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs, %s", __func__,
      safe_strerror (errno));
  /* Now we try to set SO_SNDBUF to what our caller has requested
   * (the MTU of a newly added interface). However, if the OS has
   * truncated the actual buffer size to somewhat less size, try
   * to detect it and update our records appropriately. The OS
   * may allocate more buffer space, than requested, this isn't
   * a error.
   */
  ret = setsockopt_so_sendbuf (eigrp->fd, buflen);
  newbuflen = getsockopt_so_sendbuf (eigrp->fd);
  if (ret < 0 || newbuflen < 0 || newbuflen < (int) buflen)
    zlog_warn ("%s: tried to set SO_SNDBUF to %u, but got %d",
      __func__, buflen, newbuflen);
  if (newbuflen >= 0)
    eigrp->maxsndbuflen = (unsigned int)newbuflen;
  else
    zlog_warn ("%s: failed to get SO_SNDBUF", __func__);
  if (eigrpd_privs.change (ZPRIVS_LOWER))
    zlog_err ("%s: could not lower privs, %s", __func__,
      safe_strerror (errno));
}
