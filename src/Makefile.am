AM_CFLAGS =			\
	$(GLOBAL_CFLAGS)	\
	$(libimobiledevice_CFLAGS)	\
	$(libglib2_CFLAGS)	\
	$(libplist_CFLAGS)	\
	$(libzip_CFLAGS)

AM_LDFLAGS =			\
	$(libimobiledevice_LIBS)	\
	$(libglib2_LIBS)	\
	$(libplist_LIBS)	\
	$(libzip_LIBS)

bin_PROGRAMS = ideviceput

ideviceput_SOURCES = ideviceinstaller.c
ideviceput_CFLAGS = $(AM_CFLAGS)
ideviceput_LDFLAGS = $(AM_LDFLAGS)

