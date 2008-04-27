man_MANS = grossd.8 grossd.conf.5

EXTRA_DIST = grossd.8.in grossd.conf.5.in

CLEANFILES = $(man_MANS)

SUFFIXES = .in .pod

.pod:
	pod2man $< $@

.in:
	@SED@ \
	  -e 's!@''sysconfdir''@!@sysconfdir@!' \
	  -e 's!@''sbindir''@!@sbindir@!' \
	  -e 's!@''libdir''@!@libdir@!' \
	  -e 's!@''VERSION''@!@VERSION@!' \
	$< > $@

