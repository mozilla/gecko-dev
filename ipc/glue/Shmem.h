/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Shmem_h
#define mozilla_ipc_Shmem_h

#include "mozilla/Attributes.h"

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/ipc_message_utils.h"

#include "nsISupports.h"
#include "nscore.h"
#include "nsDebug.h"

#include "mozilla/ipc/SharedMemoryMapping.h"
#include "mozilla/Range.h"
#include "mozilla/UniquePtr.h"

/**
 * |Shmem| is one agent in the IPDL shared memory scheme.  The way it
    works is essentially
 *
 *  (1) C++ code calls, say, |parentActor->AllocShmem(size)|

 *  (2) IPDL-generated code creates a |mozilla::ipc::SharedMemoryMapping|
 *  wrapping the bare OS shmem primitives.  The code then adds the new
 *  SharedMemory to the set of shmem segments being managed by IPDL.
 *
 *  (3) IPDL-generated code "shares" the new SharedMemory to the child
 *  process, and then sends a special asynchronous IPC message to the
 *  child notifying it of the creation of the segment.  (What this
 *  means is OS specific.)
 *
 *  (4a) The child receives the special IPC message, and using the
 *  |MutableSharedMemoryHandle| it was passed, creates a |SharedMemoryMapping|
 *  in the child process.
 *
 *  (4b) After sending the "shmem-created" IPC message, IPDL-generated
 *  code in the parent returns a |mozilla::ipc::Shmem| back to the C++
 *  caller of |parentActor->AllocShmem()|.  The |Shmem| is a "weak
 *  reference" to the underlying |SharedMemory|, which is managed by
 *  IPDL-generated code.  C++ consumers of |Shmem| can't get at the
 *  underlying |SharedMemoryMapping|.
 *
 * If parent code wants to give access rights to the Shmem to the
 * child, it does so by sending its |Shmem| to the child, in an IPDL
 * message.  The parent's |Shmem| then "dies", i.e. becomes
 * inaccessible.  This process could be compared to passing a
 * "shmem-access baton" between parent and child.
 */

namespace mozilla::ipc {

class IProtocol;
class IToplevelProtocol;

template <typename P>
struct IPDLParamTraits;

class Shmem final {
  friend struct IPDLParamTraits<Shmem>;
  friend class IProtocol;
  friend class IToplevelProtocol;

 public:
  using id_t = int32_t;
  // Low-level wrapper around platform shmem primitives.
  class Segment final : public SharedMemoryMapping {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Segment);

    explicit Segment(SharedMemoryMapping&& aMapping)
        : SharedMemoryMapping(std::move(aMapping)) {}

   private:
    ~Segment() = default;
  };

  class Builder {
   public:
    explicit Builder(size_t aSize);

    explicit operator bool() const { return mSegment && mSegment->IsValid(); }

    // Prepare this to be shared with another process. Return an IPC message
    // that contains enough information for the other process to map this
    // segment in OpenExisting(), and the shmem.
    std::tuple<UniquePtr<IPC::Message>, Shmem> Build(id_t aId, bool aUnsafe,
                                                     int32_t aRoutingId);

   private:
    size_t mSize;
    MutableSharedMemoryHandle mHandle;
    RefPtr<Segment> mSegment;
  };

  Shmem() : mSegment(nullptr), mData(nullptr), mSize(0), mId(0) {}
  Shmem(const Shmem& aOther) = default;
  ~Shmem() { forget(); }

  Shmem& operator=(const Shmem& aRhs) = default;

  bool operator==(const Shmem& aRhs) const { return mSegment == aRhs.mSegment; }

  // Returns whether this Shmem is writable by you, and thus whether you can
  // transfer writability to another actor.
  bool IsWritable() const { return mSegment != nullptr; }

  // Returns whether this Shmem is readable by you, and thus whether you can
  // transfer readability to another actor.
  bool IsReadable() const { return mSegment != nullptr; }

  // Return a pointer to the user-visible data segment.
  template <typename T>
  T* get() const {
    AssertInvariants();
    AssertAligned<T>();

    return reinterpret_cast<T*>(mData);
  }

  // Return the size of the segment as requested when this shmem
  // segment was allocated, in units of T.  The underlying mapping may
  // actually be larger because of page alignment and private data,
  // but this isn't exposed to clients.
  template <typename T>
  size_t Size() const {
    AssertInvariants();
    AssertAligned<T>();

    return mSize / sizeof(T);
  }

  template <typename T>
  Range<T> Range() const {
    return {get<T>(), Size<T>()};
  }

 private:
  // These shouldn't be used directly, use the IPDL interface instead.

  Shmem(RefPtr<Segment>&& aSegment, id_t aId, size_t aSize, bool aUnsafe);

  id_t Id() const { return mId; }

  Segment* GetSegment() const { return mSegment; }

#ifndef DEBUG
  void RevokeRights() {}
#else
  void RevokeRights();
#endif

  void forget() {
    mSegment = nullptr;
    mData = nullptr;
    mSize = 0;
    mId = 0;
#ifdef DEBUG
    mUnsafe = false;
#endif
  }

  // Stop sharing this with another process. Return an IPC message that
  // contains enough information for the other process to unmap this
  // segment.  Return a new message if successful (owned by the
  // caller), nullptr if not.
  UniquePtr<IPC::Message> MkDestroyedMessage(int32_t routingId);

  // Return a Segment instance in this process using the descriptor shared
  // to us by the process that created the underlying OS shmem resource.  The
  // contents of the descriptor depend on the type of SharedMemory that was
  // passed to us.
  static already_AddRefed<Segment> OpenExisting(const IPC::Message& aDescriptor,
                                                id_t* aId,
                                                bool aProtect = false);

  template <typename T>
  void AssertAligned() const {
    if (0 != (mSize % sizeof(T))) MOZ_CRASH("shmem is not T-aligned");
  }

#if !defined(DEBUG)
  void AssertInvariants() const {}
#else
  void AssertInvariants() const;
#endif

  RefPtr<Segment> mSegment;
  void* mData;
  size_t mSize;
  id_t mId;
#ifdef DEBUG
  bool mUnsafe = false;
#endif
};

}  // namespace mozilla::ipc

#endif  // ifndef mozilla_ipc_Shmem_h
