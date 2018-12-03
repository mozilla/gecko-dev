/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentInlines_h
#define nsIDocumentInlines_h

#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsStyleSheetService.h"

inline mozilla::dom::HTMLBodyElement* nsIDocument::GetBodyElement() {
  return static_cast<mozilla::dom::HTMLBodyElement*>(
      GetHtmlChildElement(nsGkAtoms::body));
}

template <typename T>
size_t nsIDocument::FindDocStyleSheetInsertionPoint(
    const nsTArray<T>& aDocSheets, const mozilla::StyleSheet& aSheet) {
  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();

  // lowest index first
  int32_t newDocIndex = IndexOfSheet(aSheet);

  size_t count = aDocSheets.Length();
  size_t index = 0;
  for (; index < count; index++) {
    auto* sheet = static_cast<mozilla::StyleSheet*>(aDocSheets[index]);
    MOZ_ASSERT(sheet);
    int32_t sheetDocIndex = IndexOfSheet(*sheet);
    if (sheetDocIndex > newDocIndex) break;

    // If the sheet is not owned by the document it can be an author
    // sheet registered at nsStyleSheetService or an additional author
    // sheet on the document, which means the new
    // doc sheet should end up before it.
    if (sheetDocIndex < 0) {
      if (sheetService) {
        auto& authorSheets = *sheetService->AuthorStyleSheets();
        if (authorSheets.IndexOf(sheet) != authorSheets.NoIndex) {
          break;
        }
      }
      if (sheet == GetFirstAdditionalAuthorSheet()) {
        break;
      }
    }
  }

  return size_t(index);
}

inline void nsIDocument::SetServoRestyleRoot(nsINode* aRoot,
                                             uint32_t aDirtyBits) {
  MOZ_ASSERT(aRoot);

  MOZ_ASSERT(!mServoRestyleRoot || mServoRestyleRoot == aRoot ||
             nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
                 mServoRestyleRoot, aRoot));
  MOZ_ASSERT(aRoot == aRoot->OwnerDocAsNode() || aRoot->IsElement());
  mServoRestyleRoot = aRoot;
  SetServoRestyleRootDirtyBits(aDirtyBits);
}

// Note: we break this out of SetServoRestyleRoot so that callers can add
// bits without doing a no-op assignment to the restyle root, which would
// involve cycle-collected refcount traffic.
inline void nsIDocument::SetServoRestyleRootDirtyBits(uint32_t aDirtyBits) {
  MOZ_ASSERT(aDirtyBits);
  MOZ_ASSERT((aDirtyBits & ~Element::kAllServoDescendantBits) == 0);
  MOZ_ASSERT((aDirtyBits & mServoRestyleRootDirtyBits) ==
             mServoRestyleRootDirtyBits);
  MOZ_ASSERT(mServoRestyleRoot);
  mServoRestyleRootDirtyBits = aDirtyBits;
}

#endif  // nsIDocumentInlines_h
