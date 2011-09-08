AM_CPPFLAGS = -I${top_srcdir}/include -I${top_srcdir} @OCV_CFLAGS@ @GLIB_CFLAGS@ @GDK_CFLAGS@ @AM_CPPFLAGS@

noinst_LTLIBRARIES = libsead.la libseadclient.la

libsead_la_SOURCES = seadlib.cpp
libsead_la_LDFLAGS = @OCV_LDFLAGS@ @GLIB_LDFLAGS@ @GDK_LDFLAGS@ @AM_LDFLAGS@

libseadclient_la_SOURCES = seadclient.cpp
