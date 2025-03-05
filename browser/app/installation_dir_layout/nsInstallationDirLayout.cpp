/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInstallationDirLayout.h"
#include "InstallationDirLayout.h"
#include "mozilla/Logging.h"
#ifdef XP_WIN
#  include <windows.h>
#endif

static mozilla::LazyLogModule sInstallDirLayoutLog("InstallDirLayout");
static const wchar_t* dllName = L"InstallationDirLayout.dll";

static InstallationDirLayoutType sLayoutType =
    InstallationDirLayoutType::Unknown;
using FuncType = InstallationDirLayoutType (*)();

namespace mozilla {

nsresult InitializeInstallationDirLayout() {
#ifdef XP_WIN
  HINSTANCE hRuntimeLibrary = LoadLibraryExW(dllName, nullptr, 0);
  if (!hRuntimeLibrary) {
    MOZ_LOG(sInstallDirLayoutLog, LogLevel::Error,
            ("Failed to open installation directory layout dll"));
    return NS_ERROR_FAILURE;
  }
  FuncType dirLayoutFunc =
      (FuncType)GetProcAddress(hRuntimeLibrary, "GetInstallationDirLayoutType");
  if (!dirLayoutFunc) {
    MOZ_LOG(sInstallDirLayoutLog, LogLevel::Error,
            ("GetInstallationDirLayoutType function not found in installation "
             "directory layout dll"));
    FreeLibrary(hRuntimeLibrary);
    return NS_ERROR_FAILURE;
  }
  sLayoutType = dirLayoutFunc();
  bool freeResult = FreeLibrary(hRuntimeLibrary);
  if (!freeResult) {
    MOZ_LOG(sInstallDirLayoutLog, LogLevel::Warning,
            ("FreeLibrary returned false"));
    // Not a fatal problem.
  }
  return NS_OK;
#else
  sLayoutType = InstallationDirLayoutType::Single;
  return NS_OK;
#endif
}

NS_IMPL_ISUPPORTS(InstallationDirLayout, nsIInstallationDirLayout)

NS_IMETHODIMP
InstallationDirLayout::GetIsInstallationLayoutVersioned(
    bool* installationDirIsVersioned) {
  bool isVersioned;
  switch (sLayoutType) {
    case InstallationDirLayoutType::Single:
      isVersioned = false;
      break;
    case InstallationDirLayoutType::Versioned:
      isVersioned = true;
      break;
    default:
      MOZ_LOG(sInstallDirLayoutLog, LogLevel::Error,
              ("Unexpected value for installation dir layout type: %d",
               (int)sLayoutType));
      return NS_ERROR_ILLEGAL_VALUE;
  }
  *installationDirIsVersioned = isVersioned;
  return NS_OK;
}

}  // namespace mozilla
