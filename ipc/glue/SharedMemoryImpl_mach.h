/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryImpl_mach_h
#define mozilla_ipc_SharedMemoryImpl_mach_h

#include "mozilla/UniquePtrExtensions.h"
#include <mach/port.h>

namespace {
enum Rights { RightsNone = 0, RightsRead = 1 << 0, RightsWrite = 1 << 1 };
}  // namespace

namespace mozilla::ipc {

class SharedMemoryImpl {
 public:
  using Handle = mozilla::UniqueMachSendRight;

  enum OpenRights {
    RightsReadOnly = RightsRead,
    RightsReadWrite = RightsRead | RightsWrite,
  };

  Handle CloneHandle();
  Handle TakeHandle();

  bool IsHandleValid(const Handle& aHandle) const;
  bool SetHandle(Handle aHandle, OpenRights aRights);

  static Handle NULLHandle();
  static void* FindFreeAddressSpace(size_t size);

 protected:
  SharedMemoryImpl();
  ~SharedMemoryImpl() {}

  bool CreateImpl(size_t size);
  bool MapImpl(size_t nBytes, void* fixedAddress);
  void UnmapImpl(size_t mappedSize);
  void* MemoryImpl() const;

 private:
  mozilla::UniqueMachSendRight mPort;
  // Pointer to mapped region, null if unmapped.
  void* mMemory;
  // Access rights to map an existing region with.
  OpenRights mOpenRights;
};

}  // namespace mozilla::ipc

#endif
