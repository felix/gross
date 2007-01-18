noinst_HEADERS = include/*.h
EXTRA_DIST = configure doc
SUBDIRS = src
sysconf_DATA =

dist-hook:
	rm -rf `find $(distdir)/doc -type d -name .svn`

install-data-local:
	if test ! -f $(sysconfdir)/grossd.conf; then $(INSTALL_DATA) $(srcdir)/doc/examples/grossd.conf $(sysconfdir); fi
