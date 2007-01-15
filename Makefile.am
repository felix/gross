noinst_HEADERS = include/*.h
EXTRA_DIST = configure doc
SUBDIRS = src
sysconf_DATA =

install-data-local:
	if test ! -f $(sysconfdir)/grossd.conf; then $(INSTALL_DATA) -t $(sysconfdir) $(srcdir)/doc/examples/grossd.conf; fi
