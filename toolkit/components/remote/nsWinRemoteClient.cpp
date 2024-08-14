/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinRemoteClient.h"
#include <windows.h>
#include "RemoteUtils.h"
#include "WinRemoteMessage.h"

using namespace mozilla;

nsresult nsWinRemoteClient::Init() { return NS_OK; }

nsresult nsWinRemoteClient::SendCommandLine(const char* aProgram,
                                            const char* aProfile, int32_t argc,
                                            const char** argv, bool aRaise) {
  nsString className;
  BuildClassName(aProgram, aProfile, className);

  HWND handle = ::FindWindowW(className.get(), 0);

  if (!handle) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  WCHAR cwd[MAX_PATH];
  _wgetcwd(cwd, MAX_PATH);
  WinRemoteMessageSender sender(argc, argv, nsDependentString(cwd));

  if (aRaise) {
    // Because we are the running process we have permission to raise the target
    // instance to the foreground. We can do so for the hidden message window as
    // we have its handle here. The target instance is then able to raise any
    // window it chooses to as part of handling the command line.
    ::SetForegroundWindow(handle);
  }
  ::SendMessageW(handle, WM_COPYDATA, 0,
                 reinterpret_cast<LPARAM>(sender.CopyData()));

  return NS_OK;
}
