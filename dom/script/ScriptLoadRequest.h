/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoadRequest_h
#define mozilla_dom_ScriptLoadRequest_h

#include "mozilla/CORSMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptElement.h"

class nsICacheInfoChannel;

namespace mozilla {
namespace dom {

class ModuleLoadRequest;
class ScriptLoadRequestList;

enum class ScriptKind { eClassic, eModule };

/*
 * Some options used when fetching script resources. This only loosely
 * corresponds to HTML's "script fetch options".
 *
 * These are common to all modules in a module graph, and hence a single
 * instance is shared by all ModuleLoadRequest objects in a graph.
 */

class ScriptFetchOptions {
  ~ScriptFetchOptions();

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ScriptFetchOptions)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ScriptFetchOptions)

  ScriptFetchOptions(mozilla::CORSMode aCORSMode,
                     mozilla::net::ReferrerPolicy aReferrerPolicy,
                     nsIScriptElement* aElement,
                     nsIPrincipal* aTriggeringPrincipal);

  const mozilla::CORSMode mCORSMode;
  const mozilla::net::ReferrerPolicy mReferrerPolicy;
  nsCOMPtr<nsIScriptElement> mElement;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
};

/*
 * A class that handles loading and evaluation of <script> elements.
 */

class ScriptLoadRequest
    : public nsISupports,
      private mozilla::LinkedListElement<ScriptLoadRequest> {
  typedef LinkedListElement<ScriptLoadRequest> super;

  // Allow LinkedListElement<ScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<ScriptLoadRequest>;
  friend class ScriptLoadRequestList;

 protected:
  virtual ~ScriptLoadRequest();

 public:
  ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                    ScriptFetchOptions* aFetchOptions,
                    const SRIMetadata& aIntegrity, nsIURI* aReferrer);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScriptLoadRequest)

  bool IsModuleRequest() const { return mKind == ScriptKind::eModule; }

  ModuleLoadRequest* AsModuleRequest();

  void FireScriptAvailable(nsresult aResult) {
    bool isInlineClassicScript = mIsInline && !IsModuleRequest();
    Element()->ScriptAvailable(aResult, Element(), isInlineClassicScript, mURI,
                               mLineNo);
  }
  void FireScriptEvaluated(nsresult aResult) {
    Element()->ScriptEvaluated(aResult, Element(), mIsInline);
  }

  bool IsPreload() { return Element() == nullptr; }

  virtual void Cancel();

  bool IsCanceled() const { return mIsCanceled; }

  virtual void SetReady();

  JS::OffThreadToken** OffThreadTokenPtr() {
    return mOffThreadToken ? &mOffThreadToken : nullptr;
  }

  bool IsTracking() const { return mIsTracking; }
  void SetIsTracking() {
    MOZ_ASSERT(!mIsTracking);
    mIsTracking = true;
  }

  enum class Progress : uint8_t {
    eLoading,         // Request either source or bytecode
    eLoading_Source,  // Explicitly Request source stream
    eCompiling,
    eFetchingImports,
    eReady
  };

  bool IsReadyToRun() const { return mProgress == Progress::eReady; }
  bool IsLoading() const {
    return mProgress == Progress::eLoading ||
           mProgress == Progress::eLoading_Source;
  }
  bool IsLoadingSource() const {
    return mProgress == Progress::eLoading_Source;
  }
  bool InCompilingStage() const {
    return mProgress == Progress::eCompiling ||
           (IsReadyToRun() && mWasCompiledOMT);
  }

  // Type of data provided by the nsChannel.
  enum class DataType : uint8_t {
    eUnknown,
    eTextSource,
    eBinASTSource,
    eBytecode
  };

  bool IsUnknownDataType() const { return mDataType == DataType::eUnknown; }
  bool IsTextSource() const { return mDataType == DataType::eTextSource; }
  bool IsBinASTSource() const {
#ifdef JS_BUILD_BINAST
    return mDataType == DataType::eBinASTSource;
#else
    return false;
#endif
  }
  bool IsSource() const { return IsTextSource() || IsBinASTSource(); }
  bool IsBytecode() const { return mDataType == DataType::eBytecode; }

  void SetUnknownDataType();
  void SetTextSource();
  void SetBinASTSource();
  void SetBytecode();

  using ScriptTextBuffer = Vector<char16_t, 0, JSMallocAllocPolicy>;
  using BinASTSourceBuffer = Vector<uint8_t>;

  const ScriptTextBuffer& ScriptText() const {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer>();
  }
  ScriptTextBuffer& ScriptText() {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer>();
  }
  const BinASTSourceBuffer& ScriptBinASTData() const {
    MOZ_ASSERT(IsBinASTSource());
    return mScriptData->as<BinASTSourceBuffer>();
  }
  BinASTSourceBuffer& ScriptBinASTData() {
    MOZ_ASSERT(IsBinASTSource());
    return mScriptData->as<BinASTSourceBuffer>();
  }

  enum class ScriptMode : uint8_t { eBlocking, eDeferred, eAsync };

  void SetScriptMode(bool aDeferAttr, bool aAsyncAttr);

  bool IsBlockingScript() const { return mScriptMode == ScriptMode::eBlocking; }

  bool IsDeferredScript() const { return mScriptMode == ScriptMode::eDeferred; }

  bool IsAsyncScript() const { return mScriptMode == ScriptMode::eAsync; }

  virtual bool IsTopLevel() const {
    // Classic scripts are always top level.
    return true;
  }

  mozilla::CORSMode CORSMode() const { return mFetchOptions->mCORSMode; }
  mozilla::net::ReferrerPolicy ReferrerPolicy() const {
    return mFetchOptions->mReferrerPolicy;
  }
  nsIScriptElement* Element() const { return mFetchOptions->mElement; }
  nsIPrincipal* TriggeringPrincipal() const {
    return mFetchOptions->mTriggeringPrincipal;
  }

  void SetElement(nsIScriptElement* aElement) {
    // Called when a preload request is later used for an actual request.
    MOZ_ASSERT(aElement);
    MOZ_ASSERT(!Element());
    mFetchOptions->mElement = aElement;
  }

  bool ShouldAcceptBinASTEncoding() const;

  void ClearScriptSource();

  void MaybeCancelOffThreadScript();
  void DropBytecodeCacheReferences();

  using super::getNext;
  using super::isInList;

  const ScriptKind
      mKind;  // Whether this is a classic script or a module script.
  ScriptMode mScriptMode;  // Whether this is a blocking, defer or async script.
  Progress mProgress;      // Are we still waiting for a load to complete?
  DataType mDataType;      // Does this contain Source or Bytecode?
  bool mScriptFromHead;    // Synchronous head script block loading of other non
                           // js/css content.
  bool mIsInline;          // Is the script inline or loaded?
  bool mHasSourceMapURL;   // Does the HTTP header have a source map url?
  bool mInDeferList;       // True if we live in mDeferRequests.
  bool mInAsyncList;       // True if we live in mLoadingAsyncRequests or
                           // mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted;  // True if we live in
                                   // mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;                    // True if we live in mXSLTRequests.
  bool mIsCanceled;                // True if we have been explicitly canceled.
  bool
      mWasCompiledOMT;  // True if the script has been compiled off main thread.
  bool mIsTracking;  // True if the script comes from a source on our tracking
                     // protection list.

  RefPtr<ScriptFetchOptions> mFetchOptions;

  JS::OffThreadToken* mOffThreadToken;  // Off-thread parsing token.
  nsString mSourceMapURL;  // Holds source map url for loaded scripts

  // Holds the top-level JSScript that corresponds to the current source, once
  // it is parsed, and planned to be saved in the bytecode cache.
  JS::Heap<JSScript*> mScript;

  // Holds script source data for non-inline scripts. Don't use nsString so we
  // can give ownership to jsapi. Holds either char16_t source text characters
  // or BinAST encoded bytes depending on mSourceEncoding.
  Maybe<Variant<ScriptTextBuffer, BinASTSourceBuffer>> mScriptData;

  // The length of script source text, set when reading completes. This is used
  // since mScriptData is cleared when the source is passed to the JS engine.
  size_t mScriptTextLength;

  // Holds the SRI serialized hash and the script bytecode for non-inline
  // scripts.
  mozilla::Vector<uint8_t> mScriptBytecode;
  uint32_t mBytecodeOffset;  // Offset of the bytecode in mScriptBytecode

  const nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;
  nsAutoCString
      mURL;  // Keep the URI's filename alive during off thread parsing.
  int32_t mLineNo;
  const SRIMetadata mIntegrity;
  const nsCOMPtr<nsIURI> mReferrer;

  // Holds the Cache information, which is used to register the bytecode
  // on the cache entry, such that we can load it the next time.
  nsCOMPtr<nsICacheInfoChannel> mCacheInfo;
};

class ScriptLoadRequestList : private mozilla::LinkedList<ScriptLoadRequest> {
  typedef mozilla::LinkedList<ScriptLoadRequest> super;

 public:
  ~ScriptLoadRequestList();

  void Clear();

#ifdef DEBUG
  bool Contains(ScriptLoadRequest* aElem) const;
#endif  // DEBUG

  using super::getFirst;
  using super::isEmpty;

  void AppendElement(ScriptLoadRequest* aElem) {
    MOZ_ASSERT(!aElem->isInList());
    NS_ADDREF(aElem);
    insertBack(aElem);
  }

  MOZ_MUST_USE
  already_AddRefed<ScriptLoadRequest> Steal(ScriptLoadRequest* aElem) {
    aElem->removeFrom(*this);
    return dont_AddRef(aElem);
  }

  MOZ_MUST_USE
  already_AddRefed<ScriptLoadRequest> StealFirst() {
    MOZ_ASSERT(!isEmpty());
    return Steal(getFirst());
  }

  void Remove(ScriptLoadRequest* aElem) {
    aElem->removeFrom(*this);
    NS_RELEASE(aElem);
  }
};

void ImplCycleCollectionUnlink(ScriptLoadRequestList& aField);

void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                 ScriptLoadRequestList& aField,
                                 const char* aName, uint32_t aFlags);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScriptLoadRequest_h
