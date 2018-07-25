# Maintainer's make file for mailutils.
# 
ifneq (,$(wildcard Makefile))
 include Makefile
 include maint/flowgraph.mk
 include maint/release.mk
else
$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all):
	$(MAKE) -f maint/bootstrap.mk
	$(MAKE) $(MAKECMDGOALS)
endif
