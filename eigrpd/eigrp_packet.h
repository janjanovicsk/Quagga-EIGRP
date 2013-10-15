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

#ifndef _ZEBRA_EIGRP_PACKET_H
#define _ZEBRA_EIGRP_PACKET_H

#define EIGRP_MAX_PACKET_SIZE  65535U   /* includes IP Header size. */

struct eigrp_packet
{
  struct eigrp_packet *next;

  /* Pointer to data stream. */
  struct stream *s;

  /* IP destination address. */
  struct in_addr dst;

  /* EIGRP packet length. */
  u_int16_t length;
};

struct eigrp_fifo
{
  unsigned long count;

  struct eigrp_packet *head;

  struct eigrp_packet *tail;
};

struct eigrp_header
{

};

/*Prototypes*/
extern int eigrp_read (struct thread *);
extern int eigrp_write (struct thread *);
extern struct eigrp_fifo *eigrp_fifo_new (void);


#endif /* _ZEBRA_EIGRP_PACKET_H */
