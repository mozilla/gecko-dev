/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_contentanalysis_h
#define mozilla_contentanalysis_h

#include "mozilla/MoveOnlyFunction.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MaybeDiscarded.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/WeakPtr.h"
#include "nsIClipboard.h"
#include "nsIContentAnalysis.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"

#include <regex>
#include <string>

#ifdef XP_WIN
#  include <windows.h>
#endif  // XP_WIN

class nsBaseClipboard;
class nsIPrincipal;
class nsIPrintSettings;
class ContentAnalysisTest;

namespace mozilla::dom {
class CanonicalBrowsingContext;
class DataTransfer;
class WindowGlobalParent;
}  // namespace mozilla::dom

namespace content_analysis::sdk {
class Client;
class ContentAnalysisRequest;
class ContentAnalysisResponse;
}  // namespace content_analysis::sdk

namespace mozilla::contentanalysis {
class ContentAnalysisCallback;

enum class DefaultResult : uint8_t {
  eBlock = 0,
  eWarn = 1,
  eAllow = 2,
  eLastValue = 2
};

class ContentAnalysisDiagnosticInfo final
    : public nsIContentAnalysisDiagnosticInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISDIAGNOSTICINFO
  ContentAnalysisDiagnosticInfo(bool aConnectedToAgent, nsString aAgentPath,
                                bool aFailedSignatureVerification,
                                int64_t aRequestCount)
      : mConnectedToAgent(aConnectedToAgent),
        mAgentPath(std::move(aAgentPath)),
        mFailedSignatureVerification(aFailedSignatureVerification),
        mRequestCount(aRequestCount) {}

 private:
  virtual ~ContentAnalysisDiagnosticInfo() = default;
  bool mConnectedToAgent;
  nsString mAgentPath;
  bool mFailedSignatureVerification;
  int64_t mRequestCount;
};

class ContentAnalysisRequest final : public nsIContentAnalysisRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISREQUEST

  ContentAnalysisRequest(AnalysisType aAnalysisType, Reason aReason,
                         nsString aString, bool aStringIsFilePath,
                         nsCString aSha256Digest, nsCOMPtr<nsIURI> aUrl,
                         OperationType aOperationType,
                         dom::WindowGlobalParent* aWindowGlobalParent,
                         dom::WindowGlobalParent* aSourceWindowGlobal = nullptr,
                         nsCString&& aUserActionId = nsCString());

  ContentAnalysisRequest(AnalysisType aAnalysisType, Reason aReason,
                         nsITransferable* aTransferable,
                         dom::WindowGlobalParent* aWindowGlobal,
                         dom::WindowGlobalParent* aSourceWindowGlobal);

  ContentAnalysisRequest(const nsTArray<uint8_t> aPrintData,
                         nsCOMPtr<nsIURI> aUrl, nsString aPrinterName,
                         Reason aReason,
                         dom::WindowGlobalParent* aWindowGlobalParent);
  static nsresult GetFileDigest(const nsAString& aFilePath,
                                nsCString& aDigestString);

 private:
  virtual ~ContentAnalysisRequest();

  // Remove unneeded copy constructor/assignment
  ContentAnalysisRequest(const ContentAnalysisRequest&) = delete;
  ContentAnalysisRequest& operator=(ContentAnalysisRequest&) = delete;

  // See nsIContentAnalysisRequest for values
  AnalysisType mAnalysisType;

  // See nsIContentAnalysisRequest for values
  Reason mReason;

  RefPtr<nsITransferable> mTransferable;
  RefPtr<dom::DataTransfer> mDataTransfer;

  // Text content to analyze.  Only one of textContent or filePath is defined.
  nsString mTextContent;

  // Name of file to analyze.  Only one of textContent or filePath is defined.
  nsString mFilePath;

  // The URL containing the file download/upload or to which web content is
  // being uploaded.
  nsCOMPtr<nsIURI> mUrl;

  // Sha256 digest of file.
  nsCString mSha256Digest;

  // URLs involved in the download.
  nsTArray<RefPtr<nsIClientDownloadResource>> mResources;

  // Email address of user.
  nsString mEmail;

  // Unique identifier for this request
  nsCString mRequestToken;

  // Unique identifier for this user action.
  // For example, all requests that come from uploading multiple files
  // or one clipboard operation should have the same value.
  nsCString mUserActionId;

  // The number of requests associated with this mUserActionId.
  int64_t mUserActionRequestsCount = 1;

  // Type of text to display, see nsIContentAnalysisRequest for values
  OperationType mOperationTypeForDisplay;

  // String to display if mOperationTypeForDisplay is
  // OPERATION_CUSTOMDISPLAYSTRING
  nsString mOperationDisplayString;

  // The name of the printer being printed to
  nsString mPrinterName;

  RefPtr<dom::WindowGlobalParent> mWindowGlobalParent;
#ifdef XP_WIN
  // The printed data to analyze, in PDF format
  HANDLE mPrintDataHandle = 0;
  // The size of the printed data in mPrintDataHandle
  uint64_t mPrintDataSize = 0;
#endif

  // WindowGlobalParent that is the origin of the data in the request, if known.
  RefPtr<mozilla::dom::WindowGlobalParent> mSourceWindowGlobal;

  friend class ::ContentAnalysisTest;
};

#define CONTENTANALYSIS_IID \
  {0xa37bed74, 0x4b50, 0x443a, {0xbf, 0x58, 0xf4, 0xeb, 0xbd, 0x30, 0x67, 0xb4}}

class ContentAnalysisResponse;
class ContentAnalysis final : public nsIContentAnalysis,
                              public SupportsWeakPtr {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CONTENTANALYSIS_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSIS

  ContentAnalysis();
  void SetLastResult(nsresult aLastResult) { mLastResult = aLastResult; }

#if defined(XP_WIN)
  struct PrintAllowedResult final {
    bool mAllowed;
    dom::MaybeDiscarded<dom::BrowsingContext>
        mCachedStaticDocumentBrowsingContext;
    PrintAllowedResult(bool aAllowed, dom::MaybeDiscarded<dom::BrowsingContext>
                                          aCachedStaticDocumentBrowsingContext)
        : mAllowed(aAllowed),
          mCachedStaticDocumentBrowsingContext(
              aCachedStaticDocumentBrowsingContext) {}
    explicit PrintAllowedResult(bool aAllowed)
        : PrintAllowedResult(aAllowed, dom::MaybeDiscardedBrowsingContext()) {}
  };
  struct PrintAllowedError final {
    nsresult mError;
    dom::MaybeDiscarded<dom::BrowsingContext>
        mCachedStaticDocumentBrowsingContext;
    PrintAllowedError(nsresult aError, dom::MaybeDiscarded<dom::BrowsingContext>
                                           aCachedStaticDocumentBrowsingContext)
        : mError(aError),
          mCachedStaticDocumentBrowsingContext(
              aCachedStaticDocumentBrowsingContext) {}
    explicit PrintAllowedError(nsresult aError)
        : PrintAllowedError(aError, dom::MaybeDiscardedBrowsingContext()) {}
  };
  using PrintAllowedPromise =
      MozPromise<PrintAllowedResult, PrintAllowedError, true>;
  MOZ_CAN_RUN_SCRIPT static RefPtr<PrintAllowedPromise>
  PrintToPDFToDetermineIfPrintAllowed(
      dom::CanonicalBrowsingContext* aBrowsingContext,
      nsIPrintSettings* aPrintSettings);
#endif  // defined(XP_WIN)

  // Find the outermost browsing context that has same-origin access to
  // aBrowsingContext, and this is the URL we will pass to the Content Analysis
  // agent.
  static nsCOMPtr<nsIURI> GetURIForBrowsingContext(
      dom::CanonicalBrowsingContext* aBrowsingContext);
  static bool CheckClipboardContentAnalysisSync(
      nsBaseClipboard* aClipboard, mozilla::dom::WindowGlobalParent* aWindow,
      const nsCOMPtr<nsITransferable>& trans,
      nsIClipboard::ClipboardType aClipboardType);
  static void CheckClipboardContentAnalysis(
      nsBaseClipboard* aClipboard, mozilla::dom::WindowGlobalParent* aWindow,
      nsITransferable* aTransferable,
      nsIClipboard::ClipboardType aClipboardType,
      ContentAnalysisCallback* aResolver, bool aForFullClipboard = false);
  static RefPtr<ContentAnalysis> GetContentAnalysisFromService();
  void CancelWithError(nsCString&& aUserActionId, nsresult aResult);

  // Duration the cache holds requests for. This holds strong references
  // to the elements of the request, such as the WindowGlobalParent,
  // for that period.
  static constexpr uint32_t kDefaultCachedDataTimeoutInMs = 5000;
  // These are the MIME types that Content Analysis can analyze.
  static constexpr const char* kKnownClipboardTypes[] = {
      kTextMime, kHTMLMime, kCustomTypesMime, kFileMime};

 private:
  virtual ~ContentAnalysis();
  // Remove unneeded copy constructor/assignment
  ContentAnalysis(const ContentAnalysis&) = delete;
  ContentAnalysis& operator=(ContentAnalysis&) = delete;
  nsresult CreateContentAnalysisClient(nsCString&& aPipePathName,
                                       nsString&& aClientSignatureSetting,
                                       bool aIsPerUser);

  nsresult RunAnalyzeRequestTask(
      const RefPtr<nsIContentAnalysisRequest>& aRequest, bool aAutoAcknowledge,
      const RefPtr<nsIContentAnalysisCallback>& aCallback);
  nsresult RunAcknowledgeTask(
      nsIContentAnalysisAcknowledgement* aAcknowledgement,
      const nsACString& aRequestToken);
  static void DoAnalyzeRequest(
      nsCString&& aUserActionId,
      content_analysis::sdk::ContentAnalysisRequest&& aRequest,
      const std::shared_ptr<content_analysis::sdk::Client>& aClient);
  void IssueResponse(ContentAnalysisResponse* response,
                     nsCString&& aUserActionId);
  void NotifyResponseObservers(ContentAnalysisResponse* aResponse,
                               nsCString&& aUserActionId);
  void NotifyObserversAndMaybeIssueResponse(ContentAnalysisResponse* aResponse,
                                            nsCString&& aUserActionId);
  bool LastRequestSucceeded();
  // Did the URL filter completely handle the request or do we need to check
  // with the agent.
  enum UrlFilterResult { eCheck, eDeny, eAllow };

  UrlFilterResult FilterByUrlLists(nsIContentAnalysisRequest* aRequest,
                                   nsIURI* aUri);
  void EnsureParsedUrlFilters();

  using ContentAnalysisRequestArray =
      CopyableTArray<RefPtr<nsIContentAnalysisRequest>>;
  using RequestsPromise =
      MozPromise<ContentAnalysisRequestArray, nsresult, true>;

  // Counts the number of times it receives an "allow content" and (1) calls
  // ContentResult on mCallback when all requests are approved, (2) calls
  // ContentResult and cancels outstanding scans when any one request is
  // rejected, or (3) calls Error and cancels outstanding scans when any one
  // fails.
  // Once constructed, this object is required to eventually issue a response to
  // the given callback.
  // This class doesn't care if it receives more calls than there are requests.
  // Canceling issues callback calls with no initiating request.  This class
  // relays the verdicts on a first-come-first-served basis, so a cancel
  // that comes before an allow overrides that allow, and vice-versa.
  class MultipartRequestCallback : public nsIContentAnalysisCallback {
   public:
    NS_INLINE_DECL_REFCOUNTING(MultipartRequestCallback, override)
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

    NS_DECL_NSICONTENTANALYSISCALLBACK

    static RefPtr<MultipartRequestCallback> Create(
        ContentAnalysis* aContentAnalysis,
        const nsTArray<ContentAnalysis::ContentAnalysisRequestArray>& aRequests,
        nsIContentAnalysisCallback* aCallback, bool aAutoAcknowledge);

   private:
    MultipartRequestCallback() = default;
    virtual ~MultipartRequestCallback();

    template <typename T, typename... Args>
    friend RefPtr<T> mozilla::MakeRefPtr(Args&&...);

    void Initialize(
        ContentAnalysis* aContentAnalysis,
        const nsTArray<ContentAnalysis::ContentAnalysisRequestArray>& aRequests,
        nsIContentAnalysisCallback* aCallback, bool aAutoAcknowledge);

    void CancelRequests();
    void RemoveFromUserActionMap();

    WeakPtr<ContentAnalysis> mWeakContentAnalysis;
    RefPtr<nsIContentAnalysisCallback> mCallback;
    nsCString mUserActionId;

    // Number of CA requests remaining for this transaction.
    size_t mNumCARequestsRemaining;

    // True if we have issued a response for these requests.
    bool mResponded = false;
  };

  Result<RefPtr<RequestsPromise::AllPromiseType>,
         RefPtr<nsIContentAnalysisResult>>
  GetFinalRequestList(const ContentAnalysisRequestArray& aRequests);

  Result<RefPtr<RequestsPromise>, nsresult> ExpandFolderRequest(
      nsIContentAnalysisRequest* aRequest, nsIFile* file);

  using ClientPromise =
      MozPromise<std::shared_ptr<content_analysis::sdk::Client>, nsresult,
                 false>;
  int64_t mRequestCount = 0;
  RefPtr<ClientPromise::Private> mCaClientPromise;
  // Only accessed from the main thread
  bool mClientCreationAttempted;

  bool mSetByEnterprise;
  nsresult mLastResult = NS_OK;

  struct UserActionData final {
    RefPtr<nsIContentAnalysisCallback> mCallback;
    nsTHashSet<nsCString> mRequestTokens;
    bool mAutoAcknowledge;
  };

  // This map is stored so that requests can be canceled while they are
  // still being checked.  It is maintained by our inner class
  // MultipartRequestCallback.
  nsTHashMap<nsCString, UserActionData> mUserActionMap;

  class CachedClipboardResponse {
   public:
    CachedClipboardResponse() = default;
    Maybe<nsIContentAnalysisResponse::Action> GetCachedResponse(
        nsIURI* aURI, int32_t aClipboardSequenceNumber);
    void SetCachedResponse(const nsCOMPtr<nsIURI>& aURI,
                           int32_t aClipboardSequenceNumber,
                           nsIContentAnalysisResponse::Action aAction);

   private:
    Maybe<int32_t> mClipboardSequenceNumber;
    nsTArray<std::pair<nsCOMPtr<nsIURI>, nsIContentAnalysisResponse::Action>>
        mData;
  };
  CachedClipboardResponse mCachedClipboardResponse;

  struct WarnResponseData {
    RefPtr<ContentAnalysisResponse> mResponse;
    nsCString mUserActionId;
  };
  nsTHashMap<nsCString, WarnResponseData> mWarnResponseDataMap;

  std::vector<std::regex> mAllowUrlList;
  std::vector<std::regex> mDenyUrlList;
  bool mParsedUrlLists = false;
  bool mForbidFutureRequests = false;
  bool mIsShuttingDown = false;

  friend class ContentAnalysisResponse;
  friend class ::ContentAnalysisTest;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ContentAnalysis, CONTENTANALYSIS_IID)

class ContentAnalysisResponse final : public nsIContentAnalysisResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESULT
  NS_DECL_NSICONTENTANALYSISRESPONSE

  void SetOwner(ContentAnalysis* aOwner);
  void DoNotAcknowledge() { mDoNotAcknowledge = true; }
  void SetCancelError(CancelError aCancelError);
  void SetIsCachedResponse() { mIsCachedResponse = true; }

 private:
  virtual ~ContentAnalysisResponse() = default;
  // Remove unneeded copy constructor/assignment
  ContentAnalysisResponse(const ContentAnalysisResponse&) = delete;
  ContentAnalysisResponse& operator=(ContentAnalysisResponse&) = delete;
  explicit ContentAnalysisResponse(
      content_analysis::sdk::ContentAnalysisResponse&& aResponse);
  ContentAnalysisResponse(Action aAction, const nsACString& aRequestToken);

  // Use MakeRefPtr as factory.
  template <typename T, typename... Args>
  friend RefPtr<T> mozilla::MakeRefPtr(Args&&...);

  static already_AddRefed<ContentAnalysisResponse> FromProtobuf(
      content_analysis::sdk::ContentAnalysisResponse&& aResponse);

  void ResolveWarnAction(bool aAllowContent);

  // Action requested by the agent
  Action mAction;

  // Identifier for the corresponding nsIContentAnalysisRequest
  nsCString mRequestToken;

  // If mAction is eCanceled, this is the error explaining why the request was
  // canceled, or eUserInitiated if the user canceled it.
  CancelError mCancelError = CancelError::eUserInitiated;

  // ContentAnalysis (or, more precisely, its Client object) must outlive
  // the transaction.
  RefPtr<ContentAnalysis> mOwner;

  // Whether the response has been acknowledged
  bool mHasAcknowledged = false;

  // If true, the request was completely handled by URL filter lists, so it
  // was not sent to the agent and should not send an Acknowledge.
  bool mDoNotAcknowledge = false;

  // Whether this is a cached result that wasn't actually sent to the DLP agent.
  // This indicates that the request was a duplicate of a previously sent one,
  // so any dialogs (for block/warn) should not be shown.
  bool mIsCachedResponse = false;

  friend class ContentAnalysis;
};

class ContentAnalysisAcknowledgement final
    : public nsIContentAnalysisAcknowledgement {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISACKNOWLEDGEMENT

  ContentAnalysisAcknowledgement(Result aResult, FinalAction aFinalAction);

 private:
  virtual ~ContentAnalysisAcknowledgement() = default;

  Result mResult;
  FinalAction mFinalAction;
};

/**
 * This class:
 * 1. Asserts if the callback is not called before destruction.
 * 2. Takes a strong reference to the nsIContentAnalysisResult when
 *    calling the callback, which guarantees that someone does.  Otherwise,
 *    if neither the caller nor the callback did, then the result would leak.
 */
class ContentAnalysisCallback final : public nsIContentAnalysisCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISCALLBACK
  ContentAnalysisCallback(
      std::function<void(nsIContentAnalysisResult*)>&& aContentResponseCallback,
      std::function<void(nsresult)>&& aErrorCallback)
      : mContentResponseCallback(std::move(aContentResponseCallback)),
        mErrorCallback(std::move(aErrorCallback)) {}

  explicit ContentAnalysisCallback(
      std::function<void(nsIContentAnalysisResult*)>&&
          aContentResponseCallback);

  // Wrap a given callback, in case it doesn't provide the guarantees that
  // this one does (such as checking that it is eventually called).
  explicit ContentAnalysisCallback(nsIContentAnalysisCallback* aDecoratedCB) {
    mContentResponseCallback = [decoratedCB = RefPtr{aDecoratedCB}](
                                   nsIContentAnalysisResult* aResult) {
      decoratedCB->ContentResult(aResult);
    };
    mErrorCallback = [decoratedCB = RefPtr{aDecoratedCB}](nsresult aRv) {
      decoratedCB->Error(aRv);
    };
  }

 private:
  virtual ~ContentAnalysisCallback() {
    MOZ_ASSERT(!mContentResponseCallback && !mErrorCallback && !mPromise,
               "ContentAnalysisCallback never called!");
  }

  // Called after callbacks are called.
  void ClearCallbacks() {
    mContentResponseCallback = nullptr;
    mErrorCallback = nullptr;
    mPromise = nullptr;
  }

  explicit ContentAnalysisCallback(dom::Promise* aPromise);
  std::function<void(nsIContentAnalysisResult*)> mContentResponseCallback;
  std::function<void(nsresult)> mErrorCallback;
  RefPtr<dom::Promise> mPromise;
  friend class ContentAnalysis;
};

}  // namespace mozilla::contentanalysis

#endif  // mozilla_contentanalysis_h
