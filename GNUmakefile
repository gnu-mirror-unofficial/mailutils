# Maintainer's make file for mailutils.
# 
ifneq (,$(wildcard Makefile))
 include Makefile
 include maint/flowgraph.mk
 include maint/release.mk
 include maint/fullcheck.mk
else
$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all):
	$(MAKE) -f maint/bootstrap.mk
	$(MAKE) $(MAKECMDGOALS)
endif
