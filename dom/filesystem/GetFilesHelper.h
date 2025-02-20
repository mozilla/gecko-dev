/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GetFilesHelper_h
#define mozilla_dom_GetFilesHelper_h

#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

class nsIGlobalObject;

namespace mozilla {
class ErrorResult;

namespace dom {

class BlobImpl;
class ContentParent;
class File;
class GetFilesHelperParent;
class OwningFileOrDirectory;
class Promise;

class GetFilesCallback {
 public:
  NS_INLINE_DECL_REFCOUNTING(GetFilesCallback);

  // This is called once per directory scan and is passed an array of all
  // of the files found.
  virtual void Callback(nsresult aStatus,
                        const FallibleTArray<RefPtr<BlobImpl>>& aBlobImpls) = 0;

 protected:
  virtual ~GetFilesCallback() = default;
};

class GetFilesHelperBase {
 protected:
  explicit GetFilesHelperBase(bool aRecursiveFlag)
      : mRecursiveFlag(aRecursiveFlag) {}

  virtual ~GetFilesHelperBase() = default;

  virtual bool IsCanceled() { return false; }

  nsresult ExploreDirectory(const nsAString& aDOMPath, nsIFile* aFile);

  bool mRecursiveFlag;

  // We populate this array in the I/O thread with the BlobImpl.
  FallibleTArray<RefPtr<BlobImpl>> mTargetBlobImplArray;
};

// Retrieving the list of files can be very time/IO consuming. We use this
// helper class to do it just once.
class GetFilesHelper : public Runnable, public GetFilesHelperBase {
  friend class GetFilesHelperParent;
  class ReleaseRunnable;

 public:
  static already_AddRefed<GetFilesHelper> Create(
      const nsTArray<OwningFileOrDirectory>& aFilesOrDirectory,
      bool aRecursiveFlag, ErrorResult& aRv);

  void AddPromise(Promise* aPromise);

  void AddCallback(GetFilesCallback* aCallback);

  using MozPromiseType =
      MozPromise<nsTArray<RefPtr<File>>, nsresult, true>::Private;
  void AddMozPromise(MozPromiseType* aPromise, nsIGlobalObject* aGlobal);

  // CC methods
  void Unlink();
  void Traverse(nsCycleCollectionTraversalCallback& cb);

 protected:
  explicit GetFilesHelper(bool aRecursiveFlag);

  virtual ~GetFilesHelper();

  void SetDirectoryPaths(nsTArray<nsString>&& aDirectoryPaths) {
    mDirectoryPaths = std::move(aDirectoryPaths);
  }

  virtual bool IsCanceled() override {
    MutexAutoLock lock(mMutex);
    return mCanceled;
  }

  virtual void Work(ErrorResult& aRv);

  virtual void Cancel() {};

  NS_IMETHOD
  Run() override;

  void RunIO();

  void OperationCompleted();

  struct MozPromiseAndGlobal {
    RefPtr<MozPromiseType> mMozPromise;
    RefPtr<nsIGlobalObject> mGlobal;
  };

  class PromiseAdapter {
   public:
    explicit PromiseAdapter(MozPromiseAndGlobal&& aMozPromise);
    explicit PromiseAdapter(Promise* aDomPromise);
    ~PromiseAdapter();

    void Clear();
    void Traverse(nsCycleCollectionTraversalCallback& cb);

    nsIGlobalObject* GetGlobalObject();
    void Resolve(nsTArray<RefPtr<File>>&& aFiles);
    void Reject(nsresult aError);

   private:
    using PromiseVariant = Variant<RefPtr<Promise>, MozPromiseAndGlobal>;
    PromiseVariant mPromise;
  };

  void AddPromiseInternal(PromiseAdapter&& aPromise);
  void ResolveOrRejectPromise(PromiseAdapter&& aPromise);

  void RunCallback(GetFilesCallback* aCallback);

  bool mListingCompleted;
  nsTArray<nsString> mDirectoryPaths;

  // Error code to propagate.
  nsresult mErrorResult;

  nsTArray<PromiseAdapter> mPromises;

  nsTArray<RefPtr<GetFilesCallback>> mCallbacks;

  Mutex mMutex MOZ_UNANNOTATED;

  // This variable is protected by mutex.
  bool mCanceled;
};

class GetFilesHelperChild final : public GetFilesHelper {
 public:
  explicit GetFilesHelperChild(bool aRecursiveFlag)
      : GetFilesHelper(aRecursiveFlag), mPendingOperation(false) {}

  virtual void Work(ErrorResult& aRv) override;

  virtual void Cancel() override;

  bool AppendBlobImpl(BlobImpl* aBlobImpl);

  void Finished(nsresult aResult);

 private:
  nsID mUUID;
  bool mPendingOperation;
};

class GetFilesHelperParentCallback;

class GetFilesHelperParent final : public GetFilesHelper {
  friend class GetFilesHelperParentCallback;

 public:
  static already_AddRefed<GetFilesHelperParent> Create(
      const nsID& aUUID, nsTArray<nsString>&& aDirectoryPaths,
      bool aRecursiveFlag, ContentParent* aContentParent, ErrorResult& aRv);

 private:
  GetFilesHelperParent(const nsID& aUUID, ContentParent* aContentParent,
                       bool aRecursiveFlag);

  ~GetFilesHelperParent();

  RefPtr<ContentParent> mContentParent;
  nsID mUUID;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_GetFilesHelper_h
