/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLWorker.h"

#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

namespace mozilla::dom {

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public WorkerMainThreadRunnable {
 private:
  BlobImpl* mBlobImpl;
  nsACString& mURL;

 public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, BlobImpl* aBlobImpl,
                    nsACString& aURL)
      : WorkerMainThreadRunnable(aWorkerPrivate, "URL :: CreateURL"_ns),
        mBlobImpl(aBlobImpl),
        mURL(aURL) {
    MOZ_ASSERT(aBlobImpl);
  }

  bool MainThreadRun() override {
    using namespace mozilla::ipc;

    MOZ_ASSERT(mWorkerRef);
    WorkerPrivate* workerPrivate = mWorkerRef->Private();

    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal = workerPrivate->GetPrincipal();

    nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
        workerPrivate->CookieJarSettings();

    nsAutoString partKey;
    cookieJarSettings->GetPartitionKey(partKey);

    nsresult rv = BlobURLProtocolHandler::AddDataEntry(
        mBlobImpl, principal, NS_ConvertUTF16toUTF8(partKey), mURL);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to add data entry for the blob!");
      mURL.SetIsVoid(true);
      return false;
    }
    return true;
  }
};

// This class revokes an URL on the main thread.
class RevokeURLRunnable : public WorkerMainThreadRunnable {
 private:
  const nsCString mURL;

 public:
  RevokeURLRunnable(WorkerPrivate* aWorkerPrivate, const nsACString& aURL)
      : WorkerMainThreadRunnable(aWorkerPrivate, "URL :: RevokeURL"_ns),
        mURL(aURL) {}

  bool MainThreadRun() override {
    AssertIsOnMainThread();

    MOZ_ASSERT(mWorkerRef);
    WorkerPrivate* workerPrivate = mWorkerRef->Private();

    nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
        workerPrivate->CookieJarSettings();

    nsAutoString partKey;
    cookieJarSettings->GetPartitionKey(partKey);

    BlobURLProtocolHandler::RemoveDataEntry(mURL, workerPrivate->GetPrincipal(),
                                            NS_ConvertUTF16toUTF8(partKey));
    return true;
  }
};

// This class checks if an URL is valid and belongs to a Blob on the main
// thread.
class IsBoundToBlobRunnable : public WorkerMainThreadRunnable {
 private:
  const nsCString mURL;
  bool mResult;

 public:
  IsBoundToBlobRunnable(WorkerPrivate* aWorkerPrivate, const nsACString& aURL)
      : WorkerMainThreadRunnable(aWorkerPrivate, "URL :: IsValidURL"_ns),
        mURL(aURL),
        mResult(false) {}

  bool MainThreadRun() override {
    AssertIsOnMainThread();

    mResult = BlobURLProtocolHandler::HasDataEntryTypeBlob(mURL);

    return true;
  }

  bool Result() const { return mResult; }
};

/* static */
void URLWorker::CreateObjectURL(const GlobalObject& aGlobal,
                                const BlobOrMediaSource& aObj,
                                nsACString& aResult,
                                mozilla::ErrorResult& aRv) {
  if (!aObj.IsBlob()) {
    MOZ_CRASH("MediaSource is not supported in workers");
    return;
  }

  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<BlobImpl> blobImpl = aObj.GetAsBlob().Impl();
  MOZ_ASSERT(blobImpl);

  RefPtr<CreateURLRunnable> runnable =
      new CreateURLRunnable(workerPrivate, blobImpl, aResult);

  runnable->Dispatch(workerPrivate, Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  WorkerGlobalScope* scope = workerPrivate->GlobalScope();
  MOZ_ASSERT(scope);

  scope->RegisterHostObjectURI(aResult);
}

/* static */
void URLWorker::RevokeObjectURL(const GlobalObject& aGlobal,
                                const nsACString& aUrl, ErrorResult& aRv) {
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<RevokeURLRunnable> runnable =
      new RevokeURLRunnable(workerPrivate, aUrl);

  runnable->Dispatch(workerPrivate, Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  WorkerGlobalScope* scope = workerPrivate->GlobalScope();
  MOZ_ASSERT(scope);

  scope->UnregisterHostObjectURI(aUrl);
}

/* static */
bool URLWorker::IsBoundToBlob(const GlobalObject& aGlobal,
                              const nsACString& aUrl, ErrorResult& aRv) {
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<IsBoundToBlobRunnable> runnable =
      new IsBoundToBlobRunnable(workerPrivate, aUrl);

  runnable->Dispatch(workerPrivate, Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return false;
  }

  return runnable->Result();
}

}  // namespace mozilla::dom
