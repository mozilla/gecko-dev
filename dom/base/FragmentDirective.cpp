/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FragmentDirective.h"
#include <cstdint>
#include "RangeBoundary.h"
#include "mozilla/Assertions.h"
#include "BasePrincipal.h"
#include "Document.h"
#include "TextDirectiveFinder.h"
#include "TextDirectiveUtil.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/FragmentDirectiveBinding.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/PresShell.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsICSSDeclaration.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIURIMutator.h"
#include "nsRange.h"
#include "nsString.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FragmentDirective, mDocument)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FragmentDirective)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FragmentDirective)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FragmentDirective)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FragmentDirective::FragmentDirective(Document* aDocument)
    : mDocument(aDocument) {}

FragmentDirective::~FragmentDirective() = default;

JSObject* FragmentDirective::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return FragmentDirective_Binding::Wrap(aCx, this, aGivenProto);
}

void FragmentDirective::SetTextDirectives(
    nsTArray<TextDirective>&& aTextDirectives) {
  MOZ_ASSERT(mDocument);
  if (!aTextDirectives.IsEmpty()) {
    mFinder =
        MakeUnique<TextDirectiveFinder>(*mDocument, std::move(aTextDirectives));
  } else {
    mFinder = nullptr;
  }
}

void FragmentDirective::ClearUninvokedDirectives() { mFinder = nullptr; }
bool FragmentDirective::HasUninvokedDirectives() const { return !!mFinder; };

bool FragmentDirective::ParseAndRemoveFragmentDirectiveFromFragmentString(
    nsCString& aFragment, nsTArray<TextDirective>* aTextDirectives,
    nsIURI* aURI) {
  auto uri = TextDirectiveUtil::ShouldLog() && aURI ? aURI->GetSpecOrDefault()
                                                    : nsCString();
  if (aFragment.IsEmpty()) {
    TEXT_FRAGMENT_LOG("URL '%s' has no fragment.", uri.Data());
    return false;
  }
  TEXT_FRAGMENT_LOG(
      "Trying to extract a fragment directive from fragment '%s' of URL '%s'.",
      aFragment.Data(), uri.Data());
  ParsedFragmentDirectiveResult fragmentDirective;
  const bool hasRemovedFragmentDirective =
      StaticPrefs::dom_text_fragments_enabled() &&
      parse_fragment_directive(&aFragment, &fragmentDirective);
  if (hasRemovedFragmentDirective) {
    TEXT_FRAGMENT_LOG(
        "Found a fragment directive '%s', which was removed from the fragment. "
        "New fragment is '%s'.",
        fragmentDirective.fragment_directive.Data(),
        fragmentDirective.hash_without_fragment_directive.Data());
    if (TextDirectiveUtil::ShouldLog()) {
      if (fragmentDirective.text_directives.IsEmpty()) {
        TEXT_FRAGMENT_LOG(
            "Found no valid text directives in fragment directive '%s'.",
            fragmentDirective.fragment_directive.Data());
      } else {
        TEXT_FRAGMENT_LOG(
            "Found %zu valid text directives in fragment directive '%s':",
            fragmentDirective.text_directives.Length(),
            fragmentDirective.fragment_directive.Data());
        for (size_t index = 0;
             index < fragmentDirective.text_directives.Length(); ++index) {
          const auto& textDirective = fragmentDirective.text_directives[index];
          TEXT_FRAGMENT_LOG(" [%zu]: %s", index,
                            ToString(textDirective).c_str());
        }
      }
    }
    aFragment = fragmentDirective.hash_without_fragment_directive;
    if (aTextDirectives) {
      aTextDirectives->SwapElements(fragmentDirective.text_directives);
    }
  } else {
    TEXT_FRAGMENT_LOG(
        "Fragment '%s' of URL '%s' did not contain a fragment directive.",
        aFragment.Data(), uri.Data());
  }
  return hasRemovedFragmentDirective;
}

void FragmentDirective::ParseAndRemoveFragmentDirectiveFromFragment(
    nsCOMPtr<nsIURI>& aURI, nsTArray<TextDirective>* aTextDirectives) {
  if (!aURI || !StaticPrefs::dom_text_fragments_enabled()) {
    return;
  }
  bool hasRef = false;
  aURI->GetHasRef(&hasRef);

  nsAutoCString hash;
  aURI->GetRef(hash);
  if (!hasRef || hash.IsEmpty()) {
    TEXT_FRAGMENT_LOG("URL '%s' has no fragment. Exiting.",
                      aURI->GetSpecOrDefault().Data());
  }

  const bool hasRemovedFragmentDirective =
      ParseAndRemoveFragmentDirectiveFromFragmentString(hash, aTextDirectives,
                                                        aURI);
  if (!hasRemovedFragmentDirective) {
    return;
  }
  Unused << NS_MutateURI(aURI).SetRef(hash).Finalize(aURI);
  TEXT_FRAGMENT_LOG("Updated hash of the URL. New URL: %s",
                    aURI->GetSpecOrDefault().Data());
}

nsTArray<RefPtr<nsRange>> FragmentDirective::FindTextFragmentsInDocument() {
  MOZ_ASSERT(mDocument);
  if (!mFinder) {
    auto uri = TextDirectiveUtil::ShouldLog() && mDocument->GetDocumentURI()
                   ? mDocument->GetDocumentURI()->GetSpecOrDefault()
                   : nsCString();
    TEXT_FRAGMENT_LOG("No uninvoked text directives in document '%s'. Exiting.",
                      uri.Data());
    return {};
  }
  auto textDirectives = mFinder->FindTextDirectivesInDocument();
  if (!mFinder->HasUninvokedDirectives()) {
    mFinder = nullptr;
  }
  return textDirectives;
}

/* static */ nsresult FragmentDirective::GetSpecIgnoringFragmentDirective(
    nsCOMPtr<nsIURI>& aURI, nsACString& aSpecIgnoringFragmentDirective) {
  bool hasRef = false;
  if (aURI->GetHasRef(&hasRef); !hasRef) {
    return aURI->GetSpec(aSpecIgnoringFragmentDirective);
  }

  nsAutoCString ref;
  nsresult rv = aURI->GetRef(ref);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aURI->GetSpecIgnoringRef(aSpecIgnoringFragmentDirective);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ParseAndRemoveFragmentDirectiveFromFragmentString(ref);

  if (!ref.IsEmpty()) {
    aSpecIgnoringFragmentDirective.Append('#');
    aSpecIgnoringFragmentDirective.Append(ref);
  }

  return NS_OK;
}

bool FragmentDirective::IsTextDirectiveAllowedToBeScrolledTo() {
  // This method follows
  // https://wicg.github.io/scroll-to-text-fragment/#check-if-a-text-directive-can-be-scrolled
  // However, there are some spec issues
  // (https://github.com/WICG/scroll-to-text-fragment/issues/240).
  // The web-platform tests currently seem more up-to-date. Therefore,
  // this method is adapted slightly to make sure all tests pass.
  // Comments are added to explain changes.

  MOZ_ASSERT(mDocument);
  auto uri = TextDirectiveUtil::ShouldLog() && mDocument->GetDocumentURI()
                 ? mDocument->GetDocumentURI()->GetSpecOrDefault()
                 : nsCString();
  TEXT_FRAGMENT_LOG(
      "Trying to find out if the load of URL '%s' is allowed to scroll to the "
      "text fragment",
      uri.Data());
  // It seems the spec does not cover same-document navigation in particular,
  // or Gecko needs to deal with this in a different way due to the
  // implementation not following the spec step-by-step.
  // Therefore, the following algorithm needs some adaptions to deal with
  // same-document navigations correctly.

  nsCOMPtr<nsILoadInfo> loadInfo =
      mDocument->GetChannel() ? mDocument->GetChannel()->LoadInfo() : nullptr;
  const bool isSameDocumentNavigation =
      loadInfo && loadInfo->GetIsSameDocumentNavigation();

  TEXT_FRAGMENT_LOG("Current load is%s a same-document navigation.",
                    isSameDocumentNavigation ? "" : " not");

  // 1. If document's pending text directives field is null or empty, return
  // false.
  // ---
  // we don't store the *pending* text directives in this class, only the
  // *uninvoked* text directives (uninvoked = `TextDirective`, pending =
  // `nsRange`).
  // Uninvoked text directives are typically already processed into pending text
  // directives when this code is called. Pending text directives are handled by
  // the caller when this code runs; therefore, the caller should decide if this
  // method should be called or not.

  // 2. Let is user involved be true if: document's text directive user
  // activation is true, or user involvement is one of "activation" or "browser
  // UI"; false otherwise.
  // 3. Set document's text directive user activation to false.
  const bool textDirectiveUserActivation =
      mDocument->ConsumeTextDirectiveUserActivation();
  TEXT_FRAGMENT_LOG(
      "Consumed Document's TextDirectiveUserActivation flag (value=%s)",
      textDirectiveUserActivation ? "true" : "false");

  // 4. If document's content type is not a text directive allowing MIME type,
  // return false.
  const bool isAllowedMIMEType = [doc = this->mDocument, func = __FUNCTION__] {
    nsAutoString contentType;
    doc->GetContentType(contentType);
    TEXT_FRAGMENT_LOG_FN("Got document MIME type: %s", func,
                         NS_ConvertUTF16toUTF8(contentType).Data());
    return contentType == u"text/html" || contentType == u"text/plain";
  }();

  if (!isAllowedMIMEType) {
    TEXT_FRAGMENT_LOG("Invalid document MIME type. Scrolling not allowed.");
    return false;
  }

  // 5. If user involvement is "browser UI", return true.
  //
  // If a navigation originates from browser UI, it's always ok to allow it
  // since it'll be user triggered and the page/script isn't providing the text
  // snippet.
  //
  // Note: The intent in this item is to distinguish cases where the app/page is
  // able to control the URL from those that are fully under the user's
  // control. In the former we want to prevent scrolling of the text fragment
  // unless the destination is loaded in a separate browsing context group (so
  // that the source cannot both control the text snippet and observe
  // side-effects in the navigation). There are some cases where "browser UI"
  // may be a grey area in this regard. E.g. an "open in new window" context
  // menu item when right clicking on a link.
  //
  // See sec-fetch-site [0] for a related discussion on how this applies.
  // [0] https://w3c.github.io/webappsec-fetch-metadata/#directly-user-initiated
  // ---
  // Gecko does not implement user involvement as defined in the spec.
  // However, if the triggering principal is the system principal, the load
  // has been triggered from browser chrome. This should be good enough for now.
  auto* triggeringPrincipal =
      loadInfo ? loadInfo->TriggeringPrincipal() : nullptr;
  const bool isTriggeredFromBrowserUI =
      triggeringPrincipal && triggeringPrincipal->IsSystemPrincipal();

  if (isTriggeredFromBrowserUI) {
    TEXT_FRAGMENT_LOG(
        "The load is triggered from browser UI. Scrolling allowed.");
    return true;
  }
  TEXT_FRAGMENT_LOG("The load is not triggered from browser UI.");
  // 6. If is user involved is false, return false.
  // ---
  // same-document navigation is not mentioned in the spec. However, we run this
  // code also in same-document navigation cases.
  // Same-document navigation is allowed even without any user interaction.
  if (!textDirectiveUserActivation && !isSameDocumentNavigation) {
    TEXT_FRAGMENT_LOG(
        "User involvement is false and not same-document navigation. Scrolling "
        "not allowed.");
    return false;
  }
  // 7. If document's node navigable has a parent, return false.
  // ---
  // this is extended to ignore this rule if this is a same-document navigation
  // in an iframe, which is allowed when the document's origin matches the
  // initiator's origin (which is checked in step 8).
  nsDocShell* docShell = nsDocShell::Cast(mDocument->GetDocShell());
  if (!isSameDocumentNavigation &&
      (!docShell || !docShell->GetIsTopLevelContentDocShell())) {
    TEXT_FRAGMENT_LOG(
        "Document's node navigable has a parent and this is not a "
        "same-document navigation. Scrolling not allowed.");
    return false;
  }
  // 8. If initiator origin is non-null and document's origin is same origin
  // with initiator origin, return true.
  const bool isSameOrigin = [doc = this->mDocument, triggeringPrincipal] {
    auto* docPrincipal = doc->GetPrincipal();
    return triggeringPrincipal && docPrincipal &&
           docPrincipal->Equals(triggeringPrincipal);
  }();

  if (isSameOrigin) {
    TEXT_FRAGMENT_LOG("Same origin. Scrolling allowed.");
    return true;
  }
  TEXT_FRAGMENT_LOG("Not same origin.");

  // 9. If document's browsing context's group's browsing context set has length
  // 1, return true.
  //
  // i.e. Only allow navigation from a cross-origin element/script if the
  // document is loaded in a noopener context. That is, a new top level browsing
  // context group to which the navigator does not have script access and which
  // can be placed into a separate process.
  if (BrowsingContextGroup* group =
          mDocument->GetBrowsingContext()
              ? mDocument->GetBrowsingContext()->Group()
              : nullptr) {
    const bool isNoOpenerContext = group->Toplevels().Length() == 1;
    if (!isNoOpenerContext) {
      TEXT_FRAGMENT_LOG(
          "Cross-origin + noopener=false. Scrolling not allowed.");
    }
    return isNoOpenerContext;
  }

  // 10.Otherwise, return false.
  TEXT_FRAGMENT_LOG("Scrolling not allowed.");
  return false;
}

void FragmentDirective::HighlightTextDirectives(
    const nsTArray<RefPtr<nsRange>>& aTextDirectiveRanges) {
  MOZ_ASSERT(mDocument);
  if (!StaticPrefs::dom_text_fragments_enabled()) {
    return;
  }
  auto uri = TextDirectiveUtil::ShouldLog() && mDocument->GetDocumentURI()
                 ? mDocument->GetDocumentURI()->GetSpecOrDefault()
                 : nsCString();
  if (aTextDirectiveRanges.IsEmpty()) {
    TEXT_FRAGMENT_LOG(
        "No text directive ranges to highlight for document '%s'. Exiting.",
        uri.Data());
    return;
  }

  TEXT_FRAGMENT_LOG(
      "Highlighting text directives for document '%s' (%zu ranges).",
      uri.Data(), aTextDirectiveRanges.Length());

  const RefPtr<Selection> targetTextSelection =
      [doc = this->mDocument]() -> Selection* {
    if (auto* presShell = doc->GetPresShell()) {
      return presShell->GetCurrentSelection(SelectionType::eTargetText);
    }
    return nullptr;
  }();
  if (!targetTextSelection) {
    return;
  }
  for (const RefPtr<nsRange>& range : aTextDirectiveRanges) {
    // Script won't be able to manipulate `aTextDirectiveRanges`,
    // therefore we can mark `range` as known live.
    targetTextSelection->AddRangeAndSelectFramesAndNotifyListeners(
        MOZ_KnownLive(*range), IgnoreErrors());
  }
}

void FragmentDirective::GetTextDirectiveRanges(
    nsTArray<RefPtr<nsRange>>& aRanges) const {
  if (!StaticPrefs::dom_text_fragments_enabled()) {
    return;
  }
  auto* presShell = mDocument ? mDocument->GetPresShell() : nullptr;
  if (!presShell) {
    return;
  }
  RefPtr<Selection> targetTextSelection =
      presShell->GetCurrentSelection(SelectionType::eTargetText);
  if (!targetTextSelection) {
    return;
  }

  aRanges.Clear();
  for (uint32_t rangeIndex = 0; rangeIndex < targetTextSelection->RangeCount();
       ++rangeIndex) {
    nsRange* range = targetTextSelection->GetRangeAt(rangeIndex);
    MOZ_ASSERT(range);
    aRanges.AppendElement(range);
  }
}
void FragmentDirective::RemoveAllTextDirectives(ErrorResult& aRv) {
  if (!StaticPrefs::dom_text_fragments_enabled()) {
    return;
  }
  auto* presShell = mDocument ? mDocument->GetPresShell() : nullptr;
  if (!presShell) {
    return;
  }
  RefPtr<Selection> targetTextSelection =
      presShell->GetCurrentSelection(SelectionType::eTargetText);
  if (!targetTextSelection) {
    return;
  }
  targetTextSelection->RemoveAllRanges(aRv);
}

}  // namespace mozilla::dom
