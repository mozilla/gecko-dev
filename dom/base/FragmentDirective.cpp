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
#include "TextDirectiveUtil.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/FragmentDirectiveBinding.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
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

JSObject* FragmentDirective::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return FragmentDirective_Binding::Wrap(aCx, this, aGivenProto);
}

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
  auto uri = TextDirectiveUtil::ShouldLog() && mDocument->GetDocumentURI()
                 ? mDocument->GetDocumentURI()->GetSpecOrDefault()
                 : nsCString();
  if (mUninvokedTextDirectives.IsEmpty()) {
    TEXT_FRAGMENT_LOG("No uninvoked text directives in document '%s'. Exiting.",
                      uri.Data());
    return {};
  }
  TEXT_FRAGMENT_LOG("Trying to find text directives in document '%s'.",
                    uri.Data());
  mDocument->FlushPendingNotifications(FlushType::Frames);
  // https://wicg.github.io/scroll-to-text-fragment/#invoke-text-directives
  // To invoke text directives, given as input a list of text directives text
  // directives and a Document document, run these steps:
  // 1. Let ranges be a list of ranges, initially empty.
  nsTArray<RefPtr<nsRange>> textDirectiveRanges(
      mUninvokedTextDirectives.Length());

  // Additionally (not mentioned in the spec), remove all text directives from
  // the input list to keep only the ones that are not found.
  // This code runs repeatedly during a page load, so it is possible that the
  // match for a text directive has not been parsed yet.
  nsTArray<TextDirective> uninvokedTextDirectives(
      mUninvokedTextDirectives.Length());

  // 2. For each text directive directive of text directives:
  for (TextDirective& textDirective : mUninvokedTextDirectives) {
    // 2.1 If the result of running find a range from a text directive given
    //     directive and document is non-null, then append it to ranges.
    if (RefPtr<nsRange> range = FindRangeForTextDirective(textDirective)) {
      textDirectiveRanges.AppendElement(range);
      TEXT_FRAGMENT_LOG("Found text directive '%s'",
                        ToString(textDirective).c_str());
    } else {
      uninvokedTextDirectives.AppendElement(std::move(textDirective));
    }
  }
  if (TextDirectiveUtil::ShouldLog()) {
    if (uninvokedTextDirectives.Length() == mUninvokedTextDirectives.Length()) {
      TEXT_FRAGMENT_LOG(
          "Did not find any of the %zu uninvoked text directives.",
          mUninvokedTextDirectives.Length());
    } else {
      TEXT_FRAGMENT_LOG(
          "Found %zu of %zu text directives in the document.",
          mUninvokedTextDirectives.Length() - uninvokedTextDirectives.Length(),
          mUninvokedTextDirectives.Length());
    }
    if (uninvokedTextDirectives.IsEmpty()) {
      TEXT_FRAGMENT_LOG("No uninvoked text directives left.");
    } else {
      TEXT_FRAGMENT_LOG("There are %zu uninvoked text directives left:",
                        uninvokedTextDirectives.Length());
      for (size_t index = 0; index < uninvokedTextDirectives.Length();
           ++index) {
        TEXT_FRAGMENT_LOG(" [%zu]: %s", index,
                          ToString(uninvokedTextDirectives[index]).c_str());
      }
    }
  }
  mUninvokedTextDirectives = std::move(uninvokedTextDirectives);

  // 3. Return ranges.
  return textDirectiveRanges;
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

RefPtr<nsRange> FragmentDirective::FindRangeForTextDirective(
    const TextDirective& aTextDirective) {
  TEXT_FRAGMENT_LOG("Find range for text directive '%s'.",
                    ToString(aTextDirective).c_str());
  // 1. Let searchRange be a range with start (document, 0) and end (document,
  // document’s length)
  ErrorResult rv;
  RefPtr<nsRange> searchRange =
      nsRange::Create(mDocument, 0, mDocument, mDocument->Length(), rv);
  if (rv.Failed()) {
    return nullptr;
  }
  // 2. While searchRange is not collapsed:
  while (!searchRange->Collapsed()) {
    // 2.1. Let potentialMatch be null.
    RefPtr<nsRange> potentialMatch;
    // 2.2. If parsedValues’s prefix is not null:
    if (!aTextDirective.prefix.IsEmpty()) {
      // 2.2.1. Let prefixMatch be the the result of running the find a string
      // in range steps with query parsedValues’s prefix, searchRange
      // searchRange, wordStartBounded true and wordEndBounded false.
      RefPtr<nsRange> prefixMatch = TextDirectiveUtil::FindStringInRange(
          searchRange, aTextDirective.prefix, true, false);
      // 2.2.2. If prefixMatch is null, return null.
      if (!prefixMatch) {
        TEXT_FRAGMENT_LOG(
            "Did not find prefix '%s'. The text directive does not exist "
            "in the document.",
            NS_ConvertUTF16toUTF8(aTextDirective.prefix).Data());
        return nullptr;
      }
      TEXT_FRAGMENT_LOG("Did find prefix '%s'.",
                        NS_ConvertUTF16toUTF8(aTextDirective.prefix).Data());

      // 2.2.3. Set searchRange’s start to the first boundary point after
      // prefixMatch’s start
      const RangeBoundary boundaryPoint =
          TextDirectiveUtil::MoveRangeBoundaryOneWord(
              {prefixMatch->GetStartContainer(), prefixMatch->StartOffset()},
              TextScanDirection::Right);
      if (!boundaryPoint.IsSetAndValid()) {
        return nullptr;
      }
      searchRange->SetStart(boundaryPoint.AsRaw(), rv);
      if (rv.Failed()) {
        return nullptr;
      }

      // 2.2.4. Let matchRange be a range whose start is prefixMatch’s end and
      // end is searchRange’s end.
      RefPtr<nsRange> matchRange = nsRange::Create(
          prefixMatch->GetEndContainer(), prefixMatch->EndOffset(),
          searchRange->GetEndContainer(), searchRange->EndOffset(), rv);
      if (rv.Failed()) {
        return nullptr;
      }
      // 2.2.5. Advance matchRange’s start to the next non-whitespace position.
      TextDirectiveUtil::AdvanceStartToNextNonWhitespacePosition(*matchRange);
      // 2.2.6. If matchRange is collapsed return null.
      // (This can happen if prefixMatch’s end or its subsequent non-whitespace
      // position is at the end of the document.)
      if (matchRange->Collapsed()) {
        return nullptr;
      }
      // 2.2.7. Assert: matchRange’s start node is a Text node.
      // (matchRange’s start now points to the next non-whitespace text data
      // following a matched prefix.)
      MOZ_ASSERT(matchRange->GetStartContainer()->IsText());

      // 2.2.8. Let mustEndAtWordBoundary be true if parsedValues’s end is
      // non-null or parsedValues’s suffix is null, false otherwise.
      const bool mustEndAtWordBoundary =
          !aTextDirective.end.IsEmpty() || aTextDirective.suffix.IsEmpty();
      // 2.2.9. Set potentialMatch to the result of running the find a string in
      // range steps with query parsedValues’s start, searchRange matchRange,
      // wordStartBounded false, and wordEndBounded mustEndAtWordBoundary.
      potentialMatch = TextDirectiveUtil::FindStringInRange(
          matchRange, aTextDirective.start, false, mustEndAtWordBoundary);
      // 2.2.10. If potentialMatch is null, return null.
      if (!potentialMatch) {
        TEXT_FRAGMENT_LOG(
            "Did not find start '%s'. The text directive does not exist "
            "in the document.",
            NS_ConvertUTF16toUTF8(aTextDirective.start).Data());
        return nullptr;
      }
      TEXT_FRAGMENT_LOG("Did find start '%s'.",
                        NS_ConvertUTF16toUTF8(aTextDirective.start).Data());
      // 2.2.11. If potentialMatch’s start is not matchRange’s start, then
      // continue.
      // (In this case, we found a prefix but it was followed by something other
      // than a matching text so we’ll continue searching for the next instance
      // of prefix.)
      if (potentialMatch->StartRef() != matchRange->StartRef()) {
        TEXT_FRAGMENT_LOG(
            "The prefix is not directly followed by the start element. "
            "Discarding this attempt.");
        continue;
      }
    }
    // 2.3. Otherwise:
    else {
      // 2.3.1. Let mustEndAtWordBoundary be true if parsedValues’s end is
      // non-null or parsedValues’s suffix is null, false otherwise.
      const bool mustEndAtWordBoundary =
          !aTextDirective.end.IsEmpty() || aTextDirective.suffix.IsEmpty();
      // 2.3.2. Set potentialMatch to the result of running the find a string in
      // range steps with query parsedValues’s start, searchRange searchRange,
      // wordStartBounded true, and wordEndBounded mustEndAtWordBoundary.
      potentialMatch = TextDirectiveUtil::FindStringInRange(
          searchRange, aTextDirective.start, true, mustEndAtWordBoundary);
      // 2.3.3. If potentialMatch is null, return null.
      if (!potentialMatch) {
        TEXT_FRAGMENT_LOG(
            "Did not find start '%s'. The text directive does not exist "
            "in the document.",
            NS_ConvertUTF16toUTF8(aTextDirective.start).Data());
        return nullptr;
      }
      // 2.3.4. Set searchRange’s start to the first boundary point after
      // potentialMatch’s start
      RangeBoundary newRangeBoundary =
          TextDirectiveUtil::MoveRangeBoundaryOneWord(
              {potentialMatch->GetStartContainer(),
               potentialMatch->StartOffset()},
              TextScanDirection::Right);
      if (!newRangeBoundary.IsSetAndValid()) {
        return nullptr;
      }
      searchRange->SetStart(newRangeBoundary.AsRaw(), rv);
      if (rv.Failed()) {
        return nullptr;
      }
    }
    // 2.4. Let rangeEndSearchRange be a range whose start is potentialMatch’s
    // end and whose end is searchRange’s end.
    RefPtr<nsRange> rangeEndSearchRange = nsRange::Create(
        potentialMatch->GetEndContainer(), potentialMatch->EndOffset(),
        searchRange->GetEndContainer(), searchRange->EndOffset(), rv);
    if (rv.Failed()) {
      return nullptr;
    }
    // 2.5. While rangeEndSearchRange is not collapsed:
    while (!rangeEndSearchRange->Collapsed()) {
      // 2.5.1. If parsedValues’s end item is non-null, then:
      if (!aTextDirective.end.IsEmpty()) {
        // 2.5.1.1. Let mustEndAtWordBoundary be true if parsedValues’s suffix
        // is null, false otherwise.
        const bool mustEndAtWordBoundary = aTextDirective.suffix.IsEmpty();
        // 2.5.1.2. Let endMatch be the result of running the find a string in
        // range steps with query parsedValues’s end, searchRange
        // rangeEndSearchRange, wordStartBounded true, and wordEndBounded
        // mustEndAtWordBoundary.
        RefPtr<nsRange> endMatch = TextDirectiveUtil::FindStringInRange(
            rangeEndSearchRange, aTextDirective.end, true,
            mustEndAtWordBoundary);
        // 2.5.1.3. If endMatch is null then return null.
        if (!endMatch) {
          TEXT_FRAGMENT_LOG(
              "Did not find end '%s'. The text directive does not exist "
              "in the document.",
              NS_ConvertUTF16toUTF8(aTextDirective.end).Data());
          return nullptr;
        }
        // 2.5.1.4. Set potentialMatch’s end to endMatch’s end.
        potentialMatch->SetEnd(endMatch->GetEndContainer(),
                               endMatch->EndOffset());
      }
      // 2.5.2. Assert: potentialMatch is non-null, not collapsed and represents
      // a range exactly containing an instance of matching text.
      MOZ_ASSERT(potentialMatch && !potentialMatch->Collapsed());

      // 2.5.3. If parsedValues’s suffix is null, return potentialMatch.
      if (aTextDirective.suffix.IsEmpty()) {
        TEXT_FRAGMENT_LOG("Did find a match.");
        return potentialMatch;
      }
      // 2.5.4. Let suffixRange be a range with start equal to potentialMatch’s
      // end and end equal to searchRange’s end.
      RefPtr<nsRange> suffixRange = nsRange::Create(
          potentialMatch->GetEndContainer(), potentialMatch->EndOffset(),
          searchRange->GetEndContainer(), searchRange->EndOffset(), rv);
      if (rv.Failed()) {
        return nullptr;
      }
      // 2.5.5. Advance suffixRange's start to the next non-whitespace position.
      TextDirectiveUtil::AdvanceStartToNextNonWhitespacePosition(*suffixRange);

      // 2.5.6. Let suffixMatch be result of running the find a string in range
      // steps with query parsedValue's suffix, searchRange suffixRange,
      // wordStartBounded false, and wordEndBounded true.
      RefPtr<nsRange> suffixMatch = TextDirectiveUtil::FindStringInRange(
          suffixRange, aTextDirective.suffix, false, true);

      // 2.5.7. If suffixMatch is null, return null.
      // (If the suffix doesn't appear in the remaining text of the document,
      // there's no possible way to make a match.)
      if (!suffixMatch) {
        TEXT_FRAGMENT_LOG(
            "Did not find suffix '%s'. The text directive does not exist "
            "in the document.",
            NS_ConvertUTF16toUTF8(aTextDirective.suffix).Data());
        return nullptr;
      }
      // 2.5.8. If suffixMatch's start is suffixRange's start, return
      // potentialMatch.
      if (suffixMatch->GetStartContainer() ==
              suffixRange->GetStartContainer() &&
          suffixMatch->StartOffset() == suffixRange->StartOffset()) {
        TEXT_FRAGMENT_LOG("Did find a match.");
        return potentialMatch;
      }
      // 2.5.9. If parsedValue's end item is null then break;
      // (If this is an exact match and the suffix doesn’t match, start
      // searching for the next range start by breaking out of this loop without
      // rangeEndSearchRange being collapsed. If we’re looking for a range
      // match, we’ll continue iterating this inner loop since the range start
      // will already be correct.)
      if (aTextDirective.end.IsEmpty()) {
        break;
      }
      // 2.5.10. Set rangeEndSearchRange's start to potentialMatch's end.
      // (Otherwise, it is possible that we found the correct range start, but
      // not the correct range end. Continue the inner loop to keep searching
      // for another matching instance of rangeEnd.)
      rangeEndSearchRange->SetStart(potentialMatch->GetEndContainer(),
                                    potentialMatch->EndOffset());
    }
    // 2.6. If rangeEndSearchRange is collapsed then:
    if (rangeEndSearchRange->Collapsed()) {
      // 2.6.1. Assert parsedValue's end item is non-null.
      // (This can only happen for range matches due to the break for exact
      // matches in step 9 of the above loop. If we couldn’t find a valid
      // rangeEnd+suffix pair anywhere in the doc then there’s no possible way
      // to make a match.)
      // ----
      // XXX(:jjaschke): Not too sure about this. If a text directive is only
      // defined by a (prefix +) start element, and the start element happens to
      // be at the end of the document, `rangeEndSearchRange` could be
      // collapsed. Therefore, the loop in section 2.5 does not run. Also,
      // if there would be either an `end` and/or a `suffix`, this would assert
      // instead of returning `nullptr`, indicating that there's no match.
      // Instead, the following would make the algorithm more safe:
      // if there is no end or suffix, the potential match is actually a match,
      // so return it. Otherwise, the text directive can't be in the document,
      // therefore return nullptr.
      if (aTextDirective.end.IsEmpty() && aTextDirective.suffix.IsEmpty()) {
        TEXT_FRAGMENT_LOG(
            "rangeEndSearchRange was collapsed, no end or suffix "
            "present. Returning a match");
        return potentialMatch;
      }
      TEXT_FRAGMENT_LOG(
          "rangeEndSearchRange was collapsed, there is an end or "
          "suffix. There can't be a match.");
      return nullptr;
    }
  }
  // 3. Return null.
  TEXT_FRAGMENT_LOG("Did not find a match.");
  return nullptr;
}

}  // namespace mozilla::dom
