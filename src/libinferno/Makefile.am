AM_CPPFLAGS = -I${top_srcdir}/include -I${top_srcdir} @MYSQL_CFLAGS@ @XML2_CFLAGS@ @CURL_CFLAGS@ @URIP_CFLAGS@ @AM_CPPFLAGS@

noinst_LTLIBRARIES = libinferno.la

libinferno_la_SOURCES = \
			logger.cpp \
			infernoconf.cpp \
			dbcache.cpp \
			htmlParser.cpp \
			multifetch.cpp
libinferno_la_LIBADD = ../libmynet/libmynet.la ../libsead/libseadclient.la
libinferno_la_LDFLAGS = @MYSQL_LDFLAGS@ @XML2_LDFLAGS@ @CURL_LDFLAGS@ @URIP_LDFLAGS@ @AM_LDFLAGS@
