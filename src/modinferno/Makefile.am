AM_CPPFLAGS = -I${top_srcdir}/include -I${top_srcdir} @ICAPFLAGS@ @MYSQL_CFLAGS@ @XML2_CFLAGS@ @AM_CPPFLAGS@

modinferno_LTLIBRARIES = srv_inferno.la
modinfernodir = @ICAPMODSDIR@

srv_inferno_la_SOURCES = srv_inferno.cpp
srv_inferno_la_LDFLAGS = -module -avoid-version @MYSQL_LDFLAGS@ @ICAPLIBS@ @AM_LDFLAGS@
srv_inferno_la_LIBADD = ../libinferno/libinferno.la

install-data-local:
	$(INSTALL_DATA) srv_inferno.conf $(ICAPCONFDIR)/srv_inferno.conf.default
	if test ! -f $(ICAPCONFDIR)/srv_inferno.conf; then $(INSTALL_DATA) srv_inferno.conf $(ICAPCONFDIR)/srv_inferno.conf; fi

EXTRA_DIST = srv_inferno.conf
