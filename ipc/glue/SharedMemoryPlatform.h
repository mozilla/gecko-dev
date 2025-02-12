/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryPlatform_h
#define mozilla_ipc_SharedMemoryPlatform_h

#include "mozilla/ipc/SharedMemoryHandle.h"
#include "mozilla/ipc/SharedMemoryMapping.h"
#include "mozilla/Logging.h"

namespace mozilla::ipc::shared_memory {

/// The shared memory logger.
// The definition resides in `SharedMemoryHandle.cpp`.
extern LazyLogModule gSharedMemoryLog;

/**
 * Functions that need to be implemented for each platform.
 *
 * These are static methods of a class to simplify access (the class can be
 * made a friend to give access to platform implementations).
 */
class Platform {
 public:
  /**
   * Create a new shared memory handle.
   *
   * @param aHandle The handle to populate.
   * @param aSize The size of the handle.
   *
   * @returns Whether the handle was successfully created.
   */
  static bool Create(Handle& aHandle, size_t aSize);

  /**
   * Create a new freezable shared memory handle.
   *
   * @param aHandle The handle to populate.
   * @param aSize The size of the handle.
   *
   * @returns Whether the handle was successfully created.
   */
  static bool CreateFreezable(FreezableHandle& aHandle, size_t aSize);

  /**
   * Return whether a platform handle is safe to map.
   *
   * This is used when handles are read from IPC.
   *
   * @param aHandle The handle to check.
   *
   * @returns Whether the handle is safe to map.
   */
  static bool IsSafeToMap(const PlatformHandle& aHandle);

  /**
   * Clone a handle.
   *
   * @param aHandle The handle to clone.
   *
   * @returns The cloned handle, or nullptr if not successful.
   */
  static PlatformHandle CloneHandle(const PlatformHandle& aHandle);

  /**
   * Freeze a handle, returning the frozen handle.
   *
   * @param aHandle The handle to freeze.
   *
   * The inner `PlatformHandle mHandle` should be the frozen handle upon
   * successful return. `mSize` must not change.
   *
   * @return Whether freezing the handle was successful.
   */
  static bool Freeze(FreezableHandle& aHandle);

  /**
   * Map the given handle with the size ane fixed address.
   *
   * @param aHandle The handle to map.
   * @param aFixedAddress The address at which to map the memory, or nullptr to
   * map anywhere.
   * @param aReadOnly Whether the mapping should be read-only.
   *
   * @returns The location of the mapping.
   */
  static Maybe<void*> Map(const HandleBase& aHandle, void* aFixedAddress,
                          bool aReadOnly);

  /**
   * Unmap previously mapped memory.
   *
   * @param aMemory The memory location to unmap.
   * @param aSize The size of the mapping.
   */
  static void Unmap(void* aMemory, size_t aSize);

  /**
   * Protect the given memory region.
   *
   * @param aAddr The address at the beginning of the memory region.
   * @param aSize The size of the region to protect.
   * @param aAccess The access level to allow.
   *
   * @returns Whether protection was successful.
   */
  static bool Protect(char* aAddr, size_t aSize, Access aAccess);

  /**
   * Find a region of free memory.
   *
   * @param aSize The size of the region to locate.
   *
   * @returns The start of the memory region, or nullptr on error.
   */
  static void* FindFreeAddressSpace(size_t aSize);

  /**
   * Return the page size of the system.
   */
  static size_t PageSize();
};

}  // namespace mozilla::ipc::shared_memory

#endif
