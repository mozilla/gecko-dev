/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "LauncherProcessWin.h"

#include <io.h> // For printf_stderr
#include <string.h>

#include "mozilla/Attributes.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/SafeMode.h"
#include "mozilla/Sprintf.h" // For printf_stderr
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <processthreadsapi.h>

#include "DllBlocklistWin.h"
#include "LaunchUnelevated.h"
#include "ProcThreadAttributes.h"

/**
 * At this point the child process has been created in a suspended state. Any
 * additional startup work (eg, blocklist setup) should go here.
 *
 * @return true if browser startup should proceed, otherwise false.
 */
static bool
PostCreationSetup(HANDLE aChildProcess, HANDLE aChildMainThread,
                  const bool aIsSafeMode)
{
  // The launcher process's DLL blocking code is incompatible with ASAN because
  // it is able to execute before ASAN itself has even initialized.
  // Also, the AArch64 build doesn't yet have a working interceptor.
#if defined(MOZ_ASAN) || defined(_M_ARM64)
  return true;
#else
  return mozilla::InitializeDllBlocklistOOP(aChildProcess);
#endif // defined(MOZ_ASAN) || defined(_M_ARM64)
}

#if !defined(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)
# define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON (0x00000001ULL << 60)
#endif // !defined(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)

#if (_WIN32_WINNT < 0x0602)
BOOL WINAPI
SetProcessMitigationPolicy(PROCESS_MITIGATION_POLICY aMitigationPolicy,
                           PVOID aBuffer, SIZE_T aBufferLen);
#endif // (_WIN32_WINNT >= 0x0602)

/**
 * Any mitigation policies that should be set on the browser process should go
 * here.
 */
static void
SetMitigationPolicies(mozilla::ProcThreadAttributes& aAttrs, const bool aIsSafeMode)
{
  if (mozilla::IsWin10AnniversaryUpdateOrLater()) {
    aAttrs.AddMitigationPolicy(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON);
  }
}

static void
ShowError(DWORD aError = ::GetLastError())
{
  if (aError == ERROR_SUCCESS) {
    return;
  }

  LPWSTR rawMsgBuf = nullptr;
  DWORD result = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                  FORMAT_MESSAGE_FROM_SYSTEM |
                                  FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                  aError, 0, reinterpret_cast<LPWSTR>(&rawMsgBuf),
                                  0, nullptr);
  if (!result) {
    return;
  }

  ::MessageBoxW(nullptr, rawMsgBuf, L"Firefox", MB_OK | MB_ICONERROR);
  ::LocalFree(rawMsgBuf);
}

static mozilla::LauncherFlags
ProcessCmdLine(int& aArgc, wchar_t* aArgv[])
{
  mozilla::LauncherFlags result = mozilla::LauncherFlags::eNone;

  if (mozilla::CheckArg(aArgc, aArgv, L"wait-for-browser",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, L"marionette",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, L"headless",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::EnvHasValue("MOZ_AUTOMATION") ||
      mozilla::EnvHasValue("MOZ_HEADLESS")) {
    result |= mozilla::LauncherFlags::eWaitForBrowser;
  }

  if (mozilla::CheckArg(aArgc, aArgv, L"no-deelevate",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::CheckOSInt |
                        mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND) {
    result |= mozilla::LauncherFlags::eNoDeelevate;
  }

  return result;
}

// Duplicated from xpcom glue. Ideally this should be shared.
static void
printf_stderr(const char *fmt, ...)
{
  if (IsDebuggerPresent()) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    VsprintfLiteral(buf, fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
  }

  FILE *fp = _fdopen(_dup(2), "a");
  if (!fp)
      return;

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  fclose(fp);
}

static void
MaybeBreakForBrowserDebugging()
{
  if (mozilla::EnvHasValue("MOZ_DEBUG_BROWSER_PROCESS")) {
    ::DebugBreak();
    return;
  }

  const wchar_t* pauseLenS = _wgetenv(L"MOZ_DEBUG_BROWSER_PAUSE");
  if (!pauseLenS || !(*pauseLenS)) {
    return;
  }

  DWORD pauseLenMs = wcstoul(pauseLenS, nullptr, 10) * 1000;
  printf_stderr("\n\nBROWSERBROWSERBROWSERBROWSER\n  debug me @ %lu\n\n",
                ::GetCurrentProcessId());
  ::Sleep(pauseLenMs);
}

#if defined(MOZ_LAUNCHER_PROCESS)

static bool
IsSameBinaryAsParentProcess()
{
  mozilla::Maybe<DWORD> parentPid = mozilla::nt::GetParentProcessId();
  if (!parentPid) {
    // If NtQueryInformationProcess failed (in GetParentProcessId()),
    // we should not behave as the launcher process because it will also
    // likely to fail in child processes.
    MOZ_CRASH("NtQueryInformationProcess failed");
  }

  nsAutoHandle parentProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                           FALSE, parentPid.value()));
  if (!parentProcess.get()) {
    // If OpenProcess failed, the parent process may not be present,
    // may be already terminated, etc. So we will have to behave as the
    // launcher proces in this case.
    return false;
  }

  WCHAR parentExe[MAX_PATH + 1] = {};
  DWORD parentExeLen = mozilla::ArrayLength(parentExe);
  if (!::QueryFullProcessImageNameW(parentProcess.get(), PROCESS_NAME_NATIVE,
                                    parentExe, &parentExeLen)) {
    // If QueryFullProcessImageNameW failed, we should not behave as the
    // launcher process for the same reason as NtQueryInformationProcess.
    MOZ_CRASH("QueryFullProcessImageNameW failed");
  }

  WCHAR ourExe[MAX_PATH + 1] = {};
  DWORD ourExeOk = ::GetModuleFileNameW(nullptr, ourExe,
                                        mozilla::ArrayLength(ourExe));
  if (!ourExeOk || ourExeOk == mozilla::ArrayLength(ourExe)) {
    // If GetModuleFileNameW failed, we should not behave as the launcher
    // process for the same reason as NtQueryInformationProcess.
    MOZ_CRASH("GetModuleFileNameW failed");
  }

  mozilla::Maybe<bool> isSame =
    mozilla::DoPathsPointToIdenticalFile(parentExe, ourExe,
                                         mozilla::eNtPath);
  if (!isSame) {
    // If DoPathsPointToIdenticalFile failed, we should not behave as the
    // launcher process for the same reason as NtQueryInformationProcess.
    MOZ_CRASH("DoPathsPointToIdenticalFile failed");
  }
  return isSame.value();
}

#endif // defined(MOZ_LAUNCHER_PROCESS)

namespace mozilla {

bool
RunAsLauncherProcess(int& argc, wchar_t** argv)
{
  // NB: We run all tests in this function instead of returning early in order
  // to ensure that all side effects take place, such as clearing environment
  // variables.
  bool result = false;

#if defined(MOZ_LAUNCHER_PROCESS)
  result = !IsSameBinaryAsParentProcess();
#endif // defined(MOZ_LAUNCHER_PROCESS)

  if (mozilla::EnvHasValue("MOZ_LAUNCHER_PROCESS")) {
    mozilla::SaveToEnv("MOZ_LAUNCHER_PROCESS=");
    result = true;
  }

  result |= CheckArg(argc, argv, L"launcher",
                     static_cast<const wchar_t**>(nullptr),
                     CheckArgFlag::RemoveArg) == ARG_FOUND;

  if (!result) {
    // In this case, we will be proceeding to run as the browser.
    // We should check MOZ_DEBUG_BROWSER_* env vars.
    MaybeBreakForBrowserDebugging();
  }

  return result;
}

int
LauncherMain(int argc, wchar_t* argv[])
{
  // Make sure that the launcher process itself has image load policies set
  if (IsWin10AnniversaryUpdateOrLater()) {
    const DynamicallyLinkedFunctionPtr<decltype(&SetProcessMitigationPolicy)>
      pSetProcessMitigationPolicy(L"kernel32.dll", "SetProcessMitigationPolicy");
    if (pSetProcessMitigationPolicy) {
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY imgLoadPol = {};
      imgLoadPol.PreferSystem32Images = 1;

      DebugOnly<BOOL> setOk = pSetProcessMitigationPolicy(ProcessImageLoadPolicy,
                                                          &imgLoadPol,
                                                          sizeof(imgLoadPol));
      MOZ_ASSERT(setOk);
    }
  }

  if (!SetArgv0ToFullBinaryPath(argv)) {
    ShowError();
    return 1;
  }

  LauncherFlags flags = ProcessCmdLine(argc, argv);

  nsAutoHandle mediumIlToken;
  Maybe<ElevationState> elevationState = GetElevationState(flags, mediumIlToken);
  if (!elevationState) {
    return 1;
  }

  // If we're elevated, we should relaunch ourselves as a normal user.
  // Note that we only call LaunchUnelevated when we don't need to wait for the
  // browser process.
  if (elevationState.value() == ElevationState::eElevated &&
      !(flags & (LauncherFlags::eWaitForBrowser | LauncherFlags::eNoDeelevate)) &&
      !mediumIlToken.get()) {
    return !LaunchUnelevated(argc, argv);
  }

  // Now proceed with setting up the parameters for process creation
  UniquePtr<wchar_t[]> cmdLine(MakeCommandLine(argc, argv));
  if (!cmdLine) {
    return 1;
  }

  const Maybe<bool> isSafeMode = IsSafeModeRequested(argc, argv,
                                                     SafeModeFlag::NoKeyPressCheck);
  if (!isSafeMode) {
    ShowError(ERROR_INVALID_PARAMETER);
    return 1;
  }

  ProcThreadAttributes attrs;
  SetMitigationPolicies(attrs, isSafeMode.value());

  HANDLE stdHandles[] = {
    ::GetStdHandle(STD_INPUT_HANDLE),
    ::GetStdHandle(STD_OUTPUT_HANDLE),
    ::GetStdHandle(STD_ERROR_HANDLE)
  };

  attrs.AddInheritableHandles(stdHandles);

  DWORD creationFlags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;

  STARTUPINFOEXW siex;
  Maybe<bool> attrsOk = attrs.AssignTo(siex);
  if (!attrsOk) {
    ShowError();
    return 1;
  }

  BOOL inheritHandles = FALSE;

  if (attrsOk.value()) {
    creationFlags |= EXTENDED_STARTUPINFO_PRESENT;

    if (attrs.HasInheritableHandles()) {
      siex.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
      siex.StartupInfo.hStdInput = stdHandles[0];
      siex.StartupInfo.hStdOutput = stdHandles[1];
      siex.StartupInfo.hStdError = stdHandles[2];

      // Since attrsOk == true, we have successfully set the handle inheritance
      // whitelist policy, so only the handles added to attrs will be inherited.
      inheritHandles = TRUE;
    }
  }

  PROCESS_INFORMATION pi = {};
  BOOL createOk;

  if (mediumIlToken.get()) {
    createOk = ::CreateProcessAsUserW(mediumIlToken.get(), argv[0], cmdLine.get(),
                                      nullptr, nullptr, inheritHandles,
                                      creationFlags, nullptr, nullptr,
                                      &siex.StartupInfo, &pi);
  } else {
    createOk = ::CreateProcessW(argv[0], cmdLine.get(), nullptr, nullptr,
                                inheritHandles, creationFlags, nullptr, nullptr,
                                &siex.StartupInfo, &pi);
  }

  if (!createOk) {
    ShowError();
    return 1;
  }

  nsAutoHandle process(pi.hProcess);
  nsAutoHandle mainThread(pi.hThread);

  if (!PostCreationSetup(process.get(), mainThread.get(), isSafeMode.value()) ||
      ::ResumeThread(mainThread.get()) == static_cast<DWORD>(-1)) {
    ShowError();
    ::TerminateProcess(process.get(), 1);
    return 1;
  }

  if (flags & LauncherFlags::eWaitForBrowser) {
    DWORD exitCode;
    if (::WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0 &&
        ::GetExitCodeProcess(process.get(), &exitCode)) {
      // Propagate the browser process's exit code as our exit code.
      return static_cast<int>(exitCode);
    }
  } else {
    const DWORD timeout = ::IsDebuggerPresent() ? INFINITE :
                          kWaitForInputIdleTimeoutMS;

    // Keep the current process around until the callback process has created
    // its message queue, to avoid the launched process's windows being forced
    // into the background.
    mozilla::WaitForInputIdle(process.get(), timeout);
  }

  return 0;
}

} // namespace mozilla
