/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Shmem.h"

#include "ProtocolUtils.h"
#include "ShmemMessageUtils.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/SharedMemoryHandle.h"

namespace mozilla {
namespace ipc {

class ShmemCreated : public IPC::Message {
 private:
  typedef Shmem::id_t id_t;

 public:
  ShmemCreated(int32_t routingId, id_t aIPDLId,
               MutableSharedMemoryHandle&& aHandle)
      : IPC::Message(
            routingId, SHMEM_CREATED_MESSAGE_TYPE, 0,
            HeaderFlags(NESTED_INSIDE_CPOW, CONTROL_PRIORITY, COMPRESSION_NONE,
                        LAZY_SEND, NOT_CONSTRUCTOR, ASYNC, NOT_REPLY)) {
    IPC::MessageWriter writer(*this);
    IPC::WriteParam(&writer, aIPDLId);
    IPC::WriteParam(&writer, std::move(aHandle));
  }

  static bool ReadInfo(IPC::MessageReader* aReader, id_t* aIPDLId,
                       MutableSharedMemoryHandle* aHandle) {
    return IPC::ReadParam(aReader, aIPDLId) && IPC::ReadParam(aReader, aHandle);
  }

  void Log(const std::string& aPrefix, FILE* aOutf) const {
    fputs("(special ShmemCreated msg)", aOutf);
  }
};

class ShmemDestroyed : public IPC::Message {
 private:
  typedef Shmem::id_t id_t;

 public:
  ShmemDestroyed(int32_t routingId, id_t aIPDLId)
      : IPC::Message(
            routingId, SHMEM_DESTROYED_MESSAGE_TYPE, 0,
            HeaderFlags(NOT_NESTED, NORMAL_PRIORITY, COMPRESSION_NONE,
                        LAZY_SEND, NOT_CONSTRUCTOR, ASYNC, NOT_REPLY)) {
    IPC::MessageWriter writer(*this);
    IPC::WriteParam(&writer, aIPDLId);
  }
};

#if defined(DEBUG)

static void Protect(const Shmem::Segment* aSegment) {
  MOZ_ASSERT(aSegment && *aSegment, "null segment");
  shared_memory::LocalProtect(aSegment->DataAs<char>(), aSegment->Size(),
                              shared_memory::AccessNone);
}

static void Unprotect(const Shmem::Segment* aSegment) {
  MOZ_ASSERT(aSegment && *aSegment, "null segment");
  shared_memory::LocalProtect(aSegment->DataAs<char>(), aSegment->Size(),
                              shared_memory::AccessReadWrite);
}

void Shmem::AssertInvariants() const {
  MOZ_ASSERT(mSegment, "null segment");
  MOZ_ASSERT(mData, "null data pointer");
  MOZ_ASSERT(mSize > 0, "invalid size");
  // if the segment isn't owned by the current process, these will
  // trigger SIGSEGV
  char checkMappingFront = *reinterpret_cast<char*>(mData);
  char checkMappingBack = *(reinterpret_cast<char*>(mData) + mSize - 1);

  // avoid "unused" warnings for these variables:
  Unused << checkMappingFront;
  Unused << checkMappingBack;
}

void Shmem::RevokeRights() {
  AssertInvariants();

  // When sending a non-unsafe shmem, remove read/write rights from the local
  // mapping of the segment.
  if (!mUnsafe) {
    Protect(mSegment);
  }
}

#endif  // if defined(DEBUG)

Shmem::Shmem(RefPtr<Segment>&& aSegment, id_t aId, size_t aSize, bool aUnsafe)
    : mSegment(std::move(aSegment)),
      mData(mSegment->Address()),
      mSize(aSize),
      mId(aId) {
#ifdef DEBUG
  mUnsafe = aUnsafe;
  Unprotect(mSegment);
#endif

  MOZ_RELEASE_ASSERT(mSegment->Size() >= mSize,
                     "illegal size in shared memory segment");
}

Shmem::Builder::Builder(size_t aSize) : mSize(aSize) {
  if (!aSize) {
    return;
  }
  size_t pageAlignedSize = shared_memory::PageAlignedSize(aSize);
  mHandle = shared_memory::Create(pageAlignedSize);
  if (!mHandle) {
    return;
  }
  auto mapping = mHandle.Map();
  if (!mapping) {
    return;
  }
  mSegment = MakeAndAddRef<Segment>(std::move(mapping));
}

std::tuple<UniquePtr<IPC::Message>, Shmem> Shmem::Builder::Build(
    id_t aId, bool aUnsafe, int32_t aRoutingId) {
  Shmem shmem(std::move(mSegment), aId, mSize, aUnsafe);
  shmem.AssertInvariants();
  MOZ_ASSERT(mHandle, "null shmem handle");

  auto msg = MakeUnique<ShmemCreated>(aRoutingId, aId, std::move(mHandle));
  return std::make_tuple(std::move(msg), std::move(shmem));
}

// static
already_AddRefed<Shmem::Segment> Shmem::OpenExisting(
    const IPC::Message& aDescriptor, id_t* aId, bool /*unused*/) {
  if (SHMEM_CREATED_MESSAGE_TYPE != aDescriptor.type()) {
    NS_ERROR("expected 'shmem created' message");
    return nullptr;
  }
  MutableSharedMemoryHandle handle;
  IPC::MessageReader reader(aDescriptor);
  if (!ShmemCreated::ReadInfo(&reader, aId, &handle)) {
    return nullptr;
  }
  reader.EndRead();
  if (!handle) {
    return nullptr;
  }

  auto mapping = handle.Map();
  if (!mapping) {
    return nullptr;
  }
  return MakeAndAddRef<Shmem::Segment>(std::move(mapping));
}

UniquePtr<IPC::Message> Shmem::MkDestroyedMessage(int32_t routingId) {
  AssertInvariants();
  return MakeUnique<ShmemDestroyed>(routingId, mId);
}

void IPDLParamTraits<Shmem>::Write(IPC::MessageWriter* aWriter,
                                   IProtocol* aActor, Shmem&& aParam) {
  WriteIPDLParam(aWriter, aActor, aParam.mId);
  WriteIPDLParam(aWriter, aActor, uint32_t(aParam.mSize));
#ifdef DEBUG
  WriteIPDLParam(aWriter, aActor, aParam.mUnsafe);
#endif

  aParam.RevokeRights();
  aParam.forget();
}

bool IPDLParamTraits<Shmem>::Read(IPC::MessageReader* aReader,
                                  IProtocol* aActor, paramType* aResult) {
  paramType::id_t id;
  uint32_t size;
  if (!ReadIPDLParam(aReader, aActor, &id) ||
      !ReadIPDLParam(aReader, aActor, &size)) {
    return false;
  }

  bool unsafe = false;
#ifdef DEBUG
  if (!ReadIPDLParam(aReader, aActor, &unsafe)) {
    return false;
  }
#endif

  auto* segment = aActor->LookupSharedMemory(id);
  if (segment) {
    if (size > segment->Size()) {
      return false;
    }

    *aResult = Shmem(segment, id, size, unsafe);
    return true;
  }
  *aResult = Shmem();
  return true;
}

}  // namespace ipc
}  // namespace mozilla
