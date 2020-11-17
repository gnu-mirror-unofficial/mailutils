m4_include([testsuite.inc])

m4_define([MDA_OPTIONS],[dnl
--set '|mailbox|mailbox-pattern=mbox://'`pwd`'/spool/${user}' dnl
--set .auth.authorization=system])
    

