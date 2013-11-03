/*
 * EIGRPd dump routine.
 * Copyright (C) 1999, 2000 Toshiaki Takada
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <zebra.h>

#include "linklist.h"
#include "thread.h"
#include "prefix.h"
#include "command.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_dump.h"

/* Expects header to be in host order */
void
eigrp_ip_header_dump (struct ip *iph)
{
  /* IP Header dump. */
  zlog_debug ("ip_v %d", iph->ip_v);
  zlog_debug ("ip_hl %d", iph->ip_hl);
  zlog_debug ("ip_tos %d", iph->ip_tos);
  zlog_debug ("ip_len %d", iph->ip_len);
  zlog_debug ("ip_id %u", (u_int32_t) iph->ip_id);
  zlog_debug ("ip_off %u", (u_int32_t) iph->ip_off);
  zlog_debug ("ip_ttl %d", iph->ip_ttl);
  zlog_debug ("ip_p %d", iph->ip_p);
  zlog_debug ("ip_sum 0x%x", (u_int32_t) iph->ip_sum);
  zlog_debug ("ip_src %s",  inet_ntoa (iph->ip_src));
  zlog_debug ("ip_dst %s", inet_ntoa (iph->ip_dst));
}

const char *
eigrp_if_name_string (struct eigrp_interface *ei)
{
  static char buf[EIGRP_IF_STRING_MAXLEN] = "";
  u_int32_t ifaddr;

  if (!ei)
    return "inactive";

  ifaddr = ntohl (ei->address->u.prefix4.s_addr);
  snprintf (buf, EIGRP_IF_STRING_MAXLEN,
            "%s:%d.%d.%d.%d", ei->ifp->name,
            (ifaddr >> 24) & 0xff, (ifaddr >> 16) & 0xff,
            (ifaddr >> 8) & 0xff, ifaddr & 0xff);
  return buf;
}

