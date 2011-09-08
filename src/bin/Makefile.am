AM_CPPFLAGS = -I${top_srcdir}/include -I${top_srcdir} @AM_CPPFLAGS@

bin_PROGRAMS = sead
noinst_PROGRAMS = fetchAll seadclient imclassifier

sead_SOURCES = sead.cpp
sead_LDADD = ../libinferno/libinferno.la ../libsead/libsead.la ../libmynet/libmynet.la
sead_CPPFLAGS = @OCV_CFLAGS@ @SVM_CFLAGS@ @MYSQL_CFLAGS@ @GLIB_CFLAGS@ @GDK_CFLAGS@ ${AM_CPPFLAGS}
sead_LDFLAGS = @OCV_LDFLAGS@ @SVM_LDFLAGS@ @MYSQL_LDFLAGS@ @GLIB_LDFLAGS@ @GDK_LDFLAGS@ @AM_LDFLAGS@

seadclient_SOURCES = seadclient.cpp
seadclient_LDADD = ../libinferno/libinferno.la ../libsead/libseadclient.la ../libmynet/libmynet.la
seadclient_CPPFLAGS = @MYSQL_CFLAGS@ ${AM_CPPFLAGS}

fetchAll_SOURCES = fetchAll.cpp
fetchAll_LDADD = ../libinferno/libinferno.la
fetchAll_CPPFLAGS = @MYSQL_CFLAGS@ ${AM_CPPFLAGS}

imclassifier_SOURCES = imclassifier.cpp
imclassifier_LDADD = ../libinferno/libinferno.la ../libsead/libsead.la
imclassifier_CPPFLAGS = @OCV_CFLAGS@ @SVM_CFLAGS@ ${AM_CPPFLAGS}
imclassifier_LDFLAGS = @OCV_LDFLAGS@ @SVM_LDFLAGS@ @AM_LDFLAGS@
