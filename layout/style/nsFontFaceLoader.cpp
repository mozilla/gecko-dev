/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for loading in @font-face defined font data */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "nsFontFaceLoader.h"

#include "nsError.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"

#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsISupportsPriority.h"
#include "nsINetworkPredictor.h"

#include "nsIConsoleService.h"

#include "nsStyleSet.h"
#include "nsPrintfCString.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo* 
GetFontDownloaderLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog)
    sLog = PR_NewLogModule("fontdownloader");
  return sLog;
}
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(GetFontDownloaderLog(), PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(GetFontDownloaderLog(), PR_LOG_DEBUG)


nsFontFaceLoader::nsFontFaceLoader(gfxMixedFontFamily* aFontFamily,
                                   gfxProxyFontEntry* aProxy,
                                   nsIURI* aFontURI,
                                   nsUserFontSet* aFontSet,
                                   nsIChannel* aChannel)
  : mFontFamily(aFontFamily),
    mFontEntry(aProxy),
    mFontURI(aFontURI),
    mFontSet(aFontSet),
    mChannel(aChannel)
{
}

nsFontFaceLoader::~nsFontFaceLoader()
{
  if (mFontEntry) {
    mFontEntry->mLoader = nullptr;
  }
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }
  if (mFontSet) {
    mFontSet->RemoveLoader(this);
  }
}

void
nsFontFaceLoader::StartedLoading(nsIStreamLoader* aStreamLoader)
{
  int32_t loadTimeout =
    Preferences::GetInt("gfx.downloadable_fonts.fallback_delay", 3000);
  if (loadTimeout > 0) {
    mLoadTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mLoadTimer) {
      mLoadTimer->InitWithFuncCallback(LoadTimerCallback,
                                       static_cast<void*>(this),
                                       loadTimeout,
                                       nsITimer::TYPE_ONE_SHOT);
    }
  } else {
    mFontEntry->mLoadingState = gfxProxyFontEntry::LOADING_SLOWLY;
  }
  mStreamLoader = aStreamLoader;
}

void
nsFontFaceLoader::LoadTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsFontFaceLoader* loader = static_cast<nsFontFaceLoader*>(aClosure);

  if (!loader->mFontSet) {
    // We've been canceled
    return;
  }

  gfxProxyFontEntry* pe = loader->mFontEntry.get();
  bool updateUserFontSet = true;

  // If the entry is loading, check whether it's >75% done; if so,
  // we allow another timeout period before showing a fallback font.
  if (pe->mLoadingState == gfxProxyFontEntry::LOADING_STARTED) {
    int64_t contentLength;
    uint32_t numBytesRead;
    if (NS_SUCCEEDED(loader->mChannel->GetContentLength(&contentLength)) &&
        contentLength > 0 &&
        contentLength < UINT32_MAX &&
        NS_SUCCEEDED(loader->mStreamLoader->GetNumBytesRead(&numBytesRead)) &&
        numBytesRead > 3 * (uint32_t(contentLength) >> 2))
    {
      // More than 3/4 the data has been downloaded, so allow 50% extra
      // time and hope the remainder will arrive before the additional
      // time expires.
      pe->mLoadingState = gfxProxyFontEntry::LOADING_ALMOST_DONE;
      uint32_t delay;
      loader->mLoadTimer->GetDelay(&delay);
      loader->mLoadTimer->InitWithFuncCallback(LoadTimerCallback,
                                               static_cast<void*>(loader),
                                               delay >> 1,
                                               nsITimer::TYPE_ONE_SHOT);
      updateUserFontSet = false;
      LOG(("fontdownloader (%p) 75%% done, resetting timer\n", loader));
    }
  }

  // If the font is not 75% loaded, or if we've already timed out once
  // before, we mark this entry as "loading slowly", so the fallback
  // font will be used in the meantime, and tell the context to refresh.
  if (updateUserFontSet) {
    pe->mLoadingState = gfxProxyFontEntry::LOADING_SLOWLY;
    gfxUserFontSet* fontSet = loader->mFontSet;
    nsPresContext* ctx = loader->mFontSet->GetPresContext();
    NS_ASSERTION(ctx, "userfontset doesn't have a presContext?");
    if (ctx) {
      fontSet->IncrementGeneration();
      ctx->UserFontSetUpdated();
      LOG(("fontdownloader (%p) timeout reflow\n", loader));
    }
  }
}

NS_IMPL_ISUPPORTS(nsFontFaceLoader, nsIStreamLoaderObserver)

NS_IMETHODIMP
nsFontFaceLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext,
                                   nsresult aStatus,
                                   uint32_t aStringLen,
                                   const uint8_t* aString)
{
  if (!mFontSet) {
    // We've been canceled
    return aStatus;
  }

  mFontSet->RemoveLoader(this);

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsAutoCString fontURI;
    mFontURI->GetSpec(fontURI);
    if (NS_SUCCEEDED(aStatus)) {
      LOG(("fontdownloader (%p) download completed - font uri: (%s)\n", 
           this, fontURI.get()));
    } else {
      LOG(("fontdownloader (%p) download failed - font uri: (%s) error: %8.8x\n", 
           this, fontURI.get(), aStatus));
    }
  }
#endif

  nsPresContext* ctx = mFontSet->GetPresContext();
  NS_ASSERTION(ctx && !ctx->PresShell()->IsDestroying(),
               "We should have been canceled already");

  if (NS_SUCCEEDED(aStatus)) {
    // for HTTP requests, check whether the request _actually_ succeeded;
    // the "request status" in aStatus does not necessarily indicate this,
    // because HTTP responses such as 404 (Not Found) will still result in
    // a success code and potentially an HTML error page from the server
    // as the resulting data. We don't want to use that as a font.
    nsCOMPtr<nsIRequest> request;
    nsCOMPtr<nsIHttpChannel> httpChannel;
    aLoader->GetRequest(getter_AddRefs(request));
    httpChannel = do_QueryInterface(request);
    if (httpChannel) {
      bool succeeded;
      nsresult rv = httpChannel->GetRequestSucceeded(&succeeded);
      if (NS_SUCCEEDED(rv) && !succeeded) {
        aStatus = NS_ERROR_NOT_AVAILABLE;
      }
    }
  }

  // The userFontSet is responsible for freeing the downloaded data
  // (aString) when finished with it; the pointer is no longer valid
  // after OnLoadComplete returns.
  // This is called even in the case of a failed download (HTTP 404, etc),
  // as there may still be data to be freed (e.g. an error page),
  // and we need the fontSet to initiate loading the next source.
  bool fontUpdate = mFontSet->OnLoadComplete(mFontFamily, mFontEntry, aString,
                                             aStringLen, aStatus);

  // when new font loaded, need to reflow
  if (fontUpdate) {
    // Update layout for the presence of the new font.  Since this is
    // asynchronous, reflows will coalesce.
    ctx->UserFontSetUpdated();
    LOG(("fontdownloader (%p) reflow\n", this));
  }

  // done with font set
  mFontSet = nullptr;
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }

  return NS_SUCCESS_ADOPTED_DATA;
}

void
nsFontFaceLoader::Cancel()
{
  mFontEntry->mLoadingState = gfxProxyFontEntry::NOT_LOADING;
  mFontEntry->mLoader = nullptr;
  mFontSet = nullptr;
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }
  mChannel->Cancel(NS_BINDING_ABORTED);
}

nsresult
nsFontFaceLoader::CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext)
{
  nsresult rv;

  if (!aSourcePrincipal)
    return NS_OK;

  // check with the security manager
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  rv = secMan->CheckLoadURIWithPrincipal(aSourcePrincipal, aTargetURI,
                                        nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // check content policy
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_FONT,
                                 aTargetURI,
                                 aSourcePrincipal,
                                 aContext,
                                 EmptyCString(), // mime type
                                 nullptr,
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return NS_OK;
}

nsUserFontSet::nsUserFontSet(nsPresContext* aContext)
  : mPresContext(aContext)
{
  NS_ASSERTION(mPresContext, "null context passed to nsUserFontSet");
}

nsUserFontSet::~nsUserFontSet()
{
  NS_ASSERTION(mLoaders.Count() == 0, "mLoaders should have been emptied");
}

static PLDHashOperator DestroyIterator(nsPtrHashKey<nsFontFaceLoader>* aKey,
                                       void* aUserArg)
{
  aKey->GetKey()->Cancel();
  return PL_DHASH_REMOVE;
}

void
nsUserFontSet::Destroy()
{
  mPresContext = nullptr;
  mLoaders.EnumerateEntries(DestroyIterator, nullptr);
  mRules.Clear();
}

void
nsUserFontSet::RemoveLoader(nsFontFaceLoader* aLoader)
{
  mLoaders.RemoveEntry(aLoader);
}

nsresult
nsUserFontSet::StartLoad(gfxMixedFontFamily* aFamily,
                         gfxProxyFontEntry* aProxy,
                         const gfxFontFaceSrc* aFontFaceSrc)
{
  nsresult rv;

  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamLoader> streamLoader;
  nsCOMPtr<nsILoadGroup> loadGroup(ps->GetDocument()->GetDocumentLoadGroup());

  nsCOMPtr<nsIChannel> channel;
  // get Content Security Policy from principal to pass into channel
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = aProxy->mPrincipal->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_FONT);
  }
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aFontFaceSrc->mURI,
                     nullptr,
                     loadGroup,
                     nullptr,
                     nsIRequest::LOAD_NORMAL,
                     channelPolicy);

  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsFontFaceLoader> fontLoader =
    new nsFontFaceLoader(aFamily, aProxy, aFontFaceSrc->mURI, this, channel);

  if (!fontLoader)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsAutoCString fontURI, referrerURI;
    aFontFaceSrc->mURI->GetSpec(fontURI);
    if (aFontFaceSrc->mReferrer)
      aFontFaceSrc->mReferrer->GetSpec(referrerURI);
    LOG(("fontdownloader (%p) download start - font uri: (%s) "
         "referrer uri: (%s)\n",
         fontLoader.get(), fontURI.get(), referrerURI.get()));
  }
#endif

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel)
    httpChannel->SetReferrer(aFontFaceSrc->mReferrer);
  nsCOMPtr<nsISupportsPriority> priorityChannel(do_QueryInterface(channel));
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGH);
  }

  rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), fontLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument *document = ps->GetDocument();
  mozilla::net::PredictorLearn(aFontFaceSrc->mURI, document->GetDocumentURI(),
                               nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
                               loadGroup);

  bool inherits = false;
  rv = NS_URIChainHasFlags(aFontFaceSrc->mURI,
                           nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                           &inherits);
  if (NS_SUCCEEDED(rv) && inherits) {
    // allow data, javascript, etc URI's
    rv = channel->AsyncOpen(streamLoader, nullptr);
  } else {
    nsRefPtr<nsCORSListenerProxy> listener =
      new nsCORSListenerProxy(streamLoader, aProxy->mPrincipal, false);
    rv = listener->Init(channel);
    if (NS_SUCCEEDED(rv)) {
      rv = channel->AsyncOpen(listener, nullptr);
    }
    if (NS_FAILED(rv)) {
      fontLoader->DropChannel();  // explicitly need to break ref cycle
    }
  }

  if (NS_SUCCEEDED(rv)) {
    mLoaders.PutEntry(fontLoader);
    fontLoader->StartedLoading(streamLoader);
    aProxy->mLoader = fontLoader; // let the font entry remember the loader,
                                  // in case we need to cancel it
  }

  return rv;
}

static PLDHashOperator DetachFontEntries(const nsAString& aKey,
                                         nsRefPtr<gfxMixedFontFamily>& aFamily,
                                         void* aUserArg)
{
  aFamily->DetachFontEntries();
  return PL_DHASH_NEXT;
}

static PLDHashOperator RemoveIfEmpty(const nsAString& aKey,
                                     nsRefPtr<gfxMixedFontFamily>& aFamily,
                                     void* aUserArg)
{
  return aFamily->GetFontList().Length() ? PL_DHASH_NEXT : PL_DHASH_REMOVE;
}

bool
nsUserFontSet::UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules)
{
  bool modified = false;

  // The @font-face rules that make up the user font set have changed,
  // so we need to update the set. However, we want to preserve existing
  // font entries wherever possible, so that we don't discard and then
  // re-download resources in the (common) case where at least some of the
  // same rules are still present.

  nsTArray<FontFaceRuleRecord> oldRules;
  mRules.SwapElements(oldRules);

  // Remove faces from the font family records; we need to re-insert them
  // because we might end up with faces in a different order even if they're
  // the same font entries as before. (The order can affect font selection
  // where multiple faces match the requested style, perhaps with overlapping
  // unicode-range coverage.)
  mFontFamilies.Enumerate(DetachFontEntries, nullptr);

  for (uint32_t i = 0, i_end = aRules.Length(); i < i_end; ++i) {
    // Insert each rule into our list, migrating old font entries if possible
    // rather than creating new ones; set  modified  to true if we detect
    // that rule ordering has changed, or if a new entry is created.
    InsertRule(aRules[i].mRule, aRules[i].mSheetType, oldRules, modified);
  }

  // Remove any residual families that have no font entries (i.e., they were
  // not defined at all by the updated set of @font-face rules).
  mFontFamilies.Enumerate(RemoveIfEmpty, nullptr);

  // If any rules are left in the old list, note that the set has changed
  // (even if the new set was built entirely by migrating old font entries).
  if (oldRules.Length() > 0) {
    modified = true;
    // Any in-progress loaders for obsolete rules should be cancelled,
    // as the resource being downloaded will no longer be required.
    // We need to explicitly remove any loaders here, otherwise the loaders
    // will keep their "orphaned" font entries alive until they complete,
    // even after the oldRules array is deleted.
    size_t count = oldRules.Length();
    for (size_t i = 0; i < count; ++i) {
      gfxFontEntry* fe = oldRules[i].mFontEntry;
      if (!fe->mIsProxy) {
        continue;
      }
      gfxProxyFontEntry* proxy = static_cast<gfxProxyFontEntry*>(fe);
      nsFontFaceLoader* loader = proxy->mLoader;
      if (loader) {
        loader->Cancel();
        RemoveLoader(loader);
      }
    }
  }

  if (modified) {
    IncrementGeneration();
  }

  // local rules have been rebuilt, so clear the flag
  mLocalRulesUsed = false;

  return modified;
}

static bool
HasLocalSrc(const nsCSSValue::Array *aSrcArr)
{
  size_t numSrc = aSrcArr->Count();
  for (size_t i = 0; i < numSrc; i++) {
    if (aSrcArr->Item(i).GetUnit() == eCSSUnit_Local_Font) {
      return true;
    }
  }
  return false;
}

void
nsUserFontSet::InsertRule(nsCSSFontFaceRule* aRule, uint8_t aSheetType,
                          nsTArray<FontFaceRuleRecord>& aOldRules,
                          bool& aFontSetModified)
{
  NS_ABORT_IF_FALSE(aRule->GetType() == mozilla::css::Rule::FONT_FACE_RULE,
                    "InsertRule passed a non-fontface CSS rule");

  // set up family name
  nsAutoString fontfamily;
  nsCSSValue val;
  uint32_t unit;

  aRule->GetDesc(eCSSFontDesc_Family, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_String) {
    val.GetStringValue(fontfamily);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face family name has unexpected unit");
  }
  if (fontfamily.IsEmpty()) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  // first, we check in oldRules; if the rule exists there, just move it
  // to the new rule list, and put the entry into the appropriate family
  for (uint32_t i = 0; i < aOldRules.Length(); ++i) {
    const FontFaceRuleRecord& ruleRec = aOldRules[i];

    if (ruleRec.mContainer.mRule == aRule &&
        ruleRec.mContainer.mSheetType == aSheetType) {

      // if local rules were used, don't use the old font entry
      // for rules containing src local usage
      if (mLocalRulesUsed) {
        aRule->GetDesc(eCSSFontDesc_Src, val);
        unit = val.GetUnit();
        if (unit == eCSSUnit_Array && HasLocalSrc(val.GetArrayValue())) {
          break;
        }
      }

      AddFontFace(fontfamily, ruleRec.mFontEntry);
      mRules.AppendElement(ruleRec);
      aOldRules.RemoveElementAt(i);
      // note the set has been modified if an old rule was skipped to find
      // this one - something has been dropped, or ordering changed
      if (i > 0) {
        aFontSetModified = true;
      }
      return;
    }
  }

  // this is a new rule:

  uint32_t weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  int32_t stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  uint32_t italicStyle = NS_STYLE_FONT_STYLE_NORMAL;
  nsString languageOverride;

  // set up weight
  aRule->GetDesc(eCSSFontDesc_Weight, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Integer || unit == eCSSUnit_Enumerated) {
    weight = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face weight has unexpected unit");
  }

  // set up stretch
  aRule->GetDesc(eCSSFontDesc_Stretch, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    stretch = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face stretch has unexpected unit");
  }

  // set up font style
  aRule->GetDesc(eCSSFontDesc_Style, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    italicStyle = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    italicStyle = NS_STYLE_FONT_STYLE_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face style has unexpected unit");
  }

  // set up font features
  nsTArray<gfxFontFeature> featureSettings;
  aRule->GetDesc(eCSSFontDesc_FontFeatureSettings, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Normal) {
    // empty list of features
  } else if (unit == eCSSUnit_PairList || unit == eCSSUnit_PairListDep) {
    nsRuleNode::ComputeFontFeatures(val.GetPairListValue(), featureSettings);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-feature-settings has unexpected unit");
  }

  // set up font language override
  aRule->GetDesc(eCSSFontDesc_FontLanguageOverride, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Normal) {
    // empty feature string
  } else if (unit == eCSSUnit_String) {
    val.GetStringValue(languageOverride);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-language-override has unexpected unit");
  }

  // set up src array
  nsTArray<gfxFontFaceSrc> srcArray;

  aRule->GetDesc(eCSSFontDesc_Src, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Array) {
    nsCSSValue::Array* srcArr = val.GetArrayValue();
    size_t numSrc = srcArr->Count();
    
    for (size_t i = 0; i < numSrc; i++) {
      val = srcArr->Item(i);
      unit = val.GetUnit();
      gfxFontFaceSrc* face = srcArray.AppendElements(1);
      if (!face)
        return;

      switch (unit) {

      case eCSSUnit_Local_Font:
        val.GetStringValue(face->mLocalName);
        face->mIsLocal = true;
        face->mURI = nullptr;
        face->mFormatFlags = 0;
        break;
      case eCSSUnit_URL:
        face->mIsLocal = false;
        face->mURI = val.GetURLValue();
        face->mReferrer = val.GetURLStructValue()->mReferrer;
        face->mOriginPrincipal = val.GetURLStructValue()->mOriginPrincipal;
        NS_ASSERTION(face->mOriginPrincipal, "null origin principal in @font-face rule");

        // agent and user stylesheets are treated slightly differently,
        // the same-site origin check and access control headers are
        // enforced against the sheet principal rather than the document
        // principal to allow user stylesheets to include @font-face rules
        face->mUseOriginPrincipal = (aSheetType == nsStyleSet::eUserSheet ||
                                     aSheetType == nsStyleSet::eAgentSheet);

        face->mLocalName.Truncate();
        face->mFormatFlags = 0;
        while (i + 1 < numSrc && (val = srcArr->Item(i+1), 
                 val.GetUnit() == eCSSUnit_Font_Format)) {
          nsDependentString valueString(val.GetStringBufferValue());
          if (valueString.LowerCaseEqualsASCII("woff")) {
            face->mFormatFlags |= FLAG_FORMAT_WOFF;
          } else if (valueString.LowerCaseEqualsASCII("opentype")) {
            face->mFormatFlags |= FLAG_FORMAT_OPENTYPE;
          } else if (valueString.LowerCaseEqualsASCII("truetype")) {
            face->mFormatFlags |= FLAG_FORMAT_TRUETYPE;
          } else if (valueString.LowerCaseEqualsASCII("truetype-aat")) {
            face->mFormatFlags |= FLAG_FORMAT_TRUETYPE_AAT;
          } else if (valueString.LowerCaseEqualsASCII("embedded-opentype")) {
            face->mFormatFlags |= FLAG_FORMAT_EOT;
          } else if (valueString.LowerCaseEqualsASCII("svg")) {
            face->mFormatFlags |= FLAG_FORMAT_SVG;
          } else {
            // unknown format specified, mark to distinguish from the
            // case where no format hints are specified
            face->mFormatFlags |= FLAG_FORMAT_UNKNOWN;
          }
          i++;
        }
        if (!face->mURI) {
          // if URI not valid, omit from src array
          srcArray.RemoveElementAt(srcArray.Length() - 1);
          NS_WARNING("null url in @font-face rule");
          continue;
        }
        break;
      default:
        NS_ASSERTION(unit == eCSSUnit_Local_Font || unit == eCSSUnit_URL,
                     "strange unit type in font-face src array");
        break;
      }
     }
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null, "@font-face src has unexpected unit");
  }

  if (srcArray.Length() > 0) {
    FontFaceRuleRecord ruleRec;
    ruleRec.mContainer.mRule = aRule;
    ruleRec.mContainer.mSheetType = aSheetType;
    ruleRec.mFontEntry = AddFontFace(fontfamily, srcArray,
                                     weight, stretch, italicStyle,
                                     featureSettings, languageOverride);
    if (ruleRec.mFontEntry) {
      mRules.AppendElement(ruleRec);
    }
    // this was a new rule and fontEntry, so note that the set was modified
    aFontSetModified = true;
  }
}

void
nsUserFontSet::ReplaceFontEntry(gfxMixedFontFamily* aFamily,
                                gfxProxyFontEntry* aProxy,
                                gfxFontEntry* aFontEntry)
{
  // aProxy is being supplanted by the "real" font aFontEntry, so we need to
  // update any rules that refer to it. Note that there may be multiple rules
  // that refer to the same proxy - e.g. if a stylesheet was loaded multiple
  // times, so that several identical @font-face rules are present.
  for (uint32_t i = 0; i < mRules.Length(); ++i) {
    if (mRules[i].mFontEntry == aProxy) {
      mRules[i].mFontEntry = aFontEntry;
    }
  }
  aFamily->ReplaceFontEntry(aProxy, aFontEntry);
}

nsCSSFontFaceRule*
nsUserFontSet::FindRuleForEntry(gfxFontEntry* aFontEntry)
{
  for (uint32_t i = 0; i < mRules.Length(); ++i) {
    if (mRules[i].mFontEntry == aFontEntry) {
      return mRules[i].mContainer.mRule;
    }
  }
  return nullptr;
}

nsresult
nsUserFontSet::LogMessage(gfxMixedFontFamily* aFamily,
                          gfxProxyFontEntry* aProxy,
                          const char*        aMessage,
                          uint32_t          aFlags,
                          nsresult          aStatus)
{
  nsCOMPtr<nsIConsoleService>
    console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ConvertUTF16toUTF8 familyName(aFamily->Name());
  nsAutoCString fontURI;
  if (aProxy->mSrcIndex == aProxy->mSrcList.Length()) {
    fontURI.AppendLiteral("(end of source list)");
  } else {
    if (aProxy->mSrcList[aProxy->mSrcIndex].mURI) {
      aProxy->mSrcList[aProxy->mSrcIndex].mURI->GetSpec(fontURI);
    } else {
      fontURI.AppendLiteral("(invalid URI)");
    }
  }

  char weightKeywordBuf[8]; // plenty to sprintf() a uint16_t
  const char* weightKeyword;
  const nsAFlatCString& weightKeywordString =
    nsCSSProps::ValueToKeyword(aProxy->Weight(),
                               nsCSSProps::kFontWeightKTable);
  if (weightKeywordString.Length() > 0) {
    weightKeyword = weightKeywordString.get();
  } else {
    sprintf(weightKeywordBuf, "%u", aProxy->Weight());
    weightKeyword = weightKeywordBuf;
  }

  nsPrintfCString message
       ("downloadable font: %s "
        "(font-family: \"%s\" style:%s weight:%s stretch:%s src index:%d)",
        aMessage,
        familyName.get(),
        aProxy->IsItalic() ? "italic" : "normal",
        weightKeyword,
        nsCSSProps::ValueToKeyword(aProxy->Stretch(),
                                   nsCSSProps::kFontStretchKTable).get(),
        aProxy->mSrcIndex);

  if (NS_FAILED(aStatus)) {
    message.AppendLiteral(": ");
    switch (aStatus) {
    case NS_ERROR_DOM_BAD_URI:
      message.AppendLiteral("bad URI or cross-site access not allowed");
      break;
    case NS_ERROR_CONTENT_BLOCKED:
      message.AppendLiteral("content blocked");
      break;
    default:
      message.AppendLiteral("status=");
      message.AppendInt(static_cast<uint32_t>(aStatus));
      break;
    }
  }
  message.AppendLiteral("\nsource: ");
  message.Append(fontURI);

#ifdef PR_LOGGING
  if (PR_LOG_TEST(GetUserFontsLog(), PR_LOG_DEBUG)) {
    PR_LOG(GetUserFontsLog(), PR_LOG_DEBUG,
           ("userfonts (%p) %s", this, message.get()));
  }
#endif

  // try to give the user an indication of where the rule came from
  nsCSSFontFaceRule* rule = FindRuleForEntry(aProxy);
  nsString href;
  nsString text;
  nsresult rv;
  if (rule) {
    rv = rule->GetCssText(text);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDOMCSSStyleSheet> sheet;
    rv = rule->GetParentStyleSheet(getter_AddRefs(sheet));
    NS_ENSURE_SUCCESS(rv, rv);
    // if the style sheet is removed while the font is loading can be null
    if (sheet) {
      rv = sheet->GetHref(href);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      NS_WARNING("null parent stylesheet for @font-face rule");
      href.AssignLiteral("unknown");
    }
  }

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t innerWindowID = GetPresContext()->Document()->InnerWindowID();
  rv = scriptError->InitWithWindowID(NS_ConvertUTF8toUTF16(message),
                                     href,         // file
                                     text,         // src line
                                     0, 0,         // line & column number
                                     aFlags,       // flags
                                     "CSS Loader", // category (make separate?)
                                     innerWindowID);
  if (NS_SUCCEEDED(rv)) {
    console->LogMessage(scriptError);
  }

  return NS_OK;
}

nsresult
nsUserFontSet::CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                             nsIPrincipal** aPrincipal,
                             bool* aBypassCache)
{
  // check same-site origin
  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(aFontFaceSrc && !aFontFaceSrc->mIsLocal,
               "bad font face url passed to fontloader");
  NS_ASSERTION(aFontFaceSrc->mURI, "null font uri");
  if (!aFontFaceSrc->mURI)
    return NS_ERROR_FAILURE;

  // use document principal, original principal if flag set
  // this enables user stylesheets to load font files via
  // @font-face rules
  *aPrincipal = ps->GetDocument()->NodePrincipal();

  NS_ASSERTION(aFontFaceSrc->mOriginPrincipal,
               "null origin principal in @font-face rule");
  if (aFontFaceSrc->mUseOriginPrincipal) {
    *aPrincipal = aFontFaceSrc->mOriginPrincipal;
  }

  nsresult rv = nsFontFaceLoader::CheckLoadAllowed(*aPrincipal,
                                                   aFontFaceSrc->mURI,
                                                   ps->GetDocument());
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aBypassCache = false;

  nsCOMPtr<nsIDocShell> docShell = ps->GetDocument()->GetDocShell();
  if (docShell) {
    uint32_t loadType;
    if (NS_SUCCEEDED(docShell->GetLoadType(&loadType))) {
      if ((loadType >> 16) & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE) {
        *aBypassCache = true;
      }
    }
  }

  return rv;
}

nsresult
nsUserFontSet::SyncLoadFontData(gfxProxyFontEntry* aFontToLoad,
                                const gfxFontFaceSrc* aFontFaceSrc,
                                uint8_t*& aBuffer,
                                uint32_t& aBufferLength)
{
  nsresult rv;

  nsCOMPtr<nsIChannel> channel;
  // get Content Security Policy from principal to pass into channel
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = aFontToLoad->mPrincipal->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_FONT);
  }
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aFontFaceSrc->mURI,
                     nullptr,
                     nullptr,
                     nullptr,
                     nsIRequest::LOAD_NORMAL,
                     channelPolicy);

  NS_ENSURE_SUCCESS(rv, rv);

  // blocking stream is OK for data URIs
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t bufferLength64;
  rv = stream->Available(&bufferLength64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (bufferLength64 == 0) {
    return NS_ERROR_FAILURE;
  }
  if (bufferLength64 > UINT32_MAX) {
    return NS_ERROR_FILE_TOO_BIG;
  }
  aBufferLength = static_cast<uint32_t>(bufferLength64);

  // read all the decoded data
  aBuffer = static_cast<uint8_t*> (NS_Alloc(sizeof(uint8_t) * aBufferLength));
  if (!aBuffer) {
    aBufferLength = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t numRead, totalRead = 0;
  while (NS_SUCCEEDED(rv =
           stream->Read(reinterpret_cast<char*>(aBuffer + totalRead),
                        aBufferLength - totalRead, &numRead)) &&
         numRead != 0)
  {
    totalRead += numRead;
    if (totalRead > aBufferLength) {
      rv = NS_ERROR_FAILURE;
      break;
    }
  }

  // make sure there's a mime type
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString mimeType;
    rv = channel->GetContentType(mimeType);
    aBufferLength = totalRead;
  }

  if (NS_FAILED(rv)) {
    NS_Free(aBuffer);
    aBuffer = nullptr;
    aBufferLength = 0;
    return rv;
  }

  return NS_OK;
}

bool
nsUserFontSet::GetPrivateBrowsing()
{
  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps) {
    return false;
  }

  nsCOMPtr<nsILoadContext> loadContext = ps->GetDocument()->GetLoadContext();
  return loadContext && loadContext->UsePrivateBrowsing();
}

void
nsUserFontSet::DoRebuildUserFontSet()
{
  if (!mPresContext) {
    // AFAICS, this can only happen if someone has already called Destroy() on
    // this font-set, which means it is in the process of being torn down --
    // so there's no point trying to update its rules.
    return;
  }

  mPresContext->RebuildUserFontSet();
}
