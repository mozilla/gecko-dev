/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemory_h
#define mozilla_ipc_SharedMemory_h

#include <cstddef>

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Assertions.h"
#include "nsISupportsImpl.h"  // NS_INLINE_DECL_REFCOUNTING

#ifdef XP_DARWIN
#  include "mozilla/ipc/SharedMemoryImpl_mach.h"
#else
#  include "mozilla/ipc/SharedMemoryImpl_chromium.h"
#endif

namespace mozilla::ipc {

class SharedMemory : public SharedMemoryImpl {
  ~SharedMemory();

 public:
  SharedMemory();

  // bug 1168843, compositor thread may create shared memory instances that are
  // destroyed by main thread on shutdown, so this must use thread-safe RC to
  // avoid hitting assertion
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedMemory)

  size_t Size() const { return mMappedSize; }
  void CloseHandle() { TakeHandle(); }

  bool WriteHandle(IPC::MessageWriter* aWriter);
  bool ReadHandle(IPC::MessageReader* aReader);
  void Protect(char* aAddr, size_t aSize, int aRights);

  static void SystemProtect(char* aAddr, size_t aSize, int aRights);
  [[nodiscard]] static bool SystemProtectFallible(char* aAddr, size_t aSize,
                                                  int aRights);
  static size_t SystemPageSize();
  static size_t PageAlignedSize(size_t aSize);

  bool Create(size_t nBytes);
  bool Map(size_t nBytes, void* fixedAddress = nullptr);
  void Unmap();
  void* Memory() const;

 private:
  // The size of the shmem region requested in Create(), if
  // successful.  SharedMemory instances that are opened from a
  // foreign handle have an alloc size of 0, even though they have
  // access to the alloc-size information.
  size_t mAllocSize;
  // The size of the region mapped in Map(), if successful.  All
  // SharedMemorys that are mapped have a non-zero mapped size.
  size_t mMappedSize;
};

}  // namespace mozilla::ipc

#endif  // ifndef mozilla_ipc_SharedMemory_h
