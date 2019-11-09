# SYNOPSIS
#   make fullcheck
# DESCRIPTION
#   Runs make check for all possible default mailbox types.
# PREREQUISITES
#   Main Makefile from the Mailutils top-level source directory must be
#   included atop of this one.
#
FORMATS  = mbox dotmail mh maildir
DISTNAME = $(PACKAGE)-$(PACKAGE_VERSION)
FULLCHECKDIR = _fullcheck

fullcheck: $(foreach fmt,$(FORMATS),check-$(fmt))
	@rmdir $(FULLCHECKDIR)
	@text="$(DISTNAME) passed all tests";\
	echo $$text | sed -e 's/./=/g';\
	echo $$text;\
	echo $$text | sed -e 's/./=/g'

fullcheck_dist: $(DISTNAME).tar.gz

$(DISTNAME).tar.gz: ChangeLog
	make dist distdir=$(DISTNAME)

define fullcheckdir_tmpl
fullcheckdir-$(1):
	rm -rf $(FULLCHECKDIR)/$(1)
	mkdir -p $(FULLCHECKDIR)/$(1)
endef

define fullcheck_tmpl
check-$(fmt): fullcheck_dist fullcheckdir-$(fmt)
	cd $(FULLCHECKDIR)/$(fmt) && \
	tar xf ../../$(DISTNAME).tar.gz && \
	chmod -R a-w $(DISTNAME) && \
	mkdir _build && \
	cd _build && \
	../$(DISTNAME)/configure MU_DEFAULT_SCHEME=$(fmt) && \
	make check
	chmod -R u+w $(FULLCHECKDIR)/$(fmt)
	rm -rf $(FULLCHECKDIR)/$(fmt) 
endef

$(foreach fmt,$(FORMATS),$(eval $(call fullcheckdir_tmpl,$(fmt))))

$(foreach fmt,$(FORMATS),$(eval $(call fullcheck_tmpl,$(fmt))))



