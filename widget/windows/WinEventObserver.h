/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_WinEventObserver_h
#define widget_windows_WinEventObserver_h

#include <windef.h>

namespace mozilla::widget {

class WinEventWindow {
 public:
  // Create the hidden window. This window will persist for the lifetime of the
  // process: we do not destroy it, but neither does it keep the process alive.
  //
  // Must be called in the parent process and on the main thread. Crashes on
  // failure.
  static void Ensure();

  // (Do not call in real code.)
  static HWND GetHwndForTestingOnly();

 private:
  // the hidden window's WNDPROC
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

}  // namespace mozilla::widget

#endif  // widget_windows_WinEventObserver_h
