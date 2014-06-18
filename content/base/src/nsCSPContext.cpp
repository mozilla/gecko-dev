/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsCSPContext.h"
#include "nsCSPParser.h"
#include "nsCSPService.h"
#include "nsError.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannelPolicy.h"
#include "nsIClassInfoImpl.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMWindowUtils.h"
#include "nsIHttpChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIPropertyBag2.h"
#include "nsIStringStream.h"
#include "nsIUploadChannel.h"
#include "nsIScriptError.h"
#include "nsIWebNavigation.h"
#include "nsIWritablePropertyBag2.h"
#include "nsNetUtil.h"
#include "nsSupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "prlog.h"

using namespace mozilla;

#if defined(PR_LOGGING)
static PRLogModuleInfo *
GetCspContextLog()
{
  static PRLogModuleInfo *gCspContextPRLog;
  if (!gCspContextPRLog)
    gCspContextPRLog = PR_NewLogModule("CSPContext");
  return gCspContextPRLog;
}
#endif

#define CSPCONTEXTLOG(args) PR_LOG(GetCspContextLog(), 4, args)

static const uint32_t CSP_CACHE_URI_CUTOFF_SIZE = 512;

/**
 * Creates a key for use in the ShouldLoad cache.
 * Looks like: <uri>!<nsIContentPolicy::LOAD_TYPE>
 */
nsresult
CreateCacheKey_Internal(nsIURI* aContentLocation,
                        nsContentPolicyType aContentType,
                        nsACString& outCacheKey)
{
  if (!aContentLocation) {
    return NS_ERROR_FAILURE;
  }

  bool isDataScheme = false;
  nsresult rv = aContentLocation->SchemeIs("data", &isDataScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  outCacheKey.Truncate();
  if (aContentType != nsIContentPolicy::TYPE_SCRIPT && isDataScheme) {
    // For non-script data: URI, use ("data:", aContentType) as the cache key.
    outCacheKey.Append(NS_LITERAL_CSTRING("data:"));
    outCacheKey.AppendInt(aContentType);
    return NS_OK;
  }

  nsAutoCString spec;
  rv = aContentLocation->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't cache for a URI longer than the cutoff size.
  if (spec.Length() <= CSP_CACHE_URI_CUTOFF_SIZE) {
    outCacheKey.Append(spec);
    outCacheKey.Append(NS_LITERAL_CSTRING("!"));
    outCacheKey.AppendInt(aContentType);
  }

  return NS_OK;
}

/* =====  nsIContentSecurityPolicy impl ====== */

NS_IMETHODIMP
nsCSPContext::ShouldLoad(nsContentPolicyType aContentType,
                         nsIURI*             aContentLocation,
                         nsIURI*             aRequestOrigin,
                         nsISupports*        aRequestContext,
                         const nsACString&   aMimeTypeGuess,
                         nsISupports*        aExtra,
                         int16_t*            outDecision)
{
#ifdef PR_LOGGING
  {
  nsAutoCString spec;
  aContentLocation->GetSpec(spec);
  CSPCONTEXTLOG(("nsCSPContext::ShouldLoad, aContentLocation: %s", spec.get()));
  }
#endif

  nsresult rv = NS_OK;

  // This ShouldLoad function is called from nsCSPService::ShouldLoad,
  // which already checked a number of things, including:
  // * aContentLocation is not null; we can consume this without further checks
  // * scheme is not a whitelisted scheme (about: chrome:, etc).
  // * CSP is enabled
  // * Content Type is not whitelisted (CSP Reports, TYPE_DOCUMENT, etc).
  // * Fast Path for Apps

  nsAutoCString cacheKey;
  rv = CreateCacheKey_Internal(aContentLocation, aContentType, cacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isCached = mShouldLoadCache.Get(cacheKey, outDecision);
  if (isCached && cacheKey.Length() > 0) {
    // this is cached, use the cached value.
    return NS_OK;
  }

  // Default decision, CSP can revise it if there's a policy to enforce
  *outDecision = nsIContentPolicy::ACCEPT;

  // This may be a load or a preload. If it is a preload, the document will
  // not have been fully parsed yet, and aRequestContext will be an
  // nsIDOMHTMLDocument rather than the nsIDOMHTMLElement associated with the
  // resource. As a result, we cannot extract the element's corresponding
  // nonce attribute, and so we cannot correctly check the nonce on a preload.
  //
  // Therefore, the decision returned here for a preload may be *incorrect* as
  // it cannot take the nonce into account. We will still check the load, but
  // we will not cache the result or report a violation. When the "real load"
  // happens subsequently, we will re-check with the additional context to
  // make a final decision.
  //
  // We don't just return false because that would block all preloads and
  // degrade performance. However, we do want to block preloads that are
  // clearly blocked (their urls are not whitelisted) by CSP.

  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryInterface(aRequestContext);
  bool isPreload = doc &&
                   (aContentType == nsIContentPolicy::TYPE_SCRIPT ||
                    aContentType == nsIContentPolicy::TYPE_STYLESHEET);

  nsAutoString nonce;
  if (!isPreload) {
    nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(aRequestContext);
    if (htmlElement) {
      rv = htmlElement->GetAttribute(NS_LITERAL_STRING("nonce"), nonce);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsAutoString violatedDirective;
  for (uint32_t p = 0; p < mPolicies.Length(); p++) {
    if (!mPolicies[p]->permits(aContentType,
                               aContentLocation,
                               nonce,
                               violatedDirective)) {
      // If the policy is violated and not report-only, reject the load and
      // report to the console
      if (!mPolicies[p]->getReportOnlyFlag()) {
        CSPCONTEXTLOG(("nsCSPContext::ShouldLoad, nsIContentPolicy::REJECT_SERVER"));
        *outDecision = nsIContentPolicy::REJECT_SERVER;
      }

      // Do not send a report or notify observers if this is a preload - the
      // decision may be wrong due to the inability to get the nonce, and will
      // incorrectly fail the unit tests.
      if (!isPreload) {
        this->AsyncReportViolation(aContentLocation,
                                   aRequestOrigin,
                                   violatedDirective,
                                   p,             /* policy index        */
                                   EmptyString(), /* no observer subject */
                                   EmptyString(), /* no source file      */
                                   EmptyString(), /* no script sample    */
                                   0);            /* no line number      */
      }
    }
  }
  // Done looping, cache any relevant result
  if (cacheKey.Length() > 0 && !isPreload) {
    mShouldLoadCache.Put(cacheKey, *outDecision);
  }

#ifdef PR_LOGGING
  {
  nsAutoCString spec;
  aContentLocation->GetSpec(spec);
  CSPCONTEXTLOG(("nsCSPContext::ShouldLoad, decision: %s, aContentLocation: %s", *outDecision ? "load" : "deny", spec.get()));
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::ShouldProcess(nsContentPolicyType aContentType,
                            nsIURI*             aContentLocation,
                            nsIURI*             aRequestOrigin,
                            nsISupports*        aRequestContext,
                            const nsACString&   aMimeType,
                            nsISupports*        aExtra,
                            int16_t*            outDecision)
{
  *outDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

/* ===== nsISupports implementation ========== */

NS_IMPL_CLASSINFO(nsCSPContext,
                  nullptr,
                  nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_CSPCONTEXT_CID)

NS_IMPL_ISUPPORTS_CI(nsCSPContext,
                     nsIContentSecurityPolicy,
                     nsISerializable)

nsCSPContext::nsCSPContext()
  : mSelfURI(nullptr)
{
  CSPCONTEXTLOG(("nsCSPContext::nsCSPContext"));
}

nsCSPContext::~nsCSPContext()
{
  CSPCONTEXTLOG(("nsCSPContext::~nsCSPContext"));
  for (uint32_t i = 0; i < mPolicies.Length(); i++) {
    delete mPolicies[i];
  }
  mShouldLoadCache.Clear();
}

NS_IMETHODIMP
nsCSPContext::GetIsInitialized(bool *outIsInitialized)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSPContext::GetPolicy(uint32_t aIndex, nsAString& outStr)
{
  if (aIndex >= mPolicies.Length()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  mPolicies[aIndex]->toString(outStr);
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::GetPolicyCount(uint32_t *outPolicyCount)
{
  *outPolicyCount = mPolicies.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::RemovePolicy(uint32_t aIndex)
{
  if (aIndex >= mPolicies.Length()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  mPolicies.RemoveElementAt(aIndex);
  // reset cache since effective policy changes
  mShouldLoadCache.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::AppendPolicy(const nsAString& aPolicyString,
                           nsIURI* aSelfURI,
                           bool aReportOnly,
                           bool aSpecCompliant)
{
  CSPCONTEXTLOG(("nsCSPContext::AppendPolicy: %s",
                 NS_ConvertUTF16toUTF8(aPolicyString).get()));

  if (aSelfURI) {
    // aSelfURI will be disregarded since we will remove it with bug 991474
    NS_WARNING("aSelfURI should be a nullptr in AppendPolicy and removed in bug 991474");
  }
  // Use the mSelfURI from setRequestContext, see bug 991474
  NS_ASSERTION(mSelfURI, "mSelfURI required for AppendPolicy, but not set");
  nsCSPPolicy* policy = nsCSPParser::parseContentSecurityPolicy(aPolicyString, mSelfURI, aReportOnly, mInnerWindowID);
  if (policy) {
    mPolicies.AppendElement(policy);
    // reset cache since effective policy changes
    mShouldLoadCache.Clear();
  }
  return NS_OK;
}

// aNonceOrContent either holds the nonce-value or otherwise the content
// of the element to be hashed.
NS_IMETHODIMP
nsCSPContext::getAllowsInternal(nsContentPolicyType aContentType,
                                enum CSPKeyword aKeyword,
                                const nsAString& aNonceOrContent,
                                bool* outShouldReportViolation,
                                bool* outIsAllowed) const
{
  *outShouldReportViolation = false;
  *outIsAllowed = true;

  // Skip things that aren't hash/nonce compatible
  if (aKeyword == CSP_NONCE || aKeyword == CSP_HASH) {
    if (!(aContentType == nsIContentPolicy::TYPE_SCRIPT ||
          aContentType == nsIContentPolicy::TYPE_STYLESHEET)) {
      *outIsAllowed = false;
      return NS_OK;
    }
  }

  for (uint32_t i = 0; i < mPolicies.Length(); i++) {
    if (!mPolicies[i]->allows(aContentType,
                              aKeyword,
                              aNonceOrContent)) {
      // policy is violated: must report the violation and allow the inline
      // script if the policy is report-only.
      *outShouldReportViolation = true;
      if (!mPolicies[i]->getReportOnlyFlag()) {
        *outIsAllowed = false;
      }
    }
  }
  CSPCONTEXTLOG(("nsCSPContext::getAllowsInternal, aContentType: %d, aKeyword: %s, aNonceOrContent: %s, isAllowed: %s",
                aContentType,
                aKeyword == CSP_HASH ? "hash" : CSP_EnumToKeyword(aKeyword),
                NS_ConvertUTF16toUTF8(aNonceOrContent).get(),
                *outIsAllowed ? "load" : "deny"));
  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::GetAllowsInlineScript(bool* outShouldReportViolation,
                                    bool* outAllowsInlineScript)
{
  return getAllowsInternal(nsIContentPolicy::TYPE_SCRIPT,
                           CSP_UNSAFE_INLINE,
                           EmptyString(),
                           outShouldReportViolation,
                           outAllowsInlineScript);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsEval(bool* outShouldReportViolation,
                            bool* outAllowsEval)
{
  return getAllowsInternal(nsIContentPolicy::TYPE_SCRIPT,
                           CSP_UNSAFE_EVAL,
                           EmptyString(),
                           outShouldReportViolation,
                           outAllowsEval);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsInlineStyle(bool* outShouldReportViolation,
                                   bool* outAllowsInlineStyle)
{
  return getAllowsInternal(nsIContentPolicy::TYPE_STYLESHEET,
                           CSP_UNSAFE_INLINE,
                           EmptyString(),
                           outShouldReportViolation,
                           outAllowsInlineStyle);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsNonce(const nsAString& aNonce,
                             uint32_t aContentType,
                             bool* outShouldReportViolation,
                             bool* outAllowsNonce)
{
  return getAllowsInternal(aContentType,
                           CSP_NONCE,
                           aNonce,
                           outShouldReportViolation,
                           outAllowsNonce);
}

NS_IMETHODIMP
nsCSPContext::GetAllowsHash(const nsAString& aContent,
                            uint16_t aContentType,
                            bool* outShouldReportViolation,
                            bool* outAllowsHash)
{
  return getAllowsInternal(aContentType,
                           CSP_HASH,
                           aContent,
                           outShouldReportViolation,
                           outAllowsHash);
}

/**
 * Reduces some code repetition for the various logging situations in
 * LogViolationDetails.
 *
 * Call-sites for the eval/inline checks recieve two return values: allows
 * and violates.  Based on those, they must choose whether to call
 * LogViolationDetails or not.  Policies that are report-only allow the
 * loads/compilations but violations should still be reported.  Not all
 * policies in this nsIContentSecurityPolicy instance will be violated,
 * which is why we must check allows() again here.
 *
 * Note: This macro uses some parameters from its caller's context:
 * p, mPolicies, this, aSourceFile, aScriptSample, aLineNum, selfISupports
 *
 * @param violationType: the VIOLATION_TYPE_* constant (partial symbol)
 *                 such as INLINE_SCRIPT
 * @param contentPolicyType: a constant from nsIContentPolicy such as TYPE_STYLESHEET
 * @param nonceOrHash: for NONCE and HASH violations, it's the nonce or content
 *               string. For other violations, it is an empty string.
 * @param keyword: the keyword corresponding to violation (UNSAFE_INLINE for most)
 * @param observerTopic: the observer topic string to send with the CSP
 *                 observer notifications.
 */
#define CASE_CHECK_AND_REPORT(violationType, contentPolicyType, nonceOrHash,   \
                              keyword, observerTopic)                          \
  case nsIContentSecurityPolicy::VIOLATION_TYPE_ ## violationType :            \
    PR_BEGIN_MACRO                                                             \
    if (!mPolicies[p]->allows(nsIContentPolicy::TYPE_ ## contentPolicyType,    \
                              keyword, nonceOrHash))                           \
    {                                                                          \
      nsAutoString violatedDirective;                                          \
      mPolicies[p]->getDirectiveStringForContentType(                          \
                        nsIContentPolicy::TYPE_ ## contentPolicyType,          \
                        violatedDirective);                                    \
      this->AsyncReportViolation(selfISupports, nullptr, violatedDirective, p, \
                                 NS_LITERAL_STRING(observerTopic),             \
                                 aSourceFile, aScriptSample, aLineNum);        \
    }                                                                          \
    PR_END_MACRO;                                                              \
    break

/**
 * For each policy, log any violation on the Error Console and send a report
 * if a report-uri is present in the policy
 *
 * @param aViolationType
 *     one of the VIOLATION_TYPE_* constants, e.g. inline-script or eval
 * @param aSourceFile
 *     name of the source file containing the violation (if available)
 * @param aContentSample
 *     sample of the violating content (to aid debugging)
 * @param aLineNum
 *     source line number of the violation (if available)
 * @param aNonce
 *     (optional) If this is a nonce violation, include the nonce so we can
 *     recheck to determine which policies were violated and send the
 *     appropriate reports.
 * @param aContent
 *     (optional) If this is a hash violation, include contents of the inline
 *     resource in the question so we can recheck the hash in order to
 *     determine which policies were violated and send the appropriate
 *     reports.
 */
NS_IMETHODIMP
nsCSPContext::LogViolationDetails(uint16_t aViolationType,
                                  const nsAString& aSourceFile,
                                  const nsAString& aScriptSample,
                                  int32_t aLineNum,
                                  const nsAString& aNonce,
                                  const nsAString& aContent)
{
  for (uint32_t p = 0; p < mPolicies.Length(); p++) {
    NS_ASSERTION(mPolicies[p], "null pointer in nsTArray<nsCSPPolicy>");

    nsCOMPtr<nsISupportsCString> selfICString(do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
    if (selfICString) {
      selfICString->SetData(nsDependentCString("self"));
    }
    nsCOMPtr<nsISupports> selfISupports(do_QueryInterface(selfICString));

    switch (aViolationType) {
      CASE_CHECK_AND_REPORT(EVAL,              SCRIPT,     NS_LITERAL_STRING(""),
                            CSP_UNSAFE_EVAL,   EVAL_VIOLATION_OBSERVER_TOPIC);
      CASE_CHECK_AND_REPORT(INLINE_STYLE,      STYLESHEET, NS_LITERAL_STRING(""),
                            CSP_UNSAFE_INLINE, INLINE_STYLE_VIOLATION_OBSERVER_TOPIC);
      CASE_CHECK_AND_REPORT(INLINE_SCRIPT,     SCRIPT,     NS_LITERAL_STRING(""),
                            CSP_UNSAFE_INLINE, INLINE_SCRIPT_VIOLATION_OBSERVER_TOPIC);
      CASE_CHECK_AND_REPORT(NONCE_SCRIPT,      SCRIPT,     aNonce,
                            CSP_UNSAFE_INLINE, SCRIPT_NONCE_VIOLATION_OBSERVER_TOPIC);
      CASE_CHECK_AND_REPORT(NONCE_STYLE,       STYLESHEET, aNonce,
                            CSP_UNSAFE_INLINE, STYLE_NONCE_VIOLATION_OBSERVER_TOPIC);
      CASE_CHECK_AND_REPORT(HASH_SCRIPT,       SCRIPT,     aContent,
                            CSP_UNSAFE_INLINE, SCRIPT_HASH_VIOLATION_OBSERVER_TOPIC);
      CASE_CHECK_AND_REPORT(HASH_STYLE,        STYLESHEET, aContent,
                            CSP_UNSAFE_INLINE, STYLE_HASH_VIOLATION_OBSERVER_TOPIC);

      default:
        NS_ASSERTION(false, "LogViolationDetails with invalid type");
        break;
    }
  }
  return NS_OK;
}

#undef CASE_CHECK_AND_REPORT

uint64_t
getInnerWindowID(nsIRequest* aRequest) {
  // can't do anything if there's no nsIRequest!
  if (!aRequest) {
    return 0;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = aRequest->GetLoadGroup(getter_AddRefs(loadGroup));

  if (NS_FAILED(rv) || !loadGroup) {
    return 0;
  }

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  rv = loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (NS_FAILED(rv) || !callbacks) {
    return 0;
  }

  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
  if (!loadContext) {
    return 0;
  }

  nsCOMPtr<nsIDOMWindow> window;
  rv = loadContext->GetAssociatedWindow(getter_AddRefs(window));
  if (NS_FAILED(rv) || !window) {
    return 0;
  }

  uint64_t id = 0;
  nsCOMPtr<nsIDOMWindowUtils> du = do_GetInterface(window);
  if (!du) {
    return 0;
  }

  rv = du->GetCurrentInnerWindowID(&id);
  if (NS_FAILED(rv)) {
    return 0;
  }

  return id;
}

NS_IMETHODIMP
nsCSPContext::SetRequestContext(nsIURI* aSelfURI,
                                nsIURI* aReferrer,
                                nsIPrincipal* aDocumentPrincipal,
                                nsIChannel* aChannel)
{
  NS_PRECONDITION(aSelfURI || aChannel, "Need aSelfURI or aChannel to set the context properly");
  NS_ENSURE_ARG(aSelfURI || aChannel);

  // first use aSelfURI.  If that's not available get the URI from aChannel.
  mSelfURI = aSelfURI;
  if (!mSelfURI) {
    nsresult rv = aChannel->GetURI(getter_AddRefs(mSelfURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(mSelfURI, "No aSelfURI and no URI available from channel in SetRequestContext, can not translate 'self' into actual URI");

  if (aChannel) {
    mInnerWindowID = getInnerWindowID(aChannel);
    aChannel->GetLoadGroup(getter_AddRefs(mCallingChannelLoadGroup));
  }
  else {
    NS_WARNING("Channel needed (but null) in SetRequestContext.  Cannot query loadgroup, which means report sending may fail.");
  }

  mReferrer = aReferrer;
  if (!mReferrer) {
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (httpChannel) {
      httpChannel->GetReferrer(getter_AddRefs(mReferrer));
    }
    else {
      NS_WARNING("Channel provided to SetRequestContext is not an nsIHttpChannel so referrer is not available for reporting." );
    }
  }

  return NS_OK;
}

nsresult
nsCSPContext::SendReports(nsISupports* aBlockedContentSource,
                          nsIURI* aOriginalURI,
                          nsAString& aViolatedDirective,
                          uint32_t aViolatedPolicyIndex,
                          nsAString& aSourceFile,
                          nsAString& aScriptSample,
                          uint32_t aLineNum)
{
  NS_ENSURE_ARG_MAX(aViolatedPolicyIndex, mPolicies.Length() - 1);

#ifdef MOZ_B2G
  // load group information (on process-split necko implementations like b2g).
  // (fix this in bug 1011086)
  if (!mCallingChannelLoadGroup) {
    NS_WARNING("Load group required but not present for report sending; cannot send CSP violation reports");
    return NS_ERROR_FAILURE;
  }
#endif

  nsresult rv;
  nsString csp_report;
  csp_report.AppendASCII("{\"csp-report\": {");

  // blocked-uri
  csp_report.AppendASCII("\"blocked-uri\": \"");
  if (aBlockedContentSource) {
    nsAutoCString reportBlockedURI;
    nsCOMPtr<nsIURI> uri = do_QueryInterface(aBlockedContentSource);
    // could be a string or URI
    if (uri) {
      uri->GetSpec(reportBlockedURI);
    } else {
      nsCOMPtr<nsISupportsCString> cstr = do_QueryInterface(aBlockedContentSource);
      if (cstr) {
        cstr->GetData(reportBlockedURI);
      }
    }
    csp_report.AppendASCII(reportBlockedURI.get());
  }
  else {
    // this can happen for frame-ancestors violation where the violating
    // ancestor is cross-origin.
    NS_WARNING("No blocked URI (null aBlockedContentSource) for CSP violation report.");
  }
  csp_report.AppendASCII("\", ");

  // document-uri
  csp_report.AppendASCII("\"document-uri\": \"");
  if (aOriginalURI) {
    nsAutoCString reportDocumentURI;
    aOriginalURI->GetSpec(reportDocumentURI);
    csp_report.AppendASCII(reportDocumentURI.get());
  }
  csp_report.AppendASCII("\", ");

  // original-policy
  csp_report.AppendASCII("\"original-policy\": \"");
  nsAutoString originalPolicy;
  rv = this->GetPolicy(aViolatedPolicyIndex, originalPolicy);
  NS_ENSURE_SUCCESS(rv, rv);
  csp_report.Append(originalPolicy);
  csp_report.AppendASCII("\", ");

  // referrer
  csp_report.AppendASCII("\"referrer\": \"");
  if (mReferrer) {
    nsAutoCString referrerURI;
    mReferrer->GetSpec(referrerURI);
    csp_report.AppendASCII(referrerURI.get());
  }
  csp_report.AppendASCII("\", ");

  // violated-directive
  csp_report.AppendASCII("\"violated-directive\": \"");
  csp_report.Append(aViolatedDirective);
  csp_report.AppendASCII("\"");

  // source-file
  if (!aSourceFile.IsEmpty()) {
    csp_report.AppendASCII(", \"source-file\": \"");
    csp_report.Append(aSourceFile);
    csp_report.AppendASCII("\"");
  }

  // script-sample
  if (!aScriptSample.IsEmpty()) {
    csp_report.AppendASCII(", \"script-sample\": \"");
    csp_report.Append(aScriptSample);
    csp_report.AppendASCII("\"");
  }

  // line-number
  if (aLineNum != 0) {
    csp_report.AppendASCII(", \"script-sample\": \"");
    csp_report.AppendInt(aLineNum);
    csp_report.AppendASCII("\"");
  }

  csp_report.AppendASCII("}}\n\n");

  // ---------- Assembled, now send it to all the report URIs ----------- //

  nsTArray<nsString> reportURIs;
  mPolicies[aViolatedPolicyIndex]->getReportURIs(reportURIs);

  nsCOMPtr<nsIURI> reportURI;
  nsCOMPtr<nsIChannel> reportChannel;

  for (uint32_t r = 0; r < reportURIs.Length(); r++) {
    // try to create a new uri from every report-uri string
    rv = NS_NewURI(getter_AddRefs(reportURI), reportURIs[r]);
    if (NS_FAILED(rv)) {
      const char16_t* params[] = { reportURIs[r].get() };
      CSPCONTEXTLOG(("Could not create nsIURI for report URI %s",
                     reportURIs[r].get()));
      CSP_LogLocalizedStr(NS_LITERAL_STRING("triedToSendReport").get(),
                          params, ArrayLength(params),
                          aSourceFile, aScriptSample, aLineNum, 0,
                          nsIScriptError::errorFlag, "CSP", mInnerWindowID);
      continue; // don't return yet, there may be more URIs
    }

    // try to create a new channel for every report-uri
    rv = NS_NewChannel(getter_AddRefs(reportChannel), reportURI);
    if (NS_FAILED(rv)) {
      CSPCONTEXTLOG(("Could not create new channel for report URI %s",
                     reportURIs[r].get()));
      continue; // don't return yet, there may be more URIs
    }

    // make sure this is an anonymous request (no cookies) so in case the
    // policy URI is injected, it can't be abused for CSRF.
    nsLoadFlags flags;
    rv = reportChannel->GetLoadFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    flags |= nsIRequest::LOAD_ANONYMOUS;
    rv = reportChannel->SetLoadFlags(flags);
    NS_ENSURE_SUCCESS(rv, rv);

    // we need to set an nsIChannelEventSink on the channel object
    // so we can tell it to not follow redirects when posting the reports
    nsRefPtr<CSPReportRedirectSink> reportSink = new CSPReportRedirectSink();
    reportChannel->SetNotificationCallbacks(reportSink);

    // apply the loadgroup from the channel taken by setRequestContext.  If
    // there's no loadgroup, AsyncOpen will fail on process-split necko (since
    // the channel cannot query the iTabChild).
    rv = reportChannel->SetLoadGroup(mCallingChannelLoadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    // check content policy
    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    nsCOMPtr<nsIContentPolicy> cp = do_GetService(NS_CONTENTPOLICY_CONTRACTID);
    if (!cp) {
      return NS_ERROR_FAILURE;
    }

    rv = cp->ShouldLoad(nsIContentPolicy::TYPE_CSP_REPORT,
                        reportURI,
                        aOriginalURI,
                        nullptr,        // Context
                        EmptyCString(), // mime type
                        nullptr,        // Extra parameter
                        nullptr,        // optional request principal
                        &shouldLoad);

    // refuse to load if we can't do a security check
    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_CP_REJECTED(shouldLoad)) {
      // skip unauthorized URIs
      CSPCONTEXTLOG(("nsIContentPolicy blocked sending report to %s",
                     reportURIs[r].get()));
      continue; // don't return yet, there may be more URIs
    }

    // wire in the string input stream to send the report
    nsCOMPtr<nsIStringInputStream> sis(do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID));
    NS_ASSERTION(sis, "nsIStringInputStream is needed but not available to send CSP violation reports");
    rv = sis->SetData(NS_ConvertUTF16toUTF8(csp_report).get(), csp_report.Length());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(reportChannel));
    NS_ASSERTION(uploadChannel, "nsIUploadChannel is needed but not available to send CSP violation reports");
    rv = uploadChannel->SetUploadStream(sis, NS_LITERAL_CSTRING("application/json"), -1);
    NS_ENSURE_SUCCESS(rv, rv);

    // if this is an HTTP channel, set the request method to post
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(reportChannel));
    if (httpChannel) {
      httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
    }

    nsRefPtr<CSPViolationReportListener> listener = new CSPViolationReportListener();
    rv = reportChannel->AsyncOpen(listener, nullptr);

    // AsyncOpen should not fail, but could if there's no load group (like if
    // SetRequestContext is not given a channel).  This should fail quietly and
    // not return an error since it's really ok if reports don't go out, but
    // it's good to log the error locally.

    if (NS_FAILED(rv)) {
      const char16_t* params[] = { reportURIs[r].get() };
      CSPCONTEXTLOG(("AsyncOpen failed for report URI %s", params[0]));
      CSP_LogLocalizedStr(NS_LITERAL_STRING("triedToSendReport").get(),
                          params, ArrayLength(params),
                          aSourceFile, aScriptSample, aLineNum, 0,
                          nsIScriptError::errorFlag, "CSP", mInnerWindowID);
    }
  }
  return NS_OK;
}

/**
 * Dispatched from the main thread to send reports for one CSP violation.
 */
class CSPReportSenderRunnable MOZ_FINAL : public nsRunnable
{
  public:
    CSPReportSenderRunnable(nsISupports* aBlockedContentSource,
                            nsIURI* aOriginalURI,
                            uint32_t aViolatedPolicyIndex,
                            const nsAString& aViolatedDirective,
                            const nsAString& aObserverSubject,
                            const nsAString& aSourceFile,
                            const nsAString& aScriptSample,
                            uint32_t aLineNum,
                            uint64_t aInnerWindowID,
                            nsCSPContext* aCSPContext)
      : mBlockedContentSource(aBlockedContentSource)
      , mOriginalURI(aOriginalURI)
      , mViolatedPolicyIndex(aViolatedPolicyIndex)
      , mViolatedDirective(aViolatedDirective)
      , mSourceFile(aSourceFile)
      , mScriptSample(aScriptSample)
      , mLineNum(aLineNum)
      , mInnerWindowID(aInnerWindowID)
      , mCSPContext(aCSPContext)
    {
      // the observer subject is an nsISupports: either an nsISupportsCString
      // from the arg passed in directly, or if that's empty, it's the blocked
      // source.
      if (aObserverSubject.IsEmpty()) {
        mObserverSubject = aBlockedContentSource;
      } else {
        nsCOMPtr<nsISupportsCString> supportscstr =
          do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID);
        NS_ASSERTION(supportscstr, "Couldn't allocate nsISupportsCString");
        supportscstr->SetData(NS_ConvertUTF16toUTF8(aObserverSubject));
        mObserverSubject = do_QueryInterface(supportscstr);
      }
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread());

      // 1) notify observers
      nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
      NS_ASSERTION(observerService, "needs observer service");
      nsresult rv = observerService->NotifyObservers(mObserverSubject,
                                                     CSP_VIOLATION_TOPIC,
                                                     mViolatedDirective.get());
      NS_ENSURE_SUCCESS(rv, rv);

      // 2) send reports for the policy that was violated
      mCSPContext->SendReports(mBlockedContentSource, mOriginalURI,
                               mViolatedDirective, mViolatedPolicyIndex,
                               mSourceFile, mScriptSample, mLineNum);

      // 3) log to console (one per policy violation)
      // mBlockedContentSource could be a URI or a string.
      nsCOMPtr<nsIURI> blockedURI = do_QueryInterface(mBlockedContentSource);
      // if mBlockedContentSource is not a URI, it could be a string
      nsCOMPtr<nsISupportsCString> blockedString = do_QueryInterface(mBlockedContentSource);

      nsCString blockedDataStr;

      if (blockedURI) {
        blockedURI->GetSpec(blockedDataStr);
      } else if (blockedString) {
        blockedString->GetData(blockedDataStr);
      }

      if (blockedDataStr.Length() > 0) {
        nsString blockedDataChar16 = NS_ConvertUTF8toUTF16(blockedDataStr);
        const char16_t* params[] = { mViolatedDirective.get(),
                                     blockedDataChar16.get() };
        CSP_LogLocalizedStr(NS_LITERAL_STRING("CSPViolationWithURI").get(),
                            params, ArrayLength(params),
                            mSourceFile, mScriptSample, mLineNum, 0,
                            nsIScriptError::errorFlag, "CSP", mInnerWindowID);
      }
      return NS_OK;
    }

  private:
    nsCOMPtr<nsISupports>   mBlockedContentSource;
    nsCOMPtr<nsIURI>        mOriginalURI;
    uint32_t                mViolatedPolicyIndex;
    nsString                mViolatedDirective;
    nsCOMPtr<nsISupports>   mObserverSubject;
    nsString                mSourceFile;
    nsString                mScriptSample;
    uint32_t                mLineNum;
    uint64_t                mInnerWindowID;
    nsCSPContext*           mCSPContext;
};

/**
 * Asynchronously notifies any nsIObservers listening to the CSP violation
 * topic that a violation occurred.  Also triggers report sending and console
 * logging.  All asynchronous on the main thread.
 *
 * @param aBlockedContentSource
 *        Either a CSP Source (like 'self', as string) or nsIURI: the source
 *        of the violation.
 * @param aOriginalUri
 *        The original URI if the blocked content is a redirect, else null
 * @param aViolatedDirective
 *        the directive that was violated (string).
 * @param aViolatedPolicyIndex
 *        the index of the policy that was violated (so we know where to send
 *        the reports).
 * @param aObserverSubject
 *        optional, subject sent to the nsIObservers listening to the CSP
 *        violation topic.
 * @param aSourceFile
 *        name of the file containing the inline script violation
 * @param aScriptSample
 *        a sample of the violating inline script
 * @param aLineNum
 *        source line number of the violation (if available)
 */
nsresult
nsCSPContext::AsyncReportViolation(nsISupports* aBlockedContentSource,
                                   nsIURI* aOriginalURI,
                                   const nsAString& aViolatedDirective,
                                   uint32_t aViolatedPolicyIndex,
                                   const nsAString& aObserverSubject,
                                   const nsAString& aSourceFile,
                                   const nsAString& aScriptSample,
                                   uint32_t aLineNum)
{
  NS_DispatchToMainThread(new CSPReportSenderRunnable(aBlockedContentSource,
                                                      aOriginalURI,
                                                      aViolatedPolicyIndex,
                                                      aViolatedDirective,
                                                      aObserverSubject,
                                                      aSourceFile,
                                                      aScriptSample,
                                                      aLineNum,
                                                      mInnerWindowID,
                                                      this));
   return NS_OK;
}

/**
 * Based on the given docshell, determines if this CSP context allows the
 * ancestry.
 *
 * In order to determine the URI of the parent document (one causing the load
 * of this protected document), this function obtains the docShellTreeItem,
 * then walks up the hierarchy until it finds a privileged (chrome) tree item.
 * Getting the parent's URI looks like this in pseudocode:
 *
 * nsIDocShell->QI(nsIInterfaceRequestor)
 *            ->GI(nsIDocShellTreeItem)
 *            ->QI(nsIInterfaceRequestor)
 *            ->GI(nsIWebNavigation)
 *            ->GetCurrentURI();
 *
 * aDocShell is the docShell for the protected document.
 */
NS_IMETHODIMP
nsCSPContext::PermitsAncestry(nsIDocShell* aDocShell, bool* outPermitsAncestry)
{
  nsresult rv;

  // Can't check ancestry without a docShell.
  if (aDocShell == nullptr) {
    return NS_ERROR_FAILURE;
  }

  *outPermitsAncestry = true;

  // extract the ancestry as an array
  nsCOMArray<nsIURI> ancestorsArray;

  nsCOMPtr<nsIInterfaceRequestor> ir(do_QueryInterface(aDocShell));
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_GetInterface(ir));
  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  nsCOMPtr<nsIWebNavigation> webNav;
  nsCOMPtr<nsIURI> currentURI;
  nsCOMPtr<nsIURI> uriClone;

  // iterate through each docShell parent item
  while (NS_SUCCEEDED(treeItem->GetParent(getter_AddRefs(parentTreeItem))) &&
         parentTreeItem != nullptr) {
    ir     = do_QueryInterface(parentTreeItem);
    NS_ASSERTION(ir, "Could not QI docShellTreeItem to nsIInterfaceRequestor");

    webNav = do_GetInterface(ir);
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

    rv = webNav->GetCurrentURI(getter_AddRefs(currentURI));
    NS_ENSURE_SUCCESS(rv, rv);

    if (currentURI) {
      // stop when reaching chrome
      bool isChrome = false;
      rv = currentURI->SchemeIs("chrome", &isChrome);
      NS_ENSURE_SUCCESS(rv, rv);
      if (isChrome) { break; }

      // delete the userpass from the URI.
      rv = currentURI->CloneIgnoringRef(getter_AddRefs(uriClone));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = uriClone->SetUserPass(EmptyCString());
      NS_ENSURE_SUCCESS(rv, rv);
#ifdef PR_LOGGING
      {
      nsAutoCString spec;
      uriClone->GetSpec(spec);
      CSPCONTEXTLOG(("nsCSPContext::PermitsAncestry, found ancestor: %s", spec.get()));
      }
#endif
      ancestorsArray.AppendElement(uriClone);
    }

    // next ancestor
    treeItem = parentTreeItem;
  }

  nsAutoString violatedDirective;

  // Now that we've got the ancestry chain in ancestorsArray, time to check
  // them against any CSP.
  for (uint32_t i = 0; i < mPolicies.Length(); i++) {

    // According to the W3C CSP spec, frame-ancestors checks are ignored for
    // report-only policies (when "monitoring").
    if (mPolicies[i]->getReportOnlyFlag()) {
      continue;
    }

    for (uint32_t a = 0; a < ancestorsArray.Length(); a++) {
      // TODO(sid) the mapping from frame-ancestors context to TYPE_DOCUMENT is
      // forced. while this works for now, we will implement something in
      // bug 999656.
#ifdef PR_LOGGING
      {
      nsAutoCString spec;
      ancestorsArray[a]->GetSpec(spec);
      CSPCONTEXTLOG(("nsCSPContext::PermitsAncestry, checking ancestor: %s", spec.get()));
      }
#endif
      if (!mPolicies[i]->permits(nsIContentPolicy::TYPE_DOCUMENT,
                                 ancestorsArray[a],
                                 EmptyString(), // no nonce
                                 violatedDirective)) {
        // Policy is violated
        // Send reports, but omit the ancestor URI if cross-origin as per spec
        // (it is a violation of the same-origin policy).
        bool okToSendAncestor = NS_SecurityCompareURIs(ancestorsArray[a], mSelfURI, true);

        this->AsyncReportViolation((okToSendAncestor ? ancestorsArray[a] : nullptr),
                                   mSelfURI,
                                   violatedDirective,
                                   i,             /* policy index        */
                                   EmptyString(), /* no observer subject */
                                   EmptyString(), /* no source file      */
                                   EmptyString(), /* no script sample    */
                                   0);            /* no line number      */
        *outPermitsAncestry = false;
      }
    }
  }
  return NS_OK;
}

/* ========== CSPViolationReportListener implementation ========== */

NS_IMPL_ISUPPORTS(CSPViolationReportListener, nsIStreamListener, nsIRequestObserver, nsISupports);

CSPViolationReportListener::CSPViolationReportListener()
{
}

CSPViolationReportListener::~CSPViolationReportListener()
{
}

NS_METHOD
AppendSegmentToString(nsIInputStream* aInputStream,
                      void* aClosure,
                      const char* aRawSegment,
                      uint32_t aToOffset,
                      uint32_t aCount,
                      uint32_t* outWrittenCount)
{
  nsCString* decodedData = static_cast<nsCString*>(aClosure);
  decodedData->Append(aRawSegment, aCount);
  *outWrittenCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
CSPViolationReportListener::OnDataAvailable(nsIRequest* aRequest,
                                            nsISupports* aContext,
                                            nsIInputStream* aInputStream,
                                            uint64_t aOffset,
                                            uint32_t aCount)
{
  uint32_t read;
  nsCString decodedData;
  return aInputStream->ReadSegments(AppendSegmentToString,
                                    &decodedData,
                                    aCount,
                                    &read);
}

NS_IMETHODIMP
CSPViolationReportListener::OnStopRequest(nsIRequest* aRequest,
                                          nsISupports* aContext,
                                          nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
CSPViolationReportListener::OnStartRequest(nsIRequest* aRequest,
                                           nsISupports* aContext)
{
  return NS_OK;
}

/* ========== CSPReportRedirectSink implementation ========== */

NS_IMPL_ISUPPORTS(CSPReportRedirectSink, nsIChannelEventSink, nsIInterfaceRequestor);

CSPReportRedirectSink::CSPReportRedirectSink()
{
}

CSPReportRedirectSink::~CSPReportRedirectSink()
{
}

NS_IMETHODIMP
CSPReportRedirectSink::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                              nsIChannel* aNewChannel,
                                              uint32_t aRedirFlags,
                                              nsIAsyncVerifyRedirectCallback* aCallback)
{
  // cancel the old channel so XHR failure callback happens
  nsresult rv = aOldChannel->Cancel(NS_ERROR_ABORT);
  NS_ENSURE_SUCCESS(rv, rv);

  // notify an observer that we have blocked the report POST due to a redirect,
  // used in testing, do this async since we're in an async call now to begin with
  nsCOMPtr<nsIURI> uri;
  rv = aOldChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  NS_ASSERTION(observerService, "Observer service required to log CSP violations");
  observerService->NotifyObservers(uri,
                                   CSP_VIOLATION_TOPIC,
                                   NS_LITERAL_STRING("denied redirect while sending violation report").get());

  return NS_BINDING_REDIRECTED;
}

NS_IMETHODIMP
CSPReportRedirectSink::GetInterface(const nsIID& aIID, void** aResult)
{
  return QueryInterface(aIID, aResult);
}

/* ===== nsISerializable implementation ====== */

NS_IMETHODIMP
nsCSPContext::Read(nsIObjectInputStream* aStream)
{
  nsresult rv;
  nsCOMPtr<nsISupports> supports;

  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  NS_ENSURE_SUCCESS(rv, rv);

  mSelfURI = do_QueryInterface(supports);
  NS_ASSERTION(mSelfURI, "need a self URI to de-serialize");

  uint32_t numPolicies;
  rv = aStream->Read32(&numPolicies);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString policyString;

  while (numPolicies > 0) {
    numPolicies--;

    rv = aStream->ReadString(policyString);
    NS_ENSURE_SUCCESS(rv, rv);

    bool reportOnly = false;
    rv = aStream->ReadBoolean(&reportOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    bool specCompliant = false;
    rv = aStream->ReadBoolean(&specCompliant);
    NS_ENSURE_SUCCESS(rv, rv);

    // Using the new backend, we don't support non-spec-compliant policies, so
    // skip any of those, will be fixed in bug 991466
    if (!specCompliant) {
      continue;
    }

    nsCSPPolicy* policy = nsCSPParser::parseContentSecurityPolicy(policyString,
                                                                  mSelfURI,
                                                                  reportOnly,
                                                                  mInnerWindowID);
    if (policy) {
      mPolicies.AppendElement(policy);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSPContext::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = NS_WriteOptionalCompoundObject(aStream,
                                               mSelfURI,
                                               NS_GET_IID(nsIURI),
                                               true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Serialize all the policies.
  aStream->Write32(mPolicies.Length());

  nsAutoString polStr;
  for (uint32_t p = 0; p < mPolicies.Length(); p++) {
    mPolicies[p]->toString(polStr);
    aStream->WriteWStringZ(polStr.get());
    aStream->WriteBoolean(mPolicies[p]->getReportOnlyFlag());
    // Setting specCompliant boolean for backwards compatibility (fix in bug 991466)
    aStream->WriteBoolean(true);
  }
  return NS_OK;
}
