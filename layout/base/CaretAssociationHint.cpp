/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CaretAssociationHint.h"

#include "mozilla/RangeBoundary.h"            // for RangeBoundaryBase
#include "mozilla/SelectionMovementUtils.h"   // for CaretFrameData
#include "mozilla/intl/BidiEmbeddingLevel.h"  // for BidiEmbeddingLevel
#include "nsCaret.h"                          // for nsCaret
#include "nsIContent.h"                       // for nsIContent
#include "nsIFrame.h"                         // for nsIFrame
#include "nsTextFrame.h"                      // for nsTextFrame

namespace mozilla {

template CaretAssociationHint ComputeCaretAssociationHint(
    CaretAssociationHint aDefault, intl::BidiEmbeddingLevel aBidiLevel,
    const RangeBoundary& aCaretPoint);
template CaretAssociationHint ComputeCaretAssociationHint(
    CaretAssociationHint aDefault, intl::BidiEmbeddingLevel aBidiLevel,
    const RawRangeBoundary& aCaretPoint);

template <typename PT, typename CT>
CaretAssociationHint ComputeCaretAssociationHint(
    CaretAssociationHint aDefault, intl::BidiEmbeddingLevel aBidiLevel,
    const RangeBoundaryBase<PT, CT>& aCaretPoint) {
  MOZ_ASSERT(aCaretPoint.IsSetAndValid());
  if (aDefault != CaretAssociationHint::Before ||
      !aCaretPoint.Container()->IsContent()) {
    return aDefault;
  }
  const nsCaret::CaretPosition pos{
      aCaretPoint.Container(),
      static_cast<int32_t>(*aCaretPoint.Offset(
          RangeBoundaryBase<PT, CT>::OffsetFilter::kValidOffsets)),
      aDefault, aBidiLevel};
  CaretFrameData frameData = nsCaret::GetFrameAndOffset(pos);
  nsTextFrame* f = do_QueryFrame(frameData.mFrame);
  if (f && f->IsAtEndOfLine() && f->HasSignificantTerminalNewline()) {
    // RangeBoundaryBase<PT, CT>::Offset() causes computing offset if it's not
    // been done yet.  However, it's called only when the container is a text
    // node.  In such case, offset has always been set since it cannot have
    // any children.  So, this doesn't cause computing offset with expensive
    // method, nsINode::ComputeIndexOf().
    const bool caretPointIsAtEndOfFrame =
        aCaretPoint.Container() == f->GetContent() &&
        f->GetContentEnd() ==
            static_cast<int32_t>(*aCaretPoint.Offset(
                RangeBoundaryBase<PT, CT>::OffsetFilter::kValidOffsets));
    const bool caretPointIsImmediatelyAfterFrameContent =
        aCaretPoint.Container() == f->GetContent()->GetParentNode() &&
        f->GetContent() == aCaretPoint.GetPreviousSiblingOfChildAtOffset();
    if (caretPointIsAtEndOfFrame || caretPointIsImmediatelyAfterFrameContent) {
      return CaretAssociationHint::After;
    }
  }
  return frameData.mFrame ? frameData.mHint : aDefault;
}

}  // namespace mozilla
