/*
 * EIGRP DUAL algorithm.
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

#ifndef _ZEBRA_EIGRP_FSM_H
#define _ZEBRA_EIGRP_FSM_H

struct
{
  int
  (*func)(struct eigrp_topology_node *);
} NSM [EIGRP_FSM_STATE_MAX][EIGRP_FSM_EVENT_MAX] =
  {
    {
    //PASSIVE STATE
          { NULL }, /* Event 1 */
          { NULL }, /* Event 2 */
          { NULL }, /* Event 3 */
          { NULL }, /* Event 4 */
          { NULL }, /* Event 5 */
          { NULL }, /* Event 6 */
          { NULL }, /* Event 7 */
          { NULL }, /* Event 8 */
          { NULL }, /* Event 9 */
          { NULL }, /* Event 10 */
          { NULL }, /* Event 11 */
          { NULL }, /* Event 12 */
          { NULL }, /* Event 13 */
          { NULL }, /* Event 14 */
          { NULL }, /* Event 15 */
          { NULL } /* Event 16 */

    },
    {
    //Active 0 state
          { NULL }, /* Event 1 */
          { NULL }, /* Event 2 */
          { NULL }, /* Event 3 */
          { NULL }, /* Event 4 */
          { NULL }, /* Event 5 */
          { NULL }, /* Event 6 */
          { NULL }, /* Event 7 */
          { NULL }, /* Event 8 */
          { NULL }, /* Event 9 */
          { NULL }, /* Event 10 */
          { NULL }, /* Event 11 */
          { NULL }, /* Event 12 */
          { NULL }, /* Event 13 */
          { NULL }, /* Event 14 */
          { NULL }, /* Event 15 */
          { NULL } /* Event 16 */

    },
    {
    //Active 1 state
          { NULL }, /* Event 1 */
          { NULL }, /* Event 2 */
          { NULL }, /* Event 3 */
          { NULL }, /* Event 4 */
          { NULL }, /* Event 5 */
          { NULL }, /* Event 6 */
          { NULL }, /* Event 7 */
          { NULL }, /* Event 8 */
          { NULL }, /* Event 9 */
          { NULL }, /* Event 10 */
          { NULL }, /* Event 11 */
          { NULL }, /* Event 12 */
          { NULL }, /* Event 13 */
          { NULL }, /* Event 14 */
          { NULL }, /* Event 15 */
          { NULL } /* Event 16 */

    },
    {
    //Active 2 state
          { NULL }, /* Event 1 */
          { NULL }, /* Event 2 */
          { NULL }, /* Event 3 */
          { NULL }, /* Event 4 */
          { NULL }, /* Event 5 */
          { NULL }, /* Event 6 */
          { NULL }, /* Event 7 */
          { NULL }, /* Event 8 */
          { NULL }, /* Event 9 */
          { NULL }, /* Event 10 */
          { NULL }, /* Event 11 */
          { NULL }, /* Event 12 */
          { NULL }, /* Event 13 */
          { NULL }, /* Event 14 */
          { NULL }, /* Event 15 */
          { NULL } /* Event 16 */

    },
    {
    //Active 3 state
          { NULL }, /* Event 1 */
          { NULL }, /* Event 2 */
          { NULL }, /* Event 3 */
          { NULL }, /* Event 4 */
          { NULL }, /* Event 5 */
          { NULL }, /* Event 6 */
          { NULL }, /* Event 7 */
          { NULL }, /* Event 8 */
          { NULL }, /* Event 9 */
          { NULL }, /* Event 10 */
          { NULL }, /* Event 11 */
          { NULL }, /* Event 12 */
          { NULL }, /* Event 13 */
          { NULL }, /* Event 14 */
          { NULL }, /* Event 15 */
          { NULL } /* Event 16 */

    }, };
extern int eigrp_get_fsm_event(struct eigrp_topology_node *, u_char, void *);


#endif /* _ZEBRA_EIGRP_DUAL_H */
