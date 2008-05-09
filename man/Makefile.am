EXTRA_DIST = grossd.8.in grossd.conf.5.in

CLEANFILES = $(man_MANS)

man_MANS = grossd.8 grossd.conf.5

if HAVE_SED

SUFFIXES = .in

.in:
	@SED@ \
	  -e 's!@''sysconfdir''@!@sysconfdir@!' \
	  -e 's!@''libdir''@!@libdir@!' \
	$< > $@
endif
