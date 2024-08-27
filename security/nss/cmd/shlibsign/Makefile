#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################

#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

include ../platlibs.mk

# sign any and all shared libraries that contain the word freebl
ifeq ($(NSS_BUILD_WITHOUT_SOFTOKEN),1)
CHECKLIBS =
CHECKLOC =
else
CHECKLIBS = $(DIST)/lib/$(DLL_PREFIX)softokn3.$(DLL_SUFFIX)
CHECKLIBS += $(wildcard $(DIST)/lib/$(DLL_PREFIX)freebl*3.$(DLL_SUFFIX))
ifndef NSS_DISABLE_DBM
CHECKLIBS += $(DIST)/lib/$(DLL_PREFIX)nssdbm3.$(DLL_SUFFIX)
endif
CHECKLOC = $(CHECKLIBS:.$(DLL_SUFFIX)=.chk)

MD_LIB_RELEASE_FILES = $(CHECKLOC)
ALL_TRASH += $(CHECKLOC)
endif

#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################


#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################


include ../platrules.mk

SRCDIR = $(call core_abspath,.)

%.chk: %.$(DLL_SUFFIX) 
    ifeq ($(CROSS_COMPILE),1)
	# do nothing
    else
	cd $(OBJDIR) ; sh $(SRCDIR)/sign.sh $(call core_abspath,$(DIST)) \
	$(call core_abspath,$(OBJDIR)) $(OS_TARGET) \
	$(call core_abspath,$(NSPR_LIB_DIR)) $(call core_abspath,$<)
    endif


libs: install
ifdef CHECKLOC
	$(MAKE) $(CHECKLOC)
endif
