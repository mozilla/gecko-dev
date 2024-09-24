/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedScriptCache_h
#define mozilla_dom_SharedScriptCache_h

#include "PLDHashTable.h"                 // PLDHashEntryHdr
#include "js/loader/LoadedScript.h"       // JS::loader::LoadedScript
#include "js/loader/ScriptKind.h"         // JS::loader::ScriptKind
#include "js/loader/ScriptLoadRequest.h"  // JS::loader::ScriptLoadRequest
#include "mozilla/WeakPtr.h"              // SupportsWeakPtr
#include "mozilla/CORSMode.h"             // mozilla::CORSMode
#include "mozilla/MemoryReporting.h"      // MallocSizeOf
#include "mozilla/SharedSubResourceCache.h"  // SharedSubResourceCache, SharedSubResourceCacheLoadingValueBase
#include "mozilla/dom/CacheExpirationTime.h"  // CacheExpirationTime
#include "nsIMemoryReporter.h"  // nsIMemoryReporter, NS_DECL_NSIMEMORYREPORTER
#include "nsIObserver.h"        // nsIObserver, NS_DECL_NSIOBSERVER
#include "nsIPrincipal.h"       // nsIPrincipal
#include "nsISupports.h"        // nsISupports, NS_DECL_ISUPPORTS
#include "nsStringFwd.h"        // nsACString
#include "mozilla/dom/SRIMetadata.h"  // mozilla::dom::SRIMetadata

namespace mozilla {
namespace dom {

class ScriptLoader;
class ScriptLoadData;

class ScriptHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = const ScriptHashKey&;
  using KeyTypePointer = const ScriptHashKey*;

  explicit ScriptHashKey(const ScriptHashKey& aKey)
      : PLDHashEntryHdr(),
        mURI(aKey.mURI),
        mPrincipal(aKey.mPrincipal),
        mLoaderPrincipal(aKey.mLoaderPrincipal),
        mPartitionPrincipal(aKey.mPartitionPrincipal),
        mCORSMode(aKey.mCORSMode),
        mSRIMetadata(aKey.mSRIMetadata),
        mKind(aKey.mKind),
        mNonce(aKey.mNonce),
        mHintCharset(aKey.mHintCharset),
        mIsLinkRelPreload(aKey.mIsLinkRelPreload) {
    MOZ_COUNT_CTOR(ScriptHashKey);
  }

  explicit ScriptHashKey(const ScriptHashKey* aKey) : ScriptHashKey(*aKey) {}

  ScriptHashKey(ScriptHashKey&& aKey)
      : PLDHashEntryHdr(),
        mURI(std::move(aKey.mURI)),
        mPrincipal(std::move(aKey.mPrincipal)),
        mLoaderPrincipal(std::move(aKey.mLoaderPrincipal)),
        mPartitionPrincipal(std::move(aKey.mPartitionPrincipal)),
        mCORSMode(std::move(aKey.mCORSMode)),
        mSRIMetadata(std::move(aKey.mSRIMetadata)),
        mKind(std::move(aKey.mKind)),
        mNonce(std::move(aKey.mNonce)),
        mHintCharset(std::move(aKey.mHintCharset)),
        mIsLinkRelPreload(std::move(aKey.mIsLinkRelPreload)) {
    MOZ_COUNT_CTOR(ScriptHashKey);
  }

  ScriptHashKey(ScriptLoader* aLoader,
                const JS::loader::ScriptLoadRequest* aRequest);
  explicit ScriptHashKey(const ScriptLoadData& aLoadData);

  MOZ_COUNTED_DTOR(ScriptHashKey)

  const ScriptHashKey& GetKey() const { return *this; }
  const ScriptHashKey* GetKeyPointer() const { return this; }

  bool KeyEquals(const ScriptHashKey* aKey) const { return KeyEquals(*aKey); }

  bool KeyEquals(const ScriptHashKey&) const;

  static const ScriptHashKey* KeyToPointer(const ScriptHashKey& aKey) {
    return &aKey;
  }
  static PLDHashNumber HashKey(const ScriptHashKey* aKey) {
    return nsURIHashKey::HashKey(aKey->mURI);
  }

  nsIPrincipal* Principal() const { return mPrincipal; }
  nsIPrincipal* LoaderPrincipal() const { return mLoaderPrincipal; }
  nsIPrincipal* PartitionPrincipal() const { return mPartitionPrincipal; }

  enum { ALLOW_MEMMOVE = true };

 protected:
  const nsCOMPtr<nsIURI> mURI;
  const nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCOMPtr<nsIPrincipal> mLoaderPrincipal;
  const nsCOMPtr<nsIPrincipal> mPartitionPrincipal;
  const CORSMode mCORSMode;
  const SRIMetadata mSRIMetadata;
  const JS::loader::ScriptKind mKind;
  const nsString mNonce;

  // charset attribute for classic script.
  // module always use UTF-8.
  nsString mHintCharset;

  // TODO: Reflect URL classifier data source.
  // mozilla::dom::ContentType
  //   maybe implicit
  // top-level document's host
  //   maybe part of principal?
  //   what if it's inside frame in different host?

  const bool mIsLinkRelPreload;
};

class ScriptLoadData final
    : public SupportsWeakPtr,
      public nsISupports,
      public SharedSubResourceCacheLoadingValueBase<ScriptLoadData> {
 protected:
  virtual ~ScriptLoadData() {}

 public:
  ScriptLoadData(ScriptLoader* aLoader,
                 JS::loader::ScriptLoadRequest* aRequest);

  NS_DECL_ISUPPORTS

  // Only completed loads are used for the cache.
  bool IsLoading() const override { return false; }
  bool IsCancelled() const override { return false; }
  bool IsSyncLoad() const override { return true; }

  void StartLoading() override {}
  void SetLoadCompleted() override {}
  void OnCoalescedTo(const ScriptLoadData& aExistingLoad) override {}
  void Cancel() override {}

  void DidCancelLoad() {}

  bool ShouldDefer() const { return false; }

  JS::loader::LoadedScript* ValueForCache() const {
    return mLoadedScript.get();
  }

  const CacheExpirationTime& ExpirationTime() const { return mExpirationTime; }

  ScriptLoader& Loader() { return *mLoader; }

  const ScriptHashKey& CacheKey() const { return mKey; }

 private:
  CacheExpirationTime mExpirationTime = CacheExpirationTime::Never();
  ScriptLoader* mLoader;
  ScriptHashKey mKey;
  RefPtr<JS::loader::LoadedScript> mLoadedScript;
};

struct SharedScriptCacheTraits {
  using Loader = ScriptLoader;
  using Key = ScriptHashKey;
  using Value = JS::loader::LoadedScript;
  using LoadingValue = ScriptLoadData;

  static ScriptHashKey KeyFromLoadingValue(const LoadingValue& aValue) {
    return ScriptHashKey(aValue);
  }
};

class SharedScriptCache final
    : public SharedSubResourceCache<SharedScriptCacheTraits, SharedScriptCache>,
      public nsIMemoryReporter,
      public nsIObserver {
 public:
  using Base =
      SharedSubResourceCache<SharedScriptCacheTraits, SharedScriptCache>;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSIOBSERVER

  SharedScriptCache();
  void Init();

  // This has to be static because it's also called for loaders that don't have
  // a sheet cache (loaders that are not owned by a document).
  static void LoadCompleted(SharedScriptCache*, ScriptLoadData&);
  using Base::LoadCompleted;
  static void Clear(const Maybe<nsCOMPtr<nsIPrincipal>>& aPrincipal = Nothing(),
                    const Maybe<nsCString>& aSchemelessSite = Nothing(),
                    const Maybe<OriginAttributesPattern>& aPattern = Nothing());

 protected:
  ~SharedScriptCache();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SharedScriptCache_h
