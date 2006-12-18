INCLUDES = -I$(top_srcdir)/include

sbin_PROGRAMS = grossd 
bin_PROGRAMS = gclient
pkglib_LTLIBRARIES = grosscheck.la
noinst_PROGRAMS = bloom

grossd_SOURCES = sha256.c bloom.c utils.c srvutils.c worker.c bloommgr.c gross.c syncmgr.c conf.c msgqueue.c srvstatus.c
EXTRA_grossd_SOURCES = dnsblc.c worker_postfix.c worker_sjsms.c proto_postfix.c
grossd_LDFLAGS = @LDFLAGS@  
grossd_LDADD = @DNSBLSOURCES@ worker_@PROTOCOL@.o proto_@PROTOCOL@.o
grossd_DEPENDENCIES = @DNSBLSOURCES@ worker_@PROTOCOL@.o proto_@PROTOCOL@.o

gclient_SOURCES = gclient.c utils.c 
EXTRA_gclient_SOURCES = client_postfix.c client_sjsms.c proto_postfix.c
gclient_LDFLAGS = @LDFLAGS@

gclient_LDADD = client_@PROTOCOL@.o proto_@PROTOCOL@.o
gclient_DEPENDENCIES = client_@PROTOCOL@.o proto_@PROTOCOL@.o

grosscheck_la_SOURCES = grosscheck.c proto_sjsms.c
grosscheck_la_LDFLAGS = -module -avoid-version -no-undefined

check_PROGRAMS = sha256
bloom_SOURCES = sha256.c bloom-test.c bloom.c srvutils.c utils.c
sha256_SOURCES = sha256-test.c sha256.c srvutils.c utils.c
