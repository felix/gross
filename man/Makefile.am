man_MANS = grossd.8 grossd.conf.5

EXTRA_DIST = grossd.8.pod.in grossd.conf.5.pod.in

CLEANFILES = $(man_MANS)

if HAVE_POD2MAN
grossd.8: grossd.8.pod.in
	@SED@ \
          -e 's!@''sysconfdir''@!@sysconfdir@!' \
          -e 's!@''sbindir''@!@sbindir@!' \
          -e 's!@''libdir''@!@libdir@!' \
          -e 's!@''VERSION''@!@VERSION@!' \
        $< | $(POD2MAN) -c "" -r -s 8 -n grossd > $@

grossd.conf.5: grossd.conf.5.pod.in
	@SED@ \
          -e 's!@''sysconfdir''@!@sysconfdir@!' \
          -e 's!@''sbindir''@!@sbindir@!' \
          -e 's!@''libdir''@!@libdir@!' \
          -e 's!@''VERSION''@!@VERSION@!' \
        $< | $(POD2MAN) -c "" -r -s 8 -n grossd.conf > $@
endif
