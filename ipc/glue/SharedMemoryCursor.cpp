/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"
#include "nsDebug.h"
#include "SharedMemoryCursor.h"

namespace mozilla::ipc::shared_memory {

bool Cursor::Read(void* aBuffer, size_t aCount) {
  return Consume(aBuffer, aCount, /* aWriteToShmem */ false);
}

bool Cursor::Write(const void* aBuffer, size_t aCount) {
  return Consume(const_cast<void*>(aBuffer), aCount, /* aWriteToShmem */ true);
}

void Cursor::Seek(uint64_t aOffset) {
  MOZ_ASSERT(aOffset <= Size());

  // Update our offset, and invalidate `mMapping` if our current chunk changed.
  uint64_t oldChunkStart = ChunkStart();
  mOffset = aOffset;
  if (mMapping && oldChunkStart != ChunkStart()) {
    mMapping = nullptr;
  }
}

MutableHandle Cursor::TakeHandle() {
  mMapping = nullptr;
  return std::move(mHandle);
}

void Cursor::SetChunkSize(size_t aChunkSize) {
  MOZ_ASSERT(IsPowerOfTwo(aChunkSize),
             "Cannot specify non power-of-two maximum chunk size");
  MOZ_ASSERT(aChunkSize >= SystemAllocationGranularity(),
             "Cannot specify a chunk size which is smaller than the system "
             "allocation granularity");
  mChunkSize = aChunkSize;
  mMapping = nullptr;  // Invalidate any existing mappings.
}

bool Cursor::Consume(void* aBuffer, size_t aCount, bool aWriteToShmem) {
  if (aCount > Remaining()) {
    NS_WARNING("count too large");
    return false;
  }

  size_t consumed = 0;
  while (consumed < aCount) {
    // Ensure we have a valid mapping each trip through the loop. This will
    // automatically back off on chunk size to avoid mapping failure.
    if (!EnsureMapping()) {
      return false;
    }

    // Determine how many of the requested bytes are available in mMapping, and
    // perform the operation on them.
    size_t mappingOffset = ChunkOffset();
    size_t mappingRemaining = mMapping.Size() - mappingOffset;
    size_t toCopy = std::min<size_t>(mappingRemaining, aCount - consumed);

    void* shmemPtr = mMapping.DataAs<char>() + mappingOffset;
    void* bufferPtr = static_cast<char*>(aBuffer) + consumed;
    if (aWriteToShmem) {
      memcpy(shmemPtr, bufferPtr, toCopy);
    } else {
      memcpy(bufferPtr, shmemPtr, toCopy);
    }

    // Seek and advance offsets. This will invalidate our mapping if it no
    // longer applies to the current chunk.
    Seek(mOffset + toCopy);
    consumed += toCopy;
  }
  return true;
}

bool Cursor::EnsureMapping() {
  MOZ_ASSERT(mHandle.IsValid());

  while (!mMapping) {
    // Attempt to map at the current chunk size.
    uint64_t chunkStart = ChunkStart();
    size_t chunkSize = std::min<uint64_t>(ChunkSize(), Size() - chunkStart);
    mMapping = mHandle.MapSubregion(chunkStart, chunkSize);
    if (MOZ_UNLIKELY(!mMapping)) {
      // If we failed to map a single allocation granularity, we can't go
      // smaller, so give up.
      if (chunkSize <= SystemAllocationGranularity()) {
        NS_WARNING(
            "Failed to map the smallest allocation granularity of shared "
            "memory region!");
        return false;
      }
      // Try to allocate a smaller chunk next time.
      mChunkSize = RoundUpPow2(chunkSize) >> 1;
    }
  }
  return true;
}

}  // namespace mozilla::ipc::shared_memory
