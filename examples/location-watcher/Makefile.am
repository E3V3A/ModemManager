noinst_PROGRAMS = location-watcher

location_watcher_CPPFLAGS = \
	$(MMCLI_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/libmm-glib \
	-I${top_srcdir}/libmm-glib/generated \
	-I${top_builddir}/libmm-glib/generated

location_watcher_SOURCES = \
	location-watcher.c

location_watcher_LDADD = \
	$(MMCLI_LIBS) \
	$(top_builddir)/libmm-glib/libmm-glib.la
