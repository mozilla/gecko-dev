/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryCursor_h
#define mozilla_ipc_SharedMemoryCursor_h

#include "mozilla/ipc/SharedMemoryHandle.h"
#include "mozilla/ipc/SharedMemoryMapping.h"

namespace mozilla::ipc::shared_memory {

// The `Cursor` is a similar type to a mutable `Mapping`, in that it
// provides read/write access to the contents of a shared memory region.
// However, it can recover from situations where address fragmentation means
// that mapping the full shared memory region fails, by instead mapping each
// page at a time, and seeking around the region.
//
// Because of this, the `Cursor` does not provide direct access to the shared
// memory region.
//
// NOTE: Cursor currently only operates on mutable mappings, even when reading.
// It can be generalized in the future if it would be found to be useful.
class Cursor {
 public:
  // Default constructor for invalid cursor. All reads and writes will fail.
  Cursor() = default;

  // Construct a new Cursor which can be used to read from or write to the
  // shared memory region indicated by aHandle.
  explicit Cursor(MutableHandle&& aHandle) : mHandle(std::move(aHandle)) {}

  bool IsValid() const { return mHandle.IsValid(); }
  uint64_t Size() const { return mHandle.Size(); }
  uint64_t Offset() const { return mOffset; }
  uint64_t Remaining() const { return Size() - Offset(); }

  // Read aCount bytes into aBuffer from the shared memory region, advancing the
  // internal offset. Returns `false` if this fails for any reason.
  bool Read(void* aBuffer, size_t aCount);

  // Write aCount bytes from aBuffer into the shared memory region, advancing
  // the internal offset. Returns `false` if this fails for any reason.
  bool Write(const void* aBuffer, size_t aCount);

  // Seek the Cursor to a given offset in the shared memory region.
  // aOffset must be less than Size().
  void Seek(uint64_t aOffset);

  // Invalidate the Cursor, and return the underlying handle.
  MutableHandle TakeHandle();

  // Set the ChunkSize for the shared memory regions in this chunk. This is
  // intended to be used for testing purposes.
  // The chunk size must be a power of two, and at least
  // SystemAllocationGranularity().
  void SetChunkSize(size_t aChunkSize);

 private:
  // Default to mapping at most 1GiB/256MiB, depending on address space size.
#ifdef HAVE_64BIT_BUILD
  static constexpr size_t kDefaultMaxChunkSize = size_t(1) << 30;  // 1GiB
#else
  static constexpr size_t kDefaultMaxChunkSize = size_t(1) << 28;  // 256MiB
#endif

  size_t ChunkSize() const { return mChunkSize; }
  uint64_t ChunkOffsetMask() const { return uint64_t(ChunkSize()) - 1; }
  uint64_t ChunkStartMask() const { return ~ChunkOffsetMask(); }
  size_t ChunkOffset() const { return Offset() & ChunkOffsetMask(); }
  uint64_t ChunkStart() const { return Offset() & ChunkStartMask(); }

  bool Consume(void* aBuffer, size_t aCount, bool aWriteToShmem);
  bool EnsureMapping();

  // Shared memory handle this Cursor allows accessing.
  MutableHandle mHandle;
  // Memory map for the currently active chunk. Lazily initialized.
  MutableMapping mMapping;
  // Absolute offset into the shared memory handle.
  uint64_t mOffset = 0;
  // Current size of each chunk. Always a power of two. May be reduced in
  // response to allocation failures.
  size_t mChunkSize = kDefaultMaxChunkSize;
};

}  // namespace mozilla::ipc::shared_memory

#endif  // mozilla_ipc_SharedMemoryCursor_h
