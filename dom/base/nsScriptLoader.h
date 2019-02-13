/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#ifndef __nsScriptLoader_h__
#define __nsScriptLoader_h__

#include "nsCOMPtr.h"
#include "nsIScriptElement.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsIDocument.h"
#include "nsIStreamLoader.h"
#include "mozilla/CORSMode.h"
#include "mozilla/LinkedList.h"
#include "mozilla/net/ReferrerPolicy.h"

class nsScriptLoadRequestList;
class nsIURI;

namespace JS {
  class SourceBufferHolder;
}

namespace mozilla {
namespace dom {
class AutoJSAPI;
}
}

//////////////////////////////////////////////////////////////
// Per-request data structure
//////////////////////////////////////////////////////////////

class nsScriptLoadRequest final : public nsISupports,
                                  private mozilla::LinkedListElement<nsScriptLoadRequest>
{
  ~nsScriptLoadRequest()
  {
    js_free(mScriptTextBuf);
  }

  typedef LinkedListElement<nsScriptLoadRequest> super;

  // Allow LinkedListElement<nsScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<nsScriptLoadRequest>;
  friend class nsScriptLoadRequestList;

public:
  nsScriptLoadRequest(nsIScriptElement* aElement,
                      uint32_t aVersion,
                      mozilla::CORSMode aCORSMode)
    : mElement(aElement),
      mLoading(true),
      mIsInline(true),
      mHasSourceMapURL(false),
      mIsDefer(false),
      mIsAsync(false),
      mIsNonAsyncScriptInserted(false),
      mIsXSLT(false),
      mIsCanceled(false),
      mScriptTextBuf(nullptr),
      mScriptTextLength(0),
      mJSVersion(aVersion),
      mLineNo(1),
      mCORSMode(aCORSMode),
      mReferrerPolicy(mozilla::net::RP_Default)
  {
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  void FireScriptAvailable(nsresult aResult)
  {
    mElement->ScriptAvailable(aResult, mElement, mIsInline, mURI, mLineNo);
  }
  void FireScriptEvaluated(nsresult aResult)
  {
    mElement->ScriptEvaluated(aResult, mElement, mIsInline);
  }

  bool IsPreload()
  {
    return mElement == nullptr;
  }

  void Cancel()
  {
    mIsCanceled = true;
  }

  bool IsCanceled() const
  {
    return mIsCanceled;
  }

  using super::getNext;
  using super::isInList;

  nsCOMPtr<nsIScriptElement> mElement;
  bool mLoading;          // Are we still waiting for a load to complete?
  bool mIsInline;         // Is the script inline or loaded?
  bool mHasSourceMapURL;  // Does the HTTP header have a source map url?
  bool mIsDefer;          // True if we live in mDeferRequests.
  bool mIsAsync;          // True if we live in mLoadingAsyncRequests or mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted; // True if we live in mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;           // True if we live in mXSLTRequests.
  bool mIsCanceled;       // True if we have been explicitly canceled.
  nsString mSourceMapURL; // Holds source map url for loaded scripts
  char16_t* mScriptTextBuf; // Holds script text for non-inline scripts. Don't
  size_t mScriptTextLength; // use nsString so we can give ownership to jsapi.
  uint32_t mJSVersion;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;
  nsAutoCString mURL;   // Keep the URI's filename alive during off thread parsing.
  int32_t mLineNo;
  const mozilla::CORSMode mCORSMode;
  mozilla::net::ReferrerPolicy mReferrerPolicy;
};

class nsScriptLoadRequestList : private mozilla::LinkedList<nsScriptLoadRequest>
{
  typedef mozilla::LinkedList<nsScriptLoadRequest> super;

public:
  ~nsScriptLoadRequestList();

  void Clear();

#ifdef DEBUG
  bool Contains(nsScriptLoadRequest* aElem);
#endif // DEBUG

  using super::getFirst;
  using super::isEmpty;

  void AppendElement(nsScriptLoadRequest* aElem)
  {
    MOZ_ASSERT(!aElem->isInList());
    NS_ADDREF(aElem);
    insertBack(aElem);
  }

  MOZ_WARN_UNUSED_RESULT
  already_AddRefed<nsScriptLoadRequest> Steal(nsScriptLoadRequest* aElem)
  {
    aElem->removeFrom(*this);
    return dont_AddRef(aElem);
  }

  MOZ_WARN_UNUSED_RESULT
  already_AddRefed<nsScriptLoadRequest> StealFirst()
  {
    MOZ_ASSERT(!isEmpty());
    return Steal(getFirst());
  }
};

//////////////////////////////////////////////////////////////
// Script loader implementation
//////////////////////////////////////////////////////////////

class nsScriptLoader final : public nsIStreamLoaderObserver
{
  class MOZ_STACK_CLASS AutoCurrentScriptUpdater
  {
  public:
    AutoCurrentScriptUpdater(nsScriptLoader* aScriptLoader,
                             nsIScriptElement* aCurrentScript)
      : mOldScript(aScriptLoader->mCurrentScript)
      , mScriptLoader(aScriptLoader)
    {
      mScriptLoader->mCurrentScript = aCurrentScript;
    }
    ~AutoCurrentScriptUpdater()
    {
      mScriptLoader->mCurrentScript.swap(mOldScript);
    }
  private:
    nsCOMPtr<nsIScriptElement> mOldScript;
    nsScriptLoader* mScriptLoader;
  };

  friend class nsScriptRequestProcessor;
  friend class AutoCurrentScriptUpdater;

public:
  explicit nsScriptLoader(nsIDocument* aDocument);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  /**
   * The loader maintains a weak reference to the document with
   * which it is initialized. This call forces the reference to
   * be dropped.
   */
  void DropDocumentReference()
  {
    mDocument = nullptr;
  }

  /**
   * Add an observer for all scripts loaded through this loader.
   *
   * @param aObserver observer for all script processing.
   */
  nsresult AddObserver(nsIScriptLoaderObserver* aObserver)
  {
    return mObservers.AppendObject(aObserver) ? NS_OK :
      NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Remove an observer.
   *
   * @param aObserver observer to be removed
   */
  void RemoveObserver(nsIScriptLoaderObserver* aObserver)
  {
    mObservers.RemoveObject(aObserver);
  }

  /**
   * Process a script element. This will include both loading the 
   * source of the element if it is not inline and evaluating
   * the script itself.
   *
   * If the script is an inline script that can be executed immediately
   * (i.e. there are no other scripts pending) then ScriptAvailable
   * and ScriptEvaluated will be called before the function returns.
   *
   * If true is returned the script could not be executed immediately.
   * In this case ScriptAvailable is guaranteed to be called at a later
   * point (as well as possibly ScriptEvaluated).
   *
   * @param aElement The element representing the script to be loaded and
   *        evaluated.
   */
  bool ProcessScriptElement(nsIScriptElement* aElement);

  /**
   * Gets the currently executing script. This is useful if you want to
   * generate a unique key based on the currently executing script.
   */
  nsIScriptElement* GetCurrentScript()
  {
    return mCurrentScript;
  }

  nsIScriptElement* GetCurrentParserInsertedScript()
  {
    return mCurrentParserInsertedScript;
  }

  /**
   * Whether the loader is enabled or not.
   * When disabled, processing of new script elements is disabled. 
   * Any call to ProcessScriptElement() will return false. Note that
   * this DOES NOT disable currently loading or executing scripts.
   */
  bool GetEnabled()
  {
    return mEnabled;
  }
  void SetEnabled(bool aEnabled)
  {
    if (!mEnabled && aEnabled) {
      ProcessPendingRequestsAsync();
    }
    mEnabled = aEnabled;
  }

  /**
   * Add/remove blocker. Blockers will stop scripts from executing, but not
   * from loading.
   */
  void AddExecuteBlocker()
  {
    ++mBlockerCount;
  }
  void RemoveExecuteBlocker()
  {
    if (!--mBlockerCount) {
      ProcessPendingRequestsAsync();
    }
  }

  /**
   * Convert the given buffer to a UTF-16 string.
   * @param aChannel     Channel corresponding to the data. May be null.
   * @param aData        The data to convert
   * @param aLength      Length of the data
   * @param aHintCharset Hint for the character set (e.g., from a charset
   *                     attribute). May be the empty string.
   * @param aDocument    Document which the data is loaded for. Must not be
   *                     null.
   * @param aBufOut      [out] char16_t array allocated by ConvertToUTF16 and
   *                     containing data converted to unicode.  Caller must
   *                     js_free() this data when no longer needed.
   * @param aLengthOut   [out] Length of array returned in aBufOut in number
   *                     of char16_t code units.
   */
  static nsresult ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                                 uint32_t aLength,
                                 const nsAString& aHintCharset,
                                 nsIDocument* aDocument,
                                 char16_t*& aBufOut, size_t& aLengthOut);

  /**
   * Processes any pending requests that are ready for processing.
   */
  void ProcessPendingRequests();

  /**
   * Check whether it's OK to load a script from aURI in
   * aDocument.
   */
  static nsresult ShouldLoadScript(nsIDocument* aDocument,
                                   nsISupports* aContext,
                                   nsIURI* aURI,
                                   const nsAString &aType);

  /**
   * Starts deferring deferred scripts and puts them in the mDeferredRequests
   * queue instead.
   */
  void BeginDeferringScripts()
  {
    mDeferEnabled = true;
    if (mDocument) {
      mDocument->BlockOnload();
    }
  }

  /**
   * Notifies the script loader that parsing is done.  If aTerminated is true,
   * this will drop any pending scripts that haven't run yet.  Otherwise, it
   * will stops deferring scripts and immediately processes the
   * mDeferredRequests queue.
   *
   * WARNING: This function will synchronously execute content scripts, so be
   * prepared that the world might change around you.
   */
  void ParsingComplete(bool aTerminated);

  /**
   * Returns the number of pending scripts, deferred or not.
   */
  uint32_t HasPendingOrCurrentScripts()
  {
    return mCurrentScript || mParserBlockingRequest;
  }

  /**
   * Adds aURI to the preload list and starts loading it.
   *
   * @param aURI The URI of the external script.
   * @param aCharset The charset parameter for the script.
   * @param aType The type parameter for the script.
   * @param aCrossOrigin The crossorigin attribute for the script.
   *                     Void if not present.
   * @param aScriptFromHead Whether or not the script was a child of head
   */
  virtual void PreloadURI(nsIURI *aURI, const nsAString &aCharset,
                          const nsAString &aType,
                          const nsAString &aCrossOrigin,
                          bool aScriptFromHead,
                          const mozilla::net::ReferrerPolicy aReferrerPolicy);

  /**
   * Process a request that was deferred so that the script could be compiled
   * off thread.
   */
  nsresult ProcessOffThreadRequest(nsScriptLoadRequest *aRequest,
                                   void **aOffThreadToken);

  bool AddPendingChildLoader(nsScriptLoader* aChild) {
    return mPendingChildLoaders.AppendElement(aChild) != nullptr;
  }

private:
  virtual ~nsScriptLoader();

  /**
   * Unblocks the creator parser of the parser-blocking scripts.
   */
  void UnblockParser(nsScriptLoadRequest* aParserBlockingRequest);

  /**
   * Asynchronously resumes the creator parser of the parser-blocking scripts.
   */
  void ContinueParserAsync(nsScriptLoadRequest* aParserBlockingRequest);


  /**
   * Helper function to check the content policy for a given request.
   */
  static nsresult CheckContentPolicy(nsIDocument* aDocument,
                                     nsISupports *aContext,
                                     nsIURI *aURI,
                                     const nsAString &aType);

  /**
   * Start a load for aRequest's URI.
   */
  nsresult StartLoad(nsScriptLoadRequest *aRequest, const nsAString &aType,
                     bool aScriptFromHead);

  /**
   * Process any pending requests asynchronously (i.e. off an event) if there
   * are any. Note that this is a no-op if there aren't any currently pending
   * requests.
   *
   * This function is virtual to allow cross-library calls to SetEnabled()
   */
  virtual void ProcessPendingRequestsAsync();

  /**
   * If true, the loader is ready to execute scripts, and so are all its
   * ancestors.  If the loader itself is ready but some ancestor is not, this
   * function will add an execute blocker and ask the ancestor to remove it
   * once it becomes ready.
   */
  bool ReadyToExecuteScripts();

  /**
   * Return whether just this loader is ready to execute scripts.
   */
  bool SelfReadyToExecuteScripts()
  {
    return mEnabled && !mBlockerCount;
  }

  nsresult AttemptAsyncScriptParse(nsScriptLoadRequest* aRequest);
  nsresult ProcessRequest(nsScriptLoadRequest* aRequest,
                          void **aOffThreadToken = nullptr);
  void FireScriptAvailable(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  void FireScriptEvaluated(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  nsresult EvaluateScript(nsScriptLoadRequest* aRequest,
                          JS::SourceBufferHolder& aSrcBuf,
                          void **aOffThreadToken);

  already_AddRefed<nsIScriptGlobalObject> GetScriptGlobalObject();
  void FillCompileOptionsForRequest(const mozilla::dom::AutoJSAPI &jsapi,
                                    nsScriptLoadRequest *aRequest,
                                    JS::Handle<JSObject *> aScopeChain,
                                    JS::CompileOptions *aOptions);

  nsresult PrepareLoadedRequest(nsScriptLoadRequest* aRequest,
                                nsIStreamLoader* aLoader,
                                nsresult aStatus,
                                uint32_t aStringLen,
                                const uint8_t* aString);

  void AddDeferRequest(nsScriptLoadRequest* aRequest);
  bool MaybeRemovedDeferRequests();

  nsIDocument* mDocument;                   // [WEAK]
  nsCOMArray<nsIScriptLoaderObserver> mObservers;
  nsScriptLoadRequestList mNonAsyncExternalScriptInsertedRequests;
  // mLoadingAsyncRequests holds async requests while they're loading; when they
  // have been loaded they are moved to mLoadedAsyncRequests.
  nsScriptLoadRequestList mLoadingAsyncRequests;
  nsScriptLoadRequestList mLoadedAsyncRequests;
  nsScriptLoadRequestList mDeferRequests;
  nsScriptLoadRequestList mXSLTRequests;
  nsRefPtr<nsScriptLoadRequest> mParserBlockingRequest;

  // In mRequests, the additional information here is stored by the element.
  struct PreloadInfo {
    nsRefPtr<nsScriptLoadRequest> mRequest;
    nsString mCharset;
  };

  struct PreloadRequestComparator {
    bool Equals(const PreloadInfo &aPi, nsScriptLoadRequest * const &aRequest)
        const
    {
      return aRequest == aPi.mRequest;
    }
  };
  struct PreloadURIComparator {
    bool Equals(const PreloadInfo &aPi, nsIURI * const &aURI) const;
  };
  nsTArray<PreloadInfo> mPreloads;

  nsCOMPtr<nsIScriptElement> mCurrentScript;
  nsCOMPtr<nsIScriptElement> mCurrentParserInsertedScript;
  // XXXbz do we want to cycle-collect these or something?  Not sure.
  nsTArray< nsRefPtr<nsScriptLoader> > mPendingChildLoaders;
  uint32_t mBlockerCount;
  bool mEnabled;
  bool mDeferEnabled;
  bool mDocumentParsingDone;
  bool mBlockingDOMContentLoaded;
};

class nsAutoScriptLoaderDisabler
{
public:
  explicit nsAutoScriptLoaderDisabler(nsIDocument* aDoc)
  {
    mLoader = aDoc->ScriptLoader();
    mWasEnabled = mLoader->GetEnabled();
    if (mWasEnabled) {
      mLoader->SetEnabled(false);
    }
  }

  ~nsAutoScriptLoaderDisabler()
  {
    if (mWasEnabled) {
      mLoader->SetEnabled(true);
    }
  }

  bool mWasEnabled;
  nsRefPtr<nsScriptLoader> mLoader;
};

#endif //__nsScriptLoader_h__
