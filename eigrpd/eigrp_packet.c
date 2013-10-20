/*
 * EIGRP Sending and Receiving EIGRP Packets.
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */


#include <zebra.h>

#include "thread.h"
#include "memory.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "sockunion.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"
#include "checksum.h"
#include "md5.h"

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_network.h"



static void eigrp_hello_send_sub (struct eigrp_interface *, in_addr_t);
static void eigrp_make_header (int, struct eigrp_interface *, struct stream *);
static int eigrp_make_hello (struct eigrp_interface *, struct stream *);
static void eigrp_packet_add_top (struct eigrp_interface *, struct eigrp_packet *);
static void eigrp_fifo_push_head (struct eigrp_fifo *fifo, struct eigrp_packet *ep);
static int eigrp_write (struct thread *);

static int
eigrp_write (struct thread *thread)
{
  struct eigrp *eigrp = THREAD_ARG (thread);
  struct eigrp_interface *ei;
  struct eigrp_packet *ep;
  struct sockaddr_in sa_dst;
  struct ip iph;
  struct msghdr msg;
  struct iovec iov[2];
  u_char type;
  int ret;
  int flags = 0;
  struct listnode *node;
#ifdef WANT_OSPF_WRITE_FRAGMENT
  static u_int16_t ipid = 0;
#endif /* WANT_OSPF_WRITE_FRAGMENT */
  u_int16_t maxdatasize;
#define EIGRP_WRITE_IPHL_SHIFT 2

  eigrp->t_write = NULL;

  node = listhead (eigrp->oi_write_q);
  assert (node);
  ei = listgetdata (node);
  assert (ei);

#ifdef WANT_OSPF_WRITE_FRAGMENT
  /* seed ipid static with low order bits of time */
  if (ipid == 0)
    ipid = (time(NULL) & 0xffff);
#endif /* WANT_OSPF_WRITE_FRAGMENT */

  /* convenience - max EIGRP data per packet,
   * and reliability - not more data, than our
   * socket can accept
   */
  maxdatasize = MIN (ei->ifp->mtu, eigrp->maxsndbuflen) -
    sizeof (struct ip);

  /* Get one packet from queue. */
  ep = eigrp_fifo_head (ei->obuf);
  assert (ep);
  assert (ep->length >= EIGRP_HEADER_SIZE);

  if (ep->dst.s_addr == htonl (EIGRP_MULTICAST_ADDRESS))
      eigrp_if_ipmulticast (eigrp, ei->address, ei->ifp->ifindex);

  memset (&iph, 0, sizeof (struct ip));
  memset (&sa_dst, 0, sizeof (sa_dst));

  sa_dst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sa_dst.sin_len = sizeof(sa_dst);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
  sa_dst.sin_addr = ep->dst;
  sa_dst.sin_port = htons (0);

  /* Set DONTROUTE flag if dst is unicast. */
    if (!IN_MULTICAST (htonl (ep->dst.s_addr)))
      flags = MSG_DONTROUTE;

  iph.ip_hl = sizeof (struct ip) >> EIGRP_WRITE_IPHL_SHIFT;
  /* it'd be very strange for header to not be 4byte-word aligned but.. */
  if ( sizeof (struct ip)
        > (unsigned int)(iph.ip_hl << EIGRP_WRITE_IPHL_SHIFT) )
    iph.ip_hl++; /* we presume sizeof struct ip cant overflow ip_hl.. */

  iph.ip_v = IPVERSION;
  iph.ip_tos = IPTOS_PREC_INTERNETCONTROL;
  iph.ip_len = (iph.ip_hl << EIGRP_WRITE_IPHL_SHIFT) + ep->length;

#if defined(__DragonFly__)
  /*
   * DragonFly's raw socket expects ip_len/ip_off in network byte order.
   */
  iph.ip_len = htons(iph.ip_len);
#endif

#ifdef WANT_OSPF_WRITE_FRAGMENT
  /* XXX-MT: not thread-safe at all..
   * XXX: this presumes this is only programme sending OSPF packets
   * otherwise, no guarantee ipid will be unique
   */
  iph.ip_id = ++ipid;
#endif /* WANT_OSPF_WRITE_FRAGMENT */

  iph.ip_off = 0;

  iph.ip_ttl = EIGRP_IP_TTL;
  iph.ip_p = IPPROTO_EIGRPIGP;
  iph.ip_sum = 0;
  iph.ip_src.s_addr = ei->address->u.prefix4.s_addr;
  iph.ip_dst.s_addr = ep->dst.s_addr;

  memset (&msg, 0, sizeof (msg));
  msg.msg_name = (caddr_t) &sa_dst;
  msg.msg_namelen = sizeof (sa_dst);
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  iov[0].iov_base = (char*)&iph;
  iov[0].iov_len = iph.ip_hl << EIGRP_WRITE_IPHL_SHIFT;
  iov[1].iov_base = STREAM_PNT (ep->s);
  iov[1].iov_len = ep->length;

  /* Sadly we can not rely on kernels to fragment packets because of either
   * IP_HDRINCL and/or multicast destination being set.
   */
//#ifdef WANT_OSPF_WRITE_FRAGMENT
//  if ( ep->length > maxdatasize )
//    ospf_write_frags (ospf->fd, op, &iph, &msg, maxdatasize,
//                      oi->ifp->mtu, flags, type);
//#endif /* WANT_OSPF_WRITE_FRAGMENT */

  /* send final fragment (could be first) */
  sockopt_iphdrincl_swab_htosys (&iph);
  ret = sendmsg (eigrp->fd, &msg, flags);
  sockopt_iphdrincl_swab_systoh (&iph);

  if (ret < 0)
    zlog_warn ("*** sendmsg in eigrp_write failed to %s, "
               "id %d, off %d, len %d, interface %s, mtu %u: %s",
               inet_ntoa (iph.ip_dst), iph.ip_id, iph.ip_off, iph.ip_len,
               ei->ifp->name, ei->ifp->mtu, safe_strerror (errno));

//  /* Show debug sending packet. */
//  if (IS_DEBUG_OSPF_PACKET (type - 1, SEND))
//    {
//      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
//        {
//          zlog_debug ("-----------------------------------------------------");
//          ospf_ip_header_dump (&iph);
//          stream_set_getp (op->s, 0);
//          ospf_packet_dump (op->s);
//        }
//
//      zlog_debug ("%s sent to [%s] via [%s].",
//                 LOOKUP (ospf_packet_type_str, type), inet_ntoa (op->dst),
//                 IF_NAME (oi));
//
//      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
//        zlog_debug ("-----------------------------------------------------");
//    }

  /* Now delete packet from queue. */
  eigrp_packet_delete (ei);

  if (eigrp_fifo_head (ei->obuf) == NULL)
    {
      ei->on_write_q = 0;
      list_delete_node (eigrp->oi_write_q, node);
    }

  /* If packets still remain in queue, call write thread. */
  if (!list_isempty (eigrp->oi_write_q))
    eigrp->t_write =
      thread_add_write (master, eigrp_write, eigrp, eigrp->fd);

  return 0;
}

/* Starting point of packet process function. */
int
eigrp_read (struct thread *thread)
{

  return 0;
}

struct eigrp_fifo *
eigrp_fifo_new (void)
{
  struct eigrp_fifo *new;

  new = XCALLOC (MTYPE_EIGRP_FIFO, sizeof (struct eigrp_fifo));
  return new;
}

struct eigrp_packet *
eigrp_packet_new (size_t size)
{
  struct eigrp_packet *new;

  new = XCALLOC (MTYPE_EIGRP_PACKET, sizeof (struct eigrp_packet));
  new->s = stream_new (size);

  return new;
}

/* Send EIGRP Hello. */
void
eigrp_hello_send (struct eigrp_interface *ei)
{
//  /* If this is passive interface, do not send OSPF Hello. */
//  if (OSPF_IF_PASSIVE_STATUS (ei) == OSPF_IF_PASSIVE)
//    return;

  if (ei->type == EIGRP_IFTYPE_NBMA)
    {

    }
  else
    {
      eigrp_hello_send_sub (ei, htonl (EIGRP_MULTICAST_ADDRESS));
    }
}

static void
eigrp_hello_send_sub (struct eigrp_interface *ei, in_addr_t addr)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_SIZE;

  ep = eigrp_packet_new (ei->ifp->mtu);

  /* Prepare EIGRP common header. */
  eigrp_make_header (EIGRP_MSG_HELLO, ei, ep->s);

  /* Prepare EIGRP Hello body. */
  length += eigrp_make_hello (ei, ep->s);

  /* Set packet length. */
  ep->length = length;

  ep->dst.s_addr = addr;

  /* Add packet to the top of the interface output queue, so that they
   * can't get delayed by things like long queues of LS Update packets
   */
  eigrp_packet_add_top (ei, ep);

  /* Hook thread to write packet. */
    if (ei->on_write_q == 0)
      {
        listnode_add (ei->eigrp->oi_write_q, ei);
        ei->on_write_q = 1;
      }
    if (ei->eigrp->t_write == NULL)
      ei->eigrp->t_write = thread_add_write (master, eigrp_write, ei->eigrp, ei->eigrp->fd);
}

/* Make EIGRP header. */
static void
eigrp_make_header (int type, struct eigrp_interface *ei, struct stream *s)
{
  struct eigrp_header *eigrph;

  eigrph = (struct eigrp_header *) STREAM_DATA (s);

  eigrph->version = (u_char) EIGRP_HEADER_VERSION;
  eigrph->opcode = (u_char) type;

  eigrph->routerID = ei->eigrp->router_id.s_addr;

  eigrph->checksum = 0;

  eigrph->ASNumber = 1;
  eigrph->ack = 0;
  eigrph->sequence =0;
  eigrph->flags = 0;

  stream_forward_endp (s, EIGRP_HEADER_SIZE);
}

/*Make EIGRP Hello body*/
static int
eigrp_make_hello (struct eigrp_interface *ei, struct stream *s)
{

  u_int16_t length = EIGRP_HELLO_MIN_SIZE;

  stream_putw(s,TLV_PARAMETER_TYPE);
  stream_putw(s,EIGRP_HELLO_MIN_SIZE);
  stream_putc(s,(u_char)1); /* K1 */
  stream_putc(s,(u_char)0); /* K2 */
  stream_putc(s,(u_char)1); /* K3 */
  stream_putc(s,(u_char)0); /* K4 */
  stream_putc(s,(u_char)0); /* K5 */
  stream_putc(s,(u_char)0); /* K6 */
  stream_putw(s,15);

  return length;
}

static void
eigrp_packet_add_top (struct eigrp_interface *ei, struct eigrp_packet *ep)
{
  if (!ei->obuf)
    {
      zlog_err("eigrp_packet_add_top ERROR");
      return;
    }

  /* Add packet to head of queue. */
  eigrp_fifo_push_head (ei->obuf, ep);

  /* Debug of packet fifo*/
  /* ospf_fifo_debug (oi->obuf); */
}

/* Add new packet to head of fifo. */
static void
eigrp_fifo_push_head (struct eigrp_fifo *fifo, struct eigrp_packet *ep)
{
  ep->next = fifo->head;

  if (fifo->tail == NULL)
    fifo->tail = ep;

  fifo->head = ep;

  fifo->count++;
}

/* Return first fifo entry. */
struct eigrp_packet *
eigrp_fifo_head (struct eigrp_fifo *fifo)
{
  return fifo->head;
}

void
eigrp_packet_delete (struct eigrp_interface *ei)
{
  struct eigrp_packet *ep;

  ep = eigrp_fifo_pop (ei->obuf);

  if (ep)
    eigrp_packet_free (ep);
}

/* Delete first packet from fifo. */
struct eigrp_packet *
eigrp_fifo_pop (struct eigrp_fifo *fifo)
{
  struct eigrp_packet *ep;

  ep = fifo->head;

  if (ep)
    {
      fifo->head = ep->next;

      if (fifo->head == NULL)
        fifo->tail = NULL;

      fifo->count--;
    }

  return ep;
}

void
eigrp_packet_free (struct eigrp_packet *ep)
{
  if (ep->s)
    stream_free (ep->s);

  XFREE (MTYPE_EIGRP_PACKET, ep);

  ep = NULL;
}

