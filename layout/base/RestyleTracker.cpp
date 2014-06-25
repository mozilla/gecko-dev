/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A class which manages pending restyles.  This handles keeping track
 * of what nodes restyles need to happen on and so forth.
 */

#include "RestyleTracker.h"
#include "nsStyleChangeList.h"
#include "RestyleManager.h"
#include "GeckoProfiler.h"

namespace mozilla {

inline nsIDocument*
RestyleTracker::Document() const {
  return mRestyleManager->PresContext()->Document();
}

#define RESTYLE_ARRAY_STACKSIZE 128

struct LaterSiblingCollector {
  RestyleTracker* tracker;
  nsTArray< nsRefPtr<dom::Element> >* elements;
};

static PLDHashOperator
CollectLaterSiblings(nsISupports* aElement,
                     RestyleTracker::RestyleData& aData,
                     void* aSiblingCollector)
{
  dom::Element* element =
    static_cast<dom::Element*>(aElement);
  LaterSiblingCollector* collector =
    static_cast<LaterSiblingCollector*>(aSiblingCollector);
  // Only collect the entries that actually need restyling by us (and
  // haven't, for example, already been restyled).
  // It's important to not mess with the flags on entries not in our
  // document.
  if (element->GetCrossShadowCurrentDoc() == collector->tracker->Document() &&
      element->HasFlag(collector->tracker->RestyleBit()) &&
      (aData.mRestyleHint & eRestyle_LaterSiblings)) {
    collector->elements->AppendElement(element);
  }

  return PL_DHASH_NEXT;
}

struct RestyleCollector {
  RestyleTracker* tracker;
  RestyleTracker::RestyleEnumerateData** restyleArrayPtr;
};

static PLDHashOperator
CollectRestyles(nsISupports* aElement,
                RestyleTracker::RestyleData& aData,
                void* aRestyleCollector)
{
  dom::Element* element =
    static_cast<dom::Element*>(aElement);
  RestyleCollector* collector =
    static_cast<RestyleCollector*>(aRestyleCollector);
  // Only collect the entries that actually need restyling by us (and
  // haven't, for example, already been restyled).
  // It's important to not mess with the flags on entries not in our
  // document.
  if (element->GetCrossShadowCurrentDoc() != collector->tracker->Document() ||
      !element->HasFlag(collector->tracker->RestyleBit())) {
    return PL_DHASH_NEXT;
  }

  NS_ASSERTION(!element->HasFlag(collector->tracker->RootBit()) ||
               // Maybe we're just not reachable via the frame tree?
               (element->GetFlattenedTreeParent() &&
                (!element->GetFlattenedTreeParent()->GetPrimaryFrame()||
                 element->GetFlattenedTreeParent()->GetPrimaryFrame()->IsLeaf())) ||
               // Or not reachable due to an async reinsert we have
               // pending?  If so, we'll have a reframe hint around.
               // That incidentally makes it safe that we still have
               // the bit, since any descendants that didn't get added
               // to the roots list because we had the bits will be
               // completely restyled in a moment.
               (aData.mChangeHint & nsChangeHint_ReconstructFrame),
               "Why did this not get handled while processing mRestyleRoots?");

  // Unset the restyle bits now, so if they get readded later as we
  // process we won't clobber that adding of the bit.
  element->UnsetFlags(collector->tracker->RestyleBit() |
                      collector->tracker->RootBit());

  RestyleTracker::RestyleEnumerateData** restyleArrayPtr =
    collector->restyleArrayPtr;
  RestyleTracker::RestyleEnumerateData* currentRestyle =
    *restyleArrayPtr;
  currentRestyle->mElement = element;
  currentRestyle->mRestyleHint = aData.mRestyleHint;
  currentRestyle->mChangeHint = aData.mChangeHint;

  // Increment to the next slot in the array
  *restyleArrayPtr = currentRestyle + 1;

  return PL_DHASH_NEXT;
}

inline void
RestyleTracker::ProcessOneRestyle(Element* aElement,
                                  nsRestyleHint aRestyleHint,
                                  nsChangeHint aChangeHint)
{
  NS_PRECONDITION((aRestyleHint & eRestyle_LaterSiblings) == 0,
                  "Someone should have handled this before calling us");
  NS_PRECONDITION(Document(), "Must have a document");
  NS_PRECONDITION(aElement->GetCrossShadowCurrentDoc() == Document(),
                  "Element has unexpected document");

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  if (aRestyleHint & (eRestyle_Self | eRestyle_Subtree)) {
    mRestyleManager->RestyleElement(aElement, primaryFrame, aChangeHint,
                                    *this, aRestyleHint);
  } else if (aChangeHint &&
             (primaryFrame ||
              (aChangeHint & nsChangeHint_ReconstructFrame))) {
    // Don't need to recompute style; just apply the hint
    nsStyleChangeList changeList;
    changeList.AppendChange(primaryFrame, aElement, aChangeHint);
    mRestyleManager->ProcessRestyledFrames(changeList);
  }
}

void
RestyleTracker::DoProcessRestyles()
{
  PROFILER_LABEL("RestyleTracker", "ProcessRestyles",
    js::ProfileEntry::Category::CSS);

  mRestyleManager->BeginProcessingRestyles();

  // loop so that we process any restyle events generated by processing
  while (mPendingRestyles.Count()) {
    if (mHaveLaterSiblingRestyles) {
      // Convert them to individual restyles on all the later siblings
      nsAutoTArray<nsRefPtr<Element>, RESTYLE_ARRAY_STACKSIZE> laterSiblingArr;
      LaterSiblingCollector siblingCollector = { this, &laterSiblingArr };
      mPendingRestyles.Enumerate(CollectLaterSiblings, &siblingCollector);
      for (uint32_t i = 0; i < laterSiblingArr.Length(); ++i) {
        Element* element = laterSiblingArr[i];
        for (nsIContent* sibling = element->GetNextSibling();
             sibling;
             sibling = sibling->GetNextSibling()) {
          if (sibling->IsElement() &&
              AddPendingRestyle(sibling->AsElement(), eRestyle_Subtree,
                                NS_STYLE_HINT_NONE)) {
              // Nothing else to do here; we'll handle the following
              // siblings when we get to |sibling| in laterSiblingArr.
            break;
          }
        }
      }

      // Now remove all those eRestyle_LaterSiblings bits
      for (uint32_t i = 0; i < laterSiblingArr.Length(); ++i) {
        Element* element = laterSiblingArr[i];
        NS_ASSERTION(element->HasFlag(RestyleBit()), "How did that happen?");
        RestyleData data;
#ifdef DEBUG
        bool found =
#endif
          mPendingRestyles.Get(element, &data);
        NS_ASSERTION(found, "Where did our entry go?");
        data.mRestyleHint =
          nsRestyleHint(data.mRestyleHint & ~eRestyle_LaterSiblings);

        mPendingRestyles.Put(element, data);
      }

      mHaveLaterSiblingRestyles = false;
    }

    uint32_t rootCount;
    while ((rootCount = mRestyleRoots.Length())) {
      // Make sure to pop the element off our restyle root array, so
      // that we can freely append to the array as we process this
      // element.
      nsRefPtr<Element> element;
      element.swap(mRestyleRoots[rootCount - 1]);
      mRestyleRoots.RemoveElementAt(rootCount - 1);

      // Do the document check before calling GetRestyleData, since we
      // don't want to do the sibling-processing GetRestyleData does if
      // the node is no longer relevant.
      if (element->GetCrossShadowCurrentDoc() != Document()) {
        // Content node has been removed from our document; nothing else
        // to do here
        continue;
      }

      RestyleData data;
      if (!GetRestyleData(element, &data)) {
        continue;
      }

      ProcessOneRestyle(element, data.mRestyleHint, data.mChangeHint);
    }

    if (mHaveLaterSiblingRestyles) {
      // Keep processing restyles for now
      continue;
    }

    // Now we only have entries with change hints left.  To be safe in
    // case of reentry from the handing of the change hint, use a
    // scratch array instead of calling out to ProcessOneRestyle while
    // enumerating the hashtable.  Use the stack if we can, otherwise
    // fall back on heap-allocation.
    nsAutoTArray<RestyleEnumerateData, RESTYLE_ARRAY_STACKSIZE> restyleArr;
    RestyleEnumerateData* restylesToProcess =
      restyleArr.AppendElements(mPendingRestyles.Count());
    if (restylesToProcess) {
      RestyleEnumerateData* lastRestyle = restylesToProcess;
      RestyleCollector collector = { this, &lastRestyle };
      mPendingRestyles.Enumerate(CollectRestyles, &collector);

      // Clear the hashtable now that we don't need it anymore
      mPendingRestyles.Clear();

      for (RestyleEnumerateData* currentRestyle = restylesToProcess;
           currentRestyle != lastRestyle;
           ++currentRestyle) {
        ProcessOneRestyle(currentRestyle->mElement,
                          currentRestyle->mRestyleHint,
                          currentRestyle->mChangeHint);
      }
    }
  }

  mRestyleManager->EndProcessingRestyles();
}

bool
RestyleTracker::GetRestyleData(Element* aElement, RestyleData* aData)
{
  NS_PRECONDITION(aElement->GetCrossShadowCurrentDoc() == Document(),
                  "Unexpected document; this will lead to incorrect behavior!");

  if (!aElement->HasFlag(RestyleBit())) {
    NS_ASSERTION(!aElement->HasFlag(RootBit()), "Bogus root bit?");
    return false;
  }

#ifdef DEBUG
  bool gotData =
#endif
  mPendingRestyles.Get(aElement, aData);
  NS_ASSERTION(gotData, "Must have data if restyle bit is set");

  if (aData->mRestyleHint & eRestyle_LaterSiblings) {
    // Someone readded the eRestyle_LaterSiblings hint for this
    // element.  Leave it around for now, but remove the other restyle
    // hints and the change hint for it.  Also unset its root bit,
    // since it's no longer a root with the new restyle data.
    RestyleData newData;
    newData.mChangeHint = nsChangeHint(0);
    newData.mRestyleHint = eRestyle_LaterSiblings;
    mPendingRestyles.Put(aElement, newData);
    aElement->UnsetFlags(RootBit());
    aData->mRestyleHint =
      nsRestyleHint(aData->mRestyleHint & ~eRestyle_LaterSiblings);
  } else {
    mPendingRestyles.Remove(aElement);
    aElement->UnsetFlags(mRestyleBits);
  }

  return true;
}

} // namespace mozilla

