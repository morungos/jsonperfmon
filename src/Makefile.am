bin_PROGRAMS = jsonperfmon

jsonperfmon_SOURCES = jsonperf.c jsonperf.h glib_compat.c glib_compat.h
jsonperfmon_CFLAGS = -g -O2 -I.
jsonperfmon_LDFLAGS = 

if OS_LINUX 
jsonperfmon_SOURCES += proclinux.c proclinux.h perflinux.c perflinux.h
endif

if OS_AIX
jsonperfmon_LDFLAGS += -lperfstat
endif