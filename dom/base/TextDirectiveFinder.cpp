/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TextDirectiveFinder.h"
#include "Document.h"
#include "TextDirectiveUtil.h"
#include "nsRange.h"
#include "fragmentdirectives_ffi_generated.h"

namespace mozilla::dom {

TextDirectiveFinder::TextDirectiveFinder(
    Document& aDocument, nsTArray<TextDirective>&& aTextDirectives)
    : mDocument(aDocument),
      mUninvokedTextDirectives(std::move(aTextDirectives)) {}

bool TextDirectiveFinder::HasUninvokedDirectives() const {
  return !mUninvokedTextDirectives.IsEmpty();
}
nsTArray<RefPtr<nsRange>> TextDirectiveFinder::FindTextDirectivesInDocument() {
  if (mUninvokedTextDirectives.IsEmpty()) {
    return {};
  }
  auto uri = TextDirectiveUtil::ShouldLog() && mDocument.GetDocumentURI()
                 ? mDocument.GetDocumentURI()->GetSpecOrDefault()
                 : nsCString();
  TEXT_FRAGMENT_LOG("Trying to find text directives in document '%s'.",
                    uri.Data());
  mDocument.FlushPendingNotifications(FlushType::Layout);
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

RefPtr<nsRange> TextDirectiveFinder::FindRangeForTextDirective(
    const TextDirective& aTextDirective) {
  TEXT_FRAGMENT_LOG("Find range for text directive '%s'.",
                    ToString(aTextDirective).c_str());
  // 1. Let searchRange be a range with start (document, 0) and end (document,
  // document’s length)
  ErrorResult rv;
  RefPtr<nsRange> searchRange =
      nsRange::Create(&mDocument, 0, &mDocument, mDocument.Length(), rv);
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
