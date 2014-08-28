/*
 * EIGRP Sending and Receiving EIGRP Hello Packets.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
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

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_macros.h"

/* Packet Type String. */
static const struct message eigrp_general_tlv_type_str[] =
{
  { EIGRP_TLV_PARAMETER,	"PARAMETER"		},
  { EIGRP_TLV_AUTH,		"AUTH"			},
  { EIGRP_TLV_SEQ,		"SEQ"			},
  { EIGRP_TLV_SW_VERSION,	"SW_VERSION"		},
  { EIGRP_TLV_NEXT_MCAST_SEQ,	"NEXT_MCAST_SEQ"	},
  { EIGRP_TLV_PEER_TERMINATION,	"PEER_TERMINATION"	},
  { EIGRP_TLV_PEER_MTRLIST,	"PEER_MTRLIST"		},
  { EIGRP_TLV_PEER_TIDLIST,	"PEER_TIDLIST"		},
};

static const size_t eigrp_general_tlv_type_str_max = sizeof(eigrp_general_tlv_type_str) /
  sizeof(eigrp_general_tlv_type_str[0]);


/*
 * @fn eigrp_hello_timer
 *
 * @param[in]	thread	current execution thread timer is associated with
 * 
 * @return int	always returns 0
 *
 * @par
 * Called once per "hello" time interval, default 5 seconds
 * Sends hello packet via multicast for all interfaces eigrp
 * is configured for
 */
int
eigrp_hello_timer (struct thread *thread)
{
  struct eigrp_interface *ei;

  ei = THREAD_ARG(thread);
  ei->t_hello = NULL;

  if (IS_DEBUG_EIGRP(0, TIMERS))
    zlog (NULL, LOG_DEBUG, "Start Hello Timer (%s) Expire [%u]",
	  IF_NAME(ei), EIGRP_IF_PARAM(ei, v_hello));

  /* Sending hello packet. */
  eigrp_hello_send(ei);

  /* Hello timer set. */
  ei->t_hello = thread_add_timer(master, eigrp_hello_timer, ei,
				 EIGRP_IF_PARAM(ei, v_hello));

  return 0;
}

/**
 * @fn eigrp_hello_parameter_decode
 *
 * @param[in]		nbr	neighbor the ACK should be sent to
 * @param[in]		param	pointer packet TLV is stored to
 *
 * @return u_int16_t	number of bytes added to packet stream
 *
 * @par
 * Encode Parameter TLV, used to convey metric weights and the hold time.
 *
 * @usage
 * Note the addition of K6 for the new extended metrics, and does not apply to
 * older TLV packet formats.
 */
static void
eigrp_hello_parameter_decode (struct eigrp_neighbor *nbr,
			       struct eigrp_tlv_hdr_type *tlv)			      
{
  struct eigrp *eigrp = nbr->ei->eigrp;
  struct TLV_Parameter_Type *param = (struct TLV_Parameter_Type *)tlv;

  /* copy over the values passed in by the neighbor */
  nbr->K1 = param->K1;
  nbr->K2 = param->K2;
  nbr->K3 = param->K3;
  nbr->K4 = param->K4;
  nbr->K5 = param->K5;
  nbr->K6 = param->K6;
  nbr->v_holddown = ntohs(param->hold_time);

  /*
   * Check K1-K5 have the correct values to be able to become neighbors
   * K6 does not have to match
   */
  if ((eigrp->k_values[0] == nbr->K1) &&
      (eigrp->k_values[1] == nbr->K2) &&
      (eigrp->k_values[2] == nbr->K3) &&
      (eigrp->k_values[3] == nbr->K4) &&
      (eigrp->k_values[4] == nbr->K5))
    {

      if (eigrp_nbr_state_get(nbr) == EIGRP_NEIGHBOR_DOWN)
	{
	  eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_PENDING);
	  zlog_info("Neighbor %s (%s) is pending: new adjacency",
		    inet_ntoa(nbr->src), ifindex2ifname(nbr->ei->ifp->ifindex));

	  eigrp_update_send_init(nbr);
	}
    }
  else
    {
      if (eigrp_nbr_state_get(nbr) != EIGRP_NEIGHBOR_DOWN)
	{
	  if ((param->K1 & param->K2 & param->K3 & param->K4 & param->K5) == 255)
	    zlog_info ("Neighbor %s (%s) going down: Peer Termination",
		       inet_ntoa (nbr->src),ifindex2ifname (nbr->ei->ifp->ifindex));
	  else
	    zlog_info ("Neighbor %s (%s) going down: Kvalue mismatch",
		       inet_ntoa (nbr->src),ifindex2ifname (nbr->ei->ifp->ifindex));
	  eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_DOWN);
	}
    }
}

/**
 * @fn eigrp_sw_version_decode
 *
 * @param[in]		nbr	neighbor the ACK shoudl be sent to
 * @param[in]		param	pointer to TLV software version information
 *
 * @return void
 *
 * @par
 * Read the software version in the specified location.
 * This consists of two bytes of OS version, and two bytes of EIGRP
 * revision number.
 */
static void
eigrp_sw_version_decode (struct eigrp_neighbor *nbr,
			 struct eigrp_tlv_hdr_type *tlv)			      
{
  struct TLV_Software_Type *version = (struct TLV_Software_Type *)tlv;

  nbr->os_rel_major = version->vender_major;
  nbr->os_rel_minor = version->vender_minor;
  nbr->tlv_rel_major = version->eigrp_major;
  nbr->tlv_rel_minor = version->eigrp_minor;
  return;
}

/**
 * @fn eigrp_peer_termination_decode
 *
 * @param[in]		nbr	neighbor the ACK shoudl be sent to
 * @param[in]		param	pointer to TLV software version information
 *
 * @return void
 *
 * @par
 * Read the list of addresses in the TLV and match to out address. If
 * a match is found, move the sending neighbor to the down state. If
 * out address is not in the TLV, then ignore the peer termination
 *
 * @usage
 *   Place holder - Not currently supported.
 */
static void
eigrp_peer_termination_decode (struct eigrp_neighbor *nbr,
			       struct eigrp_tlv_hdr_type *tlv)
{
  return;
}

/*
 * @fn eigrp_hello_receive
 *
 * @param[in]	eigrp		eigrp routing process
 * @param[in]	iph		pointer to ip header
 * @param[in]	eigrph		pointer to eigrp header
 * @param[in]	s		input ip stream
 * @param[in]	ei		eigrp interface packet arrived on
 * @param[in]	size		size of eigrp packet
 *
 * @return void
 *
 * @par
 * This is the main worker function for processing hello packets. It
 * will validate the peer associated with the src ip address of the ip
 * header, and then decode each of the general TLVs which the packet
 * may contain.
 * 
 * @usage
 * Not all TLVs are current decoder.  This is a work in progress..
 */
void
eigrp_hello_receive (struct eigrp *eigrp, struct ip *iph, struct eigrp_header *eigrph,
		     struct stream *s, struct eigrp_interface *ei, int size)
{
  struct eigrp_tlv_hdr_type *tlv_header;
  struct eigrp_neighbor *nbr;
  uint16_t	type;
  uint16_t	length;

  /* get neighbor struct */
  nbr = eigrp_nbr_get(ei, eigrph, iph);

  /* neighbor must be valid, eigrp_nbr_get creates if none existed */
  assert(nbr);
  
  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
    zlog_debug("Processing Hello size[%u] int(%s) nbr(%s)",
	       size, ifindex2ifname(nbr->ei->ifp->ifindex), 
	       inet_ntoa(nbr->src));

  size -= EIGRP_HEADER_SIZE;
  if (size <= 0)
    return;

  tlv_header = (struct eigrp_tlv_hdr_type *)eigrph->tlv;

  do {
    type = ntohs(tlv_header->type);
    length = ntohs(tlv_header->length);

    if ((length > 0) && (length <= size))
      {
	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
	  zlog_debug("  General TLV(%s)", LOOKUP(eigrp_general_tlv_type_str, type));

	// determine what General TLV is being processed
	switch (type)
	  {
	  case EIGRP_TLV_PARAMETER:
	    eigrp_hello_parameter_decode(nbr, tlv_header);
	    break;
	  case EIGRP_TLV_AUTH:
	    break;
	  case EIGRP_TLV_SEQ:
	    break;
	  case EIGRP_TLV_SW_VERSION:
	    eigrp_sw_version_decode(nbr, tlv_header);
	    break;
	  case EIGRP_TLV_NEXT_MCAST_SEQ:
	    break;
	  case EIGRP_TLV_PEER_TERMINATION:
	    eigrp_peer_termination_decode(nbr, tlv_header);
	    break;
	  case EIGRP_TLV_PEER_MTRLIST:
	  case EIGRP_TLV_PEER_TIDLIST:
	    break;
	  default:
	    break;
	  }
    }

    tlv_header = (struct eigrp_tlv_hdr_type *)((char *)tlv_header + length);
    size -= length; 

  } while (size > 0);

  /*If received packet is hello with Parameter TLV*/
  if (eigrph->ack == 0)
    {
      /* increment statistics. */
      ei->hello_in++;
      eigrp_nbr_state_update(nbr);

    } /*If packet is only ack*/
  else
    {
      struct eigrp_packet *ep;

      ep = eigrp_fifo_tail(nbr->retrans_queue);
      if (ep != NULL)
	{
	  if (ntohl(eigrph->ack) == ep->sequence_number)
	    {
	      ep = eigrp_fifo_tail(nbr->retrans_queue);
	      eigrp_packet_free(ep);
	      if (nbr->retrans_queue->count > 0)
		{
		  eigrp_send_packet_reliably(nbr);
		}
	      return;
	    }
	}
      ep = eigrp_fifo_tail(nbr->multicast_queue);
      if (ep != NULL)
	{
	  if (ntohl(eigrph->ack) == ep->sequence_number)
	    {
	      ep = eigrp_fifo_tail(nbr->multicast_queue);
	      eigrp_packet_free(ep);
	      if (nbr->multicast_queue->count > 0)
		{
		  eigrp_send_packet_reliably(nbr);
		}
	      return;
	    }
	}
    }

  if (IS_DEBUG_EIGRP_PACKET(0, RECV))
      zlog_debug("Hello Packet received from %s", inet_ntoa(nbr->src));

}

/**
 * @fn eigrp_sw_version_encode
 *
 * @param[in,out]	s	packet stream TLV is stored to
 *
 * @return u_int16_t	number of bytes added to packet stream
 *
 * @par
 * Store the software version in the specified location.
 * This consists of two bytes of OS version, and two bytes of EIGRP
 * revision number.
 */
static u_int16_t
eigrp_sw_version_encode (struct stream *s)
{
  u_int16_t length = EIGRP_TLV_SW_VERSION_LEN;

  // setup the tlv fields
  stream_putw(s, EIGRP_TLV_SW_VERSION);
  stream_putw(s, length);

  // encode the version of quagga we're running
  // DVS: need to figure out a cleaner way to do this
  stream_putc(s, 0);		//!< major os version
  stream_putc(s, 99);		//!< minor os version

  /* and the core eigrp version */
  stream_putc(s, EIGRP_MAJOR_VERSION);
  stream_putc(s, EIGRP_MINOR_VERSION);

  return(length);
}

/**
 * @fn eigrp_tidlist_encode
 *
 * @param[in,out]	s	packet stream TLV is stored to
 *
 * @return void
 *
 * @par
 * If doing mutli-topology, then store the supported TID list.
 * This is currently a place holder function
 */
static u_int16_t
eigrp_tidlist_encode (struct stream *s)
{
  return 0;
}

/**
 * @fn eigrp_hello_parameter_encode
 *
 * @param[in]		ei	pointer to interface hello packet came in on
 * @param[in,out]	s	packet stream TLV is stored to
 *
 * @return u_int16_t	number of bytes added to packet stream
 *
 * @par
 * Encode Parameter TLV, used to convey metric weights and the hold time.
 *
 * @usage
 * Note the addition of K6 for the new extended metrics, and does not apply to
 * older TLV packet formats.
 */
static u_int16_t
eigrp_hello_parameter_encode (struct eigrp_interface *ei, struct stream *s)
{
    u_int16_t length = EIGRP_TLV_PARAMETER_LEN;

  // add in the parameters TLV
  stream_putw(s, EIGRP_TLV_PARAMETER);
  stream_putw(s, EIGRP_TLV_PARAMETER_LEN);

  // set k values
  stream_putc(s, ei->eigrp->k_values[0]); /* K1 */
  stream_putc(s, ei->eigrp->k_values[1]); /* K2 */
  stream_putc(s, ei->eigrp->k_values[2]); /* K3 */
  stream_putc(s, ei->eigrp->k_values[3]); /* K4 */
  stream_putc(s, ei->eigrp->k_values[4]); /* K5 */
  stream_putc(s, ei->eigrp->k_values[5]); /* K6 */

  // and set hold time value..
  stream_putw(s, IF_DEF_PARAMS(ei->ifp)->v_wait);

  return length;
}

/**
 * @fn eigrp_hello_encode
 *
 * @param[in]		ei	pointer to interface hello packet came in on
 * @param[in]		s	packet stream TLV is stored to
 * @param[in]		ack	if non-zero, neigbors sequence packet to ack
 *
 * @return eigrp_packet		pointer initialize hello packet
 *
 * @par
 * Allocate an EIGRP hello packet, and add in the the approperate TLVs
 *
 */
static struct eigrp_packet *
eigrp_hello_encode (struct eigrp_interface *ei, in_addr_t addr, u_int32_t ack)
{
  struct eigrp_packet *ep;
  u_int16_t length = EIGRP_HEADER_LEN;

  // allocate a new packet to be sent
  ep = eigrp_packet_new(ei->ifp->mtu);

  if (ep) {
    // encode common header feilds
    eigrp_packet_header_init(EIGRP_OPC_HELLO, ei, ep->s, 0, 0, ack);

    // encode Hello packet with approperate TLVs
    length += eigrp_hello_parameter_encode(ei, ep->s);

    // figure out the version of code we're running
    length += eigrp_sw_version_encode(ep->s);

    // add in the TID list if doing multi-topology
    length += eigrp_tidlist_encode(ep->s);

    // EIGRP Checksum
    eigrp_packet_checksum(ei, ep->s, length);

    // Set packet length
    ep->length = length;

    // set soruce address for the hello packet
    ep->dst.s_addr = addr;
  }

  return(ep);
}

/**
 * @fn eigrp_hello_send
 *
 * @param[in]		nbr	neighbor the ACK should be sent to
 *
 * @return void
 *
 * @par
 *  Send (unicast) a hello packet with the destination address
 *  associated with the neighbor.  The eigrp header ACK feild will be
 *  updated to the neighbor's sequence number to acknolodge any
 *  outstanding packets
 */
void
eigrp_hello_send_ack (struct eigrp_neighbor *nbr)
{
  struct eigrp_packet *ep;

  /* if packet succesfully created, add it to the interface queue */
  ep = eigrp_hello_encode(nbr->ei, nbr->src.s_addr, nbr->recv_sequence_number);

  if (ep)
    {
      if (IS_DEBUG_EIGRP_PACKET(0, SEND))
	zlog_debug("Queueing [Hello] Ack Seq [%u] nbr [%s]",
		   nbr->recv_sequence_number, inet_ntoa(nbr->src));

      /* Add packet to the top of the interface output queue*/
      eigrp_fifo_push_head(nbr->ei->obuf, ep);

      /* Hook thread to write packet. */
      if (nbr->ei->on_write_q == 0)
	{
	  listnode_add(nbr->ei->eigrp->oi_write_q, nbr->ei);
	  nbr->ei->on_write_q = 1;
	}
      if (nbr->ei->eigrp->t_write == NULL)
	nbr->ei->eigrp->t_write =
	  thread_add_write(master, eigrp_write, nbr->ei->eigrp, nbr->ei->eigrp->fd);
    }
}

/**
 * @fn eigrp_hello_send
 *
 * @param[in]		ei	pointer to interface hello shoudl be sent
 *
 * @return void
 *
 * @par
 * Build and enqueue a generic (multicast) periodic hello packet for
 * sending.  If no packets are currently queues, the packet will be
 * sent immadiatly
 */
void
eigrp_hello_send (struct eigrp_interface *ei)
{
  struct eigrp_packet *ep = NULL;

  /* If this is passive interface, do not send EIGRP Hello.
  if ((EIGRP_IF_PASSIVE_STATUS (ei) == EIGRP_IF_PASSIVE) ||
      (ei->type != EIGRP_IFTYPE_NBMA))
    return;
  */

  if (IS_DEBUG_EIGRP_PACKET(0, SEND))
    zlog_debug("Queueing [Hello] Interface(%s)", IF_NAME(ei));

  /* if packet was succesfully created, then add it to the interface queue */
  ep = eigrp_hello_encode(ei, htonl(EIGRP_MULTICAST_ADDRESS), 0);
  if (ep)
    {
      // Add packet to the top of the interface output queue
      eigrp_fifo_push_head(ei->obuf, ep);

      /* Hook thread to write packet. */
      if (ei->on_write_q == 0)
        {
          listnode_add(ei->eigrp->oi_write_q, ei);
          ei->on_write_q = 1;
        }

      if (ei->eigrp->t_write == NULL)
        {
          ei->eigrp->t_write =
	    thread_add_write(master, eigrp_write, ei->eigrp, ei->eigrp->fd);
	}
    }
}
