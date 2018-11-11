// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_INTERCEPTION_H_
#define SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_INTERCEPTION_H_

#include <windows.h>
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

extern "C" {

typedef BOOL (WINAPI* GdiDllInitializeFunction) (
    HANDLE dll,
    DWORD reason,
    LPVOID reserved);

typedef HGDIOBJ (WINAPI *GetStockObjectFunction) (int object);

typedef ATOM (WINAPI *RegisterClassWFunction) (const WNDCLASS* wnd_class);

// Interceptor for the  GdiDllInitialize function.
SANDBOX_INTERCEPT BOOL WINAPI TargetGdiDllInitialize(
    GdiDllInitializeFunction orig_gdi_dll_initialize,
    HANDLE dll,
    DWORD reason);

// Interceptor for the GetStockObject function.
SANDBOX_INTERCEPT HGDIOBJ WINAPI TargetGetStockObject(
    GetStockObjectFunction orig_get_stock_object,
    int object);

// Interceptor for the RegisterClassW function.
SANDBOX_INTERCEPT ATOM WINAPI TargetRegisterClassW(
    RegisterClassWFunction orig_register_class_function,
    const WNDCLASS* wnd_class);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_MITIGATIONS_WIN32K_INTERCEPTION_H_

