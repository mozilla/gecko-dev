// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"

#if defined(MOZ_ZUCCHINI)
#include <windows.h>

#include "base/check.h"
#else
#include "base/win/scoped_handle_verifier.h"
#include "base/win/windows_types.h"
#endif  // defined(MOZ_ZUCCHINI)

namespace base {
namespace win {

#if !defined(MOZ_ZUCCHINI)
using base::win::internal::ScopedHandleVerifier;
#endif  // !defined(MOZ_ZUCCHINI)

std::ostream& operator<<(std::ostream& os, HandleOperation operation) {
  switch (operation) {
    case HandleOperation::kHandleAlreadyTracked:
      return os << "Handle Already Tracked";
    case HandleOperation::kCloseHandleNotTracked:
      return os << "Closing an untracked handle";
    case HandleOperation::kCloseHandleNotOwner:
      return os << "Closing a handle owned by something else";
    case HandleOperation::kCloseHandleHook:
      return os << "CloseHandleHook validation failure";
    case HandleOperation::kDuplicateHandleHook:
      return os << "DuplicateHandleHook validation failure";
  }
}

// Static.
bool HandleTraits::CloseHandle(HANDLE handle) {
#if defined(MOZ_ZUCCHINI)
  // Taken from scoped_handle_verifier.cc
  if (!::CloseHandle(handle))
    CHECK(false) << "CloseHandle failed";
  return true;
#else
  return ScopedHandleVerifier::Get()->CloseHandle(handle);
#endif  // defined(MOZ_ZUCCHINI)
}

#if !defined(MOZ_ZUCCHINI)
// Static.
void VerifierTraits::StartTracking(HANDLE handle,
                                   const void* owner,
                                   const void* pc1,
                                   const void* pc2) {
  return ScopedHandleVerifier::Get()->StartTracking(handle, owner, pc1, pc2);
}

// Static.
void VerifierTraits::StopTracking(HANDLE handle,
                                  const void* owner,
                                  const void* pc1,
                                  const void* pc2) {
  return ScopedHandleVerifier::Get()->StopTracking(handle, owner, pc1, pc2);
}

void DisableHandleVerifier() {
  return ScopedHandleVerifier::Get()->Disable();
}

void OnHandleBeingClosed(HANDLE handle, HandleOperation operation) {
  return ScopedHandleVerifier::Get()->OnHandleBeingClosed(handle, operation);
}
#endif  // !defined(MOZ_ZUCCHINI)

}  // namespace win
}  // namespace base
