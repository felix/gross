if HAVE_SED
man_MANS = grossd.8 grossd.conf.5
endif

EXTRA_DIST = grossd.8.pod grossd.conf.5.pod grossd.8.in grossd.conf.5.in

CLEANFILES = $(man_MANS)

if HAVE_POD2MAN
grossd.8.in: grossd.8.pod
	$(POD2MAN) -c "" -r -s 8 -n grossd $(srcdir)/grossd.8.pod $@

grossd.conf.5.in: grossd.conf.5.pod
	$(POD2MAN) -c "" -r -s 5 -n grossd.conf $(srcdir)/grossd.conf.5.pod $@
endif

if HAVE_SED
grossd.8: grossd.8.in
	@SED@ \
          -e 's!%{sysconfdir}!@sysconfdir@!' \
          -e 's!%{libdir}!@libdir@!' \
	$(srcdir)/$@.in > $@

grossd.conf.5: grossd.conf.5.in
	@SED@ \
          -e 's!%{sysconfdir}!@sysconfdir@!' \
          -e 's!%{libdir}!@libdir@!' \
	$(srcdir)/$@.in > $@
endif
