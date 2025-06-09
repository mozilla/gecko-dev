# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef MOZ_PKG_FORMAT
    ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
        MOZ_PKG_FORMAT = DMG
    else ifeq ($(OS_ARCH),WINNT)
        MOZ_PKG_FORMAT = ZIP
    else ifeq ($(OS_ARCH),SunOS)
        MOZ_PKG_FORMAT = XZ
    else ifeq ($(MOZ_WIDGET_TOOLKIT),gtk)
        MOZ_PKG_FORMAT = XZ
    else ifeq ($(MOZ_WIDGET_TOOLKIT),android)
        MOZ_PKG_FORMAT = APK
    else
        MOZ_PKG_FORMAT = TGZ
    endif
endif # MOZ_PKG_FORMAT

ifeq ($(OS_ARCH),WINNT)
INSTALLER_DIR   = windows
endif

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifndef _APPNAME
_APPNAME = $(MOZ_MACBUNDLE_NAME)
endif
ifndef _BINPATH
_BINPATH = $(_APPNAME)/Contents/MacOS
endif # _BINPATH
ifndef _RESPATH
# Resource path for the precomplete file
_RESPATH = $(_APPNAME)/Contents/Resources
endif
endif

PACKAGE       = $(PKG_PATH)$(PKG_BASENAME)$(PKG_SUFFIX)

# JavaScript Shell packaging
JSSHELL_BINS  = \
  js$(BIN_SUFFIX) \
  $(DLL_PREFIX)mozglue$(DLL_SUFFIX) \
  $(NULL)

ifndef MOZ_SYSTEM_NSPR
  ifdef MOZ_FOLD_LIBS
    JSSHELL_BINS += $(DLL_PREFIX)nss3$(DLL_SUFFIX)
  else
    JSSHELL_BINS += \
      $(DLL_PREFIX)nspr4$(DLL_SUFFIX) \
      $(DLL_PREFIX)plds4$(DLL_SUFFIX) \
      $(DLL_PREFIX)plc4$(DLL_SUFFIX) \
      $(NULL)
  endif # MOZ_FOLD_LIBS
endif # MOZ_SYSTEM_NSPR

ifdef MSVC_C_RUNTIME_DLL
  JSSHELL_BINS += $(MSVC_C_RUNTIME_DLL)
endif
ifdef MSVC_C_RUNTIME_1_DLL
  JSSHELL_BINS += $(MSVC_C_RUNTIME_1_DLL)
endif
ifdef MSVC_CXX_RUNTIME_DLL
  JSSHELL_BINS += $(MSVC_CXX_RUNTIME_DLL)
endif

ifdef LLVM_SYMBOLIZER
  JSSHELL_BINS += $(notdir $(LLVM_SYMBOLIZER))
endif
ifdef MOZ_CLANG_RT_ASAN_LIB_PATH
  JSSHELL_BINS += $(notdir $(MOZ_CLANG_RT_ASAN_LIB_PATH))
endif

ifdef FUZZING_INTERFACES
  JSSHELL_BINS += fuzz-tests$(BIN_SUFFIX)
endif

MAKE_JSSHELL  = $(call py_action,zip $(JSSHELL_NAME),-C $(DIST)/bin --strip $(abspath $(PKG_JSSHELL)) $(JSSHELL_BINS))

ifneq (,$(PGO_JARLOG_PATH))
  # The backslash subst is to work around an issue with our version of mozmake,
  # where backslashes get slurped in command-line arguments if a command is run
  # with a double-quote character. The command to packager.py happens to be one
  # of these commands, where double-quotes appear in certain ACDEFINES values.
  # This turns a jarlog path like "Z:\task..." into "Z:task", which fails.
  # Switching the backslashes for forward slashes works around the issue.
  JARLOG_FILE_AB_CD = $(subst \,/,$(PGO_JARLOG_PATH))
else
  JARLOG_FILE_AB_CD = $(topobjdir)/jarlog/$(AB_CD).log
endif

TAR_CREATE_FLAGS := --exclude=.mkdir.done $(TAR_CREATE_FLAGS)
CREATE_FINAL_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode=go-w --exclude=.mkdir.done -f

ifeq ($(MOZ_PKG_FORMAT),TAR)
  PKG_SUFFIX	= .tar
  INNER_MAKE_PACKAGE 	= cd $(1) && $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) > $(PACKAGE)
endif

ifeq ($(MOZ_PKG_FORMAT),TGZ)
  PKG_SUFFIX	= .tar.gz
  INNER_MAKE_PACKAGE 	= cd $(1) && $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | gzip -vf9 > $(PACKAGE)
endif

ifeq ($(MOZ_PKG_FORMAT),XZ)
  PKG_SUFFIX = .tar.xz
  INNER_MAKE_PACKAGE 	= cd $(1) && $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | xz --compress --stdout -9 --extreme > $(PACKAGE)
endif

ifeq ($(MOZ_PKG_FORMAT),BZ2)
  PKG_SUFFIX	= .tar.bz2
  ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
    INNER_MAKE_PACKAGE 	= cd $(1) && $(CREATE_FINAL_TAR) - -C $(MOZ_PKG_DIR) $(_APPNAME) | bzip2 -vf > $(PACKAGE)
  else
    INNER_MAKE_PACKAGE 	= cd $(1) && $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | bzip2 -vf > $(PACKAGE)
  endif
endif

ifeq ($(MOZ_PKG_FORMAT),ZIP)
  PKG_SUFFIX	= .zip
  INNER_MAKE_PACKAGE = $(call py_action,zip,'$(PACKAGE)' '$(MOZ_PKG_DIR)' -x '**/.mkdir.done',$(1))
endif

ifeq ($(MOZ_PKG_FORMAT),APK)
INNER_MAKE_PACKAGE = true
endif

ifeq ($(MOZ_PKG_FORMAT),DMG)
  PKG_SUFFIX	= .dmg

  _ABS_MOZSRCDIR = $(shell cd $(MOZILLA_DIR) && pwd)
  PKG_DMG_SOURCE = $(MOZ_PKG_DIR)
  MOZ_PKG_MAC_DSSTORE=$(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)/dsstore
  MOZ_PKG_MAC_BACKGROUND=$(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)/background.png
  MOZ_PKG_MAC_ICON=$(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)/disk.icns
  INNER_MAKE_PACKAGE = \
    $(call py_action,make_dmg, \
        $(if $(MOZ_PKG_MAC_DSSTORE),--dsstore '$(MOZ_PKG_MAC_DSSTORE)') \
        $(if $(MOZ_PKG_MAC_BACKGROUND),--background '$(MOZ_PKG_MAC_BACKGROUND)') \
        $(if $(MOZ_PKG_MAC_ICON),--icon '$(MOZ_PKG_MAC_ICON)') \
        --volume-name '$(MOZ_APP_DISPLAYNAME)' \
        '$(PKG_DMG_SOURCE)' '$(PACKAGE)', \
        $(1))
endif

MAKE_PACKAGE = $(INNER_MAKE_PACKAGE)

NO_PKG_FILES += \
	core \
	bsdecho \
	js \
	js-config \
	jscpucfg \
	nsinstall \
	viewer \
	TestGtkEmbed \
	elf-dynstr-gc \
	mangle* \
	maptsv* \
	mfc* \
	msdump* \
	msmap* \
	nm2tsv* \
	nsinstall* \
	res/samples \
	res/throbber \
	shlibsign* \
	certutil* \
	pk12util* \
	BadCertAndPinningServer* \
	DelegatedCredentialsServer* \
	EncryptedClientHelloServer* \
	FaultyServer* \
	OCSPStaplingServer* \
	SanctionsTestServer* \
	GenerateOCSPResponse* \
	chrome/chrome.rdf \
	chrome/app-chrome.manifest \
	chrome/overlayinfo \
	components/compreg.dat \
	components/xpti.dat \
	content_unit_tests \
	necko_unit_tests \
	*.dSYM \
	$(NULL)

# If a manifest has not been supplied, the following
# files should be excluded from the package too
ifndef MOZ_PKG_MANIFEST
  NO_PKG_FILES += ssltunnel*
endif

ifdef MOZ_DMD
  NO_PKG_FILES += SmokeDMD
endif

DEFINES += -DDLL_PREFIX=$(DLL_PREFIX) -DDLL_SUFFIX=$(DLL_SUFFIX) -DBIN_SUFFIX=$(BIN_SUFFIX)

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
  DEFINES += -DDIR_MACOS=Contents/MacOS/ -DDIR_RESOURCES=Contents/Resources/
else
  DEFINES += -DDIR_MACOS= -DDIR_RESOURCES=
endif

ifdef MOZ_FOLD_LIBS
  DEFINES += -DMOZ_FOLD_LIBS=1
endif

# The following target stages files into two directories: one directory for
# core files, and one for optional extensions based on the information in
# the MOZ_PKG_MANIFEST file.

PKG_ARG = , '$(pkg)'

ifndef MOZ_PACKAGER_FORMAT
  MOZ_PACKAGER_FORMAT = $(error MOZ_PACKAGER_FORMAT is not set)
endif

ifneq (android,$(MOZ_WIDGET_TOOLKIT))
  JAR_COMPRESSION ?= none
endif

ifeq ($(OS_TARGET), WINNT)
  INSTALLER_PACKAGE = $(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe
endif

# These are necessary because some of our packages/installers contain spaces
# in their filenames and GNU Make's $(wildcard) function doesn't properly
# deal with them.
empty :=
space = $(empty) $(empty)
QUOTED_WILDCARD = $(if $(wildcard $(subst $(space),?,$(1))),'$(1)')
ESCAPE_SPACE = $(subst $(space),\$(space),$(1))
ESCAPE_WILDCARD = $(subst $(space),?,$(1))

# This variable defines which OpenSSL algorithm to use to
# generate checksums for files that we upload
CHECKSUM_ALGORITHM_PARAM = -d sha512 -d md5 -d sha1

# This variable defines where the checksum file will be located
CHECKSUM_FILE = '$(DIST)/$(PKG_PATH)/$(CHECKSUMS_FILE_BASENAME).checksums'
CHECKSUM_FILES = $(CHECKSUM_FILE)

# Upload MAR tools only if AB_CD is unset or en_US
ifeq (,$(AB_CD:en-US=))
  ifeq (WINNT,$(OS_TARGET))
    UPLOAD_EXTRA_FILES += host/bin/mar.exe
    UPLOAD_EXTRA_FILES += host/bin/mbsdiff.exe
  else
    UPLOAD_EXTRA_FILES += host/bin/mar
    UPLOAD_EXTRA_FILES += host/bin/mbsdiff
  endif
endif

UPLOAD_FILES= \
  $(call QUOTED_WILDCARD,$(DIST)/$(PACKAGE)) \
  $(call QUOTED_WILDCARD,$(INSTALLER_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(LANGPACK)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOZHARNESS_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(SYMBOL_ARCHIVE_BASENAME).zip) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(GENERATED_SOURCE_FILE_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(MOZ_SOURCESTAMP_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_BUILDINFO_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_BUILDHUB_JSON)) \
  $(call QUOTED_WILDCARD,$(MOZ_BUILDID_INFO_TXT_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_MOZINFO_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_TEST_PACKAGES_FILE)) \
  $(call QUOTED_WILDCARD,$(PKG_JSSHELL)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(SYMBOL_FULL_ARCHIVE_BASENAME).tar.zst) \
  $(call QUOTED_WILDCARD,$(topobjdir)/$(MOZ_BUILD_APP)/installer/windows/instgen/setup.exe) \
  $(call QUOTED_WILDCARD,$(topobjdir)/$(MOZ_BUILD_APP)/installer/windows/instgen/setup-stub.exe) \
  $(call QUOTED_WILDCARD,$(topsrcdir)/toolchains.json) \
  $(call QUOTED_WILDCARD,$(topobjdir)/config.status) \
  $(if $(UPLOAD_EXTRA_FILES), $(foreach f, $(UPLOAD_EXTRA_FILES), $(wildcard $(DIST)/$(f))))

ifneq ($(filter-out en-US,$(AB_CD)),)
  UPLOAD_FILES += \
    $(call QUOTED_WILDCARD,$(topobjdir)/$(MOZ_BUILD_APP)/installer/windows/l10ngen/setup.exe) \
    $(call QUOTED_WILDCARD,$(topobjdir)/$(MOZ_BUILD_APP)/installer/windows/l10ngen/setup-stub.exe)
endif

ifdef MOZ_CODE_COVERAGE
  UPLOAD_FILES += \
    $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(CODE_COVERAGE_ARCHIVE_BASENAME).zip) \
    $(call QUOTED_WILDCARD,$(topobjdir)/chrome-map.json) \
    $(NULL)
endif


ifdef ENABLE_MOZSEARCH_PLUGIN
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(topobjdir)/chrome-map.json)
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOZSEARCH_ARCHIVE_BASENAME).zip)
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOZSEARCH_SCIP_INDEX_BASENAME).zip)
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOZSEARCH_INCLUDEMAP_BASENAME).map)
ifeq ($(MOZ_BUILD_APP),mobile/android)
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(MOZSEARCH_JAVA_INDEX_BASENAME).zip)
endif
endif

ifdef MOZ_STUB_INSTALLER
  UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_INST_PATH)$(PKG_STUB_BASENAME).exe)
endif

# Upload `.xpt` artifacts for use in artifact builds.
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(XPT_ARTIFACTS_ARCHIVE_BASENAME).zip)
# Upload update-related macOS framework artifacts for use in artifact builds.
ifeq ($(OS_ARCH),Darwin)
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(UPDATE_FRAMEWORK_ARTIFACTS_ARCHIVE_BASENAME).zip)
endif # Darwin

ifndef MOZ_PKG_SRCDIR
  MOZ_PKG_SRCDIR = $(topsrcdir)
endif

SRC_TAR_PREFIX = $(MOZ_APP_NAME)-$(MOZ_PKG_VERSION)
SRC_TAR_EXCLUDE_PATHS += \
  --exclude='.hg*' \
  --exclude='.git' \
  --exclude='.gitattributes' \
  --exclude='.gitkeep' \
  --exclude='.gitmodules' \
  --exclude='CVS' \
  --exclude='.cvs*' \
  --exclude='.mozconfig*' \
  --exclude='*.pyc' \
  --exclude='$(MOZILLA_DIR)/Makefile' \
  --exclude='$(MOZILLA_DIR)/dist'
ifdef MOZ_OBJDIR
  SRC_TAR_EXCLUDE_PATHS += --exclude='$(MOZ_OBJDIR)'
endif
CREATE_SOURCE_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode=go-w $(SRC_TAR_EXCLUDE_PATHS) --transform='s,^\./,$(SRC_TAR_PREFIX)/,' -f

SOURCE_TAR = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).tar.xz
HG_BUNDLE_FILE = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_BUNDLE_BASENAME).bundle
SOURCE_CHECKSUM_FILE = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).checksums
SOURCE_UPLOAD_FILES = $(SOURCE_TAR)

HG ?= hg
CREATE_HG_BUNDLE_CMD  = $(HG) -v -R $(topsrcdir) bundle --base null
ifdef HG_BUNDLE_REVISION
  CREATE_HG_BUNDLE_CMD += -r $(HG_BUNDLE_REVISION)
endif
CREATE_HG_BUNDLE_CMD += $(HG_BUNDLE_FILE)
ifdef UPLOAD_HG_BUNDLE
  SOURCE_UPLOAD_FILES  += $(HG_BUNDLE_FILE)
endif
