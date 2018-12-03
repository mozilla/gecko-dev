/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"
#include "FetchConsumer.h"

#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileCreatorHelper.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIInputStreamPump.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsProxyRelease.h"

// Undefine the macro of CreateFile to avoid FileCreatorHelper#CreateFile being
// replaced by FileCreatorHelper#CreateFileW.
#ifdef CreateFile
#undef CreateFile
#endif

namespace mozilla {
namespace dom {

namespace {

template <class Derived>
class BeginConsumeBodyRunnable final : public Runnable {
 public:
  BeginConsumeBodyRunnable(FetchBodyConsumer<Derived>* aConsumer,
                           ThreadSafeWorkerRef* aWorkerRef)
      : Runnable("BeginConsumeBodyRunnable"),
        mFetchBodyConsumer(aConsumer),
        mWorkerRef(aWorkerRef) {}

  NS_IMETHOD
  Run() override {
    mFetchBodyConsumer->BeginConsumeBodyMainThread(mWorkerRef);
    return NS_OK;
  }

 private:
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

/*
 * Called on successfully reading the complete stream.
 */
template <class Derived>
class ContinueConsumeBodyRunnable final : public MainThreadWorkerRunnable {
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;
  nsresult mStatus;
  uint32_t mLength;
  uint8_t* mResult;

 public:
  ContinueConsumeBodyRunnable(FetchBodyConsumer<Derived>* aFetchBodyConsumer,
                              WorkerPrivate* aWorkerPrivate, nsresult aStatus,
                              uint32_t aLength, uint8_t* aResult)
      : MainThreadWorkerRunnable(aWorkerPrivate),
        mFetchBodyConsumer(aFetchBodyConsumer),
        mStatus(aStatus),
        mLength(aLength),
        mResult(aResult) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mFetchBodyConsumer->ContinueConsumeBody(mStatus, mLength, mResult);
    return true;
  }
};

// ControlRunnable used to complete the releasing of resources on the worker
// thread when already shutting down.
template <class Derived>
class AbortConsumeBodyControlRunnable final
    : public MainThreadWorkerControlRunnable {
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;

 public:
  AbortConsumeBodyControlRunnable(
      FetchBodyConsumer<Derived>* aFetchBodyConsumer,
      WorkerPrivate* aWorkerPrivate)
      : MainThreadWorkerControlRunnable(aWorkerPrivate),
        mFetchBodyConsumer(aFetchBodyConsumer) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mFetchBodyConsumer->ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr,
                                            true /* shutting down */);
    return true;
  }
};

/*
 * In case of failure to create a stream pump or dispatch stream completion to
 * worker, ensure we cleanup properly. Thread agnostic.
 */
template <class Derived>
class MOZ_STACK_CLASS AutoFailConsumeBody final {
 public:
  AutoFailConsumeBody(FetchBodyConsumer<Derived>* aBodyConsumer,
                      ThreadSafeWorkerRef* aWorkerRef)
      : mBodyConsumer(aBodyConsumer), mWorkerRef(aWorkerRef) {}

  ~AutoFailConsumeBody() {
    AssertIsOnMainThread();

    if (!mBodyConsumer) {
      return;
    }

    // Web Worker
    if (mWorkerRef) {
      RefPtr<AbortConsumeBodyControlRunnable<Derived>> r =
          new AbortConsumeBodyControlRunnable<Derived>(mBodyConsumer,
                                                       mWorkerRef->Private());
      if (!r->Dispatch()) {
        MOZ_CRASH("We are going to leak");
      }
      return;
    }

    // Main-thread
    mBodyConsumer->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
  }

  void DontFail() { mBodyConsumer = nullptr; }

 private:
  RefPtr<FetchBodyConsumer<Derived>> mBodyConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

/*
 * Called on successfully reading the complete stream for Blob.
 */
template <class Derived>
class ContinueConsumeBlobBodyRunnable final : public MainThreadWorkerRunnable {
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;
  RefPtr<BlobImpl> mBlobImpl;

 public:
  ContinueConsumeBlobBodyRunnable(
      FetchBodyConsumer<Derived>* aFetchBodyConsumer,
      WorkerPrivate* aWorkerPrivate, BlobImpl* aBlobImpl)
      : MainThreadWorkerRunnable(aWorkerPrivate),
        mFetchBodyConsumer(aFetchBodyConsumer),
        mBlobImpl(aBlobImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBlobImpl);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mFetchBodyConsumer->ContinueConsumeBlobBody(mBlobImpl);
    return true;
  }
};

// ControlRunnable used to complete the releasing of resources on the worker
// thread when already shutting down.
template <class Derived>
class AbortConsumeBlobBodyControlRunnable final
    : public MainThreadWorkerControlRunnable {
  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;

 public:
  AbortConsumeBlobBodyControlRunnable(
      FetchBodyConsumer<Derived>* aFetchBodyConsumer,
      WorkerPrivate* aWorkerPrivate)
      : MainThreadWorkerControlRunnable(aWorkerPrivate),
        mFetchBodyConsumer(aFetchBodyConsumer) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mFetchBodyConsumer->ContinueConsumeBlobBody(nullptr,
                                                true /* shutting down */);
    return true;
  }
};

template <class Derived>
class ConsumeBodyDoneObserver final : public nsIStreamLoaderObserver,
                                      public MutableBlobStorageCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ConsumeBodyDoneObserver(FetchBodyConsumer<Derived>* aFetchBodyConsumer,
                          ThreadSafeWorkerRef* aWorkerRef)
      : mFetchBodyConsumer(aFetchBodyConsumer), mWorkerRef(aWorkerRef) {}

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aCtxt,
                   nsresult aStatus, uint32_t aResultLength,
                   const uint8_t* aResult) override {
    MOZ_ASSERT(NS_IsMainThread());

    // The loading is completed. Let's nullify the pump before continuing the
    // consuming of the body.
    mFetchBodyConsumer->NullifyConsumeBodyPump();

    uint8_t* nonconstResult = const_cast<uint8_t*>(aResult);

    // Main-thread.
    if (!mWorkerRef) {
      mFetchBodyConsumer->ContinueConsumeBody(aStatus, aResultLength,
                                              nonconstResult);
      // FetchBody is responsible for data.
      return NS_SUCCESS_ADOPTED_DATA;
    }

    // Web Worker.
    {
      RefPtr<ContinueConsumeBodyRunnable<Derived>> r =
          new ContinueConsumeBodyRunnable<Derived>(
              mFetchBodyConsumer, mWorkerRef->Private(), aStatus, aResultLength,
              nonconstResult);
      if (r->Dispatch()) {
        // FetchBody is responsible for data.
        return NS_SUCCESS_ADOPTED_DATA;
      }
    }

    // The worker is shutting down. Let's use a control runnable to complete the
    // shutting down procedure.

    RefPtr<AbortConsumeBodyControlRunnable<Derived>> r =
        new AbortConsumeBodyControlRunnable<Derived>(mFetchBodyConsumer,
                                                     mWorkerRef->Private());
    if (NS_WARN_IF(!r->Dispatch())) {
      return NS_ERROR_FAILURE;
    }

    // We haven't taken ownership of the data.
    return NS_OK;
  }

  virtual void BlobStoreCompleted(MutableBlobStorage* aBlobStorage, Blob* aBlob,
                                  nsresult aRv) override {
    // On error.
    if (NS_FAILED(aRv)) {
      OnStreamComplete(nullptr, nullptr, aRv, 0, nullptr);
      return;
    }

    // The loading is completed. Let's nullify the pump before continuing the
    // consuming of the body.
    mFetchBodyConsumer->NullifyConsumeBodyPump();

    mFetchBodyConsumer->OnBlobResult(aBlob, mWorkerRef);
  }

 private:
  ~ConsumeBodyDoneObserver() = default;

  RefPtr<FetchBodyConsumer<Derived>> mFetchBodyConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

template <class Derived>
NS_IMPL_ADDREF(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_IMPL_RELEASE(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_INTERFACE_MAP_BEGIN(ConsumeBodyDoneObserver<Derived>)
NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamLoaderObserver)
NS_INTERFACE_MAP_END

}  // anonymous

template <class Derived>
/* static */ already_AddRefed<Promise> FetchBodyConsumer<Derived>::Create(
    nsIGlobalObject* aGlobal, nsIEventTarget* aMainThreadEventTarget,
    FetchBody<Derived>* aBody, nsIInputStream* aBodyStream,
    AbortSignalImpl* aSignalImpl, FetchConsumeType aType, ErrorResult& aRv) {
  MOZ_ASSERT(aBody);
  MOZ_ASSERT(aBodyStream);
  MOZ_ASSERT(aMainThreadEventTarget);

  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<FetchBodyConsumer<Derived>> consumer = new FetchBodyConsumer<Derived>(
      aMainThreadEventTarget, aGlobal, aBody, aBodyStream, promise, aType);

  RefPtr<ThreadSafeWorkerRef> workerRef;

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<StrongWorkerRef> strongWorkerRef = StrongWorkerRef::Create(
        workerPrivate, "FetchBodyConsumer",
        [consumer]() { consumer->ShutDownMainThreadConsuming(); });
    if (NS_WARN_IF(!strongWorkerRef)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    workerRef = new ThreadSafeWorkerRef(strongWorkerRef);
  } else {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!os)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    aRv = os->AddObserver(consumer, DOM_WINDOW_DESTROYED_TOPIC, true);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    aRv = os->AddObserver(consumer, DOM_WINDOW_FROZEN_TOPIC, true);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  nsCOMPtr<nsIRunnable> r =
      new BeginConsumeBodyRunnable<Derived>(consumer, workerRef);
  aRv = aMainThreadEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (aSignalImpl) {
    consumer->Follow(aSignalImpl);
  }

  return promise.forget();
}

template <class Derived>
void FetchBodyConsumer<Derived>::ReleaseObject() {
  AssertIsOnTargetThread();

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
      os->RemoveObserver(this, DOM_WINDOW_FROZEN_TOPIC);
    }
  }

  mGlobal = nullptr;

#ifdef DEBUG
  mBody = nullptr;
#endif

  Unfollow();
}

template <class Derived>
FetchBodyConsumer<Derived>::FetchBodyConsumer(
    nsIEventTarget* aMainThreadEventTarget, nsIGlobalObject* aGlobalObject,
    FetchBody<Derived>* aBody, nsIInputStream* aBodyStream, Promise* aPromise,
    FetchConsumeType aType)
    : mTargetThread(NS_GetCurrentThread()),
      mMainThreadEventTarget(aMainThreadEventTarget)
#ifdef DEBUG
      ,
      mBody(aBody)
#endif
      ,
      mBodyStream(aBodyStream),
      mBlobStorageType(MutableBlobStorage::eOnlyInMemory),
      mBodyBlobURISpec(aBody ? aBody->BodyBlobURISpec() : VoidCString()),
      mBodyLocalPath(aBody ? aBody->BodyLocalPath() : VoidString()),
      mGlobal(aGlobalObject),
      mConsumeType(aType),
      mConsumePromise(aPromise),
      mBodyConsumed(false),
      mShuttingDown(false) {
  MOZ_ASSERT(aMainThreadEventTarget);
  MOZ_ASSERT(aBody);
  MOZ_ASSERT(aBodyStream);
  MOZ_ASSERT(aPromise);

  const mozilla::UniquePtr<mozilla::ipc::PrincipalInfo>& principalInfo =
      aBody->DerivedClass()->GetPrincipalInfo();
  // We support temporary file for blobs only if the principal is known and
  // it's system or content not in private Browsing.
  if (principalInfo &&
      (principalInfo->type() ==
           mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo ||
       (principalInfo->type() ==
            mozilla::ipc::PrincipalInfo::TContentPrincipalInfo &&
        principalInfo->get_ContentPrincipalInfo().attrs().mPrivateBrowsingId ==
            0))) {
    mBlobStorageType = MutableBlobStorage::eCouldBeInTemporaryFile;
  }

  mBodyMimeType = aBody->MimeType();
}

template <class Derived>
FetchBodyConsumer<Derived>::~FetchBodyConsumer() {}

template <class Derived>
void FetchBodyConsumer<Derived>::AssertIsOnTargetThread() const {
  MOZ_ASSERT(NS_GetCurrentThread() == mTargetThread);
}

namespace {

template <class Derived>
class FileCreationHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static void Create(Promise* aPromise, FetchBodyConsumer<Derived>* aConsumer,
                     ThreadSafeWorkerRef* aWorkerRef) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPromise);

    RefPtr<FileCreationHandler> handler =
        new FileCreationHandler<Derived>(aConsumer, aWorkerRef);
    aPromise->AppendNativeHandler(handler);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    AssertIsOnMainThread();

    if (NS_WARN_IF(!aValue.isObject())) {
      mConsumer->OnBlobResult(nullptr, mWorkerRef);
      return;
    }

    RefPtr<Blob> blob;
    if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Blob, &aValue.toObject(), blob)))) {
      mConsumer->OnBlobResult(nullptr, mWorkerRef);
      return;
    }

    mConsumer->OnBlobResult(blob, mWorkerRef);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    AssertIsOnMainThread();

    mConsumer->OnBlobResult(nullptr, mWorkerRef);
  }

 private:
  FileCreationHandler<Derived>(FetchBodyConsumer<Derived>* aConsumer,
                               ThreadSafeWorkerRef* aWorkerRef)
      : mConsumer(aConsumer), mWorkerRef(aWorkerRef) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aConsumer);
  }

  ~FileCreationHandler() = default;

  RefPtr<FetchBodyConsumer<Derived>> mConsumer;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

template <class Derived>
NS_IMPL_ADDREF(FileCreationHandler<Derived>)
template <class Derived>
NS_IMPL_RELEASE(FileCreationHandler<Derived>)
template <class Derived>
NS_INTERFACE_MAP_BEGIN(FileCreationHandler<Derived>)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace

template <class Derived>
nsresult FetchBodyConsumer<Derived>::GetBodyLocalFile(nsIFile** aFile) const {
  AssertIsOnMainThread();

  if (!mBodyLocalPath.Length()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = file->InitWithPath(mBodyLocalPath);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  bool isDir;
  rv = file->IsDirectory(&isDir);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isDir) {
    return NS_ERROR_FILE_IS_DIRECTORY;
  }

  file.forget(aFile);
  return NS_OK;
}

/*
 * BeginConsumeBodyMainThread() will automatically reject the consume promise
 * and clean up on any failures, so there is no need for callers to do so,
 * reflected in a lack of error return code.
 */
template <class Derived>
void FetchBodyConsumer<Derived>::BeginConsumeBodyMainThread(
    ThreadSafeWorkerRef* aWorkerRef) {
  AssertIsOnMainThread();

  AutoFailConsumeBody<Derived> autoReject(this, aWorkerRef);

  if (mShuttingDown) {
    // We haven't started yet, but we have been terminated. AutoFailConsumeBody
    // will dispatch a runnable to release resources.
    return;
  }

  if (mConsumeType == CONSUME_BLOB) {
    nsresult rv;

    // If we're trying to consume a blob, and the request was for a blob URI,
    // then just consume that URI's blob instance.
    if (!mBodyBlobURISpec.IsEmpty()) {
      RefPtr<BlobImpl> blobImpl;
      rv = NS_GetBlobForBlobURISpec(mBodyBlobURISpec, getter_AddRefs(blobImpl));
      if (NS_WARN_IF(NS_FAILED(rv)) || !blobImpl) {
        return;
      }
      autoReject.DontFail();
      DispatchContinueConsumeBlobBody(blobImpl, aWorkerRef);
      return;
    }

    // If we're trying to consume a blob, and the request was for a local
    // file, then generate and return a File blob.
    nsCOMPtr<nsIFile> file;
    rv = GetBodyLocalFile(getter_AddRefs(file));
    if (!NS_WARN_IF(NS_FAILED(rv)) && file) {
      ChromeFilePropertyBag bag;
      bag.mType = NS_ConvertUTF8toUTF16(mBodyMimeType);

      ErrorResult error;
      RefPtr<Promise> promise =
          FileCreatorHelper::CreateFile(mGlobal, file, bag, true, error);
      if (NS_WARN_IF(error.Failed())) {
        return;
      }

      autoReject.DontFail();
      FileCreationHandler<Derived>::Create(promise, this, aWorkerRef);
      return;
    }
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  nsresult rv =
      NS_NewInputStreamPump(getter_AddRefs(pump), mBodyStream.forget(), 0, 0,
                            false, mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<ConsumeBodyDoneObserver<Derived>> p =
      new ConsumeBodyDoneObserver<Derived>(this, aWorkerRef);

  nsCOMPtr<nsIStreamListener> listener;
  if (mConsumeType == CONSUME_BLOB) {
    listener = new MutableBlobStreamListener(
        mBlobStorageType, nullptr, mBodyMimeType, p, mMainThreadEventTarget);
  } else {
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), p);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    listener = loader;
  }

  rv = pump->AsyncRead(listener, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Now that everything succeeded, we can assign the pump to a pointer that
  // stays alive for the lifetime of the FetchConsumer.
  mConsumeBodyPump = pump;

  // It is ok for retargeting to fail and reads to happen on the main thread.
  autoReject.DontFail();

  // Try to retarget, otherwise fall back to main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(pump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_FAILED(rv)) {
      NS_WARNING("Retargeting failed");
    }
  }
}

/*
 * OnBlobResult() is called when a blob body is ready to be consumed (when its
 * network transfer completes in BeginConsumeBodyRunnable or its local File has
 * been wrapped by FileCreationHandler). The blob is sent to the target thread
 * and ContinueConsumeBody is called.
 */
template <class Derived>
void FetchBodyConsumer<Derived>::OnBlobResult(Blob* aBlob,
                                              ThreadSafeWorkerRef* aWorkerRef) {
  AssertIsOnMainThread();

  DispatchContinueConsumeBlobBody(aBlob ? aBlob->Impl() : nullptr, aWorkerRef);
}

template <class Derived>
void FetchBodyConsumer<Derived>::DispatchContinueConsumeBlobBody(
    BlobImpl* aBlobImpl, ThreadSafeWorkerRef* aWorkerRef) {
  AssertIsOnMainThread();

  // Main-thread.
  if (!aWorkerRef) {
    if (aBlobImpl) {
      ContinueConsumeBlobBody(aBlobImpl);
    } else {
      ContinueConsumeBody(NS_ERROR_DOM_ABORT_ERR, 0, nullptr);
    }
    return;
  }

  // Web Worker.
  if (aBlobImpl) {
    RefPtr<ContinueConsumeBlobBodyRunnable<Derived>> r =
        new ContinueConsumeBlobBodyRunnable<Derived>(
            this, aWorkerRef->Private(), aBlobImpl);

    if (r->Dispatch()) {
      return;
    }
  } else {
    RefPtr<ContinueConsumeBodyRunnable<Derived>> r =
        new ContinueConsumeBodyRunnable<Derived>(
            this, aWorkerRef->Private(), NS_ERROR_DOM_ABORT_ERR, 0, nullptr);

    if (r->Dispatch()) {
      return;
    }
  }

  // The worker is shutting down. Let's use a control runnable to complete the
  // shutting down procedure.

  RefPtr<AbortConsumeBlobBodyControlRunnable<Derived>> r =
      new AbortConsumeBlobBodyControlRunnable<Derived>(this,
                                                       aWorkerRef->Private());

  Unused << NS_WARN_IF(!r->Dispatch());
}

/*
 * ContinueConsumeBody() is to be called on the target thread whenever the
 * final result of the fetch is known. The fetch promise is resolved or
 * rejected based on whether the fetch succeeded, and the body can be
 * converted into the expected type of JS object.
 */
template <class Derived>
void FetchBodyConsumer<Derived>::ContinueConsumeBody(nsresult aStatus,
                                                     uint32_t aResultLength,
                                                     uint8_t* aResult,
                                                     bool aShuttingDown) {
  AssertIsOnTargetThread();

  // This makes sure that we free the data correctly.
  auto autoFree = mozilla::MakeScopeExit([&] { free(aResult); });

  if (mBodyConsumed) {
    return;
  }
  mBodyConsumed = true;

  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBody->CheckBodyUsed());

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = mConsumePromise.forget();

  RefPtr<FetchBodyConsumer<Derived>> self = this;
  auto autoReleaseObject =
      mozilla::MakeScopeExit([self] { self->ReleaseObject(); });

  if (aShuttingDown) {
    // If shutting down, we don't want to resolve any promise.
    return;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    // Per
    // https://fetch.spec.whatwg.org/#concept-read-all-bytes-from-readablestream
    // Decoding errors should reject with a TypeError
    if (aStatus == NS_ERROR_INVALID_CONTENT_ENCODING) {
      IgnoredErrorResult rv;
      rv.ThrowTypeError<MSG_DOM_DECODING_FAILED>();
      localPromise->MaybeReject(rv);
    } else {
      localPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }
  }

  // Don't warn here since we warned above.
  if (NS_FAILED(aStatus)) {
    return;
  }

  // Finish successfully consuming body according to type.
  MOZ_ASSERT(aResult);

  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    localPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = jsapi.cx();
  ErrorResult error;

  switch (mConsumeType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      BodyUtil::ConsumeArrayBuffer(cx, &arrayBuffer, aResultLength, aResult,
                                   error);

      if (!error.Failed()) {
        JS::Rooted<JS::Value> val(cx);
        val.setObjectOrNull(arrayBuffer);

        localPromise->MaybeResolve(cx, val);
        // ArrayBuffer takes over ownership.
        aResult = nullptr;
      }
      break;
    }
    case CONSUME_BLOB: {
      MOZ_CRASH("This should not happen.");
      break;
    }
    case CONSUME_FORMDATA: {
      nsCString data;
      data.Adopt(reinterpret_cast<char*>(aResult), aResultLength);
      aResult = nullptr;

      RefPtr<dom::FormData> fd =
          BodyUtil::ConsumeFormData(mGlobal, mBodyMimeType, data, error);
      if (!error.Failed()) {
        localPromise->MaybeResolve(fd);
      }
      break;
    }
    case CONSUME_TEXT:
      // fall through handles early exit.
    case CONSUME_JSON: {
      nsString decoded;
      if (NS_SUCCEEDED(
              BodyUtil::ConsumeText(aResultLength, aResult, decoded))) {
        if (mConsumeType == CONSUME_TEXT) {
          localPromise->MaybeResolve(decoded);
        } else {
          JS::Rooted<JS::Value> json(cx);
          BodyUtil::ConsumeJson(cx, &json, decoded, error);
          if (!error.Failed()) {
            localPromise->MaybeResolve(cx, json);
          }
        }
      };
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected consume body type");
  }

  error.WouldReportJSException();
  if (error.Failed()) {
    localPromise->MaybeReject(error);
  }
}

template <class Derived>
void FetchBodyConsumer<Derived>::ContinueConsumeBlobBody(BlobImpl* aBlobImpl,
                                                         bool aShuttingDown) {
  AssertIsOnTargetThread();
  MOZ_ASSERT(mConsumeType == CONSUME_BLOB);

  if (mBodyConsumed) {
    return;
  }
  mBodyConsumed = true;

  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBody->CheckBodyUsed());

  MOZ_ASSERT(mConsumePromise);
  RefPtr<Promise> localPromise = mConsumePromise.forget();

  if (!aShuttingDown) {
    RefPtr<dom::Blob> blob = dom::Blob::Create(mGlobal, aBlobImpl);
    MOZ_ASSERT(blob);

    localPromise->MaybeResolve(blob);
  }

  ReleaseObject();
}

template <class Derived>
void FetchBodyConsumer<Derived>::ShutDownMainThreadConsuming() {
  if (!NS_IsMainThread()) {
    RefPtr<FetchBodyConsumer<Derived>> self = this;

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "FetchBodyConsumer::ShutDownMainThreadConsuming",
        [self]() { self->ShutDownMainThreadConsuming(); });

    mMainThreadEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    return;
  }

  // We need this because maybe, mConsumeBodyPump has not been created yet. We
  // must be sure that we don't try to do it.
  mShuttingDown = true;

  if (mConsumeBodyPump) {
    mConsumeBodyPump->Cancel(NS_BINDING_ABORTED);
    mConsumeBodyPump = nullptr;
  }
}

template <class Derived>
NS_IMETHODIMP FetchBodyConsumer<Derived>::Observe(nsISupports* aSubject,
                                                  const char* aTopic,
                                                  const char16_t* aData) {
  AssertIsOnMainThread();

  MOZ_ASSERT((strcmp(aTopic, DOM_WINDOW_FROZEN_TOPIC) == 0) ||
             (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0));

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (SameCOMIdentity(aSubject, window)) {
    ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr);
  }

  return NS_OK;
}

template <class Derived>
void FetchBodyConsumer<Derived>::Abort() {
  AssertIsOnTargetThread();
  ShutDownMainThreadConsuming();
  ContinueConsumeBody(NS_ERROR_DOM_ABORT_ERR, 0, nullptr);
}

template <class Derived>
NS_IMPL_ADDREF(FetchBodyConsumer<Derived>)

template <class Derived>
NS_IMPL_RELEASE(FetchBodyConsumer<Derived>)

template <class Derived>
NS_IMPL_QUERY_INTERFACE(FetchBodyConsumer<Derived>, nsIObserver,
                        nsISupportsWeakReference)

}  // namespace dom
}  // namespace mozilla
