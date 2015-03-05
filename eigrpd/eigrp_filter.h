/*
 * eigrp_filter.h
 *
 *  Created on: Feb 26, 2015
 *      Author: martin
 */

#ifndef EIGRPD_EIGRP_FILTER_H_
#define EIGRPD_EIGRP_FILTER_H_


extern void eigrp_distribute_update (struct distribute *);

extern void eigrp_distribute_update_interface (struct interface *);

extern void eigrp_distribute_update_all (struct prefix_list *);

extern void eigrp_distribute_update_all_wrapper(struct access_list *);

#endif /* EIGRPD_EIGRP_FILTER_H_ */
