include $(top_srcdir)/testsuite/testsuite.am
AM_CPPFLAGS = $(MU_LIB_COMMON_INCLUDES) 
noinst_PROGRAMS = \
 fldel\
 mbdel\
 mimetest

mimetest_LDADD = $(MU_LIB_MBOX) $(MU_LIB_MAILUTILS)
mbdel_LDADD = \
 $(MU_LIB_MAILBOX)\
 $(MU_LIB_MAILER)\
 $(MU_LIB_AUTH)\
 $(MU_AUTHLIBS)\
 $(MU_LIB_MAILUTILS)
fldel_LDADD = $(mbdel_LDADD)

TESTSUITE_AT += \
 fldel.at\
 mbdel.at\
 mime.at\
 mime01.at

