/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoArgs_h
#define mozilla_GeckoArgs_h

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtrExtensions.h"

#include <array>
#include <cctype>
#include <climits>
#include <string>
#include <vector>

namespace mozilla {

namespace geckoargs {

// Type used for passing arguments to a content process, including OS files.
struct ChildProcessArgs {
  std::vector<std::string> mArgs;
  std::vector<UniqueFileHandle> mFiles;
};

template <typename T>
struct CommandLineArg {
  Maybe<T> Get(int& aArgc, char** aArgv,
               const CheckArgFlag aFlags = CheckArgFlag::RemoveArg) {
    return GetCommon(sMatch, aArgc, aArgv, aFlags);
  }
  static Maybe<T> GetCommon(const char* aMatch, int& aArgc, char** aArgv,
                            const CheckArgFlag aFlags);

  const char* Name() { return sName; };

  void Put(T aValue, ChildProcessArgs& aArgs) {
    return PutCommon(sName, std::move(aValue), aArgs);
  }
  static void PutCommon(const char* aName, T aValue, ChildProcessArgs& aArgs);

  const char* sName;
  const char* sMatch;
};

/// Get()

template <>
inline Maybe<const char*> CommandLineArg<const char*>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  MOZ_ASSERT(aArgv, "aArgv must be initialized before CheckArg()");
  const char* rv = nullptr;
  if (ARG_FOUND == CheckArg(aArgc, aArgv, aMatch, &rv, aFlags)) {
    return Some(rv);
  }
  return Nothing();
}

template <>
inline Maybe<bool> CommandLineArg<bool>::GetCommon(const char* aMatch,
                                                   int& aArgc, char** aArgv,
                                                   const CheckArgFlag aFlags) {
  MOZ_ASSERT(aArgv, "aArgv must be initialized before CheckArg()");
  if (ARG_FOUND ==
      CheckArg(aArgc, aArgv, aMatch, (const char**)nullptr, aFlags)) {
    return Some(true);
  }
  return Nothing();
}

template <>
inline Maybe<uint64_t> CommandLineArg<uint64_t>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  if (Maybe<const char*> arg = CommandLineArg<const char*>::GetCommon(
          aMatch, aArgc, aArgv, aFlags)) {
    errno = 0;
    char* endptr = nullptr;
    uint64_t conv = std::strtoull(*arg, &endptr, 10);
    if (errno == 0 && endptr && *endptr == '\0') {
      return Some(conv);
    }
  }
  return Nothing();
}

template <>
inline Maybe<uint32_t> CommandLineArg<uint32_t>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  return CommandLineArg<uint64_t>::GetCommon(aMatch, aArgc, aArgv, aFlags);
}

template <>
Maybe<UniqueFileHandle> CommandLineArg<UniqueFileHandle>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags);

/// Put()

template <>
inline void CommandLineArg<const char*>::PutCommon(const char* aName,
                                                   const char* aValue,
                                                   ChildProcessArgs& aArgs) {
  aArgs.mArgs.push_back(aName);
  aArgs.mArgs.push_back(aValue);
}

template <>
inline void CommandLineArg<bool>::PutCommon(const char* aName, bool aValue,
                                            ChildProcessArgs& aArgs) {
  if (aValue) {
    aArgs.mArgs.push_back(aName);
  }
}

template <>
inline void CommandLineArg<uint64_t>::PutCommon(const char* aName,
                                                uint64_t aValue,
                                                ChildProcessArgs& aArgs) {
  aArgs.mArgs.push_back(aName);
  aArgs.mArgs.push_back(std::to_string(aValue));
}

template <>
inline void CommandLineArg<uint32_t>::PutCommon(const char* aName,
                                                uint32_t aValue,
                                                ChildProcessArgs& aArgs) {
  CommandLineArg<uint64_t>::PutCommon(aName, aValue, aArgs);
}

template <>
void CommandLineArg<UniqueFileHandle>::PutCommon(const char* aName,
                                                 UniqueFileHandle aValue,
                                                 ChildProcessArgs& aArgs);

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static CommandLineArg<uint64_t> sParentPid{"-parentPid", "parentpid"};
static CommandLineArg<const char*> sInitialChannelID{"-initialChannelId",
                                                     "initialchannelid"};
static CommandLineArg<const char*> sParentBuildID{"-parentBuildID",
                                                  "parentbuildid"};
static CommandLineArg<const char*> sAppDir{"-appDir", "appdir"};
static CommandLineArg<const char*> sGREOmni{"-greomni", "greomni"};
static CommandLineArg<const char*> sAppOmni{"-appomni", "appomni"};
static CommandLineArg<const char*> sProfile{"-profile", "profile"};

static CommandLineArg<UniqueFileHandle> sIPCHandle{"-ipcHandle", "ipchandle"};

static CommandLineArg<UniqueFileHandle> sJsInitHandle{"-jsInitHandle",
                                                      "jsinithandle"};
static CommandLineArg<uint64_t> sJsInitLen{"-jsInitLen", "jsinitlen"};
static CommandLineArg<UniqueFileHandle> sPrefsHandle{"-prefsHandle",
                                                     "prefshandle"};
static CommandLineArg<uint64_t> sPrefsLen{"-prefsLen", "prefslen"};
static CommandLineArg<UniqueFileHandle> sPrefMapHandle{"-prefMapHandle",
                                                       "prefmaphandle"};
static CommandLineArg<uint64_t> sPrefMapSize{"-prefMapSize", "prefmapsize"};

static CommandLineArg<uint64_t> sSandboxingKind{"-sandboxingKind",
                                                "sandboxingkind"};

static CommandLineArg<bool> sSafeMode{"-safeMode", "safemode"};

static CommandLineArg<bool> sIsForBrowser{"-isForBrowser", "isforbrowser"};
static CommandLineArg<bool> sNotForBrowser{"-notForBrowser", "notforbrowser"};

static CommandLineArg<const char*> sPluginPath{"-pluginPath", "pluginpath"};
static CommandLineArg<bool> sPluginNativeEvent{"-pluginNativeEvent",
                                               "pluginnativeevent"};

#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA)
static CommandLineArg<const char*> sCrashReporter{"-crashReporter",
                                                  "crashreporter"};
#elif defined(XP_UNIX)
static CommandLineArg<UniqueFileHandle> sCrashReporter{"-crashReporter",
                                                       "crashreporter"};
#endif

#if defined(XP_WIN)
#  if defined(MOZ_SANDBOX)
static CommandLineArg<bool> sWin32kLockedDown{"-win32kLockedDown",
                                              "win32klockeddown"};
#  endif  // defined(MOZ_SANDBOX)
static CommandLineArg<bool> sDisableDynamicDllBlocklist{
    "-disableDynamicBlocklist", "disabledynamicblocklist"};
#endif  // defined(XP_WIN)

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

}  // namespace geckoargs

}  // namespace mozilla

#endif  // mozilla_GeckoArgs_h
