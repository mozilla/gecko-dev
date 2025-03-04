/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryHandle_h
#define mozilla_ipc_SharedMemoryHandle_h

#include <utility>

#include "mozilla/UniquePtrExtensions.h"

namespace IPC {
template <class P>
struct ParamTraits;
class MessageWriter;
class MessageReader;
}  // namespace IPC

namespace mozilla::geckoargs {
template <typename T>
struct CommandLineArg;
}

namespace mozilla::ipc {

namespace shared_memory {

enum class Type {
  Mutable,
  ReadOnly,
  MutableOrReadOnly,
  Freezable,
};

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

template <Type T>
struct Handle;

template <Type T, bool WithHandle = false>
struct Mapping;

using MutableHandle = Handle<Type::Mutable>;
using ReadOnlyHandle = Handle<Type::ReadOnly>;
using FreezableHandle = Handle<Type::Freezable>;

using MutableMapping = Mapping<Type::Mutable>;
using ReadOnlyMapping = Mapping<Type::ReadOnly>;
using MutableOrReadOnlyMapping = Mapping<Type::MutableOrReadOnly>;
using FreezableMapping = Mapping<Type::Freezable>;

using MutableMappingWithHandle = Mapping<Type::Mutable, true>;
using ReadOnlyMappingWithHandle = Mapping<Type::ReadOnly, true>;

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

  /**
   * Take the platform handle.
   *
   * This should be used with caution, as it drops all of the guarantees of the
   * shared memory handle classes.
   */
  PlatformHandle TakePlatformHandle() && { return std::move(mHandle); }

  friend class Platform;
  friend struct IPC::ParamTraits<mozilla::ipc::shared_memory::MutableHandle>;
  friend struct IPC::ParamTraits<mozilla::ipc::shared_memory::ReadOnlyHandle>;
  friend struct mozilla::geckoargs::CommandLineArg<
      mozilla::ipc::shared_memory::ReadOnlyHandle>;

 protected:
  HandleBase();
  MOZ_IMPLICIT HandleBase(std::nullptr_t) {}
  ~HandleBase();

  HandleBase(HandleBase&& aOther)
      : mHandle(std::move(aOther.mHandle)),
        mSize(std::exchange(aOther.mSize, 0)) {}

  HandleBase& operator=(HandleBase&& aOther);

  HandleBase(const HandleBase&) = delete;
  HandleBase& operator=(const HandleBase&) = delete;

  HandleBase Clone() const;

  template <Type T>
  Handle<T> CloneAs() const {
    return Clone().ConvertTo<T>();
  }

  template <Type T>
  Handle<T> ConvertTo() && {
    Handle<T> d;
    static_cast<HandleBase&>(d) = std::move(*this);
    return d;
  }

  void ToMessageWriter(IPC::MessageWriter* aWriter) &&;
  bool FromMessageReader(IPC::MessageReader* aReader);

 private:
  /**
   * Set the size of the handle.
   *
   * This method must be used rather than setting `mSize` directly, as there is
   * additional bookkeeping that goes along with this value.
   */
  void SetSize(uint64_t aSize);

  PlatformHandle mHandle = nullptr;
  uint64_t mSize = 0;
};

/**
 * A handle to a shared memory region.
 */
template <>
struct Handle<Type::Mutable> : HandleBase {
  /**
   * Create an empty Handle.
   */
  Handle() = default;
  MOZ_IMPLICIT Handle(std::nullptr_t) {}

  /**
   * Clone the handle.
   */
  Handle Clone() const { return CloneAs<Type::Mutable>(); }

  /**
   * Convert the handle to a read-only handle.
   *
   * Note that this doesn't enforce any sort of security or guarantees on the
   * underlying shared memory.
   */
  ReadOnlyHandle ToReadOnly() &&;

  /**
   * Use the handle as a read-only handle.
   *
   * Note that this doesn't enforce any sort of security or guarantees on the
   * underlying shared memory.
   */
  const ReadOnlyHandle& AsReadOnly() const;

  /**
   * Map the shared memory region into memory.
   */
  MutableMapping Map(void* aFixedAddress = nullptr) const;

  /**
   * Map a subregion of the shared memory region into memory.
   */
  MutableMapping MapSubregion(uint64_t aOffset, size_t aSize,
                              void* aFixedAddress = nullptr) const;

  /**
   * Map the shared memory region into memory, keeping the handle with it.
   */
  MutableMappingWithHandle MapWithHandle(void* aFixedAddress = nullptr) &&;
};

/**
 * A read-only handle to a shared memory region.
 */
template <>
struct Handle<Type::ReadOnly> : HandleBase {
  /**
   * Create an empty ReadOnlyHandle.
   */
  Handle() = default;
  MOZ_IMPLICIT Handle(std::nullptr_t) {}

  /**
   * Clone the handle.
   */
  Handle Clone() const { return CloneAs<Type::ReadOnly>(); }

  /**
   * Map the shared memory region into memory.
   */
  ReadOnlyMapping Map(void* aFixedAddress = nullptr) const;

  /**
   * Map a subregion of the shared memory region into memory.
   */
  ReadOnlyMapping MapSubregion(uint64_t aOffset, size_t aSize,
                               void* aFixedAddress = nullptr) const;

  /**
   * Map the shared memory region into memory, keeping the handle with it.
   */
  ReadOnlyMappingWithHandle MapWithHandle(void* aFixedAddress = nullptr) &&;
};

/**
 * A freezable handle to a shared memory region.
 *
 * One cannot clone this handle, ensuring that at most one writable mapping
 * exists. After freezing, no new writable mappings can be created.
 */
template <>
struct Handle<Type::Freezable> : HandleBase {
  /**
   * Create an empty FreezableHandle.
   */
  Handle() = default;
  MOZ_IMPLICIT Handle(std::nullptr_t) {}
  ~Handle();

  Handle(Handle&&) = default;
  Handle& operator=(Handle&&) = default;

  /**
   * Convert to a normal handle if we will not freeze this handle.
   */
  MutableHandle WontFreeze() &&;

  /**
   * Freeze this handle, returning a read-only handle.
   */
  ReadOnlyHandle Freeze() &&;

  /**
   * Map the shared memory region into memory.
   */
  FreezableMapping Map(void* aFixedAddress = nullptr) &&;

  /**
   * Map a subregion of the shared memory region into memory.
   */
  FreezableMapping MapSubregion(uint64_t aOffset, size_t aSize,
                                void* aFixedAddress = nullptr) &&;

  friend class Platform;
#if !defined(XP_DARWIN) && !defined(XP_WIN) && !defined(ANDROID)
 private:
  PlatformHandle mFrozenFile;
#endif
};

/**
 * Create a new shared memory region.
 */
MutableHandle Create(uint64_t aSize);

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
FreezableHandle CreateFreezable(uint64_t aSize);

#if defined(XP_LINUX)
// If named POSIX shm is being used, append the prefix (including
// the leading '/') that would be used by a process with the given
// pid to the given string and return true.  If not, return false.
// (This is public so that the Linux sandboxing code can use it.)
bool AppendPosixShmPrefix(std::string* str, pid_t pid);

// Returns whether POSIX shm is in use.
bool UsingPosixShm();
#endif

}  // namespace shared_memory

using MutableSharedMemoryHandle = shared_memory::MutableHandle;
using ReadOnlySharedMemoryHandle = shared_memory::ReadOnlyHandle;
using FreezableSharedMemoryHandle = shared_memory::FreezableHandle;

}  // namespace mozilla::ipc

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::shared_memory::MutableHandle> {
  static void Write(MessageWriter* aWriter,
                    mozilla::ipc::shared_memory::MutableHandle&& aParam);
  static bool Read(MessageReader* aReader,
                   mozilla::ipc::shared_memory::MutableHandle* aResult);
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
