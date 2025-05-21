# vim:set ts=8 sw=8 sts=8 noet:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Shared makefile that can be used to easily kick off l10n builds
# of Mozilla applications.
# This makefile should be included, and then assumes that the including
# makefile defines the following targets:
# l10n-%
#   This target should call into the various l10n targets that this
#   application depends on.
# installer-%
#   This target should list all required targets, a typical rule would be
#	installers-%: clobber-% langpack-% repackage-zip-%
#		@echo "repackaging done"
#   to initially clobber the locale staging area, and then to build the
#   language pack and zip package.
#   Other targets like windows installers might be listed, too, and should
#   be defined in the including makefile.
#   The installer-% targets should not set AB_CD, so that the unpackaging
#   step finds the original package.
# The including makefile should provide values for the variables
#   MOZ_APP_VERSION and MOZ_LANGPACK_EID.


run_for_effects := $(shell if test ! -d $(DIST); then $(NSINSTALL) -D $(DIST); fi)

# This makefile uses variable overrides from the l10n-% target to
# build non-default locales to non-default dist/ locations. Be aware!

LPROJ_ROOT = $(firstword $(subst -, ,$(AB_CD)))
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifeq (zh-TW,$(AB_CD))
LPROJ_ROOT := $(subst -,_,$(AB_CD))
endif
endif

# Allows overriding the final destination of the repackaged file
ZIP_OUT ?= $(ABS_DIST)/$(PACKAGE)

ACDEFINES += \
	-DAB_CD=$(AB_CD) \
	-DMOZ_LANGPACK_EID=$(MOZ_LANGPACK_EID) \
	-DMOZ_APP_ID='$(MOZ_APP_ID)' \
	-DMOZ_APP_VERSION=$(MOZ_APP_VERSION) \
	-DMOZ_APP_MAXVERSION=$(MOZ_APP_MAXVERSION) \
	-DLOCALE_SRCDIR=$(abspath $(LOCALE_SRCDIR)) \
	-DPKG_BASENAME='$(PKG_BASENAME)' \
	-DPKG_INST_BASENAME='$(PKG_INST_BASENAME)' \
	$(NULL)

# export some global defines for l10n repacks
BASE_MERGE:=$(CURDIR)/merge-dir
export REAL_LOCALE_MERGEDIR=$(BASE_MERGE)/$(AB_CD)
# is an l10n repack step:
export IS_LANGUAGE_REPACK
# is a language pack:
export IS_LANGPACK

clobber-%: AB_CD=$*
clobber-%:
	$(RM) -rf $(DIST)/xpi-stage/locale-$*


PACKAGER_NO_LIBS = 1

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
STAGEDIST = $(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)/$(_APPNAME)/Contents/Resources
else
STAGEDIST = $(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)
endif

include $(MOZILLA_DIR)/toolkit/mozapps/installer/packager.mk

unpack:
	$(RM) -r -f '$(ABS_DIST)/l10n-stage'
	$(PYTHON3) $(topsrcdir)/mach --log-no-times artifact install --unfiltered-project-package --distdir '$(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)' --verbose

# The path to the object dir for the mozilla-central build system,
# may be overridden if necessary.
MOZDEPTH ?= $(DEPTH)

repackage-zip:
	$(PYTHON3) $(MOZILLA_DIR)/toolkit/mozapps/installer/l10n-repack.py '$(STAGEDIST)' $(ABS_DIST)/xpi-stage/locale-$(AB_CD) \
		$(MOZ_PKG_EXTRAL10N) \
		$(if $(MOZ_PACKAGER_MINIFY),--minify) \
		$(if $(filter omni,$(MOZ_PACKAGER_FORMAT)),$(if $(NON_OMNIJAR_FILES),--non-resource $(NON_OMNIJAR_FILES)))

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifneq (en,$(LPROJ_ROOT))
	mv '$(STAGEDIST)'/en.lproj '$(STAGEDIST)'/$(LPROJ_ROOT).lproj
endif
endif
ifeq (WINNT,$(OS_ARCH))
	$(MAKE) -C ../installer/windows CONFIG_DIR=l10ngen l10ngen/helper.exe
	cp ../installer/windows/l10ngen/helper.exe $(STAGEDIST)/uninstall
endif

	$(NSINSTALL) -D $(DIST)/l10n-stage/$(PKG_PATH)
	$(call MAKE_PACKAGE,$(DIST)/l10n-stage)
# packaging done, undo l10n stuff
ifneq (en,$(LPROJ_ROOT))
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
	mv '$(STAGEDIST)'/$(LPROJ_ROOT).lproj '$(STAGEDIST)'/en.lproj
endif
endif
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	mv -f '$(DIST)/l10n-stage/$(PACKAGE)' '$(ZIP_OUT)'
	if test -f '$(DIST)/l10n-stage/$(PACKAGE).asc'; then mv -f '$(DIST)/l10n-stage/$(PACKAGE).asc' '$(ZIP_OUT).asc'; fi

repackage-zip-%: unpack
	@$(MAKE) repackage-zip AB_CD=$*

# Dealing with app sub dirs: If DIST_SUBDIRS is defined it contains a
# listing of app sub-dirs we should include in langpack xpis. If not,
# check DIST_SUBDIR, and if that isn't present, just package the default
# chrome directory and top-level localization for Fluent.
PKG_ZIP_DIRS = chrome localization $(or $(DIST_SUBDIRS),$(DIST_SUBDIR))
GIT ?= git

merge-%: IS_LANGUAGE_REPACK=1
merge-%: AB_CD=$*
merge-%:
	$(RM) -rf $(REAL_LOCALE_MERGEDIR)
	$(PYTHON3) -m moz.l10n.bin.build --config $(srcdir)/l10n.toml --base $(L10NBASEDIR) --target $(BASE_MERGE) --locales $(AB_CD)
# Hunspell dictionaries are interesting, as we don't ship the en-US
# dictionary in repacks. Thus we can't use the merge logic from
# compare-locales above, which would add en-US.dic and en-US.aff to
# the merge directory.
# Copy them to the merge dir, if exist. The repackaged app can still decide
# on whether to package them or not in `l10n-%` and `chrome-%`.
	if  test -d $(L10NBASEDIR)/$(AB_CD)/extensions/spellcheck ; then \
		$(NSINSTALL) -D $(REAL_LOCALE_MERGEDIR)/extensions/spellcheck/hunspell ; \
		cp $(L10NBASEDIR)/$(AB_CD)/extensions/spellcheck/hunspell/*.* $(REAL_LOCALE_MERGEDIR)/extensions/spellcheck/hunspell ; \
	fi

LANGPACK_METADATA = $(LOCALE_SRCDIR)/langpack-metadata.ftl

langpack-%: IS_LANGUAGE_REPACK=1
langpack-%: IS_LANGPACK=1
langpack-%: AB_CD=$*
langpack-%: clobber-%
	@echo 'Making langpack $(LANGPACK_FILE)'
	@$(MAKE) l10n-$(AB_CD)
	@$(MAKE) package-langpack-$(AB_CD)

package-langpack-%: LANGPACK_FILE=$(ABS_DIST)/$(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
package-langpack-%: XPI_NAME=locale-$*
package-langpack-%: AB_CD=$*
package-langpack-%:
	$(NSINSTALL) -D $(DIST)/$(PKG_LANGPACK_PATH)
	$(call py_action,langpack_manifest $(AB_CD),--locales $(AB_CD) --app-version $(MOZ_APP_VERSION) --max-app-ver $(MOZ_APP_MAXVERSION) --app-name '$(MOZ_APP_DISPLAYNAME)' --l10n-basedir '$(L10NBASEDIR)' --metadata $(LANGPACK_METADATA) --langpack-eid '$(MOZ_LANGPACK_EID)' --input $(DIST)/xpi-stage/locale-$(AB_CD))
	$(call py_action,zip $(PKG_LANGPACK_BASENAME).xpi,-C $(DIST)/xpi-stage/locale-$(AB_CD) -x **/*.manifest -x **/*.js -x **/*.ini $(LANGPACK_FILE) $(PKG_ZIP_DIRS) manifest.json)
