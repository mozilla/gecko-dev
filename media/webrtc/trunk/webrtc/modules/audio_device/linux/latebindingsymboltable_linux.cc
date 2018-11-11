/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/linux/latebindingsymboltable_linux.h"

#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
#include <dlfcn.h>
#endif

// TODO(grunell): Either put inside webrtc namespace or use webrtc:: instead.
using namespace webrtc;

namespace webrtc_adm_linux {

inline static const char *GetDllError() {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  const char *err = dlerror();
  if (err) {
    return err;
  } else {
    return "No error";
  }
#else
#error Not implemented
#endif
}

DllHandle InternalLoadDll(const char dll_name[]) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  DllHandle handle = dlopen(dll_name, RTLD_NOW);
#else
#error Not implemented
#endif
  if (handle == kInvalidDllHandle) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
               "Can't load %s : %s", dll_name, GetDllError());
  }
  return handle;
}

void InternalUnloadDll(DllHandle handle) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
// TODO(pbos): Remove this dlclose() exclusion when leaks and suppressions from
// here are gone (or AddressSanitizer can display them properly).
//
// Skip dlclose() on AddressSanitizer as leaks including this module in the
// stack trace gets displayed as <unknown module> instead of the actual library
// -> it can not be suppressed.
// https://code.google.com/p/address-sanitizer/issues/detail?id=89
#if !defined(ADDRESS_SANITIZER)
  if (dlclose(handle) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
               "%s", GetDllError());
  }
#endif  // !defined(ADDRESS_SANITIZER)
#else
#error Not implemented
#endif
}

static bool LoadSymbol(DllHandle handle,
                       const char *symbol_name,
                       void **symbol) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  *symbol = dlsym(handle, symbol_name);
  const char *err = dlerror();
  if (err) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
               "Error loading symbol %s : %d", symbol_name, err);
    return false;
  } else if (!*symbol) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
               "Symbol %s is NULL", symbol_name);
    return false;
  }
  return true;
#else
#error Not implemented
#endif
}

// This routine MUST assign SOME value for every symbol, even if that value is
// NULL, or else some symbols may be left with uninitialized data that the
// caller may later interpret as a valid address.
bool InternalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char *const symbol_names[],
                         void *symbols[]) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  // Clear any old errors.
  dlerror();
#endif
  for (int i = 0; i < num_symbols; ++i) {
    if (!LoadSymbol(handle, symbol_names[i], &symbols[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace webrtc_adm_linux
