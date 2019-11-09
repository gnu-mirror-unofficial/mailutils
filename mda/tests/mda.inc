m4_include([testsuite.inc])

m4_define([MDA_OPTIONS],[dnl
MUT_DEFAULT_OPTIONS dnl
--set '|mailbox|mailbox-pattern='`pwd`/spool/'${user}' dnl
--set .auth.authorization=system])

m4_define([dumpmail],
[m4_case(MU_DEFAULT_SCHEME,
[mbox],
[sed -e '/^From /d' -e '$d' $1],
[dotmail],
[sed -e '/^\.$/d' $1],
[mh],
[sed -e /^X-IMAPbase:/d\
     -e /^X-UID:/d\
     -e /^X-Envelope-Sender:/d\
     -e /^X-Envelope-Date:/d $1/1],
[maildir],
[f=$(find $1/new -type f | head -n 1)
 if test -n $f; then
    sed -e /^X-IMAPbase:/d\
        -e /^X-UID:/d\
        -e /^X-Envelope-Sender:/d\
        -e /^X-Envelope-Date:/d $f
 fi],
[# Should not happen
 m4_fatal([Unknown default mailbox format (MU_DEFAULT_SCHEME)])])])
    

