/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchStream_h
#define mozilla_dom_FetchStream_h

#include "Fetch.h"
#include "jsapi.h"
#include "js/Stream.h"
#include "nsIAsyncInputStream.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsWeakReference.h"

class nsIGlobalObject;

class nsIInputStream;

namespace mozilla {
namespace dom {

class FetchStreamHolder;
class WeakWorkerRef;

class FetchStream final : public nsIInputStreamCallback,
                          public nsIObserver,
                          public nsSupportsWeakReference,
                          private JS::ReadableStreamUnderlyingSource {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOBSERVER

  static void Create(JSContext* aCx, FetchStreamHolder* aStreamHolder,
                     nsIGlobalObject* aGlobal, nsIInputStream* aInputStream,
                     JS::MutableHandle<JSObject*> aStream, ErrorResult& aRv);

  void Close();

  static nsresult RetrieveInputStream(
      JS::ReadableStreamUnderlyingSource* aUnderlyingReadableStreamSource,
      nsIInputStream** aInputStream);

 private:
  FetchStream(nsIGlobalObject* aGlobal, FetchStreamHolder* aStreamHolder,
              nsIInputStream* aInputStream);
  ~FetchStream();

#ifdef DEBUG
  void AssertIsOnOwningThread();
#else
  void AssertIsOnOwningThread() {}
#endif

  void requestData(JSContext* aCx, JS::HandleObject aStream,
                   size_t aDesiredSize) override;

  void writeIntoReadRequestBuffer(JSContext* aCx, JS::HandleObject aStream,
                                  void* aBuffer, size_t aLength,
                                  size_t* aBytesWritten) override;

  JS::Value cancel(JSContext* aCx, JS::HandleObject aStream,
                   JS::HandleValue aReason) override;

  void onClosed(JSContext* aCx, JS::HandleObject aStream) override;

  void onErrored(JSContext* aCx, JS::HandleObject aStream,
                 JS::HandleValue aReason) override;

  void finalize() override;

  void ErrorPropagation(JSContext* aCx, const MutexAutoLock& aProofOfLock,
                        JS::HandleObject aStream, nsresult aRv);

  void CloseAndReleaseObjects(JSContext* aCx, const MutexAutoLock& aProofOfLock,
                              JS::HandleObject aSteam);

  class WorkerShutdown;

  void ReleaseObjects(const MutexAutoLock& aProofOfLock);

  void ReleaseObjects();

  // Common methods

  enum State {
    // This is the beginning state before any reading operation.
    eInitializing,

    // RequestDataCallback has not been called yet. We haven't started to read
    // data from the stream yet.
    eWaiting,

    // We are reading data in a separate I/O thread.
    eReading,

    // We are ready to write something in the JS Buffer.
    eWriting,

    // After a writing, we want to check if the stream is closed. After the
    // check, we go back to eWaiting. If a reading request happens in the
    // meantime, we move to eReading state.
    eChecking,

    // Operation completed.
    eClosed,
  };

  // We need a mutex because JS engine can release FetchStream on a non-owning
  // thread. We must be sure that the releasing of resources doesn't trigger
  // race conditions.
  Mutex mMutex;

  // Protected by mutex.
  State mState;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<FetchStreamHolder> mStreamHolder;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  // This is the original inputStream received during the CTOR. It will be
  // converted into an nsIAsyncInputStream and stored into mInputStream at the
  // first use.
  nsCOMPtr<nsIInputStream> mOriginalInputStream;
  nsCOMPtr<nsIAsyncInputStream> mInputStream;

  RefPtr<WeakWorkerRef> mWorkerRef;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FetchStream_h
