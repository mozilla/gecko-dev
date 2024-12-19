/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryMapping_h
#define mozilla_ipc_SharedMemoryMapping_h

#include <tuple>
#include <utility>
#include "mozilla/Span.h"
#include "SharedMemoryHandle.h"

namespace mozilla::ipc {

namespace shared_memory {

/**
 * A leaked memory mapping.
 *
 * This memory will never be unmapped.
 */
struct LeakedMapping : Span<uint8_t> {
  using Span::Span;
};

class MappingBase {
 public:
  /**
   * The size of the mapping.
   */
  size_t Size() const { return mSize; }

  /**
   * The pointer to the mapping in memory.
   */
  void* Data() const;

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
  ~MappingBase();

  /**
   * Mappings are movable (but not copyable).
   */
  MappingBase(MappingBase&& aOther)
      : mMemory(std::exchange(aOther.mMemory, nullptr)),
        mSize(std::exchange(aOther.mSize, 0)) {}

  MappingBase& operator=(MappingBase&& aOther) {
    mMemory = std::exchange(aOther.mMemory, nullptr);
    mSize = std::exchange(aOther.mSize, 0);
    return *this;
  }

  bool Map(const HandleBase& aHandle, void* aFixedAddress, bool aReadOnly);

  template <typename Derived>
  Derived ConvertTo() && {
    Derived d;
    static_cast<MappingBase&>(d) = std::move(*this);
    return std::move(d);
  }

  /**
   * Leak this mapping's memory.
   *
   * This will cause the memory to be mapped until the process exits.
   */
  LeakedMapping release() &&;

 private:
  void* mMemory = nullptr;
  size_t mSize = 0;
};

/**
 * A shared memory mapping.
 */
struct Mapping : MappingBase {
  /**
   * Create an empty Mapping.
   */
  Mapping() = default;
  MOZ_IMPLICIT Mapping(std::nullptr_t) {}

  explicit Mapping(const Handle& aHandle, void* aFixedAddress = nullptr);

  using MappingBase::release;
};

/**
 * A read-only shared memory mapping.
 */
struct ReadOnlyMapping : MappingBase {
  /**
   * Create an empty ReadOnlyMapping.
   */
  ReadOnlyMapping() = default;
  MOZ_IMPLICIT ReadOnlyMapping(std::nullptr_t) {}

  explicit ReadOnlyMapping(const ReadOnlyHandle& aHandle,
                           void* aFixedAddress = nullptr);
};

/**
 * A freezable shared memory mapping.
 */
struct FreezableMapping : MappingBase {
  /**
   * Create an empty FreezableMapping.
   */
  FreezableMapping() = default;
  MOZ_IMPLICIT FreezableMapping(std::nullptr_t) {}

  /**
   * Freezable mappings take ownership of a handle to ensure there is only one
   * writeable mapping at a time.
   *
   * Call `Unmap()` to get the handle back.
   */
  explicit FreezableMapping(FreezableHandle&& aHandle,
                            void* aFixedAddress = nullptr);

  /**
   * Freeze the shared memory region.
   *
   * The returned Mapping will still be valid and writable until it is deleted,
   * however no new writable mappings can be created.
   */
  std::tuple<Mapping, ReadOnlyHandle> Freeze() &&;

  /**
   * Unmap the shared memory, returning the freezable handle.
   *
   * It is only necessary to call this if you need to get the FreezableHandle
   * back.
   */
  FreezableHandle Unmap() &&;

 private:
  FreezableHandle mHandle;
};

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
 * Return a size which is page-aligned and can fit at least `minimum` bytes.
 *
 * @param aMinimum The minimum number of bytes required.
 *
 * @returns The page-aligned size that can hold `minimum` bytes.
 */
size_t PageAlignedSize(size_t aMinimum);

}  // namespace shared_memory

using SharedMemoryMapping = shared_memory::Mapping;
using ReadOnlySharedMemoryMapping = shared_memory::ReadOnlyMapping;
using FreezableSharedMemoryMapping = shared_memory::FreezableMapping;

}  // namespace mozilla::ipc

#endif
