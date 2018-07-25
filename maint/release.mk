# SYNOPSIS
#   make release
#
# DESRIPTION
#   Publishes a new release.  The version number is determined by
#   analyzing gitinfo.pl output.  The destination upload server is
#   determined depending on the version number.  Two-part version
#   number is considered to be a stable release.  A three (or more)-part
#   version number is an alpha version
#
# PREREQUISITES
#   Main Makefile from the Mailutils top-level source directory must be
#   included atop of this one.

release:
	@set -- `$(GITINFO) -H'$$refversion{?$$refdist>0??-$$refdist?} $$upload_dest'`;\
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) DIST_ARCHIVES="$(subst -$(VERSION),-$$1,$(DIST_ARCHIVES))" UPLOAD_TO=$$2 distdir=$(PACKAGE)-$$1 release_archives

GNUPLOAD_OPT=\
 --to $(UPLOAD_TO).gnu.org:mailutils\
 --to download.gnu.org.ua:$(UPLOAD_TO)/mailutils\
 --symlink-regex

release_archives: $(DIST_ARCHIVES)
	@case "$(UPLOAD_TO)" in \
	alpha|ftp) ;; \
	*) echo >&2 "Don't use make release_archives, use make release instead";\
           exit 1; \
	esac; \
	echo "Releasing $(DIST_ARCHIVES) to $(UPLOAD_TO)"; \
	gnupload $(GNUPLOAD_OPT) $(DIST_ARCHIVES)

%.tar.gz:
	test -f $@ || $(MAKE) distcheck distdir=$*
