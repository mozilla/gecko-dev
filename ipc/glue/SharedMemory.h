/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#ifndef mozilla_ipc_SharedMemory_h
#define mozilla_ipc_SharedMemory_h

#include <cstddef>

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsISupportsImpl.h"  // NS_INLINE_DECL_REFCOUNTING

#if !(defined(XP_DARWIN) || defined(XP_WIN))
#  include <string>
#endif

namespace IPC {
class MessageWriter;
class MessageReader;
}  // namespace IPC

namespace {
enum Rights { RightsNone = 0, RightsRead = 1 << 0, RightsWrite = 1 << 1 };
}  // namespace

namespace mozilla::ipc {

// Rust Bindgen code doesn't actually use these types, but `UniqueFileHandle`
// and `UniqueMachSendRight` aren't defined and some headers need to type check,
// so we define a dummy type.
#if defined(RUST_BINDGEN)
using SharedMemoryHandle = void*;
#elif defined(XP_DARWIN)
using SharedMemoryHandle = mozilla::UniqueMachSendRight;
#else
using SharedMemoryHandle = mozilla::UniqueFileHandle;
#endif

class SharedMemory {
  ~SharedMemory();

  /// # Provided methods
 public:
  using Handle = SharedMemoryHandle;

  enum OpenRights {
    RightsReadOnly = RightsRead,
    RightsReadWrite = RightsRead | RightsWrite,
  };

  SharedMemory();

  // bug 1168843, compositor thread may create shared memory instances that are
  // destroyed by main thread on shutdown, so this must use thread-safe RC to
  // avoid hitting assertion
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedMemory)

  size_t Size() const { return mMappedSize; }
  size_t MaxSize() const { return mAllocSize; }

  bool Create(size_t nBytes, bool freezable = false);
  bool Map(size_t nBytes, void* fixedAddress = nullptr);
  void Unmap();
  void* Memory() const;
  /// Take the mapping memory.
  ///
  /// This prevents unmapping the memory.
  Span<uint8_t> TakeMapping();

  Handle TakeHandleAndUnmap() {
    auto handle = TakeHandle();
    Unmap();
    return handle;
  }
  Handle TakeHandle();
  Handle CloneHandle() {
    mFreezable = false;
    return SharedMemory::CloneHandle(mHandle);
  }
  void CloseHandle() { TakeHandle(); }
  bool SetHandle(Handle aHandle, OpenRights aRights);
  bool IsValid() const { return IsHandleValid(mHandle); }
  static bool IsHandleValid(const Handle& aHandle) {
    // `operator!=` had ambiguous overload resolution with the windows Handle
    // (mozilla::UniqueFileHandle), so invert `operator==` instead.
    return !(aHandle == NULLHandle());
  }
  static Handle NULLHandle() { return nullptr; }

  bool CreateFreezable(size_t nBytes) { return Create(nBytes, true); }

  [[nodiscard]] bool Freeze() {
    Unmap();
    return ReadOnlyCopy(this);
  }

  bool WriteHandle(IPC::MessageWriter* aWriter);
  bool ReadHandle(IPC::MessageReader* aReader);
  void Protect(char* aAddr, size_t aSize, int aRights);

  static size_t PageAlignedSize(size_t aSize);

  /// Public methods which should be defined as part of each implementation.
 public:
  [[nodiscard]] bool ReadOnlyCopy(SharedMemory* ro_out);

  static void SystemProtect(char* aAddr, size_t aSize, int aRights);
  [[nodiscard]] static bool SystemProtectFallible(char* aAddr, size_t aSize,
                                                  int aRights);
  static Handle CloneHandle(const Handle& aHandle);
  static size_t SystemPageSize();
  static void* FindFreeAddressSpace(size_t size);

  /// Private methods which should be defined as part of each implementation.
 private:
  bool CreateImpl(size_t size, bool freezable);
  Maybe<void*> MapImpl(size_t nBytes, void* fixedAddress);
  static void UnmapImpl(size_t nBytes, void* address);
  Maybe<Handle> ReadOnlyCopyImpl();
  void ResetImpl();

  /// Common members
 private:
  struct MappingDeleter {
    size_t mMappedSize = 0;
    explicit MappingDeleter(size_t size) : mMappedSize(size) {}
    MappingDeleter() = default;
    void operator()(void* ptr) {
      MOZ_ASSERT(mMappedSize != 0);
      UnmapImpl(mMappedSize, ptr);
      // Guard against multiple calls of the same deleter, which shouldn't
      // happen (but could, if `UniquePtr::reset` were used).  Calling
      // `munmap` with an incorrect non-zero length would be bad.
      mMappedSize = 0;
    }
  };
#ifndef RUST_BINDGEN
  using UniqueMapping = mozilla::UniquePtr<void, MappingDeleter>;
#else
  using UniqueMapping = void*;
#endif

  // The held handle, if any.
  Handle mHandle = NULLHandle();
  // The size of the shmem region requested in Create(), if
  // successful.  SharedMemory instances that are opened from a
  // foreign handle have an alloc size of 0, even though they have
  // access to the alloc-size information.
  size_t mAllocSize;
  // The memory mapping, if any.
  UniqueMapping mMemory;
  // The size of the region mapped in Map(), if successful.  All
  // SharedMemorys that are mapped have a non-zero mapped size.
  size_t mMappedSize;
  // Whether the handle held is freezable.
  bool mFreezable = false;
  // Whether the handle held is read-only.
  bool mReadOnly = false;
  // Whether the handle held is external (set with `SetHandle`).
  bool mExternalHandle = false;

#if !defined(XP_DARWIN) && !defined(XP_WIN)
  /// # Unix/POSIX-specific methods and members.
 public:
  // If named POSIX shm is being used, append the prefix (including
  // the leading '/') that would be used by a process with the given
  // pid to the given string and return true.  If not, return false.
  // (This is public so that the Linux sandboxing code can use it.)
  static bool AppendPosixShmPrefix(std::string* str, pid_t pid);

  // Similar, but simply returns whether POSIX shm is in use.
  static bool UsingPosixShm();

#  if !defined(ANDROID) && !defined(RUST_BINDGEN)
 private:
  mozilla::UniqueFileHandle mFrozenFile;
  bool mIsMemfd = false;
#  endif
#endif
};

}  // namespace mozilla::ipc

#endif  // ifndef mozilla_ipc_SharedMemory_h
