/*
 * EIGRP SNMP Support.
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

#ifdef HAVE_SNMP
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

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
#include "smux.h"

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"
#include "eigrpd/eigrp_snmp.h"


struct list *eigrp_snmp_iflist;

/* Declare static local variables for convenience. */
SNMP_LOCAL_VARIABLES

/* EIGRP-MIB. */
#define EIGRPMIB 1,3,6,1,4,1,9,9,449,1

/* EIGRP-MIB instances. */
oid eigrp_oid [] = { EIGRPMIB };

/* EIGRP VPN entry */
#define EIGRPVPNID						1
#define EIGRPVPNNAME						2

/* EIGRP Traffic statistics entry */
#define EIGRPASNUMBER						1
#define EIGRPNBRCOUNT						2
#define EIGRPHELLOSSENT						3
#define EIGRPHELLOSRCVD						4
#define EIGRPUPDATESSENT					5
#define EIGRPUPDATESRCVD					6
#define EIGRPQUERIESSENT					7
#define EIGRPQUERIESRCVD					8
#define EIGRPREPLIESSENT					9
#define EIGRPREPLIESRCVD					10
#define EIGRPACKSSENT						11
#define EIGRPACKSRCVD						12
#define EIGRPINPUTQHIGHMARK					13
#define EIGRPINPUTQDROPS					14
#define EIGRPSIAQUERIESSENT					15
#define EIGRPSIAQUERIESRCVD					16
#define EIGRPASROUTERIDTYPE					17
#define EIGRPASROUTERID						18
#define EIGRPTOPOROUTES						19
#define EIGRPHEADSERIAL						20
#define EIGRPNEXTSERIAL						21
#define EIGRPXMITPENDREPLIES				        22
#define EIGRPXMITDUMMIES					23

/* EIGRP topology entry */
#define EIGRPDESTNETTYPE					1
#define EIGRPDESTNET						2
#define EIGRPDESTNETPREFIXLEN				        4
#define EIGRPACTIVE						5
#define EIGRPSTUCKINACTIVE					6
#define EIGRPDESTSUCCESSORS					7
#define EIGRPFDISTANCE						8
#define EIGRPROUTEORIGINTYPE				        9
#define EIGRPROUTEORIGINADDRTYPE			        10
#define EIGRPROUTEORIGINADDR				        11
#define EIGRPNEXTHOPADDRESSTYPE				        12
#define EIGRPNEXTHOPADDRESS					13
#define EIGRPNEXTHOPINTERFACE				        14
#define EIGRPDISTANCE						15
#define EIGRPREPORTDISTANCE					16

/* EIGRP peer entry */
#define EIGRPHANDLE						1
#define EIGRPPEERADDRTYPE					2
#define EIGRPPEERADDR						3
#define EIGRPPEERIFINDEX					4
#define EIGRPHOLDTIME						5
#define EIGRPUPTIME						6
#define EIGRPSRTT						7
#define EIGRPRTO						8
#define EIGRPPKTSENQUEUED					9
#define EIGRPLASTSEQ						10
#define EIGRPVERSION						11
#define EIGRPRETRANS						12
#define EIGRPRETRIES						13

/* EIGRP interface entry */
#define EIGRPPEERCOUNT						3
#define EIGRPXMITRELIABLEQ					4
#define EIGRPXMITUNRELIABLEQ	        			5
#define EIGRPMEANSRTT						6
#define EIGRPPACINGRELIABLE					7
#define EIGRPPACINGUNRELIABLE		        		8
#define EIGRPMFLOWTIMER						9
#define EIGRPPENDINGROUTES					10
#define EIGRPHELLOINTERVAL					11
#define EIGRPXMITNEXTSERIAL					12
#define EIGRPUMCASTS						13
#define EIGRPRMCASTS						14
#define EIGRPUUCASTS						15
#define EIGRPRUCASTS						16
#define EIGRPMCASTEXCEPTS					17
#define EIGRPCRPKTS						18
#define EIGRPACKSSUPPRESSED					19
#define EIGRPRETRANSSENT					20
#define EIGRPOOSRCVD						21
#define EIGRPAUTHMODE						22
#define EIGRPAUTHKEYCHAIN					23

/* SNMP value hack. */
#define COUNTER     ASN_COUNTER
#define INTEGER     ASN_INTEGER
#define GAUGE       ASN_GAUGE
#define TIMETICKS   ASN_TIMETICKS
#define IPADDRESS   ASN_IPADDRESS
#define STRING      ASN_OCTET_STR
#define IPADDRESSPREFIXLEN ASN_INTEGER
#define IPADDRESSTYPE ASN_OCTET_STR
#define INTERFACEINDEXORZERO ASN_INTEGER




/* Hook functions. */
static u_char *eigrpVpnEntry (struct variable *, oid *, size_t *,
				 int, size_t *, WriteMethod **);
static u_char *eigrpTraffStatsEntry (struct variable *, oid *, size_t *, int,
			      size_t *, WriteMethod **);
static u_char *eigrpTopologyEntry (struct variable *, oid *, size_t *,
				  int, size_t *, WriteMethod **);
static u_char *eigrpPeerEntry (struct variable *, oid *, size_t *, int,
			      size_t *, WriteMethod **);
static u_char *eigrpInterfaceEntry (struct variable *, oid *, size_t *, int,
				   size_t *, WriteMethod **);


struct variable eigrp_variables[] =
{
  /* EIGRP vpn variables */
  {EIGRPVPNID,              	INTEGER, NOACCESS, eigrpVpnEntry,
   5, {1, 1, 1, 1, 1}},
  {EIGRPVPNNAME,            	STRING, RONLY, eigrpVpnEntry,
   5, {1, 1, 1, 1, 2}},

  /* EIGRP traffic stats variables */
  {EIGRPASNUMBER,              	INTEGER, NOACCESS, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 1}},
  {EIGRPNBRCOUNT,              	INTEGER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 2}},
  {EIGRPHELLOSSENT,            	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 3}},
  {EIGRPHELLOSRCVD,           	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 4}},
  {EIGRPUPDATESSENT,          	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 5}},
  {EIGRPUPDATESRCVD,           	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 6}},
  {EIGRPQUERIESSENT,            COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 7}},
  {EIGRPQUERIESRCVD,            COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 8}},
  {EIGRPREPLIESSENT,           	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 9}},
  {EIGRPREPLIESRCVD,            COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 10}},
  {EIGRPACKSSENT,               COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 11}},
  {EIGRPACKSRCVD,               COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 12}},
  {EIGRPINPUTQHIGHMARK,         INTEGER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 13}},
  {EIGRPINPUTQDROPS,           	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 14}},
  {EIGRPSIAQUERIESSENT,        	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 15}},
  {EIGRPSIAQUERIESRCVD,       	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 16}},
  {EIGRPASROUTERIDTYPE,         IPADDRESSTYPE, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 17}},
  {EIGRPASROUTERID,       	   	IPADDRESS, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 18}},
  {EIGRPTOPOROUTES,       	   	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 19}},
  {EIGRPHEADSERIAL,       	   	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 20}},
  {EIGRPNEXTSERIAL,            	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 21}},
  {EIGRPXMITPENDREPLIES,       	INTEGER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 22}},
  {EIGRPXMITDUMMIES,       	   	COUNTER, RONLY, eigrpTraffStatsEntry,
   5, {1, 2, 1, 1, 23}},

  /* EIGRP topology variables */
  {EIGRPDESTNETTYPE,       	   	IPADDRESSTYPE, NOACCESS, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 1}},
  {EIGRPDESTNET,       	   	   	IPADDRESSPREFIXLEN, NOACCESS, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 2}},
  {EIGRPDESTNETPREFIXLEN,      	IPADDRESSTYPE, NOACCESS, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 4}},
  {EIGRPACTIVE,       	   		INTEGER, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 5}},
  {EIGRPSTUCKINACTIVE,       	INTEGER, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 6}},
  {EIGRPDESTSUCCESSORS,       	INTEGER, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 7}},
  {EIGRPFDISTANCE,       	   	INTEGER, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 8}},
  {EIGRPROUTEORIGINTYPE,       	STRING, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 9}},
  {EIGRPROUTEORIGINADDRTYPE,    IPADDRESSTYPE, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 10}},
  {EIGRPROUTEORIGINADDR,       	IPADDRESS, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 11}},
  {EIGRPNEXTHOPADDRESSTYPE,     IPADDRESSTYPE, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 12}},
  {EIGRPNEXTHOPADDRESS,       	IPADDRESS, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 13}},
  {EIGRPNEXTHOPINTERFACE,       STRING, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 14}},
  {EIGRPDISTANCE,       	    INTEGER, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 15}},
  {EIGRPREPORTDISTANCE,       	INTEGER, RONLY, eigrpTopologyEntry,
   5, {1, 3, 1, 1, 16}},

  /* EIGRP peer variables */
  {EIGRPHANDLE,       	    	INTEGER, NOACCESS, eigrpPeerEntry,
   5, {1, 4, 1, 1, 1}},
  {EIGRPPEERADDRTYPE,       	IPADDRESSTYPE, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 2}},
  {EIGRPPEERADDR,       	    IPADDRESS, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 3}},
  {EIGRPPEERIFINDEX,       	    INTERFACEINDEXORZERO, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 4}},
  {EIGRPHOLDTIME,       	    INTEGER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 5}},
  {EIGRPUPTIME,       	    	STRING, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 6}},
  {EIGRPSRTT,       	   		INTEGER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 7}},
  {EIGRPRTO,       	    		INTEGER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 8}},
  {EIGRPPKTSENQUEUED,			INTEGER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 9}},
  {EIGRPLASTSEQ,       	    	INTEGER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 10}},
  {EIGRPVERSION,       	    	STRING, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 11}},
  {EIGRPRETRANS,       	    	COUNTER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 12}},
  {EIGRPRETRIES,       	    	INTEGER, RONLY, eigrpPeerEntry,
   5, {1, 4, 1, 1, 13}},

  /* EIGRP interface variables */
  {EIGRPPEERCOUNT,       	    GAUGE, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 3}},
  {EIGRPXMITRELIABLEQ,       	GAUGE, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 4}},
  {EIGRPXMITUNRELIABLEQ,       	GAUGE, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 5}},
  {EIGRPMEANSRTT,       	    INTEGER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 6}},
  {EIGRPPACINGRELIABLE,       	INTEGER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 7}},
  {EIGRPPACINGUNRELIABLE,       INTEGER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 8}},
  {EIGRPMFLOWTIMER,       	    INTEGER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 9}},
  {EIGRPPENDINGROUTES,       	GAUGE, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 10}},
  {EIGRPHELLOINTERVAL,       	INTEGER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 11}},
  {EIGRPXMITNEXTSERIAL,       	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 12}},
  {EIGRPUMCASTS,       	    	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 13}},
  {EIGRPRMCASTS,       	    	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 14}},
  {EIGRPUUCASTS,       	    	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 15}},
  {EIGRPRUCASTS,       	    	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 16}},
  {EIGRPMCASTEXCEPTS,       	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 17}},
  {EIGRPCRPKTS,       	   	 	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 18}},
  {EIGRPACKSSUPPRESSED,       	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 19}},
  {EIGRPRETRANSSENT,       	    COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 20}},
  {EIGRPOOSRCVD,       	    	COUNTER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 21}},
  {EIGRPAUTHMODE,       	    INTEGER, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 22}},
  {EIGRPAUTHKEYCHAIN,       	STRING, RONLY, eigrpInterfaceEntry,
   5, {1, 5, 1, 1, 23}}
};


  static u_char *
  eigrpVpnEntry (struct variable *v, oid *name, size_t *length,
   				 int exact, size_t *var_len, WriteMethod **write_method)
  {
	  return NULL;
  }
  static u_char *
  eigrpTraffStatsEntry (struct variable *v, oid *name, size_t *length,
			 	 int exact, size_t *var_len, WriteMethod **write_method)
  {
	struct eigrp *eigrp;

	zlog_warn("SOM TU \n");
	eigrp = eigrp_lookup ();

	/* Check whether the instance identifier is valid */
	if (smux_header_generic (v, name, length, exact, var_len, write_method)
	  == MATCH_FAILED)
	return NULL;

	/* Return the current value of the variable */
	switch (v->magic)
	{
	case EIGRPASNUMBER:		/* 1 */
		  /* AS-number of this EIGRP instance. */
		  if (eigrp)
		return SNMP_INTEGER (eigrp->AS);
		  else
		return SNMP_INTEGER (0);
		  break;
//	case OSPFADMINSTAT:		/* 2 */
//		  /* The administrative status of OSPF in the router. */
//		  if (ospf_admin_stat (ospf))
//		return SNMP_INTEGER (OSPF_STATUS_ENABLED);
//		  else
//	return SNMP_INTEGER (OSPF_STATUS_DISABLED);
//	  break;
//	case OSPFVERSIONNUMBER:	/* 3 */
//	  /* OSPF version 2. */
//	  return SNMP_INTEGER (OSPF_VERSION);
//	  break;
//	case OSPFAREABDRRTRSTATUS:	/* 4 */
//	  /* Area Border router status. */
//	  if (ospf && CHECK_FLAG (ospf->flags, OSPF_FLAG_ABR))
//	return SNMP_INTEGER (SNMP_TRUE);
//	  else
//	return SNMP_INTEGER (SNMP_FALSE);
//	  break;
//	case OSPFASBDRRTRSTATUS:	/* 5 */
//	  /* AS Border router status. */
//	  if (ospf && CHECK_FLAG (ospf->flags, OSPF_FLAG_ASBR))
//	return SNMP_INTEGER (SNMP_TRUE);
//	  else
//	return SNMP_INTEGER (SNMP_FALSE);
//	  break;
//	case OSPFEXTERNLSACOUNT:	/* 6 */
//	  /* External LSA counts. */
//	  if (ospf)
//	return SNMP_INTEGER (ospf_lsdb_count_all (ospf->lsdb));
//	  else
//	return SNMP_INTEGER (0);
//	  break;
//	case OSPFEXTERNLSACKSUMSUM:	/* 7 */
//	  /* External LSA checksum. */
//	  return SNMP_INTEGER (0);
//	  break;
//	case OSPFTOSSUPPORT:	/* 8 */
//	  /* TOS is not supported. */
//	  return SNMP_INTEGER (SNMP_FALSE);
//	  break;
//	case OSPFORIGINATENEWLSAS:	/* 9 */
//	  /* The number of new link-state advertisements. */
//	  if (ospf)
//	return SNMP_INTEGER (ospf->lsa_originate_count);
//	  else
//	return SNMP_INTEGER (0);
//	  break;
//	case OSPFRXNEWLSAS:		/* 10 */
//	  /* The number of link-state advertisements received determined
//		 to be new instantiations. */
//	  if (ospf)
//	return SNMP_INTEGER (ospf->rx_lsa_count);
//	  else
//	return SNMP_INTEGER (0);
//	  break;
//	case OSPFEXTLSDBLIMIT:	/* 11 */
//	  /* There is no limit for the number of non-default
//		 AS-external-LSAs. */
//	  return SNMP_INTEGER (-1);
//	  break;
//	case OSPFMULTICASTEXTENSIONS: /* 12 */
//	  /* Multicast Extensions to OSPF is not supported. */
//	  return SNMP_INTEGER (0);
//	  break;
//	case OSPFEXITOVERFLOWINTERVAL: /* 13 */
//	  /* Overflow is not supported. */
//	  return SNMP_INTEGER (0);
//	  break;
//	case OSPFDEMANDEXTENSIONS:	/* 14 */
//	  /* Demand routing is not supported. */
//	  return SNMP_INTEGER (SNMP_FALSE);
//	  break;
	default:
	  return NULL;
	}
	return NULL;
  }
  static u_char *
  eigrpTopologyEntry (struct variable *v, oid *name, size_t *length,
			 	 int exact, size_t *var_len, WriteMethod **write_method)
  {
	  return NULL;
  }
  static u_char *
  eigrpPeerEntry (struct variable *v, oid *name, size_t *length,
			 	 int exact, size_t *var_len, WriteMethod **write_method)
  {
	  return NULL;
  }
  static u_char *
  eigrpInterfaceEntry (struct variable *v, oid *name, size_t *length,
			 	 int exact, size_t *var_len, WriteMethod **write_method)
  {
	  return NULL;
  }


  /* Register EIGRP-MIB. */
  void
  eigrp_snmp_init ()
  {
    eigrp_snmp_iflist = list_new ();
    smux_init (eigrp_om->master);
    REGISTER_MIB("iana/ciscoEigrpMIB", eigrp_variables, variable, eigrp_oid);
  }


#endif
