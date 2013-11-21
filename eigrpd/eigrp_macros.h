/*
 * eigrp_macros.h
 *
 *  Created on: Nov 7, 2013
 *      Author: janovic
 */

#ifndef _ZEBRA_EIGRP_MACROS_H_
#define _ZEBRA_EIGRP_MACROS_H_

#define DECLARE_IF_PARAM(T, P) T P; u_char P##__config:1
#define IF_EIGRP_IF_INFO(I) ((struct eigrp_if_info *)((I)->info))
#define IF_OIFS(I)  (IF_EIGRP_IF_INFO (I)->eifs)
#define IF_OIFS_PARAMS(I) (IF_EIGRP_IF_INFO (I)->params)

#define SET_IF_PARAM(S, P) ((S)->P##__config) = 1
#define IF_DEF_PARAMS(I) (IF_EIGRP_IF_INFO (I)->def_params)

#define UNSET_IF_PARAM(S, P) ((S)->P##__config) = 0

#define EIGRP_IF_PARAM_CONFIGURED(S, P) ((S) && (S)->P##__config)
#define EIGRP_IF_PARAM(O, P) \
        (EIGRP_IF_PARAM_CONFIGURED ((O)->params, P)?\
                        (O)->params->P:IF_DEF_PARAMS((O)->ifp)->P)

#define EIGRP_IF_PASSIVE_STATUS(O) \
       (EIGRP_IF_PARAM_CONFIGURED((O)->params, passive_interface) ? \
         (O)->params->passive_interface : \
         (EIGRP_IF_PARAM_CONFIGURED(IF_DEF_PARAMS((O)->ifp), passive_interface) ? \
           IF_DEF_PARAMS((O)->ifp)->passive_interface : \
           (O)->eigrp->passive_interface_default))

//------------------------------------------------------------------------------------------------------------------------------------

#define EIGRP_IF_STRING_MAXLEN  40
#define IF_NAME(I)      eigrp_if_name_string ((I))

//------------------------------------------------------------------------------------------------------------------------------------

/*Macros for EIGRP interface multicast membership*/
#define EI_MEMBER_FLAG(M) (1 << (M))
#define EI_MEMBER_COUNT(O,M) (IF_EIGRP_IF_INFO(ei->ifp)->membership_counts[(M)])
#define EI_MEMBER_CHECK(O,M) \
    (CHECK_FLAG((O)->multicast_memberships, EI_MEMBER_FLAG(M)))
#define EI_MEMBER_JOINED(O,M) \
  do { \
    SET_FLAG ((O)->multicast_memberships, EI_MEMBER_FLAG(M)); \
    IF_EIGRP_IF_INFO((O)->ifp)->membership_counts[(M)]++; \
  } while (0)
#define EI_MEMBER_LEFT(O,M) \
  do { \
    UNSET_FLAG ((O)->multicast_memberships, EI_MEMBER_FLAG(M)); \
    IF_EIGRP_IF_INFO((O)->ifp)->membership_counts[(M)]--; \
  } while (0)


#endif /* _ZEBRA_EIGRP_MACROS_H_ */
