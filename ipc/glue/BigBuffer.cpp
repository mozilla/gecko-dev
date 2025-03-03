/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/BigBuffer.h"

#include "chrome/common/ipc_message_utils.h"
#include "nsDebug.h"

namespace mozilla::ipc {

BigBuffer::BigBuffer(Adopt, SharedMemoryMappingWithHandle&& aSharedMemory,
                     size_t aSize)
    : mSize(aSize), mData(AsVariant(std::move(aSharedMemory))) {
  MOZ_RELEASE_ASSERT(mData.as<1>(), "shared memory must be valid");
  MOZ_RELEASE_ASSERT(mSize <= mData.as<1>().Size(),
                     "shared memory region isn't large enough");
}

BigBuffer::BigBuffer(Adopt, uint8_t* aData, size_t aSize)
    : mSize(aSize), mData(AsVariant(UniqueFreePtr<uint8_t[]>{aData})) {}

uint8_t* BigBuffer::Data() {
  return mData.is<0>() ? mData.as<0>().get() : mData.as<1>().DataAs<uint8_t>();
}
const uint8_t* BigBuffer::Data() const {
  return mData.is<0>() ? mData.as<0>().get()
                       : mData.as<1>().DataAs<const uint8_t>();
}

auto BigBuffer::TryAllocBuffer(size_t aSize) -> Maybe<Storage> {
  if (aSize <= kShmemThreshold) {
    auto mem = UniqueFreePtr<uint8_t[]>{
        reinterpret_cast<uint8_t*>(malloc(aSize))};  // Fallible!
    if (!mem) return {};
    return Some(AsVariant(std::move(mem)));
  }

  size_t capacity = shared_memory::PageAlignedSize(aSize);
  auto mapping = shared_memory::Create(capacity).MapWithHandle();
  if (!mapping) {
    return {};
  }
  return Some(AsVariant(std::move(mapping)));
}

}  // namespace mozilla::ipc

void IPC::ParamTraits<mozilla::ipc::BigBuffer>::Write(MessageWriter* aWriter,
                                                      paramType&& aParam) {
  using namespace mozilla::ipc;
  size_t size = std::exchange(aParam.mSize, 0);
  auto data = std::exchange(aParam.mData, BigBuffer::NoData());

  WriteParam(aWriter, size);
  bool isShmem = data.is<1>();
  WriteParam(aWriter, isShmem);

  if (isShmem) {
    auto handle = data.as<1>().Handle().Clone();
    if (!handle) {
      aWriter->FatalError("Failed to write data shmem");
    } else {
      WriteParam(aWriter, std::move(handle));
    }
  } else {
    aWriter->WriteBytes(data.as<0>().get(), size);
  }
}

bool IPC::ParamTraits<mozilla::ipc::BigBuffer>::Read(MessageReader* aReader,
                                                     paramType* aResult) {
  using namespace mozilla::ipc;
  size_t size = 0;
  bool isShmem = false;
  if (!ReadParam(aReader, &size) || !ReadParam(aReader, &isShmem)) {
    aReader->FatalError("Failed to read data size and format");
    return false;
  }

  if (isShmem) {
    MutableSharedMemoryHandle handle;
    size_t expected_size = shared_memory::PageAlignedSize(size);
    if (!ReadParam(aReader, &handle) || !handle) {
      aReader->FatalError("Failed to read data shmem");
      return false;
    }
    auto mapping = std::move(handle).MapWithHandle();
    if (!mapping || mapping.Size() != expected_size) {
      aReader->FatalError("Failed to map data shmem");
      return false;
    }
    *aResult = BigBuffer(BigBuffer::Adopt{}, std::move(mapping), size);
    return true;
  }

  mozilla::UniqueFreePtr<uint8_t[]> buf{
      reinterpret_cast<uint8_t*>(malloc(size))};
  if (!buf) {
    aReader->FatalError("Failed to allocate data buffer");
    return false;
  }
  if (!aReader->ReadBytesInto(buf.get(), size)) {
    aReader->FatalError("Failed to read data");
    return false;
  }
  *aResult = BigBuffer(BigBuffer::Adopt{}, buf.release(), size);
  return true;
}
