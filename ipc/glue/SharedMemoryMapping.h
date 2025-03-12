/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryMapping_h
#define mozilla_ipc_SharedMemoryMapping_h

#include <tuple>
#include <type_traits>
#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/Span.h"
#include "SharedMemoryHandle.h"

namespace mozilla::ipc {

namespace shared_memory {

/**
 * A leaked memory mapping.
 *
 * This memory will never be unmapped.
 */
template <Type T>
struct LeakedMapping : Span<uint8_t> {
  using Span::Span;
};

template <>
struct LeakedMapping<Type::ReadOnly> : Span<const uint8_t> {
  using Span::Span;
};

using LeakedMutableMapping = LeakedMapping<Type::Mutable>;
using LeakedReadOnlyMapping = LeakedMapping<Type::ReadOnly>;

class MappingBase {
 public:
  /**
   * The size of the mapping.
   */
  size_t Size() const { return mSize; }

  /**
   * The pointer to the mapping in memory.
   */
  void* Address() const;

  /**
   * Whether this shared memory mapping is valid.
   */
  bool IsValid() const { return (bool)*this; }

  /**
   * Whether this shared memory mapping is valid.
   */
  explicit operator bool() const { return (bool)mMemory; }

 protected:
  /**
   * Create an empty Mapping.
   */
  MappingBase();
  MOZ_IMPLICIT MappingBase(std::nullptr_t) {}
  ~MappingBase() { Unmap(); }

  /**
   * Mappings are movable (but not copyable).
   */
  MappingBase(MappingBase&& aOther)
      : mMemory(std::exchange(aOther.mMemory, nullptr)),
        mSize(std::exchange(aOther.mSize, 0)) {}

  MappingBase& operator=(MappingBase&& aOther);

  MappingBase(const MappingBase&) = delete;
  MappingBase& operator=(const MappingBase&) = delete;

  bool Map(const HandleBase& aHandle, void* aFixedAddress, bool aReadOnly);
  bool MapSubregion(const HandleBase& aHandle, uint64_t aOffset, size_t aSize,
                    void* aFixedAddress, bool aReadOnly);
  void Unmap();

  template <Type T, Type S>
  static Mapping<T> ConvertMappingTo(Mapping<S>&& from) {
    Mapping<T> to;
    static_cast<MappingBase&>(to) = std::move(from);
    return to;
  }

  std::tuple<void*, size_t> Release() &&;

 private:
  void* mMemory = nullptr;
  size_t mSize = 0;
};

template <bool CONST_MEMORY>
struct MappingData : MappingBase {
 private:
  template <typename T>
  using DataType =
      std::conditional_t<CONST_MEMORY, std::add_const_t<std::remove_const_t<T>>,
                         T>;

 protected:
  MappingData() = default;
  explicit MappingData(MappingBase&& aOther) : MappingBase(std::move(aOther)) {}

 public:
  /**
   * Get a pointer to the data in the mapping as a type T.
   *
   * The mapping data must meet the alignment requirements of @p T.
   *
   * @tparam T The type of data in the mapping.
   *
   * @return A pointer of type @p T*.
   */
  template <typename T>
  DataType<T>* DataAs() const {
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(Address()) % alignof(T)) == 0,
               "memory map does not meet alignment requirements of type");
    return static_cast<DataType<T>*>(Address());
  }

  /**
   * Get a `Span<T>` over the mapping.
   *
   * The mapping data must meet the alignment requirements of @p T.
   *
   * @tparam T The type of data in the mapping.
   *
   * @return A span of type @p T covering as much of the mapping as possible.
   */
  template <typename T>
  Span<DataType<T>> DataAsSpan() const {
    return {DataAs<T>(), Size() / sizeof(T)};
  }
};

/**
 * A shared memory mapping.
 */
template <Type T>
struct Mapping<T> : MappingData<T == Type::ReadOnly> {
  /**
   * Create an empty Mapping.
   */
  Mapping() = default;
  MOZ_IMPLICIT Mapping(std::nullptr_t) {}

  explicit Mapping(const Handle<T>& aHandle, void* aFixedAddress = nullptr) {
    MappingBase::Map(aHandle, aFixedAddress, T == Type::ReadOnly);
  }
  Mapping(const Handle<T>& aHandle, uint64_t aOffset, size_t aSize,
          void* aFixedAddress = nullptr) {
    MappingBase::MapSubregion(aHandle, aOffset, aSize, aFixedAddress,
                              T == Type::ReadOnly);
  }

  /**
   * Leak this mapping's memory.
   *
   * This will cause the memory to be mapped until the process exits.
   */
  LeakedMapping<T> Release() && {
    auto [ptr, size] = std::move(*this).MappingBase::Release();
    return LeakedMapping<T>{
        static_cast<typename LeakedMapping<T>::pointer>(ptr), size};
  }
};

/**
 * A shared memory mapping which has runtime-stored mutability.
 */
template <>
struct Mapping<Type::MutableOrReadOnly> : MappingData<true> {
  /**
   * Create an empty MutableOrReadOnlyMapping.
   */
  Mapping() = default;
  MOZ_IMPLICIT Mapping(std::nullptr_t) {}

  explicit Mapping(const ReadOnlyHandle& aHandle,
                   void* aFixedAddress = nullptr);
  explicit Mapping(const MutableHandle& aHandle, void* aFixedAddress = nullptr);
  MOZ_IMPLICIT Mapping(ReadOnlyMapping&& aMapping);
  MOZ_IMPLICIT Mapping(MutableMapping&& aMapping);

  /**
   * Return whether the mapping is read-only.
   */
  bool IsReadOnly() const { return mReadOnly; }

 private:
  bool mReadOnly = false;
};

/**
 * A freezable shared memory mapping.
 */
template <>
struct Mapping<Type::Freezable> : MappingData<false> {
  /**
   * Create an empty FreezableMapping.
   */
  Mapping() = default;
  MOZ_IMPLICIT Mapping(std::nullptr_t) {}

  /**
   * Freezable mappings take ownership of a handle to ensure there is only one
   * writeable mapping at a time.
   *
   * Call `Unmap()` to get the handle back.
   */
  explicit Mapping(FreezableHandle&& aHandle, void* aFixedAddress = nullptr);
  Mapping(FreezableHandle&& aHandle, uint64_t aOffset, size_t aSize,
          void* aFixedAddress = nullptr);

  /**
   * Freeze the shared memory region.
   */
  ReadOnlyHandle Freeze() &&;

  /**
   * Freeze the shared memory region.
   *
   * The returned Mapping will still be valid and writable until it is deleted,
   * however no new writable mappings can be created.
   */
  std::tuple<ReadOnlyHandle, MutableMapping> FreezeWithMutableMapping() &&;

  /**
   * Unmap the shared memory, returning the freezable handle.
   *
   * It is only necessary to call this if you need to get the FreezableHandle
   * back.
   */
  FreezableHandle Unmap() &&;

 protected:
  FreezableHandle mHandle;
};

template <Type T>
struct Mapping<T, true> : public Mapping<T> {
  Mapping() {}
  MOZ_IMPLICIT Mapping(std::nullptr_t) : Mapping<T>(nullptr) {}

  explicit Mapping(shared_memory::Handle<T>&& aHandle,
                   void* aFixedAddress = nullptr)
      : Mapping<T>(aHandle, aFixedAddress), mHandle(std::move(aHandle)) {}

  const shared_memory::Handle<T>& Handle() const { return mHandle; };

  std::tuple<shared_memory::Handle<T>, Mapping<T>> Split() && {
    auto handle = std::move(mHandle);
    return std::make_tuple(std::move(handle), std::move(*this));
  }

 private:
  shared_memory::Handle<T> mHandle;
};

// To uphold the guarantees of freezable mappings, we do not allow access to the
// handle (and since this should never be used in this way, we make it a useless
// type).
template <>
struct Mapping<Type::Freezable, true>;

// The access level permitted for memory protection.
enum Access {
  AccessNone = 0,
  AccessRead = 1 << 0,
  AccessWrite = 1 << 1,
  AccessReadWrite = AccessRead | AccessWrite,
};

/**
 * Protect the given memory region.
 *
 * This protection extends only to the local memory mapping. It doesn't change
 * the permissions of other mappings nor the associated handle.
 *
 * @param aAddr The address at the beginning of the memory region.
 * @param aSize The size of the region to protect.
 * @param aAccess The access level to allow.
 *
 * @returns Whether protection was successful.
 */
bool LocalProtect(char* aAddr, size_t aSize, Access aAccess);

/**
 * Find a region of free memory.
 *
 * @param aSize The size of the region to locate.
 *
 * @returns The start of the memory region, or nullptr on error.
 */
void* FindFreeAddressSpace(size_t aSize);

/**
 * Get the system page size.
 */
size_t SystemPageSize();

/**
 * Get the system allocation granularity.
 *
 * This may be distinct from the page size, and controls the required
 * alignment for fixed mapping addresses and shared memory offsets.
 */
size_t SystemAllocationGranularity();

/**
 * Return a size which is page-aligned and can fit at least `minimum` bytes.
 *
 * @param aMinimum The minimum number of bytes required.
 *
 * @returns The page-aligned size that can hold `minimum` bytes.
 */
size_t PageAlignedSize(size_t aMinimum);

}  // namespace shared_memory

using SharedMemoryMapping = shared_memory::MutableMapping;
using ReadOnlySharedMemoryMapping = shared_memory::ReadOnlyMapping;
using MutableOrReadOnlySharedMemoryMapping =
    shared_memory::MutableOrReadOnlyMapping;
using FreezableSharedMemoryMapping = shared_memory::FreezableMapping;

using SharedMemoryMappingWithHandle = shared_memory::MutableMappingWithHandle;
using ReadOnlySharedMemoryMappingWithHandle =
    shared_memory::ReadOnlyMappingWithHandle;

}  // namespace mozilla::ipc

#endif
