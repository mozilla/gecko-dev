// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SIDESTEP_RESOLVER_H__
#define SANDBOX_SRC_SIDESTEP_RESOLVER_H__

#include "base/basictypes.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/resolver.h"

namespace sandbox {

// This is the concrete resolver used to perform sidestep interceptions.
class SidestepResolverThunk : public ResolverThunk {
 public:
  SidestepResolverThunk() {}
  virtual ~SidestepResolverThunk() {}

  // Implementation of Resolver::Setup.
  virtual NTSTATUS Setup(const void* target_module,
                         const void* interceptor_module,
                         const char* target_name,
                         const char* interceptor_name,
                         const void* interceptor_entry_point,
                         void* thunk_storage,
                         size_t storage_bytes,
                         size_t* storage_used);

  // Implementation of Resolver::GetThunkSize.
  virtual size_t GetThunkSize() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SidestepResolverThunk);
};

// This is the concrete resolver used to perform smart sidestep interceptions.
// This means basically a sidestep interception that skips the interceptor when
// the caller resides on the same dll being intercepted. It is intended as
// a helper only, because that determination is not infallible.
class SmartSidestepResolverThunk : public SidestepResolverThunk {
 public:
  SmartSidestepResolverThunk() {}
  virtual ~SmartSidestepResolverThunk() {}

  // Implementation of Resolver::Setup.
  virtual NTSTATUS Setup(const void* target_module,
                         const void* interceptor_module,
                         const char* target_name,
                         const char* interceptor_name,
                         const void* interceptor_entry_point,
                         void* thunk_storage,
                         size_t storage_bytes,
                         size_t* storage_used);

  // Implementation of Resolver::GetThunkSize.
  virtual size_t GetThunkSize() const;

 private:
  // Performs the actual call to the interceptor if the conditions are correct
  // (as determined by IsInternalCall).
  static void SmartStub();

  // Returns true if return_address is inside the module loaded at base.
  static bool IsInternalCall(const void* base, void* return_address);

  DISALLOW_COPY_AND_ASSIGN(SmartSidestepResolverThunk);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_SIDESTEP_RESOLVER_H__
