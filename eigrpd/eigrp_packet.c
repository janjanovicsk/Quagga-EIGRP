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
#include "eigrpd/eigrp_dump.h"



static void eigrp_hello_send_sub (struct eigrp_interface *, in_addr_t);
static void eigrp_make_header (int, struct eigrp_interface *, struct stream *);
static int eigrp_make_hello (struct eigrp_interface *, struct stream *);
static void eigrp_packet_add_top (struct eigrp_interface *, struct eigrp_packet *);
static void eigrp_fifo_push_head (struct eigrp_fifo *fifo, struct eigrp_packet *ep);
static int eigrp_write (struct thread *);
static void eigrp_packet_checksum (struct eigrp_interface *,
                                   struct stream *, u_int16_t);
static struct stream *eigrp_recv_packet (int, struct interface **, struct stream *);
static unsigned eigrp_packet_examin (struct eigrp_header *,
                                            const unsigned);
static int eigrp_verify_header (struct stream *, struct eigrp_interface *,
                                struct ip *, struct eigrp_header *);
static int eigrp_check_network_mask (struct eigrp_interface *, struct in_addr);


/* Minimum (besides OSPF_HEADER_SIZE) lengths for OSPF packets of
   particular types, offset is the "type" field of a packet. */
static const u_int16_t eigrp_packet_minlen[] =
{
  0,
  EIGRP_HELLO_MIN_SIZE
};

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
  int ret;
  struct stream *ibuf;
  struct eigrp *eigrp;
  struct eigrp_interface *ei;
  struct ip *iph;
  struct eigrp_header *eigrph;
  u_int16_t length;
  struct interface *ifp;

  /* first of all get interface pointer. */
  eigrp = THREAD_ARG (thread);

  /* prepare for next packet. */
  eigrp->t_read = thread_add_read (master, eigrp_read, eigrp, eigrp->fd);

  stream_reset(eigrp->ibuf);
  if (!(ibuf = eigrp_recv_packet (eigrp->fd, &ifp, eigrp->ibuf)))
    return -1;
  /* This raw packet is known to be at least as big as its IP header. */

  /* Note that there should not be alignment problems with this assignment
     because this is at the beginning of the stream data buffer. */
  iph = (struct ip *) STREAM_DATA (ibuf);
  /* Note that sockopt_iphdrincl_swab_systoh was called in ospf_recv_packet. */

  if (ifp == NULL)
    /* Handle cases where the platform does not support retrieving the ifindex,
       and also platforms (such as Solaris 8) that claim to support ifindex
       retrieval but do not. */
    ifp = if_lookup_address (iph->ip_src);

  if (ifp == NULL)
    return 0;

  /* IP Header dump. */
//    if (IS_DEBUG_OSPF_PACKET(0, RECV))
            eigrp_ip_header_dump (iph);

  /* Self-originated packet should be discarded silently. */
  if (eigrp_if_lookup_by_local_addr (eigrp, NULL, iph->ip_src))
    {
//      if (IS_DEBUG_OSPF_PACKET (0, RECV))
//        {
//          zlog_debug ("ospf_read[%s]: Dropping self-originated packet",
//                     inet_ntoa (iph->ip_src));
//        }
      return 0;
    }

  /* Advance from IP header to EIGRP header (iph->ip_hl has been verified
     by ospf_recv_packet() to be correct). */
  stream_forward_getp (ibuf, iph->ip_hl * 4);

  eigrph = (struct eigrp_header *) STREAM_PNT (ibuf);
//  if (MSG_OK != eigrp_packet_examin (eigrph, stream_get_endp (ibuf) - stream_get_getp (ibuf)))
//    return -1;
  /* Now it is safe to access all fields of OSPF packet header. */

  /* associate packet with eigrp interface */
  ei = eigrp_if_lookup_recv_if (eigrp, iph->ip_src, ifp);

  /* eigrp_verify_header() relies on a valid "ei" and thus can be called only
     after the hecks below are passed. These checks
     in turn access the fields of unverified "eigrph" structure for their own
     purposes and must remain very accurate in doing this. */

  /* If incoming interface is passive one, ignore it. */
  if (ei && EIGRP_IF_PASSIVE_STATUS (ei) == EIGRP_IF_PASSIVE)
    {
      char buf[3][INET_ADDRSTRLEN];

//      if (IS_DEBUG_OSPF_EVENT)
//        zlog_debug ("ignoring packet from router %s sent to %s, "
//                    "received on a passive interface, %s",
//                    inet_ntop(AF_INET, &ospfh->router_id, buf[0], sizeof(buf[0])),
//                    inet_ntop(AF_INET, &iph->ip_dst, buf[1], sizeof(buf[1])),
//                    inet_ntop(AF_INET, &oi->address->u.prefix4,
//                              buf[2], sizeof(buf[2])));

      if (iph->ip_dst.s_addr == htonl(EIGRP_MULTICAST_ADDRESS))
        {
          /* Try to fix multicast membership.
           * Some OS:es may have problems in this area,
           * make sure it is removed.
           */
          EI_MEMBER_JOINED(ei, MEMBER_ALLROUTERS);
          eigrp_if_set_multicast(ei);
        }
      return 0;
  }

  /* else it must be a local eigrp interface, check it was received on
   * correct link
   */
  else if (ei->ifp != ifp)
    {
//      if (IS_DEBUG_OSPF_EVENT)
//        zlog_warn ("Packet from [%s] received on wrong link %s",
//                   inet_ntoa (iph->ip_src), ifp->name);
      return 0;
    }

  /* Verify more EIGRP header fields. */
  ret = eigrp_verify_header (ibuf, ei, iph, eigrph);
  if (ret < 0)
  {
//    if (IS_DEBUG_OSPF_PACKET (0, RECV))
//      zlog_debug ("ospf_read[%s]: Header check failed, "
//                  "dropping.",
//                  inet_ntoa (iph->ip_src));
    return ret;
  }

//      zlog_debug ("received from [%s] via [%s]",
//                 inet_ntoa (iph->ip_src), IF_NAME (ei));
//      zlog_debug (" src [%s],", inet_ntoa (iph->ip_src));
//      zlog_debug (" dst [%s]", inet_ntoa (iph->ip_dst));
//
//        zlog_debug ("-----------------------------------------------------");


  stream_forward_getp (ibuf, EIGRP_HEADER_SIZE);

  /* Read rest of the packet and call each sort of packet routine. */
  switch (eigrph->opcode)
    {
    case EIGRP_MSG_HELLO:
//      ospf_hello (iph, ospfh, ibuf, oi, length);
      break;
    case EIGRP_MSG_PROBE:
//      ospf_db_desc (iph, ospfh, ibuf, oi, length);
      break;
    case EIGRP_MSG_QUERY:
//      ospf_ls_req (iph, ospfh, ibuf, oi, length);
      break;
    case EIGRP_MSG_REPLY:
//      ospf_ls_upd (iph, ospfh, ibuf, oi, length);
      break;
    case EIGRP_MSG_REQUEST:
//      ospf_ls_ack (iph, ospfh, ibuf, oi, length);
      break;
    case EIGRP_MSG_SIAQUERY:
//          ospf_ls_ack (iph, ospfh, ibuf, oi, length);
          break;
    case EIGRP_MSG_SIAREPLY:
//          ospf_ls_ack (iph, ospfh, ibuf, oi, length);
      break;
    case EIGRP_MSG_UPDATE:
//          ospf_ls_ack (iph, ospfh, ibuf, oi, length);
      break;
    default:
      zlog (NULL, LOG_WARNING,
            "interface %s: EIGRP packet header type %d is illegal",
            IF_NAME (ei), eigrph->opcode);
      break;
    }

  return 0;
}

static struct stream *
eigrp_recv_packet (int fd, struct interface **ifp, struct stream *ibuf)
{
  int ret;
  struct ip *iph;
  u_int16_t ip_len;
  unsigned int ifindex = 0;
  struct iovec iov;
  /* Header and data both require alignment. */
  char buff [CMSG_SPACE(SOPT_SIZE_CMSG_IFINDEX_IPV4())];
  struct msghdr msgh;

  memset (&msgh, 0, sizeof (struct msghdr));
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;
  msgh.msg_control = (caddr_t) buff;
  msgh.msg_controllen = sizeof (buff);

  ret = stream_recvmsg (ibuf, fd, &msgh, 0, EIGRP_MAX_PACKET_SIZE+1);
  if (ret < 0)
    {
      zlog_warn("stream_recvmsg failed: %s", safe_strerror(errno));
      return NULL;
    }
  if ((unsigned int)ret < sizeof(iph)) /* ret must be > 0 now */
    {
      zlog_warn("ospf_recv_packet: discarding runt packet of length %d "
                "(ip header size is %u)",
                ret, (u_int)sizeof(iph));
      return NULL;
    }

  /* Note that there should not be alignment problems with this assignment
     because this is at the beginning of the stream data buffer. */
  iph = (struct ip *) STREAM_DATA(ibuf);
  sockopt_iphdrincl_swab_systoh (iph);

  ip_len = iph->ip_len;

#if !defined(GNU_LINUX) && (OpenBSD < 200311) && (__FreeBSD_version < 1000000)
  /*
   * Kernel network code touches incoming IP header parameters,
   * before protocol specific processing.
   *
   *   1) Convert byteorder to host representation.
   *      --> ip_len, ip_id, ip_off
   *
   *   2) Adjust ip_len to strip IP header size!
   *      --> If user process receives entire IP packet via RAW
   *          socket, it must consider adding IP header size to
   *          the "ip_len" field of "ip" structure.
   *
   * For more details, see <netinet/ip_input.c>.
   */
  ip_len = ip_len + (iph->ip_hl << 2);
#endif

#if defined(__DragonFly__)
  /*
   * in DragonFly's raw socket, ip_len/ip_off are read
   * in network byte order.
   * As OpenBSD < 200311 adjust ip_len to strip IP header size!
   */
  ip_len = ntohs(iph->ip_len) + (iph->ip_hl << 2);
#endif

  ifindex = getsockopt_ifindex (AF_INET, &msgh);

  *ifp = if_lookup_by_index (ifindex);

  if (ret != ip_len)
    {
      zlog_warn ("eigrp_recv_packet read length mismatch: ip_len is %d, "
                 "but recvmsg returned %d", ip_len, ret);
      return NULL;
    }

  return ibuf;
}

/* Verify a complete OSPF packet for proper sizing/alignment. */
static unsigned
eigrp_packet_examin (struct eigrp_header * eh, const unsigned bytesonwire)
{
  u_int16_t bytesdeclared, bytesauth;
  unsigned ret;

  /* Length, 1st approximation. */
  if (bytesonwire < EIGRP_HEADER_SIZE)
  {
//    if (IS_DEBUG_OSPF_PACKET (0, RECV))
//      zlog_debug ("%s: undersized (%u B) packet", __func__, bytesonwire);
    return MSG_NG;
  }
  /* Now it is safe to access header fields. Performing length check, allow
   * for possible extra bytes of crypto auth/padding, which are not counted
   * in the OSPF header "length" field. */
  if (eh->version != EIGRP_HEADER_VERSION)
  {
//    if (IS_DEBUG_OSPF_PACKET (0, RECV))
//      zlog_debug ("%s: invalid (%u) protocol version", __func__, oh->version);
    return MSG_NG;
  }

  /* Length, 2nd approximation. The type-specific constraint is checked
     against declared length, not amount of bytes on wire. */


  switch (eh->opcode)
  {
  case EIGRP_MSG_HELLO:
    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
       by N>=0 router-IDs. */
    break;
  case EIGRP_MSG_PROBE:
    /* RFC2328 A.3.3, packet header + OSPF_DB_DESC_MIN_SIZE bytes followed
       by N>=0 header-only LSAs. */

    break;
  case EIGRP_MSG_QUERY:
    /* RFC2328 A.3.4, packet header followed by N>=0 12-bytes request blocks. */
    break;
  case EIGRP_MSG_REPLY:
    /* RFC2328 A.3.5, packet header + OSPF_LS_UPD_MIN_SIZE bytes followed
       by N>=0 full LSAs (with N declared beforehand). */
    break;
  case EIGRP_MSG_REQUEST:
    /* RFC2328 A.3.6, packet header followed by N>=0 header-only LSAs. */
    break;
  case EIGRP_MSG_SIAQUERY:
    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
       by N>=0 router-IDs. */

    break;
  case EIGRP_MSG_SIAREPLY:
    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
       by N>=0 router-IDs. */

    break;
  case EIGRP_MSG_UPDATE:
    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
       by N>=0 router-IDs. */

    break;
  default:
//    if (IS_DEBUG_OSPF_PACKET (0, RECV))
//      zlog_debug ("%s: invalid packet type 0x%02x", __func__, eh->opcode);
    return MSG_NG;
  }
//  if (ret != MSG_OK && IS_DEBUG_OSPF_PACKET (0, RECV))
//    zlog_debug ("%s: malformed %s packet", __func__, LOOKUP (ospf_packet_type_str, eh->opcode));
  return MSG_OK;
}

struct eigrp_fifo *
eigrp_fifo_new (void)
{
  struct eigrp_fifo *new;

  new = XCALLOC (MTYPE_EIGRP_FIFO, sizeof (struct eigrp_fifo));
  return new;
}

/* Free eigrp packet fifo. */
void
eigrp_fifo_free (struct eigrp_fifo *fifo)
{
    struct eigrp_packet *ep;
    struct eigrp_packet *next;

    for (ep = fifo->head; ep; ep = next)
      {
        next = ep->next;
        eigrp_packet_free (ep);
      }
    fifo->head = fifo->tail = NULL;
    fifo->count = 0;

  XFREE (MTYPE_EIGRP_FIFO, fifo);
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

  /* EIGRP Checksum */
    eigrp_packet_checksum (ei, ep->s, length);

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

/*Send EIGRP Update packet*/
void
eigrp_update_send (struct eigrp_interface*ei)
{

}

/* Calculate EIGRP checksum */
static void
eigrp_packet_checksum (struct eigrp_interface *ei,
                  struct stream *s, u_int16_t length)
{
  struct eigrp_header *eigrph;

  eigrph = (struct eigrp_header *) STREAM_DATA (s);

  /* Calculate checksum. */
    eigrph->checksum = in_cksum (eigrph, length);

}

/* Make EIGRP header. */
static void
eigrp_make_header (int type, struct eigrp_interface *ei, struct stream *s)
{
  struct eigrp_header *eigrph;

  eigrph = (struct eigrp_header *) STREAM_DATA (s);

  eigrph->version = (u_char) EIGRP_HEADER_VERSION;
  eigrph->opcode = (u_char) type;

  eigrph->routerID = 0;

  eigrph->checksum = 0;

  eigrph->ASNumber = htons(ei->eigrp->AS);
  eigrph->ack = 0;
  eigrph->sequence = htonl(ei->eigrp->sequence_number);
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

/* EIGRP Header verification. */
static int
eigrp_verify_header (struct stream *ibuf, struct eigrp_interface *ei,
                    struct ip *iph, struct eigrp_header *eigrph)
{

  /* Check network mask, Silently discarded. */
  if (! eigrp_check_network_mask (ei, iph->ip_src))
    {
      zlog_warn ("interface %s: eigrp_read network address is not same [%s]",
                 IF_NAME (ei), inet_ntoa (iph->ip_src));
      return -1;
    }
//
//  /* Check authentication. The function handles logging actions, where required. */
//  if (! ospf_check_auth (oi, ospfh))
//    return -1;

  return 0;
}

/* Unbound socket will accept any Raw IP packets if proto is matched.
   To prevent it, compare src IP address and i/f address with masking
   i/f network mask. */
static int
eigrp_check_network_mask (struct eigrp_interface *ei, struct in_addr ip_src)
{
  struct in_addr mask, me, him;

  if (ei->type == EIGRP_IFTYPE_POINTOPOINT)
    return 1;

  masklen2ip (ei->address->prefixlen, &mask);

  me.s_addr = ei->address->u.prefix4.s_addr & mask.s_addr;
  him.s_addr = ip_src.s_addr & mask.s_addr;

 if (IPV4_ADDR_SAME (&me, &him))
   return 1;

 return 0;
}

