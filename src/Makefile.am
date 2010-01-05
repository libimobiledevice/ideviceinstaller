AM_CFLAGS =			\
	$(GLOBAL_CFLAGS)	\
	$(libiphone_CFLAGS)	\
	$(libplist_CFLAGS)	\
	$(libzip_CFLAGS)

AM_LDFLAGS =			\
	$(libiphone_LIBS)	\
	$(libplist_LIBS)	\
	$(libzip_LIBS)

bin_PROGRAMS = iphoneinstaller

iphoneinstaller_SOURCES = iphoneinstaller.c
iphoneinstaller_CFLAGS = $(AM_CFLAGS)
iphoneinstaller_LDFLAGS = $(AM_LDFLAGS)

