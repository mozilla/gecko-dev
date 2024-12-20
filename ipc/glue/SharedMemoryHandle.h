/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryHandle_h
#define mozilla_ipc_SharedMemoryHandle_h

#include <utility>

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla::ipc {

namespace shared_memory {

// Rust Bindgen code doesn't actually use these types, but `UniqueFileHandle`
// and `UniqueMachSendRight` aren't defined and some headers need to type check,
// so we define a dummy pointer type.
#if defined(RUST_BINDGEN)
using PlatformHandle = void*;
#elif defined(XP_DARWIN)
using PlatformHandle = mozilla::UniqueMachSendRight;
#else
using PlatformHandle = mozilla::UniqueFileHandle;
#endif

struct Handle;
struct ReadOnlyHandle;

class HandleBase {
 public:
  /**
   * The size of the shared memory region to which this handle refers.
   */
  uint64_t Size() const { return mSize; }

  /**
   * Whether this shared memory handle is valid.
   */
  bool IsValid() const { return (bool)*this; }

  /**
   * Whether this shared memory handle is valid.
   */
  explicit operator bool() const { return (bool)mHandle; }

  friend class Platform;
  friend struct IPC::ParamTraits<mozilla::ipc::shared_memory::Handle>;
  friend struct IPC::ParamTraits<mozilla::ipc::shared_memory::ReadOnlyHandle>;

 protected:
  HandleBase();
  MOZ_IMPLICIT HandleBase(std::nullptr_t) {}
  ~HandleBase();

  HandleBase(HandleBase&& aOther)
      : mHandle(std::move(aOther.mHandle)),
        mSize(std::exchange(aOther.mSize, 0)) {}

  HandleBase& operator=(HandleBase&& aOther) {
    mHandle = std::move(aOther.mHandle);
    mSize = std::exchange(aOther.mSize, 0);
    return *this;
  }

  HandleBase Clone() const;

  template <typename Derived>
  Derived CloneAs() const {
    return Clone().ConvertTo<Derived>();
  }

  template <typename Derived>
  Derived ConvertTo() && {
    Derived d;
    static_cast<HandleBase&>(d) = std::move(*this);
    return std::move(d);
  }

  void ToMessageWriter(IPC::MessageWriter* aWriter) &&;
  bool FromMessageReader(IPC::MessageReader* aReader);

 private:
  PlatformHandle mHandle = nullptr;
  uint64_t mSize = 0;
};

/**
 * A handle to a shared memory region.
 */
struct Handle : HandleBase {
  /**
   * Create an empty Handle.
   */
  Handle() = default;
  MOZ_IMPLICIT Handle(std::nullptr_t) {}

  /**
   * Clone the handle.
   */
  Handle Clone() const { return CloneAs<Handle>(); }

  /**
   * Map the shared memory region into memory.
   */
  struct Mapping Map(void* aFixedAddress = nullptr) const;
};

/**
 * A read-only handle to a shared memory region.
 */
struct ReadOnlyHandle : HandleBase {
  /**
   * Create an empty ReadOnlyHandle.
   */
  ReadOnlyHandle() = default;
  MOZ_IMPLICIT ReadOnlyHandle(std::nullptr_t) {}

  /**
   * Clone the handle.
   */
  ReadOnlyHandle Clone() const { return CloneAs<ReadOnlyHandle>(); }

  /**
   * Map the shared memory region into memory.
   */
  struct ReadOnlyMapping Map(void* aFixedAddress = nullptr) const;
};

/**
 * A freezable handle to a shared memory region.
 *
 * One cannot clone this handle, ensuring that at most one writable mapping
 * exists. After freezing, no new writable mappings can be created.
 */
struct FreezableHandle : HandleBase {
  /**
   * Create an empty FreezableHandle.
   */
  FreezableHandle() = default;
  MOZ_IMPLICIT FreezableHandle(std::nullptr_t) {}
  ~FreezableHandle();

  FreezableHandle(FreezableHandle&&) = default;
  FreezableHandle& operator=(FreezableHandle&&) = default;

  /**
   * Convert to a normal handle if we will not freeze this handle.
   */
  Handle WontFreeze() &&;

  /**
   * Freeze this handle, returning a read-only handle.
   */
  ReadOnlyHandle Freeze() &&;

  /**
   * Map the shared memory region into memory.
   */
  struct FreezableMapping Map(void* aFixedAddress = nullptr) &&;

  friend class Platform;
#if !defined(XP_DARWIN) && !defined(XP_WIN) && !defined(ANDROID)
 private:
  PlatformHandle mFrozenFile;
#endif
};

/**
 * Create a new shared memory region.
 */
Maybe<Handle> Create(uint64_t aSize);

/**
 * Create a new freezable shared memory region.
 *
 * Freezable shared memory regions are distinguished by the property that there
 * is guaranteed to be at most one writable mapping of the region at a time.
 *
 * Furthermore, a freezable shared memory region can be frozen while mapped. In
 * this case, the mapping remains valid but there can be no new writable
 * mappings.
 */
Maybe<FreezableHandle> CreateFreezable(uint64_t aSize);

}  // namespace shared_memory

using MutableSharedMemoryHandle = shared_memory::Handle;
using ReadOnlySharedMemoryHandle = shared_memory::ReadOnlyHandle;
using FreezableSharedMemoryHandle = shared_memory::FreezableHandle;

}  // namespace mozilla::ipc

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::shared_memory::Handle> {
  static void Write(MessageWriter* aWriter,
                    mozilla::ipc::shared_memory::Handle&& aParam);
  static bool Read(MessageReader* aReader,
                   mozilla::ipc::shared_memory::Handle* aResult);
};

template <>
struct ParamTraits<mozilla::ipc::shared_memory::ReadOnlyHandle> {
  static void Write(MessageWriter* aWriter,
                    mozilla::ipc::shared_memory::ReadOnlyHandle&& aParam);
  static bool Read(MessageReader* aReader,
                   mozilla::ipc::shared_memory::ReadOnlyHandle* aResult);
};

}  // namespace IPC

#endif
