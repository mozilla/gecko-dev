/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobChild.h"
#include "BlobParent.h"

#include "BackgroundParent.h"
#include "ContentChild.h"
#include "ContentParent.h"
#include "FileDescriptorSetChild.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/unused.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PBlobStreamChild.h"
#include "mozilla/dom/PBlobStreamParent.h"
#include "mozilla/dom/indexedDB/FileSnapshot.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "MultipartBlobImpl.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsID.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIRemoteBlob.h"
#include "nsISeekableStream.h"
#include "nsIUUIDGenerator.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "WorkerPrivate.h"

#ifdef DEBUG
#include "BackgroundChild.h" // BackgroundChild::GetForCurrentThread().
#endif

#ifdef OS_POSIX
#include "chrome/common/file_descriptor_set_posix.h"
#endif

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#define PRIVATE_REMOTE_INPUT_STREAM_IID \
  {0x30c7699f, 0x51d2, 0x48c8, {0xad, 0x56, 0xc0, 0x16, 0xd7, 0x6f, 0x71, 0x27}}

namespace mozilla {
namespace dom {

using namespace mozilla::ipc;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::workers;

namespace {

const char kUUIDGeneratorContractId[] = "@mozilla.org/uuid-generator;1";

const uint32_t kMaxFileDescriptorsPerMessage = 250;

#ifdef OS_POSIX
// Keep this in sync with other platforms.
static_assert(FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE == 250,
              "MAX_DESCRIPTORS_PER_MESSAGE mismatch!");
#endif

StaticRefPtr<nsIUUIDGenerator> gUUIDGenerator;

GeckoProcessType gProcessType = GeckoProcessType_Invalid;

void
CommonStartup()
{
  MOZ_ASSERT(NS_IsMainThread());

  gProcessType = XRE_GetProcessType();
  MOZ_ASSERT(gProcessType != GeckoProcessType_Invalid);

  nsCOMPtr<nsIUUIDGenerator> uuidGen = do_GetService(kUUIDGeneratorContractId);
  MOZ_RELEASE_ASSERT(uuidGen);

  gUUIDGenerator = uuidGen;
  ClearOnShutdown(&gUUIDGenerator);
}

template <class ManagerType>
struct ConcreteManagerTypeTraits;

template <>
struct ConcreteManagerTypeTraits<nsIContentChild>
{
  typedef ContentChild Type;
};

template <>
struct ConcreteManagerTypeTraits<PBackgroundChild>
{
  typedef PBackgroundChild Type;
};

template <>
struct ConcreteManagerTypeTraits<nsIContentParent>
{
  typedef ContentParent Type;
};

template <>
struct ConcreteManagerTypeTraits<PBackgroundParent>
{
  typedef PBackgroundParent Type;
};

void
AssertCorrectThreadForManager(nsIContentChild* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
AssertCorrectThreadForManager(nsIContentParent* aManager)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());
}

void
AssertCorrectThreadForManager(PBackgroundChild* aManager)
{
#ifdef DEBUG
  if (aManager) {
    PBackgroundChild* backgroundChild = BackgroundChild::GetForCurrentThread();
    MOZ_ASSERT(backgroundChild);
    MOZ_ASSERT(backgroundChild == aManager);
  }
#endif
}

void
AssertCorrectThreadForManager(PBackgroundParent* aManager)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);
  AssertIsOnBackgroundThread();
}

intptr_t
ActorManagerProcessID(nsIContentParent* aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return reinterpret_cast<intptr_t>(aManager);
}

intptr_t
ActorManagerProcessID(PBackgroundParent* aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return BackgroundParent::GetRawContentParentForComparison(aManager);
}

bool
ActorManagerIsSameProcess(nsIContentParent* aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return false;
}

bool
ActorManagerIsSameProcess(PBackgroundParent* aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return !BackgroundParent::IsOtherProcessActor(aManager);
}

bool
EventTargetIsOnCurrentThread(nsIEventTarget* aEventTarget)
{
  if (!aEventTarget) {
    return NS_IsMainThread();
  }

  bool current;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aEventTarget->IsOnCurrentThread(&current)));

  return current;
}

class CancelableRunnableWrapper final
  : public nsCancelableRunnable
{
  nsCOMPtr<nsIRunnable> mRunnable;
#ifdef DEBUG
  nsCOMPtr<nsIEventTarget> mDEBUGEventTarget;
#endif

public:
  CancelableRunnableWrapper(nsIRunnable* aRunnable,
                            nsIEventTarget* aEventTarget)
    : mRunnable(aRunnable)
#ifdef DEBUG
    , mDEBUGEventTarget(aEventTarget)
#endif
  {
    MOZ_ASSERT(aRunnable);
    MOZ_ASSERT(aEventTarget);
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~CancelableRunnableWrapper()
  { }

  NS_DECL_NSIRUNNABLE
  NS_DECL_NSICANCELABLERUNNABLE
};

NS_IMPL_ISUPPORTS_INHERITED0(CancelableRunnableWrapper, nsCancelableRunnable)

NS_IMETHODIMP
CancelableRunnableWrapper::Run()
{
  DebugOnly<bool> onTarget;
  MOZ_ASSERT(mDEBUGEventTarget);
  MOZ_ASSERT(NS_SUCCEEDED(mDEBUGEventTarget->IsOnCurrentThread(&onTarget)));
  MOZ_ASSERT(onTarget);

  nsCOMPtr<nsIRunnable> runnable;
  mRunnable.swap(runnable);

  if (runnable) {
    return runnable->Run();
  }

  return NS_OK;
}

NS_IMETHODIMP
CancelableRunnableWrapper::Cancel()
{
  DebugOnly<bool> onTarget;
  MOZ_ASSERT(mDEBUGEventTarget);
  MOZ_ASSERT(NS_SUCCEEDED(mDEBUGEventTarget->IsOnCurrentThread(&onTarget)));
  MOZ_ASSERT(onTarget);

  if (NS_WARN_IF(!mRunnable)) {
    return NS_ERROR_UNEXPECTED;
  }

  unused << Run();
  MOZ_ASSERT(!mRunnable);

  return NS_OK;
}

// Ensure that a nsCOMPtr/nsRefPtr is released on the target thread.
template <template <class> class SmartPtr, class T>
void
ReleaseOnTarget(SmartPtr<T>& aDoomed, nsIEventTarget* aTarget)
{
  MOZ_ASSERT(aDoomed);
  MOZ_ASSERT(!EventTargetIsOnCurrentThread(aTarget));

  T* doomedRaw;
  aDoomed.forget(&doomedRaw);

  auto* doomedSupports = static_cast<nsISupports*>(doomedRaw);

  nsCOMPtr<nsIRunnable> releaseRunnable =
    NS_NewNonOwningRunnableMethod(doomedSupports, &nsISupports::Release);
  MOZ_ASSERT(releaseRunnable);

  if (aTarget) {
    // If we're targeting a non-main thread then make sure the runnable is
    // cancelable.
    releaseRunnable = new CancelableRunnableWrapper(releaseRunnable, aTarget);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aTarget->Dispatch(releaseRunnable,
                                                   NS_DISPATCH_NORMAL)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(releaseRunnable)));
  }
}

template <class ManagerType>
void
ConstructFileDescriptorSet(ManagerType* aManager,
                           nsTArray<FileDescriptor>& aFDs,
                           OptionalFileDescriptorSet& aOptionalFDSet)
{
  typedef typename ConcreteManagerTypeTraits<ManagerType>::Type
          ConcreteManagerType;

  MOZ_ASSERT(aManager);

  if (aFDs.IsEmpty()) {
    aOptionalFDSet = void_t();
    return;
  }

  if (aFDs.Length() <= kMaxFileDescriptorsPerMessage) {
    aOptionalFDSet = nsTArray<FileDescriptor>();
    aOptionalFDSet.get_ArrayOfFileDescriptor().SwapElements(aFDs);
    return;
  }

  auto* concreteManager = static_cast<ConcreteManagerType*>(aManager);

  PFileDescriptorSetParent* fdSet =
    concreteManager->SendPFileDescriptorSetConstructor(aFDs[0]);
  if (!fdSet) {
    aOptionalFDSet = void_t();
    return;
  }

  for (uint32_t index = 1; index < aFDs.Length(); index++) {
    if (!fdSet->SendAddFileDescriptor(aFDs[index])) {
      aOptionalFDSet = void_t();
      return;
    }
  }

  aOptionalFDSet = fdSet;
}

void
OptionalFileDescriptorSetToFDs(OptionalFileDescriptorSet& aOptionalSet,
                               nsTArray<FileDescriptor>& aFDs)
{
  MOZ_ASSERT(aFDs.IsEmpty());

  switch (aOptionalSet.type()) {
    case OptionalFileDescriptorSet::Tvoid_t:
      return;

    case OptionalFileDescriptorSet::TArrayOfFileDescriptor:
      aOptionalSet.get_ArrayOfFileDescriptor().SwapElements(aFDs);
      return;

    case OptionalFileDescriptorSet::TPFileDescriptorSetChild: {
      FileDescriptorSetChild* fdSetActor =
        static_cast<FileDescriptorSetChild*>(
          aOptionalSet.get_PFileDescriptorSetChild());
      MOZ_ASSERT(fdSetActor);

      fdSetActor->ForgetFileDescriptors(aFDs);
      MOZ_ASSERT(!aFDs.IsEmpty());

      PFileDescriptorSetChild::Send__delete__(fdSetActor);
      return;
    }

    default:
      MOZ_CRASH("Unknown type!");
  }

  MOZ_CRASH("Should never get here!");
}

class NS_NO_VTABLE IPrivateRemoteInputStream
  : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(PRIVATE_REMOTE_INPUT_STREAM_IID)

  // This will return the underlying stream.
  virtual nsIInputStream*
  BlockAndGetInternalStream() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IPrivateRemoteInputStream,
                              PRIVATE_REMOTE_INPUT_STREAM_IID)

// This class exists to keep a blob alive at least as long as its internal
// stream.
class BlobInputStreamTether final
  : public nsIMultiplexInputStream
  , public nsISeekableStream
  , public nsIIPCSerializableInputStream
{
  nsCOMPtr<nsIInputStream> mStream;
  nsRefPtr<BlobImpl> mBlobImpl;

  nsIMultiplexInputStream* mWeakMultiplexStream;
  nsISeekableStream* mWeakSeekableStream;
  nsIIPCSerializableInputStream* mWeakSerializableStream;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_NSIINPUTSTREAM(mStream->)
  NS_FORWARD_SAFE_NSIMULTIPLEXINPUTSTREAM(mWeakMultiplexStream)
  NS_FORWARD_SAFE_NSISEEKABLESTREAM(mWeakSeekableStream)
  NS_FORWARD_SAFE_NSIIPCSERIALIZABLEINPUTSTREAM(mWeakSerializableStream)

  BlobInputStreamTether(nsIInputStream* aStream, BlobImpl* aBlobImpl)
    : mStream(aStream)
    , mBlobImpl(aBlobImpl)
    , mWeakMultiplexStream(nullptr)
    , mWeakSeekableStream(nullptr)
    , mWeakSerializableStream(nullptr)
  {
    MOZ_ASSERT(aStream);
    MOZ_ASSERT(aBlobImpl);

    nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_QueryInterface(aStream);
    if (multiplexStream) {
      MOZ_ASSERT(SameCOMIdentity(aStream, multiplexStream));
      mWeakMultiplexStream = multiplexStream;
    }

    nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(aStream);
    if (seekableStream) {
      MOZ_ASSERT(SameCOMIdentity(aStream, seekableStream));
      mWeakSeekableStream = seekableStream;
    }

    nsCOMPtr<nsIIPCSerializableInputStream> serializableStream =
      do_QueryInterface(aStream);
    if (serializableStream) {
      MOZ_ASSERT(SameCOMIdentity(aStream, serializableStream));
      mWeakSerializableStream = serializableStream;
    }
  }

private:
  ~BlobInputStreamTether()
  { }
};

NS_IMPL_ADDREF(BlobInputStreamTether)
NS_IMPL_RELEASE(BlobInputStreamTether)

NS_INTERFACE_MAP_BEGIN(BlobInputStreamTether)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMultiplexInputStream,
                                     mWeakMultiplexStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, mWeakSeekableStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mWeakSerializableStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

class RemoteInputStream final
  : public nsIInputStream
  , public nsISeekableStream
  , public nsIIPCSerializableInputStream
  , public IPrivateRemoteInputStream
{
  Monitor mMonitor;
  BlobChild* mActor;
  nsCOMPtr<nsIInputStream> mStream;
  nsRefPtr<BlobImpl> mBlobImpl;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  nsISeekableStream* mWeakSeekableStream;
  uint64_t mStart;
  uint64_t mLength;

public:
  RemoteInputStream(BlobImpl* aBlobImpl,
                    uint64_t aStart,
                    uint64_t aLength);

  RemoteInputStream(BlobChild* aActor,
                    BlobImpl* aBlobImpl,
                    uint64_t aStart,
                    uint64_t aLength);

  bool
  IsOnOwningThread() const
  {
    return EventTargetIsOnCurrentThread(mEventTarget);
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnOwningThread());
  }

  bool
  IsWorkerStream() const
  {
    return !!mActor;
  }

  void
  SetStream(nsIInputStream* aStream);

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~RemoteInputStream();

  nsresult
  BlockAndWaitForStream();

  void
  ReallyBlockAndWaitForStream();

  bool
  IsSeekableStream();

  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

  virtual nsIInputStream*
  BlockAndGetInternalStream() override;
};

class InputStreamChild final
  : public PBlobStreamChild
{
  nsRefPtr<RemoteInputStream> mRemoteStream;

public:
  explicit
  InputStreamChild(RemoteInputStream* aRemoteStream)
    : mRemoteStream(aRemoteStream)
  {
    MOZ_ASSERT(aRemoteStream);
    aRemoteStream->AssertIsOnOwningThread();
  }

  InputStreamChild()
  { }

  ~InputStreamChild()
  { }

private:
  // This method is only called by the IPDL message machinery.
  virtual bool
  Recv__delete__(const InputStreamParams& aParams,
                 const OptionalFileDescriptorSet& aFDs) override;
};

class InputStreamParent final
  : public PBlobStreamParent
{
  typedef mozilla::ipc::InputStreamParams InputStreamParams;
  typedef mozilla::ipc::OptionalFileDescriptorSet OptionalFileDescriptorSet;

  bool* mSyncLoopGuard;
  InputStreamParams* mParams;
  OptionalFileDescriptorSet* mFDs;

#ifdef DEBUG
  PRThread* mOwningThread;
#endif

public:
  InputStreamParent()
    : mSyncLoopGuard(nullptr)
    , mParams(nullptr)
    , mFDs(nullptr)
  {
#ifdef DEBUG
    mOwningThread = PR_GetCurrentThread();
#endif

    AssertIsOnOwningThread();

    MOZ_COUNT_CTOR(InputStreamParent);
  }

  InputStreamParent(bool* aSyncLoopGuard,
                    InputStreamParams* aParams,
                    OptionalFileDescriptorSet* aFDs)
    : mSyncLoopGuard(aSyncLoopGuard)
    , mParams(aParams)
    , mFDs(aFDs)
  {
#ifdef DEBUG
    mOwningThread = PR_GetCurrentThread();
#endif

    AssertIsOnOwningThread();
    MOZ_ASSERT(aSyncLoopGuard);
    MOZ_ASSERT(!*aSyncLoopGuard);
    MOZ_ASSERT(aParams);
    MOZ_ASSERT(aFDs);

    MOZ_COUNT_CTOR(InputStreamParent);
  }

  ~InputStreamParent()
  {
    AssertIsOnOwningThread();

    MOZ_COUNT_DTOR(InputStreamParent);
  }

  void
  AssertIsOnOwningThread() const
  {
#ifdef DEBUG
    MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
#endif
  }

  bool
  Destroy(const InputStreamParams& aParams,
          const OptionalFileDescriptorSet& aFDs)
  {
    AssertIsOnOwningThread();

    if (mSyncLoopGuard) {
      MOZ_ASSERT(!*mSyncLoopGuard);

      *mSyncLoopGuard = true;
      *mParams = aParams;
      *mFDs = aFDs;

      // We're not a live actor so manage the memory ourselves.
      delete this;
      return true;
    }

    // This will be destroyed by BlobParent::DeallocPBlobStreamParent.
    return PBlobStreamParent::Send__delete__(this, aParams, aFDs);
  }

private:
  // This method is only called by the IPDL message machinery.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override
  {
    // Nothing needs to be done here.
  }
};

class EmptyBlobImpl final
  : public BlobImplBase
{
public:
  explicit EmptyBlobImpl(const nsAString& aContentType)
    : BlobImplBase(aContentType, 0)
  {
    mImmutable = true;
  }

  EmptyBlobImpl(const nsAString& aName,
                const nsAString& aContentType,
                int64_t aLastModifiedDate)
    : BlobImplBase(aName, aContentType, 0, aLastModifiedDate)
  {
    mImmutable = true;
  }

private:
  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t /* aStart */,
              uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& /* aRv */) override
  {
    MOZ_ASSERT(!aLength);

    nsRefPtr<BlobImpl> sliceImpl = new EmptyBlobImpl(aContentType);

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(sliceImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);

    return sliceImpl.forget();
  }

  virtual void
  GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv) override
  {
    if (NS_WARN_IF(!aStream)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsString emptyString;
    aRv = NS_NewStringInputStream(aStream, emptyString);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }
};

// This is only needed for IndexedDB BlobImplSnapshot.
class SameProcessInputStreamBlobImpl final
  : public BlobImplBase
{
  nsCOMPtr<nsIInputStream> mInputStream;

public:
  SameProcessInputStreamBlobImpl(const nsAString& aContentType,
                                 uint64_t aLength,
                                 nsIInputStream* aInputStream)
    : BlobImplBase(aContentType, aLength)
    , mInputStream(aInputStream)
  {
    MOZ_ASSERT(aLength != UINT64_MAX);
    MOZ_ASSERT(aInputStream);

    mImmutable = true;
  }

  SameProcessInputStreamBlobImpl(const nsAString& aName,
                                 const nsAString& aContentType,
                                 uint64_t aLength,
                                 int64_t aLastModifiedDate,
                                 nsIInputStream* aInputStream)
    : BlobImplBase(aName, aContentType, aLength, aLastModifiedDate)
    , mInputStream(aInputStream)
  {
    MOZ_ASSERT(aLength != UINT64_MAX);
    MOZ_ASSERT(aLastModifiedDate != INT64_MAX);
    MOZ_ASSERT(aInputStream);

    mImmutable = true;
  }

private:
  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t /* aStart */,
              uint64_t /* aLength */,
              const nsAString& /* aContentType */,
              ErrorResult& /* aRv */) override
  {
    MOZ_CRASH("Not implemented");
  }

  virtual void
  GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv) override
  {
    if (NS_WARN_IF(!aStream)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsCOMPtr<nsIInputStream> inputStream = mInputStream;
    inputStream.forget(aStream);
  }
};

struct MOZ_STACK_CLASS CreateBlobImplMetadata final
{
  nsString mContentType;
  nsString mName;
  uint64_t mLength;
  int64_t mLastModifiedDate;
  bool mHasRecursed;
  const bool mIsSameProcessActor;

  explicit CreateBlobImplMetadata(bool aIsSameProcessActor)
    : mLength(0)
    , mLastModifiedDate(0)
    , mHasRecursed(false)
    , mIsSameProcessActor(aIsSameProcessActor)
  {
    MOZ_COUNT_CTOR(CreateBlobImplMetadata);

    mName.SetIsVoid(true);
  }

  ~CreateBlobImplMetadata()
  {
    MOZ_COUNT_DTOR(CreateBlobImplMetadata);
  }

  bool
  IsFile() const
  {
    return !mName.IsVoid();
  }
};

already_AddRefed<BlobImpl>
CreateBlobImpl(const nsID& aKnownBlobIDData,
               const CreateBlobImplMetadata& aMetadata)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);
  MOZ_ASSERT(aMetadata.mHasRecursed);

  nsRefPtr<BlobImpl> blobImpl = BlobParent::GetBlobImplForID(aKnownBlobIDData);
  if (NS_WARN_IF(!blobImpl)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  return blobImpl.forget();
}

already_AddRefed<BlobImpl>
CreateBlobImpl(const nsTArray<uint8_t>& aMemoryData,
               const CreateBlobImplMetadata& aMetadata)
{
  static_assert(sizeof(aMemoryData.Length()) <= sizeof(size_t),
                "String length won't fit in size_t!");
  static_assert(sizeof(size_t) <= sizeof(uint64_t),
                "size_t won't fit in uint64_t!");

  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

  nsRefPtr<BlobImpl> blobImpl;

  if (auto length = static_cast<size_t>(aMemoryData.Length())) {
    static MOZ_CONSTEXPR_VAR size_t elementSizeMultiplier =
      sizeof(aMemoryData[0]) / sizeof(char);

    if (!aMetadata.mHasRecursed &&
        NS_WARN_IF(aMetadata.mLength != uint64_t(length))) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }

    void* buffer = malloc(length * elementSizeMultiplier);
    if (NS_WARN_IF(!buffer)) {
      return nullptr;
    }

    memcpy(buffer, aMemoryData.Elements(), length * elementSizeMultiplier);

    if (!aMetadata.mHasRecursed && aMetadata.IsFile()) {
      blobImpl =
        new BlobImplMemory(buffer,
                           uint64_t(length),
                           aMetadata.mName,
                           aMetadata.mContentType,
                           aMetadata.mLastModifiedDate);
    } else {
      blobImpl =
        new BlobImplMemory(buffer, uint64_t(length), aMetadata.mContentType);
    }
  } else if (!aMetadata.mHasRecursed && aMetadata.IsFile()) {
    blobImpl =
      new EmptyBlobImpl(aMetadata.mName,
                        aMetadata.mContentType,
                        aMetadata.mLastModifiedDate);
  } else {
    blobImpl = new EmptyBlobImpl(aMetadata.mContentType);
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(blobImpl->SetMutable(false)));

  return blobImpl.forget();
}

already_AddRefed<BlobImpl>
CreateBlobImpl(intptr_t aAddRefedInputStream,
               const CreateBlobImplMetadata& aMetadata)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);
  MOZ_ASSERT(aMetadata.mIsSameProcessActor);
  MOZ_ASSERT(aAddRefedInputStream);

  nsCOMPtr<nsIInputStream> inputStream =
    dont_AddRef(
      reinterpret_cast<nsIInputStream*>(aAddRefedInputStream));

  nsRefPtr<BlobImpl> blobImpl;
  if (!aMetadata.mHasRecursed && aMetadata.IsFile()) {
    blobImpl =
      new SameProcessInputStreamBlobImpl(aMetadata.mName,
                                         aMetadata.mContentType,
                                         aMetadata.mLength,
                                         aMetadata.mLastModifiedDate,
                                         inputStream);
  } else {
    blobImpl =
      new SameProcessInputStreamBlobImpl(aMetadata.mContentType,
                                         aMetadata.mLength,
                                         inputStream);
  }

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  return blobImpl.forget();
}

already_AddRefed<BlobImpl>
CreateBlobImpl(const nsTArray<BlobData>& aBlobData,
               CreateBlobImplMetadata& aMetadata);

already_AddRefed<BlobImpl>
CreateBlobImplFromBlobData(const BlobData& aBlobData,
                           CreateBlobImplMetadata& aMetadata)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

  nsRefPtr<BlobImpl> blobImpl;

  switch (aBlobData.type()) {
    case BlobData::TnsID: {
      blobImpl = CreateBlobImpl(aBlobData.get_nsID(), aMetadata);
      break;
    }

    case BlobData::TArrayOfuint8_t: {
      blobImpl = CreateBlobImpl(aBlobData.get_ArrayOfuint8_t(), aMetadata);
      break;
    }

    case BlobData::Tintptr_t: {
      blobImpl = CreateBlobImpl(aBlobData.get_intptr_t(), aMetadata);
      break;
    }

    case BlobData::TArrayOfBlobData: {
      blobImpl = CreateBlobImpl(aBlobData.get_ArrayOfBlobData(), aMetadata);
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  return blobImpl.forget();
}

already_AddRefed<BlobImpl>
CreateBlobImpl(const nsTArray<BlobData>& aBlobDatas,
               CreateBlobImplMetadata& aMetadata)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

  // Special case for a multipart blob with only one part.
  if (aBlobDatas.Length() == 1) {
    const BlobData& blobData = aBlobDatas[0];

    nsRefPtr<BlobImpl> blobImpl =
      CreateBlobImplFromBlobData(blobData, aMetadata);
    if (NS_WARN_IF(!blobImpl)) {
      return nullptr;
    }

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);

    return blobImpl.forget();
  }

  FallibleTArray<nsRefPtr<BlobImpl>> fallibleBlobImpls;
  if (NS_WARN_IF(!fallibleBlobImpls.SetLength(aBlobDatas.Length(), fallible))) {
    return nullptr;
  }

  nsTArray<nsRefPtr<BlobImpl>> blobImpls;
  fallibleBlobImpls.SwapElements(blobImpls);

  const bool hasRecursed = aMetadata.mHasRecursed;
  aMetadata.mHasRecursed = true;

  for (uint32_t count = aBlobDatas.Length(), index = 0;
       index < count;
       index++) {
    const BlobData& blobData = aBlobDatas[index];
    nsRefPtr<BlobImpl>& blobImpl = blobImpls[index];

    blobImpl = CreateBlobImplFromBlobData(blobData, aMetadata);
    if (NS_WARN_IF(!blobImpl)) {
      return nullptr;
    }

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);
  }

  nsRefPtr<BlobImpl> blobImpl;
  if (!hasRecursed && aMetadata.IsFile()) {
    blobImpl =
      new MultipartBlobImpl(blobImpls, aMetadata.mName, aMetadata.mContentType);
  } else {
    blobImpl =
      new MultipartBlobImpl(blobImpls, aMetadata.mContentType);
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(blobImpl->SetMutable(false)));

  return blobImpl.forget();
}

already_AddRefed<BlobImpl>
CreateBlobImpl(const ParentBlobConstructorParams& aParams,
               const BlobData& aBlobData,
               bool aIsSameProcessActor)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);
  MOZ_ASSERT(aParams.blobParams().type() ==
               AnyBlobConstructorParams::TNormalBlobConstructorParams ||
             aParams.blobParams().type() ==
               AnyBlobConstructorParams::TFileBlobConstructorParams);

  CreateBlobImplMetadata metadata(aIsSameProcessActor);

  if (aParams.blobParams().type() ==
        AnyBlobConstructorParams::TNormalBlobConstructorParams) {
    const NormalBlobConstructorParams& params =
      aParams.blobParams().get_NormalBlobConstructorParams();

    if (NS_WARN_IF(params.length() == UINT64_MAX)) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }

    metadata.mContentType = params.contentType();
    metadata.mLength = params.length();
  } else {
    const FileBlobConstructorParams& params =
      aParams.blobParams().get_FileBlobConstructorParams();

    if (NS_WARN_IF(params.length() == UINT64_MAX)) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }

    if (NS_WARN_IF(params.modDate() == INT64_MAX)) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }

    metadata.mContentType = params.contentType();
    metadata.mName = params.name();
    metadata.mLength = params.length();
    metadata.mLastModifiedDate = params.modDate();
  }

  nsRefPtr<BlobImpl> blobImpl =
    CreateBlobImplFromBlobData(aBlobData, metadata);
  return blobImpl.forget();
}

void
BlobDataFromBlobImpl(BlobImpl* aBlobImpl, BlobData& aBlobData)
{
  MOZ_ASSERT(gProcessType != GeckoProcessType_Default);
  MOZ_ASSERT(aBlobImpl);

  const nsTArray<nsRefPtr<BlobImpl>>* subBlobs = aBlobImpl->GetSubBlobImpls();

  if (subBlobs) {
    aBlobData = nsTArray<BlobData>();

    nsTArray<BlobData>& subBlobDatas = aBlobData.get_ArrayOfBlobData();
    subBlobDatas.SetLength(subBlobs->Length());

    for (uint32_t count = subBlobs->Length(), index = 0;
         index < count;
         index++) {
      BlobDataFromBlobImpl(subBlobs->ElementAt(index), subBlobDatas[index]);
    }

    return;
  }

  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlobImpl);
  if (remoteBlob) {
    BlobChild* actor = remoteBlob->GetBlobChild();
    MOZ_ASSERT(actor);

    aBlobData = actor->ParentID();
    return;
  }

  MOZ_ASSERT(aBlobImpl->IsMemoryFile());

  ErrorResult rv;
  nsCOMPtr<nsIInputStream> inputStream;
  aBlobImpl->GetInternalStream(getter_AddRefs(inputStream), rv);
  MOZ_ALWAYS_TRUE(!rv.Failed());

  DebugOnly<bool> isNonBlocking;
  MOZ_ASSERT(NS_SUCCEEDED(inputStream->IsNonBlocking(&isNonBlocking)));
  MOZ_ASSERT(isNonBlocking);

  uint64_t available;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(inputStream->Available(&available)));

  MOZ_ASSERT(available <= uint64_t(UINT32_MAX));

  aBlobData = nsTArray<uint8_t>();

  nsTArray<uint8_t>& blobData = aBlobData.get_ArrayOfuint8_t();

  blobData.SetLength(size_t(available));

  uint32_t readCount;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    inputStream->Read(reinterpret_cast<char*>(blobData.Elements()),
                      uint32_t(available),
                      &readCount)));
}

RemoteInputStream::RemoteInputStream(BlobImpl* aBlobImpl,
                                     uint64_t aStart,
                                     uint64_t aLength)
  : mMonitor("RemoteInputStream.mMonitor")
  , mActor(nullptr)
  , mBlobImpl(aBlobImpl)
  , mWeakSeekableStream(nullptr)
  , mStart(aStart)
  , mLength(aLength)
{
  MOZ_ASSERT(aBlobImpl);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  MOZ_ASSERT(IsOnOwningThread());
}

RemoteInputStream::RemoteInputStream(BlobChild* aActor,
                                     BlobImpl* aBlobImpl,
                                     uint64_t aStart,
                                     uint64_t aLength)
  : mMonitor("RemoteInputStream.mMonitor")
  , mActor(aActor)
  , mBlobImpl(aBlobImpl)
  , mEventTarget(NS_GetCurrentThread())
  , mWeakSeekableStream(nullptr)
  , mStart(aStart)
  , mLength(aLength)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aBlobImpl);

  MOZ_ASSERT(IsOnOwningThread());
}

RemoteInputStream::~RemoteInputStream()
{
  if (!IsOnOwningThread()) {
    mStream = nullptr;
    mWeakSeekableStream = nullptr;

    if (mBlobImpl) {
      ReleaseOnTarget(mBlobImpl, mEventTarget);
    }
  }
}

void
RemoteInputStream::SetStream(nsIInputStream* aStream)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStream);

  nsCOMPtr<nsIInputStream> stream = aStream;
  nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(aStream);

  MOZ_ASSERT_IF(seekableStream, SameCOMIdentity(aStream, seekableStream));

  {
    MonitorAutoLock lock(mMonitor);

    MOZ_ASSERT_IF(mStream, IsWorkerStream());

    if (!mStream) {
      MOZ_ASSERT(!mWeakSeekableStream);

      mStream.swap(stream);
      mWeakSeekableStream = seekableStream;

      mMonitor.Notify();
    }
  }
}

nsresult
RemoteInputStream::BlockAndWaitForStream()
{
  if (IsOnOwningThread()) {
    if (NS_IsMainThread()) {
      NS_WARNING("Blocking the main thread is not supported!");
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(IsWorkerStream());

    InputStreamParams params;
    OptionalFileDescriptorSet optionalFDs;

    mActor->SendBlobStreamSync(mStart, mLength, &params, &optionalFDs);

    nsTArray<FileDescriptor> fds;
    OptionalFileDescriptorSetToFDs(optionalFDs, fds);

    nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(params, fds);
    MOZ_ASSERT(stream);

    SetStream(stream);
    return NS_OK;
  }

  ReallyBlockAndWaitForStream();

  return NS_OK;
}

void
RemoteInputStream::ReallyBlockAndWaitForStream()
{
  MOZ_ASSERT(!IsOnOwningThread());

  DebugOnly<bool> waited;

  {
    MonitorAutoLock lock(mMonitor);

    waited = !mStream;

    while (!mStream) {
      mMonitor.Wait();
    }
  }

  MOZ_ASSERT(mStream);

#ifdef DEBUG
  if (waited && mWeakSeekableStream) {
    int64_t position;
    MOZ_ASSERT(NS_SUCCEEDED(mWeakSeekableStream->Tell(&position)),
                "Failed to determine initial stream position!");
    MOZ_ASSERT(!position, "Stream not starting at 0!");
  }
#endif
}

bool
RemoteInputStream::IsSeekableStream()
{
  if (IsOnOwningThread()) {
    if (!mStream) {
      NS_WARNING("Don't know if this stream is seekable yet!");
      return true;
    }
  } else {
    ReallyBlockAndWaitForStream();
  }

  return !!mWeakSeekableStream;
}

NS_IMPL_ADDREF(RemoteInputStream)
NS_IMPL_RELEASE(RemoteInputStream)

NS_INTERFACE_MAP_BEGIN(RemoteInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, IsSeekableStream())
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(IPrivateRemoteInputStream)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
RemoteInputStream::Close()
{
  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<BlobImpl> blobImpl;
  mBlobImpl.swap(blobImpl);

  rv = mStream->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::Available(uint64_t* aAvailable)
{
  if (!IsOnOwningThread()) {
    nsresult rv = BlockAndWaitForStream();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->Available(aAvailable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG
  if (NS_IsMainThread()) {
    NS_WARNING("Someone is trying to do main-thread I/O...");
  }
#endif

  nsresult rv;

  // See if we already have our real stream.
  nsCOMPtr<nsIInputStream> inputStream;
  {
    MonitorAutoLock lock(mMonitor);

    inputStream = mStream;
  }

  // If we do then just call through.
  if (inputStream) {
    rv = inputStream->Available(aAvailable);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // If the stream is already closed then we can't do anything.
  if (!mBlobImpl) {
    return NS_BASE_STREAM_CLOSED;
  }

  // Otherwise fake it...
  NS_WARNING("Available() called before real stream has been delivered, "
              "guessing the amount of data available!");

  ErrorResult error;
  *aAvailable = mBlobImpl->GetSize(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aResult)
{
  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStream->Read(aBuffer, aCount, aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                void* aClosure,
                                uint32_t aCount,
                                uint32_t* aResult)
{
  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::IsNonBlocking(bool* aNonBlocking)
{
  NS_ENSURE_ARG_POINTER(aNonBlocking);

  *aNonBlocking = false;
  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mWeakSeekableStream) {
    NS_WARNING("Underlying blob stream is not seekable!");
    return NS_ERROR_NO_INTERFACE;
  }

  rv = mWeakSeekableStream->Seek(aWhence, aOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::Tell(int64_t* aResult)
{
  // We can cheat here and assume that we're going to start at 0 if we don't yet
  // have our stream. Though, really, this should abort since most input streams
  // could block here.
  if (IsOnOwningThread() && !mStream) {
    *aResult = 0;
    return NS_OK;
  }

  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mWeakSeekableStream) {
    NS_WARNING("Underlying blob stream is not seekable!");
    return NS_ERROR_NO_INTERFACE;
  }

  rv = mWeakSeekableStream->Tell(aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
RemoteInputStream::SetEOF()
{
  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mWeakSeekableStream) {
    NS_WARNING("Underlying blob stream is not seekable!");
    return NS_ERROR_NO_INTERFACE;
  }

  rv = mWeakSeekableStream->SetEOF();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
RemoteInputStream::Serialize(InputStreamParams& aParams,
                             FileDescriptorArray& /* aFDs */)
{
  MOZ_RELEASE_ASSERT(mBlobImpl);

  nsCOMPtr<nsIRemoteBlob> remote = do_QueryInterface(mBlobImpl);
  MOZ_ASSERT(remote);

  BlobChild* actor = remote->GetBlobChild();
  MOZ_ASSERT(actor);

  aParams = RemoteInputStreamParams(actor->ParentID());
}

bool
RemoteInputStream::Deserialize(const InputStreamParams& /* aParams */,
                               const FileDescriptorArray& /* aFDs */)
{
  // See InputStreamUtils.cpp to see how deserialization of a
  // RemoteInputStream is special-cased.
  MOZ_CRASH("RemoteInputStream should never be deserialized");
}

nsIInputStream*
RemoteInputStream::BlockAndGetInternalStream()
{
  MOZ_ASSERT(!IsOnOwningThread());

  nsresult rv = BlockAndWaitForStream();
  NS_ENSURE_SUCCESS(rv, nullptr);

  return mStream;
}

} // anonymous namespace

StaticAutoPtr<BlobParent::IDTable> BlobParent::sIDTable;
StaticAutoPtr<Mutex> BlobParent::sIDTableMutex;

/*******************************************************************************
 * BlobParent::IDTableEntry Declaration
 ******************************************************************************/

class BlobParent::IDTableEntry final
{
  const nsID mID;
  const intptr_t mProcessID;
  const nsRefPtr<BlobImpl> mBlobImpl;

public:
  static already_AddRefed<IDTableEntry>
  Create(const nsID& aID, intptr_t aProcessID, BlobImpl* aBlobImpl)
  {
    MOZ_ASSERT(aBlobImpl);

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(aBlobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);

    return GetOrCreateInternal(aID,
                               aProcessID,
                               aBlobImpl,
                               /* aMayCreate */ true,
                               /* aMayGet */ false,
                               /* aIgnoreProcessID */ false);
  }

  static already_AddRefed<IDTableEntry>
  Get(const nsID& aID, intptr_t aProcessID)
  {
    return GetOrCreateInternal(aID,
                               aProcessID,
                               nullptr,
                               /* aMayCreate */ false,
                               /* aMayGet */ true,
                               /* aIgnoreProcessID */ false);
  }

  static already_AddRefed<IDTableEntry>
  Get(const nsID& aID)
  {
    return GetOrCreateInternal(aID,
                               0,
                               nullptr,
                               /* aMayCreate */ false,
                               /* aMayGet */ true,
                               /* aIgnoreProcessID */ true);
  }

  static already_AddRefed<IDTableEntry>
  GetOrCreate(const nsID& aID, intptr_t aProcessID, BlobImpl* aBlobImpl)
  {
    MOZ_ASSERT(aBlobImpl);

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(aBlobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);

    return GetOrCreateInternal(aID,
                               aProcessID,
                               aBlobImpl,
                               /* aMayCreate */ true,
                               /* aMayGet */ true,
                               /* aIgnoreProcessID */ false);
  }

  const nsID&
  ID() const
  {
    return mID;
  }

  intptr_t
  ProcessID() const
  {
    return mProcessID;
  }

  BlobImpl*
  GetBlobImpl() const
  {
    return mBlobImpl;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IDTableEntry)

private:
  IDTableEntry(const nsID& aID, intptr_t aProcessID, BlobImpl* aBlobImpl);
  ~IDTableEntry();

  static already_AddRefed<IDTableEntry>
  GetOrCreateInternal(const nsID& aID,
                      intptr_t aProcessID,
                      BlobImpl* aBlobImpl,
                      bool aMayCreate,
                      bool aMayGet,
                      bool aIgnoreProcessID);
};

/*******************************************************************************
 * BlobParent::OpenStreamRunnable Declaration
 ******************************************************************************/

// Each instance of this class will be dispatched to the network stream thread
// pool to run the first time where it will open the file input stream. It will
// then dispatch itself back to the owning thread to send the child process its
// response (assuming that the child has not crashed). The runnable will then
// dispatch itself to the thread pool again in order to close the file input
// stream.
class BlobParent::OpenStreamRunnable final
  : public nsRunnable
{
  friend class nsRevocableEventPtr<OpenStreamRunnable>;

  // Only safe to access these pointers if mRevoked is false!
  BlobParent* mBlobActor;
  InputStreamParent* mStreamActor;

  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIIPCSerializableInputStream> mSerializable;
  nsCOMPtr<nsIEventTarget> mActorTarget;
  nsCOMPtr<nsIThread> mIOTarget;

  bool mRevoked;
  bool mClosing;

public:
  OpenStreamRunnable(BlobParent* aBlobActor,
                     InputStreamParent* aStreamActor,
                     nsIInputStream* aStream,
                     nsIIPCSerializableInputStream* aSerializable,
                     nsIThread* aIOTarget)
    : mBlobActor(aBlobActor)
    , mStreamActor(aStreamActor)
    , mStream(aStream)
    , mSerializable(aSerializable)
    , mIOTarget(aIOTarget)
    , mRevoked(false)
    , mClosing(false)
  {
    MOZ_ASSERT(aBlobActor);
    aBlobActor->AssertIsOnOwningThread();
    MOZ_ASSERT(aStreamActor);
    MOZ_ASSERT(aStream);
    // aSerializable may be null.
    MOZ_ASSERT(aIOTarget);

    if (!NS_IsMainThread()) {
      AssertIsOnBackgroundThread();

      mActorTarget = do_GetCurrentThread();
      MOZ_ASSERT(mActorTarget);
    }

    AssertIsOnOwningThread();
  }

  nsresult
  Dispatch()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mIOTarget);

    nsresult rv = mIOTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~OpenStreamRunnable()
  { }

  bool
  IsOnOwningThread() const
  {
    return EventTargetIsOnCurrentThread(mActorTarget);
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnOwningThread());
  }

  void
  Revoke()
  {
    AssertIsOnOwningThread();
#ifdef DEBUG
    mBlobActor = nullptr;
    mStreamActor = nullptr;
#endif
    mRevoked = true;
  }

  nsresult
  OpenStream()
  {
    MOZ_ASSERT(!IsOnOwningThread());
    MOZ_ASSERT(mStream);

    if (!mSerializable) {
      nsCOMPtr<IPrivateRemoteInputStream> remoteStream =
        do_QueryInterface(mStream);
      MOZ_ASSERT(remoteStream, "Must QI to IPrivateRemoteInputStream here!");

      nsCOMPtr<nsIInputStream> realStream =
        remoteStream->BlockAndGetInternalStream();
      NS_ENSURE_TRUE(realStream, NS_ERROR_FAILURE);

      mSerializable = do_QueryInterface(realStream);
      if (!mSerializable) {
        MOZ_ASSERT(false, "Must be serializable!");
        return NS_ERROR_FAILURE;
      }

      mStream.swap(realStream);
    }

    // To force the stream open we call Available(). We don't actually care
    // how much data is available.
    uint64_t available;
    if (NS_FAILED(mStream->Available(&available))) {
      NS_WARNING("Available failed on this stream!");
    }

    if (mActorTarget) {
      nsresult rv = mActorTarget->Dispatch(this, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));
    }

    return NS_OK;
  }

  nsresult
  CloseStream()
  {
    MOZ_ASSERT(!IsOnOwningThread());
    MOZ_ASSERT(mStream);

    // Going to always release here.
    nsCOMPtr<nsIInputStream> stream;
    mStream.swap(stream);

    nsCOMPtr<nsIThread> ioTarget;
    mIOTarget.swap(ioTarget);

    NS_WARN_IF_FALSE(NS_SUCCEEDED(stream->Close()), "Failed to close stream!");

    nsCOMPtr<nsIRunnable> shutdownRunnable =
      NS_NewRunnableMethod(ioTarget, &nsIThread::Shutdown);
    MOZ_ASSERT(shutdownRunnable);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(shutdownRunnable)));

    return NS_OK;
  }

  nsresult
  SendResponse()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mStream);
    MOZ_ASSERT(mSerializable);
    MOZ_ASSERT(mIOTarget);
    MOZ_ASSERT(!mClosing);

    nsCOMPtr<nsIIPCSerializableInputStream> serializable;
    mSerializable.swap(serializable);

    if (mRevoked) {
      MOZ_ASSERT(!mBlobActor);
      MOZ_ASSERT(!mStreamActor);
    }
    else {
      MOZ_ASSERT(mBlobActor);
      MOZ_ASSERT(mBlobActor->HasManager());
      MOZ_ASSERT(mStreamActor);

      InputStreamParams params;
      nsTArray<FileDescriptor> fds;
      serializable->Serialize(params, fds);

      MOZ_ASSERT(params.type() != InputStreamParams::T__None);

      OptionalFileDescriptorSet optionalFDSet;
      if (nsIContentParent* contentManager = mBlobActor->GetContentManager()) {
        ConstructFileDescriptorSet(contentManager, fds, optionalFDSet);
      } else {
        ConstructFileDescriptorSet(mBlobActor->GetBackgroundManager(),
                                   fds,
                                   optionalFDSet);
      }

      mStreamActor->Destroy(params, optionalFDSet);

      mBlobActor->NoteRunnableCompleted(this);

#ifdef DEBUG
      mBlobActor = nullptr;
      mStreamActor = nullptr;
#endif
    }

    // If our luck is *really* bad then it is possible for the CloseStream() and
    // nsIThread::Shutdown() functions to run before the Dispatch() call here
    // finishes... Keep the thread alive until this method returns.
    nsCOMPtr<nsIThread> kungFuDeathGrip = mIOTarget;

    mClosing = true;

    nsresult rv = mIOTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(mIOTarget);

    if (IsOnOwningThread()) {
      return SendResponse();
    }

    if (!mClosing) {
      return OpenStream();
    }

    return CloseStream();
  }
};

NS_IMPL_ISUPPORTS_INHERITED0(BlobParent::OpenStreamRunnable, nsRunnable)

/*******************************************************************************
 * BlobChild::RemoteBlobImpl Declaration
 ******************************************************************************/

class BlobChild::RemoteBlobImpl
  : public BlobImplBase
  , public nsIRemoteBlob
{
protected:
  class CreateStreamHelper;

  BlobChild* mActor;
  nsCOMPtr<nsIEventTarget> mActorTarget;

  nsRefPtr<BlobImpl> mSameProcessBlobImpl;

  const bool mIsSlice;

public:
  // For File.
  RemoteBlobImpl(BlobChild* aActor,
                 const nsAString& aName,
                 const nsAString& aContentType,
                 uint64_t aLength,
                 int64_t aModDate);

  // For Blob.
  RemoteBlobImpl(BlobChild* aActor,
                 const nsAString& aContentType,
                 uint64_t aLength);

  // For same-process blobs.
  RemoteBlobImpl(BlobChild* aActor,
                 BlobImpl* aSameProcessBlobImpl,
                 const nsAString& aName,
                 const nsAString& aContentType,
                 uint64_t aLength,
                 int64_t aModDate);

  // For same-process blobs.
  RemoteBlobImpl(BlobChild* aActor,
                 BlobImpl* aSameProcessBlobImpl,
                 const nsAString& aContentType,
                 uint64_t aLength);

  // For mystery blobs.
  explicit
  RemoteBlobImpl(BlobChild* aActor);

  void
  NoteDyingActor();

  BlobChild*
  GetActor() const
  {
    MOZ_ASSERT(ActorEventTargetIsOnCurrentThread());

    return mActor;
  }

  nsIEventTarget*
  GetActorEventTarget() const
  {
    return mActorTarget;
  }

  bool
  ActorEventTargetIsOnCurrentThread() const
  {
    return EventTargetIsOnCurrentThread(BaseRemoteBlobImpl()->mActorTarget);
  }

  bool
  IsSlice() const
  {
    return mIsSlice;
  }

  RemoteBlobSliceImpl*
  AsSlice() const;

  RemoteBlobImpl*
  BaseRemoteBlobImpl() const;

  NS_DECL_ISUPPORTS_INHERITED

  virtual void
  GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart,
              uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) override;

  virtual void
  GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv) override;

  virtual int64_t
  GetFileId() override;

  virtual int64_t
  GetLastModified(ErrorResult& aRv) override;

  virtual void
  SetLastModified(int64_t aLastModified) override;

  virtual nsresult
  SetMutable(bool aMutable) override;

  virtual BlobChild*
  GetBlobChild() override;

  virtual BlobParent*
  GetBlobParent() override;

protected:
  // For SliceImpl.
  RemoteBlobImpl(const nsAString& aContentType, uint64_t aLength);

  ~RemoteBlobImpl()
  {
    MOZ_ASSERT_IF(mActorTarget,
                  EventTargetIsOnCurrentThread(mActorTarget));
  }

  void
  CommonInit(BlobChild* aActor);

  void
  Destroy();
};

class BlobChild::RemoteBlobImpl::CreateStreamHelper final
  : public nsRunnable
{
  Monitor mMonitor;
  nsRefPtr<RemoteBlobImpl> mRemoteBlobImpl;
  nsRefPtr<RemoteInputStream> mInputStream;
  const uint64_t mStart;
  const uint64_t mLength;
  bool mDone;

public:
  explicit CreateStreamHelper(RemoteBlobImpl* aRemoteBlobImpl);

  nsresult
  GetStream(nsIInputStream** aInputStream);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

private:
  ~CreateStreamHelper()
  {
    MOZ_ASSERT(!mRemoteBlobImpl);
    MOZ_ASSERT(!mInputStream);
    MOZ_ASSERT(mDone);
  }

  void
  RunInternal(RemoteBlobImpl* aBaseRemoteBlobImpl, bool aNotify);
};

class BlobChild::RemoteBlobSliceImpl final
  : public RemoteBlobImpl
{
  nsRefPtr<RemoteBlobImpl> mParent;
  bool mActorWasCreated;

public:
  RemoteBlobSliceImpl(RemoteBlobImpl* aParent,
                      uint64_t aStart,
                      uint64_t aLength,
                      const nsAString& aContentType);

  RemoteBlobImpl*
  Parent() const
  {
    MOZ_ASSERT(mParent);

    return const_cast<RemoteBlobImpl*>(mParent.get());
  }

  uint64_t
  Start() const
  {
    return mStart;
  }

  void
  EnsureActorWasCreated()
  {
    MOZ_ASSERT_IF(!ActorEventTargetIsOnCurrentThread(),
                  mActorWasCreated);

    if (!mActorWasCreated) {
      EnsureActorWasCreatedInternal();
    }
  }

  NS_DECL_ISUPPORTS_INHERITED

  virtual BlobChild*
  GetBlobChild() override;

private:
  ~RemoteBlobSliceImpl()
  { }

  void
  EnsureActorWasCreatedInternal();
};

/*******************************************************************************
 * BlobParent::RemoteBlobImpl Declaration
 ******************************************************************************/

class BlobParent::RemoteBlobImpl final
  : public BlobImpl
  , public nsIRemoteBlob
{
  BlobParent* mActor;
  nsCOMPtr<nsIEventTarget> mActorTarget;
  nsRefPtr<BlobImpl> mBlobImpl;

public:
  RemoteBlobImpl(BlobParent* aActor, BlobImpl* aBlobImpl);

  void
  NoteDyingActor();

  NS_DECL_ISUPPORTS_INHERITED

  virtual void
  GetName(nsAString& aName) override;

  virtual void
  GetPath(nsAString& aPath, ErrorResult& aRv) override;

  virtual int64_t
  GetLastModified(ErrorResult& aRv) override;

  virtual void
  SetLastModified(int64_t aLastModified) override;

  virtual void
  GetMozFullPath(nsAString& aName, ErrorResult& aRv) const override;

  virtual void
  GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const override;

  virtual uint64_t
  GetSize(ErrorResult& aRv) override;

  virtual void
  GetType(nsAString& aType) override;

  virtual uint64_t
  GetSerialNumber() const override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart,
              uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) override;

  virtual const nsTArray<nsRefPtr<BlobImpl>>*
  GetSubBlobImpls() const override;

  virtual void
  GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv) override;

  virtual int64_t
  GetFileId() override;

  virtual void
  AddFileInfo(FileInfo* aFileInfo) override;

  virtual FileInfo*
  GetFileInfo(FileManager* aFileManager) override;

  virtual nsresult
  GetSendInfo(nsIInputStream** aBody,
              uint64_t* aContentLength,
              nsACString& aContentType,
              nsACString& aCharset) override;

  virtual nsresult
  GetMutable(bool* aMutable) const override;

  virtual nsresult
  SetMutable(bool aMutable) override;

  virtual void
  SetLazyData(const nsAString& aName,
              const nsAString& aContentType,
              uint64_t aLength,
              int64_t aLastModifiedDate) override;

  virtual bool
  IsMemoryFile() const override;

  virtual bool
  IsSizeUnknown() const override;

  virtual bool
  IsDateUnknown() const override;

  virtual bool
  IsFile() const override;

  virtual bool
  MayBeClonedToOtherThreads() const override;

  virtual BlobChild*
  GetBlobChild() override;

  virtual BlobParent*
  GetBlobParent() override;

private:
  ~RemoteBlobImpl()
  {
    MOZ_ASSERT_IF(mActorTarget,
                  EventTargetIsOnCurrentThread(mActorTarget));
  }

  void
  Destroy();
};

/*******************************************************************************
 * BlobChild::RemoteBlobImpl
 ******************************************************************************/

BlobChild::
RemoteBlobImpl::RemoteBlobImpl(BlobChild* aActor,
                               const nsAString& aName,
                               const nsAString& aContentType,
                               uint64_t aLength,
                               int64_t aModDate)
  : BlobImplBase(aName, aContentType, aLength, aModDate)
  , mIsSlice(false)
{
  CommonInit(aActor);
}

BlobChild::
RemoteBlobImpl::RemoteBlobImpl(BlobChild* aActor,
                               const nsAString& aContentType,
                               uint64_t aLength)
  : BlobImplBase(aContentType, aLength)
  , mIsSlice(false)
{
  CommonInit(aActor);
}

BlobChild::
RemoteBlobImpl::RemoteBlobImpl(BlobChild* aActor,
                               BlobImpl* aSameProcessBlobImpl,
                               const nsAString& aName,
                               const nsAString& aContentType,
                               uint64_t aLength,
                               int64_t aModDate)
  : BlobImplBase(aName, aContentType, aLength, aModDate)
  , mSameProcessBlobImpl(aSameProcessBlobImpl)
  , mIsSlice(false)
{
  MOZ_ASSERT(aSameProcessBlobImpl);
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

  CommonInit(aActor);
}

BlobChild::
RemoteBlobImpl::RemoteBlobImpl(BlobChild* aActor,
                               BlobImpl* aSameProcessBlobImpl,
                               const nsAString& aContentType,
                               uint64_t aLength)
  : BlobImplBase(aContentType, aLength)
  , mSameProcessBlobImpl(aSameProcessBlobImpl)
  , mIsSlice(false)
{
  MOZ_ASSERT(aSameProcessBlobImpl);
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

  CommonInit(aActor);
}

BlobChild::
RemoteBlobImpl::RemoteBlobImpl(BlobChild* aActor)
  : BlobImplBase(EmptyString(), EmptyString(), UINT64_MAX, INT64_MAX)
  , mIsSlice(false)
{
  CommonInit(aActor);
}

BlobChild::
RemoteBlobImpl::RemoteBlobImpl(const nsAString& aContentType, uint64_t aLength)
  : BlobImplBase(aContentType, aLength)
  , mActor(nullptr)
  , mIsSlice(true)
{
  mImmutable = true;
}

void
BlobChild::
RemoteBlobImpl::CommonInit(BlobChild* aActor)
{
  MOZ_ASSERT(aActor);
  aActor->AssertIsOnOwningThread();
  MOZ_ASSERT(!mIsSlice);

  mActor = aActor;
  mActorTarget = aActor->EventTarget();

  mImmutable = true;
}

void
BlobChild::
RemoteBlobImpl::NoteDyingActor()
{
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();

  mActor = nullptr;
}

BlobChild::RemoteBlobSliceImpl*
BlobChild::
RemoteBlobImpl::AsSlice() const
{
  MOZ_ASSERT(IsSlice());

  return static_cast<RemoteBlobSliceImpl*>(const_cast<RemoteBlobImpl*>(this));
}

BlobChild::RemoteBlobImpl*
BlobChild::
RemoteBlobImpl::BaseRemoteBlobImpl() const
{
  if (IsSlice()) {
    return AsSlice()->Parent()->BaseRemoteBlobImpl();
  }

  return const_cast<RemoteBlobImpl*>(this);
}

void
BlobChild::
RemoteBlobImpl::Destroy()
{
  if (EventTargetIsOnCurrentThread(mActorTarget)) {
    if (mActor) {
      mActor->AssertIsOnOwningThread();
      mActor->NoteDyingRemoteBlobImpl();
    }

    delete this;
    return;
  }

  nsCOMPtr<nsIRunnable> destroyRunnable =
    NS_NewNonOwningRunnableMethod(this, &RemoteBlobImpl::Destroy);

  if (mActorTarget) {
    destroyRunnable =
      new CancelableRunnableWrapper(destroyRunnable, mActorTarget);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mActorTarget->Dispatch(destroyRunnable,
                                                        NS_DISPATCH_NORMAL)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(destroyRunnable)));
  }
}

NS_IMPL_ADDREF(BlobChild::RemoteBlobImpl)
NS_IMPL_RELEASE_WITH_DESTROY(BlobChild::RemoteBlobImpl, Destroy())
NS_IMPL_QUERY_INTERFACE_INHERITED(BlobChild::RemoteBlobImpl,
                                  BlobImpl,
                                  nsIRemoteBlob)

void
BlobChild::
RemoteBlobImpl::GetMozFullPathInternal(nsAString& aFilePath,
                                       ErrorResult& aRv) const
{
  if (!EventTargetIsOnCurrentThread(mActorTarget)) {
    MOZ_CRASH("Not implemented!");
  }

  if (mSameProcessBlobImpl) {
    MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

    mSameProcessBlobImpl->GetMozFullPathInternal(aFilePath, aRv);
    return;
  }

  if (!mActor) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsString filePath;
  if (!mActor->SendGetFilePath(&filePath)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aFilePath = filePath;
}

already_AddRefed<BlobImpl>
BlobChild::
RemoteBlobImpl::CreateSlice(uint64_t aStart,
                            uint64_t aLength,
                            const nsAString& aContentType,
                            ErrorResult& aRv)
{
  // May be called on any thread.
  if (mSameProcessBlobImpl) {
    MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

    return mSameProcessBlobImpl->CreateSlice(aStart,
                                             aLength,
                                             aContentType,
                                             aRv);
  }

  nsRefPtr<RemoteBlobSliceImpl> slice =
    new RemoteBlobSliceImpl(this, aStart, aLength, aContentType);
  return slice.forget();
}

void
BlobChild::
RemoteBlobImpl::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  // May be called on any thread.
  if (mSameProcessBlobImpl) {
    MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

    nsCOMPtr<nsIInputStream> realStream;
    mSameProcessBlobImpl->GetInternalStream(getter_AddRefs(realStream), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    nsRefPtr<BlobInputStreamTether> tether =
      new BlobInputStreamTether(realStream, mSameProcessBlobImpl);
    tether.forget(aStream);
    return;
  }

  nsRefPtr<CreateStreamHelper> helper = new CreateStreamHelper(this);
  aRv = helper->GetStream(aStream);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

int64_t
BlobChild::
RemoteBlobImpl::GetFileId()
{
  if (!EventTargetIsOnCurrentThread(mActorTarget)) {
    MOZ_CRASH("Not implemented!");
  }

  if (mSameProcessBlobImpl) {
    MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

    return mSameProcessBlobImpl->GetFileId();
  }

  int64_t fileId;
  if (mActor && mActor->SendGetFileId(&fileId)) {
    return fileId;
  }

  return -1;
}

int64_t
BlobChild::
RemoteBlobImpl::GetLastModified(ErrorResult& aRv)
{
  if (IsDateUnknown()) {
    return 0;
  }

  return mLastModificationDate;
}

void
BlobChild::
RemoteBlobImpl::SetLastModified(int64_t aLastModified)
{
  MOZ_CRASH("SetLastModified of a remote blob is not allowed!");
}

nsresult
BlobChild::
RemoteBlobImpl::SetMutable(bool aMutable)
{
  if (!aMutable && IsSlice()) {
    // Make sure that slices are backed by a real actor now while we are still
    // on the correct thread.
    AsSlice()->EnsureActorWasCreated();
  }

  nsresult rv = BlobImplBase::SetMutable(aMutable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!aMutable, mImmutable);

  return NS_OK;
}

BlobChild*
BlobChild::
RemoteBlobImpl::GetBlobChild()
{
  return mActor;
}

BlobParent*
BlobChild::
RemoteBlobImpl::GetBlobParent()
{
  return nullptr;
}

/*******************************************************************************
 * BlobChild::RemoteBlobImpl::CreateStreamHelper
 ******************************************************************************/

BlobChild::RemoteBlobImpl::
CreateStreamHelper::CreateStreamHelper(RemoteBlobImpl* aRemoteBlobImpl)
  : mMonitor("BlobChild::RemoteBlobImpl::CreateStreamHelper::mMonitor")
  , mRemoteBlobImpl(aRemoteBlobImpl)
  , mStart(aRemoteBlobImpl->IsSlice() ? aRemoteBlobImpl->AsSlice()->Start() : 0)
  , mLength(0)
  , mDone(false)
{
  // This may be created on any thread.
  MOZ_ASSERT(aRemoteBlobImpl);

  ErrorResult rv;
  const_cast<uint64_t&>(mLength) = aRemoteBlobImpl->GetSize(rv);
  MOZ_ASSERT(!rv.Failed());
}

nsresult
BlobChild::RemoteBlobImpl::
CreateStreamHelper::GetStream(nsIInputStream** aInputStream)
{
  // This may be called on any thread.
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(mRemoteBlobImpl);
  MOZ_ASSERT(!mInputStream);
  MOZ_ASSERT(!mDone);

  nsRefPtr<RemoteBlobImpl> baseRemoteBlobImpl =
    mRemoteBlobImpl->BaseRemoteBlobImpl();
  MOZ_ASSERT(baseRemoteBlobImpl);

  if (EventTargetIsOnCurrentThread(baseRemoteBlobImpl->GetActorEventTarget())) {
    RunInternal(baseRemoteBlobImpl, false);
  } else {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCOMPtr<nsIEventTarget> target = baseRemoteBlobImpl->GetActorEventTarget();
    if (!target) {
      target = do_GetMainThread();
    }

    MOZ_ASSERT(target);

    nsresult rv = target->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    DebugOnly<bool> warned = false;

    {
      MonitorAutoLock lock(mMonitor);

      while (!mDone) {
#ifdef DEBUG
        if (!warned) {
          NS_WARNING("RemoteBlobImpl::GetInternalStream() called on thread "
                     "that can't send messages, blocking here to wait for the "
                     "actor's thread to send the message!");
        }
#endif
        lock.Wait();
      }
    }
  }

  MOZ_ASSERT(!mRemoteBlobImpl);
  MOZ_ASSERT(mDone);

  if (!mInputStream) {
    return NS_ERROR_UNEXPECTED;
  }

  mInputStream.forget(aInputStream);
  return NS_OK;
}

void
BlobChild::RemoteBlobImpl::
CreateStreamHelper::RunInternal(RemoteBlobImpl* aBaseRemoteBlobImpl,
                                bool aNotify)
{
  MOZ_ASSERT(aBaseRemoteBlobImpl);
  MOZ_ASSERT(aBaseRemoteBlobImpl->ActorEventTargetIsOnCurrentThread());
  MOZ_ASSERT(!mInputStream);
  MOZ_ASSERT(!mDone);

  if (BlobChild* actor = aBaseRemoteBlobImpl->GetActor()) {
    nsRefPtr<RemoteInputStream> stream;

    if (!NS_IsMainThread() && GetCurrentThreadWorkerPrivate()) {
      stream =
        new RemoteInputStream(actor, mRemoteBlobImpl, mStart, mLength);
    } else {
      stream = new RemoteInputStream(mRemoteBlobImpl, mStart, mLength);
    }

    InputStreamChild* streamActor = new InputStreamChild(stream);
    if (actor->SendPBlobStreamConstructor(streamActor, mStart, mLength)) {
      stream.swap(mInputStream);
    }
  }

  mRemoteBlobImpl = nullptr;

  if (aNotify) {
    MonitorAutoLock lock(mMonitor);
    mDone = true;
    lock.Notify();
  } else {
    mDone = true;
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(BlobChild::RemoteBlobImpl::CreateStreamHelper,
                             nsRunnable)

NS_IMETHODIMP
BlobChild::RemoteBlobImpl::
CreateStreamHelper::Run()
{
  MOZ_ASSERT(mRemoteBlobImpl);
  MOZ_ASSERT(mRemoteBlobImpl->ActorEventTargetIsOnCurrentThread());

  nsRefPtr<RemoteBlobImpl> baseRemoteBlobImpl =
    mRemoteBlobImpl->BaseRemoteBlobImpl();
  MOZ_ASSERT(baseRemoteBlobImpl);

  RunInternal(baseRemoteBlobImpl, true);
  return NS_OK;
}

/*******************************************************************************
 * BlobChild::RemoteBlobSliceImpl
 ******************************************************************************/

BlobChild::
RemoteBlobSliceImpl::RemoteBlobSliceImpl(RemoteBlobImpl* aParent,
                                         uint64_t aStart,
                                         uint64_t aLength,
                                         const nsAString& aContentType)
  : RemoteBlobImpl(aContentType, aLength)
  , mParent(aParent->BaseRemoteBlobImpl())
  , mActorWasCreated(false)
{
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(mParent->BaseRemoteBlobImpl() == mParent);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(aParent->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

#ifdef DEBUG
  {
    ErrorResult rv;
    uint64_t parentSize = aParent->GetSize(rv);
    MOZ_ASSERT(!rv.Failed());
    MOZ_ASSERT(parentSize >= aStart + aLength);
  }
#endif

  // Account for the offset of the parent slice, if any.
  mStart = aParent->IsSlice() ? aParent->AsSlice()->mStart + aStart : aStart;
}

void
BlobChild::
RemoteBlobSliceImpl::EnsureActorWasCreatedInternal()
{
  MOZ_ASSERT(ActorEventTargetIsOnCurrentThread());
  MOZ_ASSERT(!mActorWasCreated);

  mActorWasCreated = true;

  BlobChild* baseActor = mParent->GetActor();
  MOZ_ASSERT(baseActor);
  MOZ_ASSERT(baseActor->HasManager());

  nsID id;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(gUUIDGenerator->GenerateUUIDInPlace(&id)));

  ParentBlobConstructorParams params(
    SlicedBlobConstructorParams(nullptr /* sourceParent */,
                                baseActor /* sourceChild */,
                                id /* id */,
                                mStart /* begin */,
                                mStart + mLength /* end */,
                                mContentType /* contentType */));

  if (nsIContentChild* contentManager = baseActor->GetContentManager()) {
    mActor = SendSliceConstructor(contentManager, this, params);
  } else {
    mActor =
      SendSliceConstructor(baseActor->GetBackgroundManager(), this, params);
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(BlobChild::RemoteBlobSliceImpl,
                             BlobChild::RemoteBlobImpl)

BlobChild*
BlobChild::
RemoteBlobSliceImpl::GetBlobChild()
{
  EnsureActorWasCreated();

  return RemoteBlobImpl::GetBlobChild();
}

/*******************************************************************************
 * BlobParent::RemoteBlobImpl
 ******************************************************************************/

BlobParent::
RemoteBlobImpl::RemoteBlobImpl(BlobParent* aActor, BlobImpl* aBlobImpl)
  : mActor(aActor)
  , mActorTarget(aActor->EventTarget())
  , mBlobImpl(aBlobImpl)
{
  MOZ_ASSERT(aActor);
  aActor->AssertIsOnOwningThread();
  MOZ_ASSERT(aBlobImpl);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(aBlobImpl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);
}

void
BlobParent::
RemoteBlobImpl::NoteDyingActor()
{
  MOZ_ASSERT(mActor);
  mActor->AssertIsOnOwningThread();

  mActor = nullptr;
}

void
BlobParent::
RemoteBlobImpl::Destroy()
{
  if (EventTargetIsOnCurrentThread(mActorTarget)) {
    if (mActor) {
      mActor->AssertIsOnOwningThread();
      mActor->NoteDyingRemoteBlobImpl();
    }

    delete this;
    return;
  }

  nsCOMPtr<nsIRunnable> destroyRunnable =
    NS_NewNonOwningRunnableMethod(this, &RemoteBlobImpl::Destroy);

  if (mActorTarget) {
    destroyRunnable =
      new CancelableRunnableWrapper(destroyRunnable, mActorTarget);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mActorTarget->Dispatch(destroyRunnable,
                                                        NS_DISPATCH_NORMAL)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(destroyRunnable)));
  }
}

NS_IMPL_ADDREF(BlobParent::RemoteBlobImpl)
NS_IMPL_RELEASE_WITH_DESTROY(BlobParent::RemoteBlobImpl, Destroy())
NS_IMPL_QUERY_INTERFACE_INHERITED(BlobParent::RemoteBlobImpl,
                                  BlobImpl,
                                  nsIRemoteBlob)

void
BlobParent::
RemoteBlobImpl::GetName(nsAString& aName)
{
  mBlobImpl->GetName(aName);
}

void
BlobParent::
RemoteBlobImpl::GetPath(nsAString& aPath, ErrorResult& aRv)
{
  mBlobImpl->GetPath(aPath, aRv);
}

int64_t
BlobParent::
RemoteBlobImpl::GetLastModified(ErrorResult& aRv)
{
  return mBlobImpl->GetLastModified(aRv);
}

void
BlobParent::
RemoteBlobImpl::SetLastModified(int64_t aLastModified)
{
  MOZ_CRASH("SetLastModified of a remote blob is not allowed!");
}

void
BlobParent::
RemoteBlobImpl::GetMozFullPath(nsAString& aName, ErrorResult& aRv) const
{
  mBlobImpl->GetMozFullPath(aName, aRv);
}

void
BlobParent::
RemoteBlobImpl::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const
{
  mBlobImpl->GetMozFullPathInternal(aFileName, aRv);
}

uint64_t
BlobParent::
RemoteBlobImpl::GetSize(ErrorResult& aRv)
{
  return mBlobImpl->GetSize(aRv);
}

void
BlobParent::
RemoteBlobImpl::GetType(nsAString& aType)
{
  mBlobImpl->GetType(aType);
}

uint64_t
BlobParent::
RemoteBlobImpl::GetSerialNumber() const
{
  return mBlobImpl->GetSerialNumber();
}

already_AddRefed<BlobImpl>
BlobParent::
RemoteBlobImpl::CreateSlice(uint64_t aStart,
                            uint64_t aLength,
                            const nsAString& aContentType,
                            ErrorResult& aRv)
{
  return mBlobImpl->CreateSlice(aStart, aLength, aContentType, aRv);
}

const nsTArray<nsRefPtr<BlobImpl>>*
BlobParent::
RemoteBlobImpl::GetSubBlobImpls() const
{
  return mBlobImpl->GetSubBlobImpls();
}

void
BlobParent::
RemoteBlobImpl::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  mBlobImpl->GetInternalStream(aStream, aRv);
}

int64_t
BlobParent::
RemoteBlobImpl::GetFileId()
{
  return mBlobImpl->GetFileId();
}

void
BlobParent::
RemoteBlobImpl::AddFileInfo(FileInfo* aFileInfo)
{
  return mBlobImpl->AddFileInfo(aFileInfo);
}

FileInfo*
BlobParent::
RemoteBlobImpl::GetFileInfo(FileManager* aFileManager)
{
  return mBlobImpl->GetFileInfo(aFileManager);
}

nsresult
BlobParent::
RemoteBlobImpl::GetSendInfo(nsIInputStream** aBody,
                            uint64_t* aContentLength,
                            nsACString& aContentType,
                            nsACString& aCharset)
{
  return mBlobImpl->GetSendInfo(aBody,
                                aContentLength,
                                aContentType,
                                aCharset);
}

nsresult
BlobParent::
RemoteBlobImpl::GetMutable(bool* aMutable) const
{
  return mBlobImpl->GetMutable(aMutable);
}

nsresult
BlobParent::
RemoteBlobImpl::SetMutable(bool aMutable)
{
  return mBlobImpl->SetMutable(aMutable);
}

void
BlobParent::
RemoteBlobImpl::SetLazyData(const nsAString& aName,
                            const nsAString& aContentType,
                            uint64_t aLength,
                            int64_t aLastModifiedDate)
{
  MOZ_CRASH("This should never be called!");
}

bool
BlobParent::
RemoteBlobImpl::IsMemoryFile() const
{
  return mBlobImpl->IsMemoryFile();
}

bool
BlobParent::
RemoteBlobImpl::IsSizeUnknown() const
{
  return mBlobImpl->IsSizeUnknown();
}

bool
BlobParent::
RemoteBlobImpl::IsDateUnknown() const
{
  return mBlobImpl->IsDateUnknown();
}

bool
BlobParent::
RemoteBlobImpl::IsFile() const
{
  return mBlobImpl->IsFile();
}

bool
BlobParent::
RemoteBlobImpl::MayBeClonedToOtherThreads() const
{
  return mBlobImpl->MayBeClonedToOtherThreads();
}

BlobChild*
BlobParent::
RemoteBlobImpl::GetBlobChild()
{
  return nullptr;
}

BlobParent*
BlobParent::
RemoteBlobImpl::GetBlobParent()
{
  return mActor;
}

/*******************************************************************************
 * BlobChild
 ******************************************************************************/

BlobChild::BlobChild(nsIContentChild* aManager, BlobImpl* aBlobImpl)
  : mBackgroundManager(nullptr)
  , mContentManager(aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  CommonInit(aBlobImpl);
}

BlobChild::BlobChild(PBackgroundChild* aManager, BlobImpl* aBlobImpl)
  : mBackgroundManager(aManager)
  , mContentManager(nullptr)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  CommonInit(aBlobImpl);
}

BlobChild::BlobChild(nsIContentChild* aManager, BlobChild* aOther)
  : mBackgroundManager(nullptr)
  , mContentManager(aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  CommonInit(aOther, /* aBlobImpl */ nullptr);
}

BlobChild::BlobChild(PBackgroundChild* aManager,
                     BlobChild* aOther,
                     BlobImpl* aBlobImpl)
  : mBackgroundManager(aManager)
  , mContentManager(nullptr)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlobImpl);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  CommonInit(aOther, aBlobImpl);
}

BlobChild::BlobChild(nsIContentChild* aManager,
                     const ChildBlobConstructorParams& aParams)
  : mBackgroundManager(nullptr)
  , mContentManager(aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  CommonInit(aParams);
}

BlobChild::BlobChild(PBackgroundChild* aManager,
                     const ChildBlobConstructorParams& aParams)
  : mBackgroundManager(aManager)
  , mContentManager(nullptr)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  CommonInit(aParams);
}

BlobChild::BlobChild(nsIContentChild* aManager,
                     const nsID& aParentID,
                     RemoteBlobSliceImpl* aRemoteBlobSliceImpl)
  : mBackgroundManager(nullptr)
  , mContentManager(aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  CommonInit(aParentID, aRemoteBlobSliceImpl);
}

BlobChild::BlobChild(PBackgroundChild* aManager,
                     const nsID& aParentID,
                     RemoteBlobSliceImpl* aRemoteBlobSliceImpl)
  : mBackgroundManager(aManager)
  , mContentManager(nullptr)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  if (!NS_IsMainThread()) {
    mEventTarget = do_GetCurrentThread();
    MOZ_ASSERT(mEventTarget);
  }

  CommonInit(aParentID, aRemoteBlobSliceImpl);
}

BlobChild::~BlobChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BlobChild);
}

void
BlobChild::CommonInit(BlobImpl* aBlobImpl)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBlobImpl);

  MOZ_COUNT_CTOR(BlobChild);

  mBlobImpl = aBlobImpl;
  mRemoteBlobImpl = nullptr;

  mBlobImpl->AddRef();
  mOwnsBlobImpl = true;

  memset(&mParentID, 0, sizeof(mParentID));
}

void
BlobChild::CommonInit(BlobChild* aOther, BlobImpl* aBlobImpl)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aOther);
  MOZ_ASSERT_IF(mContentManager, aOther->GetBackgroundManager());
  MOZ_ASSERT_IF(mContentManager, !aBlobImpl);
  MOZ_ASSERT_IF(mBackgroundManager, aBlobImpl);

  nsRefPtr<BlobImpl> otherImpl;
  if (mBackgroundManager && aOther->GetBackgroundManager()) {
    otherImpl = aBlobImpl;
  } else {
    otherImpl = aOther->GetBlobImpl();
  }
  MOZ_ASSERT(otherImpl);

  nsString contentType;
  otherImpl->GetType(contentType);

  ErrorResult rv;
  uint64_t length = otherImpl->GetSize(rv);
  MOZ_ASSERT(!rv.Failed());

  nsRefPtr<RemoteBlobImpl> remoteBlob;
  if (otherImpl->IsFile()) {
    nsString name;
    otherImpl->GetName(name);

    int64_t modDate = otherImpl->GetLastModified(rv);
    MOZ_ASSERT(!rv.Failed());

    remoteBlob = new RemoteBlobImpl(this, name, contentType, length, modDate);
  } else {
    remoteBlob = new RemoteBlobImpl(this, contentType, length);
  }

  CommonInit(aOther->ParentID(), remoteBlob);
}

void
BlobChild::CommonInit(const ChildBlobConstructorParams& aParams)
{
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BlobChild);

  const AnyBlobConstructorParams& blobParams = aParams.blobParams();

  AnyBlobConstructorParams::Type paramsType = blobParams.type();
  MOZ_ASSERT(paramsType != AnyBlobConstructorParams::T__None &&
             paramsType !=
               AnyBlobConstructorParams::TSlicedBlobConstructorParams &&
             paramsType !=
               AnyBlobConstructorParams::TKnownBlobConstructorParams);

  nsRefPtr<RemoteBlobImpl> remoteBlob;

  switch (paramsType) {
    case AnyBlobConstructorParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        blobParams.get_NormalBlobConstructorParams();
      remoteBlob =
        new RemoteBlobImpl(this, params.contentType(), params.length());
      break;
    }

    case AnyBlobConstructorParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        blobParams.get_FileBlobConstructorParams();
      remoteBlob = new RemoteBlobImpl(this,
                                      params.name(),
                                      params.contentType(),
                                      params.length(),
                                      params.modDate());
      break;
    }

    case AnyBlobConstructorParams::TSameProcessBlobConstructorParams: {
      MOZ_ASSERT(gProcessType == GeckoProcessType_Default);

      const SameProcessBlobConstructorParams& params =
        blobParams.get_SameProcessBlobConstructorParams();
      MOZ_ASSERT(params.addRefedBlobImpl());

      nsRefPtr<BlobImpl> blobImpl =
        dont_AddRef(reinterpret_cast<BlobImpl*>(params.addRefedBlobImpl()));

      ErrorResult rv;
      uint64_t size = blobImpl->GetSize(rv);
      MOZ_ASSERT(!rv.Failed());

      nsString contentType;
      blobImpl->GetType(contentType);

      if (blobImpl->IsFile()) {
        nsString name;
        blobImpl->GetName(name);

        int64_t lastModifiedDate = blobImpl->GetLastModified(rv);
        MOZ_ASSERT(!rv.Failed());

        remoteBlob =
          new RemoteBlobImpl(this,
                             blobImpl,
                             name,
                             contentType,
                             size,
                             lastModifiedDate);
      } else {
        remoteBlob = new RemoteBlobImpl(this, blobImpl, contentType, size);
      }

      break;
    }

    case AnyBlobConstructorParams::TMysteryBlobConstructorParams: {
      remoteBlob = new RemoteBlobImpl(this);
      break;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_ASSERT(remoteBlob);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(remoteBlob->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  mRemoteBlobImpl = remoteBlob;

  remoteBlob.forget(&mBlobImpl);
  mOwnsBlobImpl = true;

  mParentID = aParams.id();
}

void
BlobChild::CommonInit(const nsID& aParentID, RemoteBlobImpl* aRemoteBlobImpl)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRemoteBlobImpl);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(aRemoteBlobImpl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  MOZ_COUNT_CTOR(BlobChild);

  nsRefPtr<RemoteBlobImpl> remoteBlob = aRemoteBlobImpl;

  mRemoteBlobImpl = remoteBlob;

  remoteBlob.forget(&mBlobImpl);
  mOwnsBlobImpl = true;

  mParentID = aParentID;
}

#ifdef DEBUG

void
BlobChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(IsOnOwningThread());
}

#endif // DEBUG

// static
void
BlobChild::Startup(const FriendKey& /* aKey */)
{
  MOZ_ASSERT(XRE_GetProcessType() != GeckoProcessType_Default);

  CommonStartup();
}

// static
BlobChild*
BlobChild::GetOrCreate(nsIContentChild* aManager, BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return GetOrCreateFromImpl(aManager, aBlobImpl);
}

// static
BlobChild*
BlobChild::GetOrCreate(PBackgroundChild* aManager, BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return GetOrCreateFromImpl(aManager, aBlobImpl);
}

// static
BlobChild*
BlobChild::Create(nsIContentChild* aManager,
                  const ChildBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
BlobChild*
BlobChild::Create(PBackgroundChild* aManager,
                  const ChildBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
template <class ChildManagerType>
BlobChild*
BlobChild::GetOrCreateFromImpl(ChildManagerType* aManager,
                               BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlobImpl);

  // If the blob represents a remote blob then we can simply pass its actor back
  // here.
  if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlobImpl)) {
    BlobChild* actor =
      MaybeGetActorFromRemoteBlob(remoteBlob, aManager, aBlobImpl);
    if (actor) {
      return actor;
    }
  }

  // All blobs shared between threads or processes must be immutable.
  if (NS_WARN_IF(NS_FAILED(aBlobImpl->SetMutable(false)))) {
    return nullptr;
  }

  MOZ_ASSERT(!aBlobImpl->IsSizeUnknown());
  MOZ_ASSERT(!aBlobImpl->IsDateUnknown());

  AnyBlobConstructorParams blobParams;

  nsCOMPtr<nsIInputStream> snapshotInputStream;

  if (gProcessType == GeckoProcessType_Default) {
    nsCOMPtr<PIBlobImplSnapshot> snapshot = do_QueryInterface(aBlobImpl);
    if (snapshot) {
      ErrorResult rv;
      aBlobImpl->GetInternalStream(getter_AddRefs(snapshotInputStream), rv);
      MOZ_ALWAYS_TRUE(!rv.Failed());
    }
  }

  if (gProcessType == GeckoProcessType_Default && !snapshotInputStream) {
    nsRefPtr<BlobImpl> sameProcessImpl = aBlobImpl;
    auto addRefedBlobImpl =
      reinterpret_cast<intptr_t>(sameProcessImpl.forget().take());

    blobParams = SameProcessBlobConstructorParams(addRefedBlobImpl);
  } else {
    BlobData blobData;
    if (snapshotInputStream) {
      blobData =
        reinterpret_cast<intptr_t>(snapshotInputStream.forget().take());
    } else {
      BlobDataFromBlobImpl(aBlobImpl, blobData);
    }

    nsString contentType;
    aBlobImpl->GetType(contentType);

    ErrorResult rv;
    uint64_t length = aBlobImpl->GetSize(rv);
    MOZ_ASSERT(!rv.Failed());

    if (aBlobImpl->IsFile()) {
      nsString name;
      aBlobImpl->GetName(name);

      int64_t modDate = aBlobImpl->GetLastModified(rv);
      MOZ_ASSERT(!rv.Failed());

      blobParams =
        FileBlobConstructorParams(name, contentType, length, modDate, blobData);
    } else {
      blobParams = NormalBlobConstructorParams(contentType, length, blobData);
    }
  }

  BlobChild* actor = new BlobChild(aManager, aBlobImpl);

  ParentBlobConstructorParams params(blobParams);

  if (NS_WARN_IF(!aManager->SendPBlobConstructor(actor, params))) {
    return nullptr;
  }

  return actor;
}

// static
template <class ChildManagerType>
BlobChild*
BlobChild::CreateFromParams(ChildManagerType* aManager,
                            const ChildBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  const AnyBlobConstructorParams& blobParams = aParams.blobParams();

  switch (blobParams.type()) {
    case AnyBlobConstructorParams::TNormalBlobConstructorParams:
    case AnyBlobConstructorParams::TFileBlobConstructorParams:
    case AnyBlobConstructorParams::TSameProcessBlobConstructorParams:
    case AnyBlobConstructorParams::TMysteryBlobConstructorParams: {
      return new BlobChild(aManager, aParams);
    }

    case AnyBlobConstructorParams::TSlicedBlobConstructorParams: {
      MOZ_CRASH("Parent should never send SlicedBlobConstructorParams!");
    }

    case AnyBlobConstructorParams::TKnownBlobConstructorParams: {
      MOZ_CRASH("Parent should never send KnownBlobConstructorParams!");
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_CRASH("Should never get here!");
}

// static
template <class ChildManagerType>
BlobChild*
BlobChild::SendSliceConstructor(ChildManagerType* aManager,
                                RemoteBlobSliceImpl* aRemoteBlobSliceImpl,
                                const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aRemoteBlobSliceImpl);
  MOZ_ASSERT(aParams.blobParams().type() ==
               AnyBlobConstructorParams::TSlicedBlobConstructorParams);

  const nsID& id = aParams.blobParams().get_SlicedBlobConstructorParams().id();

  BlobChild* newActor = new BlobChild(aManager, id, aRemoteBlobSliceImpl);

  if (aManager->SendPBlobConstructor(newActor, aParams)) {
    if (gProcessType != GeckoProcessType_Default || !NS_IsMainThread()) {
      newActor->SendWaitForSliceCreation();
    }
    return newActor;
  }

  return nullptr;
}

// static
BlobChild*
BlobChild::MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                                       nsIContentChild* aManager,
                                       BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aRemoteBlob);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlobImpl);

  if (BlobChild* actor = aRemoteBlob->GetBlobChild()) {
    if (actor->GetContentManager() == aManager) {
      return actor;
    }

    MOZ_ASSERT(actor->GetBackgroundManager());

    actor = new BlobChild(aManager, actor);

    ParentBlobConstructorParams params(
      KnownBlobConstructorParams(actor->ParentID()));

    aManager->SendPBlobConstructor(actor, params);

    return actor;
  }

  return nullptr;
}

// static
BlobChild*
BlobChild::MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                                       PBackgroundChild* aManager,
                                       BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aRemoteBlob);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlobImpl);

  if (BlobChild* actor = aRemoteBlob->GetBlobChild()) {
    if (actor->GetBackgroundManager() == aManager) {
      return actor;
    }

    actor = new BlobChild(aManager, actor, aBlobImpl);

    ParentBlobConstructorParams params(
      KnownBlobConstructorParams(actor->ParentID()));

    aManager->SendPBlobConstructor(actor, params);

    return actor;
  }

  return nullptr;
}

const nsID&
BlobChild::ParentID() const
{
  MOZ_ASSERT(mRemoteBlobImpl);

  return mParentID;
}

already_AddRefed<BlobImpl>
BlobChild::GetBlobImpl()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);

  nsRefPtr<BlobImpl> blobImpl;

  // Remote blobs are held alive until the first call to GetBlobImpl. Thereafter
  // we only hold a weak reference. Normal blobs are held alive until the actor
  // is destroyed.
  if (mRemoteBlobImpl && mOwnsBlobImpl) {
    blobImpl = dont_AddRef(mBlobImpl);
    mOwnsBlobImpl = false;
  } else {
    blobImpl = mBlobImpl;
  }

  MOZ_ASSERT(blobImpl);

  return blobImpl.forget();
}

bool
BlobChild::SetMysteryBlobInfo(const nsString& aName,
                              const nsString& aContentType,
                              uint64_t aLength,
                              int64_t aLastModifiedDate)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(mRemoteBlobImpl);
  MOZ_ASSERT(aLastModifiedDate != INT64_MAX);

  mBlobImpl->SetLazyData(aName, aContentType, aLength, aLastModifiedDate);

  FileBlobConstructorParams params(aName,
                                   aContentType,
                                   aLength,
                                   aLastModifiedDate,
                                   void_t() /* optionalBlobData */);
  return SendResolveMystery(params);
}

bool
BlobChild::SetMysteryBlobInfo(const nsString& aContentType, uint64_t aLength)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(mRemoteBlobImpl);

  nsString voidString;
  voidString.SetIsVoid(true);

  mBlobImpl->SetLazyData(voidString, aContentType, aLength, INT64_MAX);

  NormalBlobConstructorParams params(aContentType,
                                     aLength,
                                     void_t() /* optionalBlobData */);
  return SendResolveMystery(params);
}

void
BlobChild::NoteDyingRemoteBlobImpl()
{
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(mRemoteBlobImpl);
  MOZ_ASSERT(!mOwnsBlobImpl);

  // This may be called on any thread due to the fact that RemoteBlobImpl is
  // designed to be passed between threads. We must start the shutdown process
  // on the owning thread, so we proxy here if necessary.
  if (!IsOnOwningThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &BlobChild::NoteDyingRemoteBlobImpl);

    if (mEventTarget) {
      runnable = new CancelableRunnableWrapper(runnable, mEventTarget);

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mEventTarget->Dispatch(runnable,
                                                          NS_DISPATCH_NORMAL)));
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mBlobImpl = nullptr;
  mRemoteBlobImpl = nullptr;

  PBlobChild::Send__delete__(this);
}

bool
BlobChild::IsOnOwningThread() const
{
  return EventTargetIsOnCurrentThread(mEventTarget);
}

void
BlobChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mRemoteBlobImpl) {
    mRemoteBlobImpl->NoteDyingActor();
  }

  if (mBlobImpl && mOwnsBlobImpl) {
    mBlobImpl->Release();
  }

#ifdef DEBUG
  mBlobImpl = nullptr;
  mRemoteBlobImpl = nullptr;
  mBackgroundManager = nullptr;
  mContentManager = nullptr;
  mOwnsBlobImpl = false;
#endif
}

PBlobStreamChild*
BlobChild::AllocPBlobStreamChild(const uint64_t& aStart,
                                 const uint64_t& aLength)
{
  AssertIsOnOwningThread();

  return new InputStreamChild();
}

bool
BlobChild::DeallocPBlobStreamChild(PBlobStreamChild* aActor)
{
  AssertIsOnOwningThread();

  delete static_cast<InputStreamChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BlobParent
 ******************************************************************************/

BlobParent::BlobParent(nsIContentParent* aManager, IDTableEntry* aIDTableEntry)
  : mBackgroundManager(nullptr)
  , mContentManager(aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  CommonInit(aIDTableEntry);
}

BlobParent::BlobParent(PBackgroundParent* aManager, IDTableEntry* aIDTableEntry)
  : mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mEventTarget(do_GetCurrentThread())
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mEventTarget);

  CommonInit(aIDTableEntry);
}

BlobParent::BlobParent(nsIContentParent* aManager,
                       BlobImpl* aBlobImpl,
                       IDTableEntry* aIDTableEntry)
  : mBackgroundManager(nullptr)
  , mContentManager(aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  CommonInit(aBlobImpl, aIDTableEntry);
}

BlobParent::BlobParent(PBackgroundParent* aManager,
                       BlobImpl* aBlobImpl,
                       IDTableEntry* aIDTableEntry)
  : mBackgroundManager(aManager)
  , mContentManager(nullptr)
  , mEventTarget(do_GetCurrentThread())
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mEventTarget);

  CommonInit(aBlobImpl, aIDTableEntry);
}

BlobParent::~BlobParent()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BlobParent);
}

void
BlobParent::CommonInit(IDTableEntry* aIDTableEntry)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aIDTableEntry);
  MOZ_ASSERT(aIDTableEntry->GetBlobImpl());

  MOZ_COUNT_CTOR(BlobParent);

  mBlobImpl = aIDTableEntry->GetBlobImpl();
  mRemoteBlobImpl = nullptr;

  mBlobImpl->AddRef();
  mOwnsBlobImpl = true;

  mIDTableEntry = aIDTableEntry;
}

void
BlobParent::CommonInit(BlobImpl* aBlobImpl, IDTableEntry* aIDTableEntry)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(aIDTableEntry);

  MOZ_COUNT_CTOR(BlobParent);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(aBlobImpl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  nsRefPtr<RemoteBlobImpl> remoteBlobImpl = new RemoteBlobImpl(this, aBlobImpl);

  MOZ_ASSERT(NS_SUCCEEDED(remoteBlobImpl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  mRemoteBlobImpl = remoteBlobImpl;

  remoteBlobImpl.forget(&mBlobImpl);
  mOwnsBlobImpl = true;

  mIDTableEntry = aIDTableEntry;
}

#ifdef DEBUG

void
BlobParent::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(IsOnOwningThread());
}

#endif // DEBUG

// static
void
BlobParent::Startup(const FriendKey& /* aKey */)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  CommonStartup();

  ClearOnShutdown(&sIDTable);

  sIDTableMutex = new Mutex("BlobParent::sIDTableMutex");
  ClearOnShutdown(&sIDTableMutex);
}

// static
BlobParent*
BlobParent::GetOrCreate(nsIContentParent* aManager, BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return GetOrCreateFromImpl(aManager, aBlobImpl);
}

// static
BlobParent*
BlobParent::GetOrCreate(PBackgroundParent* aManager, BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return GetOrCreateFromImpl(aManager, aBlobImpl);
}

// static
BlobParent*
BlobParent::Create(nsIContentParent* aManager,
                   const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
BlobParent*
BlobParent::Create(PBackgroundParent* aManager,
                   const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  return CreateFromParams(aManager, aParams);
}

// static
already_AddRefed<BlobImpl>
BlobParent::GetBlobImplForID(const nsID& aID)
{
  if (NS_WARN_IF(gProcessType != GeckoProcessType_Default)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  nsRefPtr<IDTableEntry> idTableEntry = IDTableEntry::Get(aID);
  if (NS_WARN_IF(!idTableEntry)) {
    return nullptr;
  }

  nsRefPtr<BlobImpl> blobImpl = idTableEntry->GetBlobImpl();
  MOZ_ASSERT(blobImpl);

  return blobImpl.forget();
}

// static
template <class ParentManagerType>
BlobParent*
BlobParent::GetOrCreateFromImpl(ParentManagerType* aManager,
                                BlobImpl* aBlobImpl)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aBlobImpl);

  // If the blob represents a remote blob for this manager then we can simply
  // pass its actor back here.
  if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlobImpl)) {
    BlobParent* actor = MaybeGetActorFromRemoteBlob(remoteBlob, aManager);
    if (actor) {
      return actor;
    }
  }

  // All blobs shared between threads or processes must be immutable.
  if (NS_WARN_IF(NS_FAILED(aBlobImpl->SetMutable(false)))) {
    return nullptr;
  }

  const bool isSameProcessActor = ActorManagerIsSameProcess(aManager);

  AnyBlobConstructorParams blobParams;

  bool isSnapshot;

  if (isSameProcessActor) {
    nsCOMPtr<PIBlobImplSnapshot> snapshot = do_QueryInterface(aBlobImpl);
    isSnapshot = !!snapshot;
  } else {
    isSnapshot = false;
  }

  if (isSameProcessActor && !isSnapshot) {
    nsRefPtr<BlobImpl> sameProcessImpl = aBlobImpl;
    auto addRefedBlobImpl =
      reinterpret_cast<intptr_t>(sameProcessImpl.forget().take());

    blobParams = SameProcessBlobConstructorParams(addRefedBlobImpl);
  } else {
    if (aBlobImpl->IsSizeUnknown() || aBlobImpl->IsDateUnknown()) {
      // We don't want to call GetSize or GetLastModifiedDate yet since that may
      // stat a file on the this thread. Instead we'll learn the size lazily
      // from the other side.
      blobParams = MysteryBlobConstructorParams();
    } else {
      nsString contentType;
      aBlobImpl->GetType(contentType);

      ErrorResult rv;
      uint64_t length = aBlobImpl->GetSize(rv);
      MOZ_ASSERT(!rv.Failed());

      if (aBlobImpl->IsFile()) {
        nsString name;
        aBlobImpl->GetName(name);

        int64_t modDate = aBlobImpl->GetLastModified(rv);
        MOZ_ASSERT(!rv.Failed());

        blobParams =
          FileBlobConstructorParams(name, contentType, length, modDate, void_t());
      } else {
        blobParams = NormalBlobConstructorParams(contentType, length, void_t());
      }
    }
  }

  nsID id;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(gUUIDGenerator->GenerateUUIDInPlace(&id)));

  nsRefPtr<IDTableEntry> idTableEntry =
    IDTableEntry::GetOrCreate(id, ActorManagerProcessID(aManager), aBlobImpl);
  MOZ_ASSERT(idTableEntry);

  BlobParent* actor = new BlobParent(aManager, idTableEntry);

  ChildBlobConstructorParams params(id, blobParams);
  if (NS_WARN_IF(!aManager->SendPBlobConstructor(actor, params))) {
    return nullptr;
  }

  return actor;
}

// static
template <class ParentManagerType>
BlobParent*
BlobParent::CreateFromParams(ParentManagerType* aManager,
                             const ParentBlobConstructorParams& aParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  const AnyBlobConstructorParams& blobParams = aParams.blobParams();

  switch (blobParams.type()) {
    case AnyBlobConstructorParams::TMysteryBlobConstructorParams: {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }

    case AnyBlobConstructorParams::TNormalBlobConstructorParams:
    case AnyBlobConstructorParams::TFileBlobConstructorParams: {
      const OptionalBlobData& optionalBlobData =
        blobParams.type() ==
          AnyBlobConstructorParams::TNormalBlobConstructorParams ?
        blobParams.get_NormalBlobConstructorParams().optionalBlobData() :
        blobParams.get_FileBlobConstructorParams().optionalBlobData();

      if (NS_WARN_IF(optionalBlobData.type() != OptionalBlobData::TBlobData)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      nsRefPtr<BlobImpl> blobImpl =
        CreateBlobImpl(aParams,
                       optionalBlobData.get_BlobData(),
                       ActorManagerIsSameProcess(aManager));
      if (NS_WARN_IF(!blobImpl)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      nsID id;
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(gUUIDGenerator->GenerateUUIDInPlace(&id)));

      nsRefPtr<IDTableEntry> idTableEntry =
        IDTableEntry::Create(id, ActorManagerProcessID(aManager), blobImpl);
      if (NS_WARN_IF(!idTableEntry)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      return new BlobParent(aManager, blobImpl, idTableEntry);
    }

    case AnyBlobConstructorParams::TSlicedBlobConstructorParams: {
      const SlicedBlobConstructorParams& params =
        blobParams.get_SlicedBlobConstructorParams();

      if (NS_WARN_IF(params.end() < params.begin())) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      auto* actor =
        const_cast<BlobParent*>(
          static_cast<const BlobParent*>(params.sourceParent()));
      MOZ_ASSERT(actor);

      nsRefPtr<BlobImpl> source = actor->GetBlobImpl();
      MOZ_ASSERT(source);

      ErrorResult rv;
      nsRefPtr<BlobImpl> slice =
        source->CreateSlice(params.begin(),
                            params.end() - params.begin(),
                            params.contentType(),
                            rv);
      if (NS_WARN_IF(rv.Failed())) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(slice->SetMutable(false)));

      nsRefPtr<IDTableEntry> idTableEntry =
        IDTableEntry::Create(params.id(),
                             ActorManagerProcessID(aManager),
                             slice);
      if (NS_WARN_IF(!idTableEntry)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      return new BlobParent(aManager, slice, idTableEntry);
    }

    case AnyBlobConstructorParams::TKnownBlobConstructorParams: {
      const KnownBlobConstructorParams& params =
        blobParams.get_KnownBlobConstructorParams();

      nsRefPtr<IDTableEntry> idTableEntry =
        IDTableEntry::Get(params.id(), ActorManagerProcessID(aManager));
      if (NS_WARN_IF(!idTableEntry)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      return new BlobParent(aManager, idTableEntry);
    }

    case AnyBlobConstructorParams::TSameProcessBlobConstructorParams: {
      if (NS_WARN_IF(!ActorManagerIsSameProcess(aManager))) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      const SameProcessBlobConstructorParams& params =
        blobParams.get_SameProcessBlobConstructorParams();

      nsRefPtr<BlobImpl> blobImpl =
        dont_AddRef(reinterpret_cast<BlobImpl*>(params.addRefedBlobImpl()));
      MOZ_ASSERT(blobImpl);

      nsID id;
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(gUUIDGenerator->GenerateUUIDInPlace(&id)));

      nsRefPtr<IDTableEntry> idTableEntry =
        IDTableEntry::Create(id, ActorManagerProcessID(aManager), blobImpl);
      MOZ_ASSERT(idTableEntry);

      return new BlobParent(aManager, blobImpl, idTableEntry);
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_CRASH("Should never get here!");
}

// static
template <class ParentManagerType>
BlobParent*
BlobParent::SendSliceConstructor(
                             ParentManagerType* aManager,
                             const ParentBlobConstructorParams& aParams,
                             const ChildBlobConstructorParams& aOtherSideParams)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aManager);

  BlobParent* newActor = BlobParent::Create(aManager, aParams);
  MOZ_ASSERT(newActor);

  if (aManager->SendPBlobConstructor(newActor, aOtherSideParams)) {
    return newActor;
  }

  return nullptr;
}

// static
BlobParent*
BlobParent::MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                                        nsIContentParent* aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aRemoteBlob);
  MOZ_ASSERT(aManager);

  BlobParent* actor = aRemoteBlob->GetBlobParent();
  if (actor && actor->GetContentManager() == aManager) {
    return actor;
  }

  return nullptr;
}

// static
BlobParent*
BlobParent::MaybeGetActorFromRemoteBlob(nsIRemoteBlob* aRemoteBlob,
                                        PBackgroundParent* aManager)
{
  AssertCorrectThreadForManager(aManager);
  MOZ_ASSERT(aRemoteBlob);
  MOZ_ASSERT(aManager);

  BlobParent* actor = aRemoteBlob->GetBlobParent();
  if (actor && actor->GetBackgroundManager() == aManager) {
    return actor;
  }

  return nullptr;
}

already_AddRefed<BlobImpl>
BlobParent::GetBlobImpl()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);

  nsRefPtr<BlobImpl> blobImpl;

  // Remote blobs are held alive until the first call to GetBlobImpl. Thereafter
  // we only hold a weak reference. Normal blobs are held alive until the actor
  // is destroyed.
  if (mRemoteBlobImpl && mOwnsBlobImpl) {
    blobImpl = dont_AddRef(mBlobImpl);
    mOwnsBlobImpl = false;
  } else {
    blobImpl = mBlobImpl;
  }

  MOZ_ASSERT(blobImpl);

  return blobImpl.forget();
}

void
BlobParent::NoteDyingRemoteBlobImpl()
{
  MOZ_ASSERT(mRemoteBlobImpl);
  MOZ_ASSERT(!mOwnsBlobImpl);

  // This may be called on any thread due to the fact that RemoteBlobImpl is
  // designed to be passed between threads. We must start the shutdown process
  // on the main thread, so we proxy here if necessary.
  if (!IsOnOwningThread()) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &BlobParent::NoteDyingRemoteBlobImpl);

    if (mEventTarget) {
      runnable = new CancelableRunnableWrapper(runnable, mEventTarget);

      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mEventTarget->Dispatch(runnable,
                                                          NS_DISPATCH_NORMAL)));
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
    }

    return;
  }

  // Must do this before calling Send__delete__ or we'll crash there trying to
  // access a dangling pointer.
  mBlobImpl = nullptr;
  mRemoteBlobImpl = nullptr;

  unused << PBlobParent::Send__delete__(this);
}

void
BlobParent::NoteRunnableCompleted(OpenStreamRunnable* aRunnable)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRunnable);

  for (uint32_t count = mOpenStreamRunnables.Length(), index = 0;
       index < count;
       index++) {
    nsRevocableEventPtr<OpenStreamRunnable>& runnable =
      mOpenStreamRunnables[index];

    if (runnable.get() == aRunnable) {
      runnable.Forget();
      mOpenStreamRunnables.RemoveElementAt(index);
      return;
    }
  }

  MOZ_CRASH("Runnable not in our array!");
}

bool
BlobParent::IsOnOwningThread() const
{
  return EventTargetIsOnCurrentThread(mEventTarget);
}

void
BlobParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mRemoteBlobImpl) {
    mRemoteBlobImpl->NoteDyingActor();
  }

  if (mBlobImpl && mOwnsBlobImpl) {
    mBlobImpl->Release();
  }

#ifdef DEBUG
  mBlobImpl = nullptr;
  mRemoteBlobImpl = nullptr;
  mBackgroundManager = nullptr;
  mContentManager = nullptr;
  mOwnsBlobImpl = false;
#endif
}

PBlobStreamParent*
BlobParent::AllocPBlobStreamParent(const uint64_t& aStart,
                                   const uint64_t& aLength)
{
  AssertIsOnOwningThread();

  if (NS_WARN_IF(mRemoteBlobImpl)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  return new InputStreamParent();
}

bool
BlobParent::RecvPBlobStreamConstructor(PBlobStreamParent* aActor,
                                       const uint64_t& aStart,
                                       const uint64_t& aLength)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(!mRemoteBlobImpl);
  MOZ_ASSERT(mOwnsBlobImpl);

  auto* actor = static_cast<InputStreamParent*>(aActor);

  // Make sure we can't overflow.
  if (NS_WARN_IF(UINT64_MAX - aLength < aStart)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  ErrorResult errorResult;
  uint64_t blobLength = mBlobImpl->GetSize(errorResult);
  MOZ_ASSERT(!errorResult.Failed());

  if (NS_WARN_IF(aStart + aLength > blobLength)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  nsRefPtr<BlobImpl> blobImpl;

  if (!aStart && aLength == blobLength) {
    blobImpl = mBlobImpl;
  } else {
    nsString type;
    mBlobImpl->GetType(type);

    blobImpl = mBlobImpl->CreateSlice(aStart, aLength, type, errorResult);
    if (NS_WARN_IF(errorResult.Failed())) {
      return false;
    }
  }

  nsCOMPtr<nsIInputStream> stream;
  blobImpl->GetInternalStream(getter_AddRefs(stream), errorResult);
  if (NS_WARN_IF(errorResult.Failed())) {
    return false;
  }

  // If the stream is entirely backed by memory then we can serialize and send
  // it immediately.
  if (mBlobImpl->IsMemoryFile()) {
    InputStreamParams params;
    nsTArray<FileDescriptor> fds;
    SerializeInputStream(stream, params, fds);

    MOZ_ASSERT(params.type() != InputStreamParams::T__None);
    MOZ_ASSERT(fds.IsEmpty());

    return actor->Destroy(params, void_t());
  }

  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(mBlobImpl);
  nsCOMPtr<IPrivateRemoteInputStream> remoteStream;
  if (remoteBlob) {
    remoteStream = do_QueryInterface(stream);
  }

  // There are three cases in which we can use the stream obtained from the blob
  // directly as our serialized stream:
  //
  //   1. The blob is not a remote blob.
  //   2. The blob is a remote blob that represents this actor.
  //   3. The blob is a remote blob representing a different actor but we
  //      already have a non-remote, i.e. serialized, serialized stream.
  //
  // In all other cases we need to be on a background thread before we can get
  // to the real stream.
  nsCOMPtr<nsIIPCSerializableInputStream> serializableStream;
  if (!remoteBlob ||
      remoteBlob->GetBlobParent() == this ||
      !remoteStream) {
    serializableStream = do_QueryInterface(stream);
    if (!serializableStream) {
      MOZ_ASSERT(false, "Must be serializable!");
      return false;
    }
  }

  nsCOMPtr<nsIThread> target;
  errorResult = NS_NewNamedThread("Blob Opener", getter_AddRefs(target));
  if (NS_WARN_IF(errorResult.Failed())) {
    return false;
  }

  nsRefPtr<OpenStreamRunnable> runnable =
    new OpenStreamRunnable(this, actor, stream, serializableStream, target);

  errorResult = runnable->Dispatch();
  if (NS_WARN_IF(errorResult.Failed())) {
    return false;
  }

  // nsRevocableEventPtr lacks some of the operators needed for anything nicer.
  *mOpenStreamRunnables.AppendElement() = runnable;
  return true;
}

bool
BlobParent::DeallocPBlobStreamParent(PBlobStreamParent* aActor)
{
  AssertIsOnOwningThread();

  delete static_cast<InputStreamParent*>(aActor);
  return true;
}

bool
BlobParent::RecvResolveMystery(const ResolveMysteryParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() != ResolveMysteryParams::T__None);
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(!mRemoteBlobImpl);
  MOZ_ASSERT(mOwnsBlobImpl);

  switch (aParams.type()) {
    case ResolveMysteryParams::TNormalBlobConstructorParams: {
      const NormalBlobConstructorParams& params =
        aParams.get_NormalBlobConstructorParams();

      if (NS_WARN_IF(params.length() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      nsString voidString;
      voidString.SetIsVoid(true);

      mBlobImpl->SetLazyData(voidString,
                             params.contentType(),
                             params.length(),
                             INT64_MAX);
      return true;
    }

    case ResolveMysteryParams::TFileBlobConstructorParams: {
      const FileBlobConstructorParams& params =
        aParams.get_FileBlobConstructorParams();
      if (NS_WARN_IF(params.name().IsVoid())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(params.length() == UINT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(params.modDate() == INT64_MAX)) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      mBlobImpl->SetLazyData(params.name(),
                             params.contentType(),
                             params.length(),
                             params.modDate());
      return true;
    }

    default:
      MOZ_CRASH("Unknown params!");
  }

  MOZ_CRASH("Should never get here!");
}

bool
BlobParent::RecvBlobStreamSync(const uint64_t& aStart,
                               const uint64_t& aLength,
                               InputStreamParams* aParams,
                               OptionalFileDescriptorSet* aFDs)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(!mRemoteBlobImpl);
  MOZ_ASSERT(mOwnsBlobImpl);

  bool finished = false;

  {
    // Calling RecvPBlobStreamConstructor() may synchronously delete the actor
    // we pass in so don't touch it outside this block.
    auto* streamActor = new InputStreamParent(&finished, aParams, aFDs);

    if (NS_WARN_IF(!RecvPBlobStreamConstructor(streamActor, aStart, aLength))) {
      // If RecvPBlobStreamConstructor() returns false then it is our
      // responsibility to destroy the actor.
      delete streamActor;
      return false;
    }
  }

  if (finished) {
    // The actor is already dead and we have already set our out params.
    return true;
  }

  // The actor is alive and will be doing asynchronous work to load the stream.
  // Spin a nested loop here while we wait for it.
  nsIThread* currentThread = NS_GetCurrentThread();
  MOZ_ASSERT(currentThread);

  while (!finished) {
    MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(currentThread));
  }

  return true;
}

bool
BlobParent::RecvWaitForSliceCreation()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(mOwnsBlobImpl);

  // The whole point of this message is to ensure that the sliced blob created
  // by the child has been inserted into our IDTable.
  MOZ_ASSERT(mIDTableEntry);

#ifdef DEBUG
  {
    MOZ_ASSERT(sIDTableMutex);
    MutexAutoLock lock(*sIDTableMutex);

    MOZ_ASSERT(sIDTable);
    MOZ_ASSERT(sIDTable->Contains(mIDTableEntry->ID()));
  }
#endif

  return true;
}

bool
BlobParent::RecvGetFileId(int64_t* aFileId)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(!mRemoteBlobImpl);
  MOZ_ASSERT(mOwnsBlobImpl);

  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  *aFileId = mBlobImpl->GetFileId();
  return true;
}

bool
BlobParent::RecvGetFilePath(nsString* aFilePath)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBlobImpl);
  MOZ_ASSERT(!mRemoteBlobImpl);
  MOZ_ASSERT(mOwnsBlobImpl);

  // In desktop e10s the file picker code sends this message.
#ifdef MOZ_CHILD_PERMISSIONS
  if (NS_WARN_IF(!IndexedDatabaseManager::InTestingMode())) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }
#endif

  nsString filePath;
  ErrorResult rv;
  mBlobImpl->GetMozFullPathInternal(filePath, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  *aFilePath = filePath;
  return true;
}

/*******************************************************************************
 * BlobParent::IDTableEntry
 ******************************************************************************/

BlobParent::
IDTableEntry::IDTableEntry(const nsID& aID,
                           intptr_t aProcessID,
                           BlobImpl* aBlobImpl)
  : mID(aID)
  , mProcessID(aProcessID)
  , mBlobImpl(aBlobImpl)
{
  MOZ_ASSERT(aBlobImpl);
}

BlobParent::
IDTableEntry::~IDTableEntry()
{
  MOZ_ASSERT(sIDTableMutex);
  sIDTableMutex->AssertNotCurrentThreadOwns();
  MOZ_ASSERT(sIDTable);

  {
    MutexAutoLock lock(*sIDTableMutex);
    MOZ_ASSERT(sIDTable->Get(mID) == this);

    sIDTable->Remove(mID);

    if (!sIDTable->Count()) {
      sIDTable = nullptr;
    }
  }
}

// static
already_AddRefed<BlobParent::IDTableEntry>
BlobParent::
IDTableEntry::GetOrCreateInternal(const nsID& aID,
                                  intptr_t aProcessID,
                                  BlobImpl* aBlobImpl,
                                  bool aMayCreate,
                                  bool aMayGet,
                                  bool aIgnoreProcessID)
{
  MOZ_ASSERT(gProcessType == GeckoProcessType_Default);
  MOZ_ASSERT(sIDTableMutex);
  sIDTableMutex->AssertNotCurrentThreadOwns();

  nsRefPtr<IDTableEntry> entry;

  {
    MutexAutoLock lock(*sIDTableMutex);

    if (!sIDTable) {
      if (NS_WARN_IF(!aMayCreate)) {
        return nullptr;
      }

      sIDTable = new IDTable();
    }

    entry = sIDTable->Get(aID);

    if (entry) {
      MOZ_ASSERT_IF(aBlobImpl, entry->GetBlobImpl() == aBlobImpl);

      if (NS_WARN_IF(!aMayGet)) {
        return nullptr;
      }

      if (!aIgnoreProcessID && NS_WARN_IF(entry->mProcessID != aProcessID)) {
        return nullptr;
      }
    } else {
      if (NS_WARN_IF(!aMayCreate)) {
        return nullptr;
      }

      MOZ_ASSERT(aBlobImpl);

      entry = new IDTableEntry(aID, aProcessID, aBlobImpl);

      sIDTable->Put(aID, entry);
    }
  }

  MOZ_ASSERT(entry);

  return entry.forget();
}

/*******************************************************************************
 * Other stuff
 ******************************************************************************/

bool
InputStreamChild::Recv__delete__(const InputStreamParams& aParams,
                                 const OptionalFileDescriptorSet& aOptionalSet)
{
  MOZ_ASSERT(mRemoteStream);
  mRemoteStream->AssertIsOnOwningThread();

  nsTArray<FileDescriptor> fds;
  OptionalFileDescriptorSetToFDs(
    // XXX Fix this somehow...
    const_cast<OptionalFileDescriptorSet&>(aOptionalSet),
    fds);

  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aParams, fds);
  MOZ_ASSERT(stream);

  mRemoteStream->SetStream(stream);
  return true;
}

} // namespace dom
} // namespace mozilla
