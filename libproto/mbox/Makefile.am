AM_CPPFLAGS = $(MU_LIB_COMMON_INCLUDES)

lib_LTLIBRARIES = libmu_mbox.la
libmu_mbox_la_LDFLAGS=-version-info @VI_CURRENT@:@VI_REVISION@:@VI_AGE@
libmu_mbox_la_LIBADD = $(MU_LIB_MAILUTILS)
libmu_mbox_la_SOURCES = \
 mboxrb.c\
 message.c

SUBDIRS = . tests
