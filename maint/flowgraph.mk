# SYNOPSIS
#   make flowgraph
#   make flowclean
# DESCRIPTION
#   Creates or cleans up flowgraphs in each source directory.
#   Flowgraphs are created using GNU cflow.
# PREREQUISITES
#   Main Makefile from the Mailutils top-level source directory must be
#   included atop of this one.
#
CFLOW_FLAGS=-i^s --brief --all\
 --define '__attribute__\(c\)'\
 --symbol __inline:=inline\
 --symbol __inline__:=inline\
 --symbol __const__:=const\
 --symbol __const:=const\
 --symbol __restrict:=restrict\
 --symbol __extension__:qualifier\
 --symbol __asm__:wrapper\
 --symbol __nonnull:wrapper\
 --symbol __wur:wrapper

FLOWCLEAN_FILES=

define flowgraph_tmpl
$(if $($(2)_OBJECTS),
FLOWCLEAN_FILES += $(1).cflow
$(2)_CFLOW_INPUT=$$($(2)_OBJECTS:.$(3)=.c)
$(1).cflow: $$($(2)_CFLOW_INPUT) Makefile
	$$(AM_V_GEN)cflow -o$$@ $$(CFLOW_FLAGS) $$(DEFS) \
		$$(DEFAULT_INCLUDES) $$(INCLUDES) $$(AM_CPPFLAGS) \
		$$(CPPFLAGS) \
	$$($(2)_CFLOW_INPUT)
,
$(1).cflow:;
)
endef

$(foreach prog,$(bin_PROGRAMS) $(sbin_PROGRAMS),\
$(eval $(call flowgraph_tmpl,$(prog),$(subst -,_,$(prog)),$(OBJEXT))))

$(foreach prog,$(lib_LTLIBRARIES),\
$(eval $(call flowgraph_tmpl,$(prog),$(subst .,_,$(prog)),lo)))

flowgraph-local: $(foreach prog,$(bin_PROGRAMS) $(sbin_PROGRAMS) $(lib_LTLIBRARIES),$(prog).cflow)

flowclean-local:
	-@test -n "$(FLOWCLEAN_FILES)" && rm -f $(FLOWCLEAN_FILES)

##

MAINT_MK = $(abspath $(firstword $(MAKEFILE_LIST)))
MAINT_INC = $(dir $(MAINT_MK))
LOCAL_MK = $(notdir $(firstword $(MAKEFILE_LIST)))

flowgraph flowclean:
	@$(MAKE) -f $(MAINT_MK) $@-recursive

flowgraph-recursive flowclean-recursive:
	failcom='exit 1';					\
	for f in x $$MAKEFLAGS; do				\
	  case $$f in						\
	    *=* | --[!k]*);;					\
	    *k*) failcom='fail=yes';;				\
	  esac;							\
	done;							\
	target=`echo $@ | sed s/-recursive/-local/`;		\
	list='$(SUBDIRS)'; for subdir in $$list; do		\
	  echo "Making $$target in $$subdir";			\
	  if test "$$subdir" = "."; then			\
            continue;						\
	  fi;							\
	  if test -f $$subdir/$(LOCAL_MK); then			\
	    makefile='$(LOCAL_MK)';				\
	  else							\
	    makefile="$(MAINT_MK)";				\
	  fi;							\
	  $(MAKE) -C $$subdir -f $$makefile -I $(MAINT_INC) $$target \
	     || eval $$failcom;					\
	done;							\
	test -z "$$fail"
