## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ $(LOCAL_OPTS) -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

lib_LTLIBRARIES = libeigrp.la
libeigrp_la_LDFLAGS = -version-info 0:0:0

sbin_PROGRAMS = eigrpd

libeigrp_la_SOURCES = \
	eigrpd.c eigrp_vty.c

eigrpdheaderdir = $(pkgincludedir)/eigrpd

eigrpdheader_HEADERS = \
	eigrpd.h 
	
noinst_HEADERS = \
	eigrp_vty.h
	
eigrpd_SOURCES = eigrp_main.c

eigrpd_LDADD = libeigrp.la ../lib/libzebra.la @LIBCAP@

examplesdir = $(exampledir)
dist_examples_DATA = eigrpd.conf.sample