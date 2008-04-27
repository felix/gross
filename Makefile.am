# $Id$

noinst_HEADERS = include/bloom.h include/conf.h include/syncmgr.h include/check_blocker.h include/msgqueue.h include/thread_pool.h include/check_dnsbl.h include/proto_sjsms.h include/utils.h include/check_random.h include/sha256.h include/worker.h include/check_spf.h include/srvutils.h include/common.h include/stats.h

EXTRA_DIST = configure doc
SUBDIRS = src man
# This is important, as it creates the etc directory if needed
sysconf_DATA =

dist-hook:
	rm -rf `find $(distdir)/doc -type d -name .svn`

install-data-local:
	if test ! -f $(DESTDIR)$(sysconfdir)/grossd.conf; then $(INSTALL_DATA) $(srcdir)/doc/examples/grossd.conf $(DESTDIR)$(sysconfdir); fi
