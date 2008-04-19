# $Id$

AM_CPPFLAGS = @REENTRANT_FLAG@
INCLUDES = -I$(top_srcdir)/include

sbin_PROGRAMS = grossd 
bin_PROGRAMS = gclient
lib_LTLIBRARIES = grosscheck.la
noinst_PROGRAMS = bloom

grossd_SOURCES = sha256.c bloom.c utils.c srvutils.c worker.c bloommgr.c gross.c syncmgr.c conf.c msgqueue.c srvstatus.c thread_pool.c stats.c worker_postfix.c worker_sjsms.c check_blocker.c check_random.c
EXTRA_grossd_SOURCES = check_dnsbl.c worker_milter.c
grossd_LDFLAGS = @LDFLAGS@ proto_sjsms.o
grossd_LDADD = @DNSBLSOURCES@ @MILTERSOURCES@ @SPFSOURCES@
grossd_DEPENDENCIES = @DNSBLSOURCES@ @MILTERSOURCES@ @SPFSOURCES@ proto_sjsms.c

gclient_SOURCES = gclient.c utils.c client_postfix.c client_sjsms.c
gclient_LDFLAGS = @LDFLAGS@ proto_sjsms.o
gclient_DEPENDENCIES = proto_sjsms.c

grosscheck_la_SOURCES = grosscheck.c proto_sjsms.c
grosscheck_la_LDFLAGS = -module -avoid-version @STATIC_GLIBC_FLAG@

check_PROGRAMS = sha256
bloom_SOURCES = sha256.c bloom-test.c bloom.c srvutils.c utils.c
sha256_SOURCES = sha256-test.c sha256.c srvutils.c utils.c bloom.c
