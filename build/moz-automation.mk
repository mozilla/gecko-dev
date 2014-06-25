#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifneq (,$(filter automation/%,$(MAKECMDGOALS)))
ifeq (4.0,$(firstword $(sort 4.0 $(MAKE_VERSION))))
MAKEFLAGS += --output-sync=target
else
.NOTPARALLEL:
endif
endif

include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

# Log file from the 'make upload' step. We need this to parse out the URLs of
# the uploaded files.
AUTOMATION_UPLOAD_OUTPUT = $(DIST)/automation-upload.txt

# Helper variables to convert from MOZ_AUTOMATION_* variables to the
# corresponding the make target
tier_BUILD_SYMBOLS = buildsymbols
tier_CHECK = check
tier_L10N_CHECK = l10n-check
tier_PACKAGE = package
tier_PACKAGE_TESTS = package-tests
tier_UPDATE_PACKAGING = update-packaging
tier_UPLOAD_SYMBOLS = uploadsymbols
tier_UPLOAD = upload

# Automation build steps. Everything in MOZ_AUTOMATION_TIERS also gets used in
# TIERS for mach display. As such, the MOZ_AUTOMATION_TIERS are roughly sorted
# here in the order that they will be executed (since mach doesn't know of the
# dependencies between them).
moz_automation_symbols = PACKAGE_TESTS BUILD_SYMBOLS UPLOAD_SYMBOLS PACKAGE UPDATE_PACKAGING CHECK L10N_CHECK UPLOAD
MOZ_AUTOMATION_TIERS := $(foreach sym,$(moz_automation_symbols),$(if $(filter 1,$(MOZ_AUTOMATION_$(sym))),$(tier_$(sym))))

# Dependencies between automation build steps
automation/uploadsymbols: automation/buildsymbols

automation/update-packaging: automation/package

automation/l10n-check: automation/package

automation/upload: automation/package
automation/upload: automation/package-tests
automation/upload: automation/buildsymbols
automation/upload: automation/update-packaging

# automation/package and automation/check should depend on build (which is
# implicit due to the way client.mk invokes automation/build), but buildsymbols
# changes the binaries/libs, and that's what we package/test.
automation/package: automation/buildsymbols
automation/check: automation/buildsymbols

automation/build: $(addprefix automation/,$(MOZ_AUTOMATION_TIERS))
	$(PYTHON) $(topsrcdir)/build/gen_mach_buildprops.py --complete-mar-file $(DIST)/$(COMPLETE_MAR) --upload-output $(AUTOMATION_UPLOAD_OUTPUT)

# make check runs with the keep-going flag so we can see all the failures
AUTOMATION_EXTRA_CMDLINE-check = -k

# We need the log from make upload to grep it for urls in order to set
# properties.
AUTOMATION_EXTRA_CMDLINE-upload = 2>&1 | tee $(AUTOMATION_UPLOAD_OUTPUT)

# The commands only run if the corresponding MOZ_AUTOMATION_* variable is
# enabled. This means, for example, if we enable MOZ_AUTOMATION_UPLOAD, then
# 'buildsymbols' will only run if MOZ_AUTOMATION_BUILD_SYMBOLS is also set.
# However, the target automation/buildsymbols will still be executed in this
# case because it is a prerequisite of automation/upload.
define automation_commands
$(call BUILDSTATUS,TIER_START $1)
@$(MAKE) $1 $(AUTOMATION_EXTRA_CMDLINE-$1)
$(call BUILDSTATUS,TIER_FINISH $1)
endef

automation/%:
	$(if $(filter $*,$(MOZ_AUTOMATION_TIERS)),$(call automation_commands,$*))
