## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ $(LOCAL_OPTS) -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

lib_LTLIBRARIES = libeigrp.la
libeigrp_la_LDFLAGS = -version-info 0:0:0

sbin_PROGRAMS = eigrpd

libeigrp_la_SOURCES = \
	eigrpd.c eigrp_vty.c eigrp_topology.c eigrp_zebra.c eigrp_neighbor.c eigrp_packet.c \
	eigrp_network.c eigrp_fsm.c eigrp_interface.c

eigrpdheaderdir = $(pkgincludedir)/eigrpd

eigrpdheader_HEADERS = \
	eigrpd.h eigrp_topology.h
	
noinst_HEADERS = \
	eigrp_vty.h eigrp_neighbor.h eigrp_network.h eigrp_packet.h \
	eigrp_zebra.h eigrp_interface.h
	
eigrpd_SOURCES = eigrp_main.c

eigrpd_LDADD = libeigrp.la ../lib/libzebra.la @LIBCAP@

examplesdir = $(exampledir)
dist_examples_DATA = eigrpd.conf.sample