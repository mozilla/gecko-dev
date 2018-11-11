/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGObserverUtils.h"

// Keep others in (case-insensitive) order:
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/RestyleManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsISupportsImpl.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsIReflowCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "SVGGeometryElement.h"
#include "SVGTextPathElement.h"
#include "SVGUseElement.h"
#include "ImageLoader.h"
#include "mozilla/net/ReferrerPolicy.h"

using namespace mozilla::dom;

static already_AddRefed<URLAndReferrerInfo>
ResolveURLUsingLocalRef(nsIFrame* aFrame, const css::URLValue* aURL)
{
  MOZ_ASSERT(aFrame);

  if (!aURL) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri = aURL->GetURI();

  if (aURL->IsLocalRef()) {
    uri = SVGObserverUtils::GetBaseURLForLocalRef(aFrame->GetContent(), uri);
    uri = aURL->ResolveLocalRef(uri);
  }

  if (!uri) {
    return nullptr;
  }

  RefPtr<URLAndReferrerInfo> info =
    new URLAndReferrerInfo(uri, aURL->ExtraData());
  return info.forget();
}


namespace mozilla {

class SVGFilterObserverList;


/**
 * A class used as a member of the "observer" classes below to help them
 * avoid dereferencing their frame during presshell teardown when their frame
 * may have been destroyed (leaving their pointer to their frame dangling).
 *
 * When a presshell is torn down, the properties for each frame may not be
 * deleted until after the frames are destroyed.  "Observer" objects (attached
 * as frame properties) must therefore check whether the presshell is being
 * torn down before using their pointer to their frame.
 *
 * mFramePresShell may be null, but when mFrame is non-null, mFramePresShell
 * is guaranteed to be non-null, too.
 */
struct nsSVGFrameReferenceFromProperty
{
  explicit nsSVGFrameReferenceFromProperty(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mFramePresShell(aFrame->PresShell())
  {}

  // Clear our reference to the frame.
  void Detach() {
    mFrame = nullptr;
    mFramePresShell = nullptr;
  }

  // null if the frame has become invalid
  nsIFrame* Get() {
    if (mFramePresShell && mFramePresShell->IsDestroying()) {
      Detach(); // mFrame is no longer valid.
    }
    return mFrame;
  }

private:
  // The frame that our property is attached to (may be null).
  nsIFrame* mFrame;
  nsIPresShell* mFramePresShell;
};


void
SVGRenderingObserver::StartObserving()
{
  Element* target = GetReferencedElementWithoutObserving();
  if (target) {
    target->AddMutationObserver(this);
  }
}

void
SVGRenderingObserver::StopObserving()
{
  Element* target = GetReferencedElementWithoutObserving();

  if (target) {
    target->RemoveMutationObserver(this);
    if (mInObserverList) {
      SVGObserverUtils::RemoveRenderingObserver(target, this);
      mInObserverList = false;
    }
  }
  NS_ASSERTION(!mInObserverList, "still in an observer list?");
}

Element*
SVGRenderingObserver::GetAndObserveReferencedElement()
{
#ifdef DEBUG
  DebugObserverSet();
#endif
  Element* referencedElement = GetReferencedElementWithoutObserving();
  if (referencedElement && !mInObserverList) {
    SVGObserverUtils::AddRenderingObserver(referencedElement, this);
    mInObserverList = true;
  }
  return referencedElement;
}

nsIFrame*
SVGRenderingObserver::GetAndObserveReferencedFrame()
{
  Element* referencedElement = GetAndObserveReferencedElement();
  return referencedElement ? referencedElement->GetPrimaryFrame() : nullptr;
}

nsIFrame*
SVGRenderingObserver::GetAndObserveReferencedFrame(LayoutFrameType aFrameType,
                                                   bool* aOK)
{
  nsIFrame* frame = GetAndObserveReferencedFrame();
  if (frame) {
    if (frame->Type() == aFrameType)
      return frame;
    if (aOK) {
      *aOK = false;
    }
  }
  return nullptr;
}

void
SVGRenderingObserver::OnNonDOMMutationRenderingChange()
{
  mInObserverList = false;
  OnRenderingChange();
}

void
SVGRenderingObserver::NotifyEvictedFromRenderingObserverSet()
{
  mInObserverList = false; // We've been removed from rendering-obs. list.
  StopObserving();            // Remove ourselves from mutation-obs. list.
}

void
SVGRenderingObserver::AttributeChanged(dom::Element* aElement,
                                       int32_t aNameSpaceID,
                                       nsAtom* aAttribute,
                                       int32_t aModType,
                                       const nsAttrValue* aOldValue)
{
  // An attribute belonging to the element that we are observing *or one of its
  // descendants* has changed.
  //
  // In the case of observing a gradient element, say, we want to know if any
  // of its 'stop' element children change, but we don't actually want to do
  // anything for changes to SMIL element children, for example. Maybe it's not
  // worth having logic to optimize for that, but in most cases it could be a
  // small check?
  //
  // XXXjwatt: do we really want to blindly break the link between our
  // observers and ourselves for all attribute changes? For non-ID changes
  // surely that is unnecessary.

  OnRenderingChange();
}

void
SVGRenderingObserver::ContentAppended(nsIContent* aFirstNewContent)
{
  OnRenderingChange();
}

void
SVGRenderingObserver::ContentInserted(nsIContent* aChild)
{
  OnRenderingChange();
}

void
SVGRenderingObserver::ContentRemoved(nsIContent* aChild,
                                     nsIContent* aPreviousSibling)
{
  OnRenderingChange();
}


/**
 * SVG elements reference supporting resources by element ID. We need to
 * track when those resources change and when the document changes in ways
 * that affect which element is referenced by a given ID (e.g., when
 * element IDs change). The code here is responsible for that.
 *
 * When a frame references a supporting resource, we create a property
 * object derived from SVGIDRenderingObserver to manage the relationship. The
 * property object is attached to the referencing frame.
 */
class SVGIDRenderingObserver : public SVGRenderingObserver
{
public:
  SVGIDRenderingObserver(URLAndReferrerInfo* aURI, nsIContent* aObservingContent,
                         bool aReferenceImage);

protected:
  virtual ~SVGIDRenderingObserver() {
    // This needs to call our GetReferencedElementWithoutObserving override,
    // so must be called here rather than in our base class's dtor.
    StopObserving();
  }

  Element* GetReferencedElementWithoutObserving() final {
    return mObservedElementTracker.get();
  }

  void OnRenderingChange() override;

  /**
   * Helper that provides a reference to the element with the ID that our
   * observer wants to observe, and that will invalidate our observer if the
   * element that that ID identifies changes to a different element (or none).
   */
  class ElementTracker final : public IDTracker
  {
  public:
    explicit ElementTracker(SVGIDRenderingObserver* aOwningObserver)
      : mOwningObserver(aOwningObserver)
    {}
  protected:
    void ElementChanged(Element* aFrom, Element* aTo) override {
      mOwningObserver->StopObserving(); // stop observing the old element
      IDTracker::ElementChanged(aFrom, aTo);
      mOwningObserver->StartObserving(); // start observing the new element
      mOwningObserver->OnRenderingChange();
    }
    /**
     * Override IsPersistent because we want to keep tracking the element
     * for the ID even when it changes.
     */
    bool IsPersistent() override { return true; }
  private:
    SVGIDRenderingObserver* mOwningObserver;
  };

  ElementTracker mObservedElementTracker;
};

/**
 * Note that in the current setup there are two separate observer lists.
 *
 * In SVGIDRenderingObserver's ctor, the new object adds itself to the
 * mutation observer list maintained by the referenced element. In this way the
 * SVGIDRenderingObserver is notified if there are any attribute or content
 * tree changes to the element or any of its *descendants*.
 *
 * In SVGIDRenderingObserver::GetAndObserveReferencedElement() the
 * SVGIDRenderingObserver object also adds itself to an
 * SVGRenderingObserverSet object belonging to the referenced
 * element.
 *
 * XXX: it would be nice to have a clear and concise executive summary of the
 * benefits/necessity of maintaining a second observer list.
 */
SVGIDRenderingObserver::SVGIDRenderingObserver(URLAndReferrerInfo* aURI,
                                               nsIContent* aObservingContent,
                                               bool aReferenceImage)
  : mObservedElementTracker(this)
{
  // Start watching the target element
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIURI> referrer;
  uint32_t referrerPolicy = mozilla::net::RP_Unset;
  if (aURI) {
    uri = aURI->GetURI();
    referrer = aURI->GetReferrer();
    referrerPolicy = aURI->GetReferrerPolicy();
  }

  mObservedElementTracker.ResetToURIFragmentID(aObservingContent, uri, referrer,
                                               referrerPolicy, true,
                                               aReferenceImage);
  StartObserving();
}

void
SVGIDRenderingObserver::OnRenderingChange()
{
  if (mObservedElementTracker.get() && mInObserverList) {
    SVGObserverUtils::RemoveRenderingObserver(mObservedElementTracker.get(), this);
    mInObserverList = false;
  }
}


class nsSVGRenderingObserverProperty : public SVGIDRenderingObserver
{
public:
  NS_DECL_ISUPPORTS

  nsSVGRenderingObserverProperty(URLAndReferrerInfo* aURI, nsIFrame *aFrame,
                                 bool aReferenceImage)
    : SVGIDRenderingObserver(aURI, aFrame->GetContent(), aReferenceImage)
    , mFrameReference(aFrame)
  {}

protected:
  virtual ~nsSVGRenderingObserverProperty() = default; // non-public

  void OnRenderingChange() override;

  nsSVGFrameReferenceFromProperty mFrameReference;
};

NS_IMPL_ISUPPORTS(nsSVGRenderingObserverProperty, nsIMutationObserver)

void
nsSVGRenderingObserverProperty::OnRenderingChange()
{
  SVGIDRenderingObserver::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();

  if (frame && frame->HasAllStateBits(NS_FRAME_SVG_LAYOUT)) {
    // We need to notify anything that is observing the referencing frame or
    // any of its ancestors that the referencing frame has been invalidated.
    // Since walking the parent chain checking for observers is expensive we
    // do that using a change hint (multiple change hints of the same type are
    // coalesced).
    nsLayoutUtils::PostRestyleEvent(
      frame->GetContent()->AsElement(), nsRestyleHint(0),
      nsChangeHint_InvalidateRenderingObservers);
  }
}


class SVGTextPathObserver final : public nsSVGRenderingObserverProperty
{
public:
  SVGTextPathObserver(URLAndReferrerInfo* aURI, nsIFrame* aFrame, bool aReferenceImage)
    : nsSVGRenderingObserverProperty(aURI, aFrame, aReferenceImage)
    , mValid(true)
  {}

  bool ObservesReflow() override {
    return false;
  }

protected:
  void OnRenderingChange() override;

private:
  /**
   * Returns true if the target of the textPath is the frame of a 'path' element.
   */
  bool TargetIsValid() {
    Element* target = GetReferencedElementWithoutObserving();
    return target && target->IsSVGElement(nsGkAtoms::path);
  }

  bool mValid;
};

void
SVGTextPathObserver::OnRenderingChange()
{
  nsSVGRenderingObserverProperty::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame) {
    return;
  }

  MOZ_ASSERT(frame->IsFrameOfType(nsIFrame::eSVG) ||
             nsSVGUtils::IsInSVGTextSubtree(frame),
             "SVG frame expected");

  // Avoid getting into an infinite loop of reflows if the <textPath> is
  // pointing to one of its ancestors.  TargetIsValid returns true iff
  // the target element is a <path> element, and we would not have this
  // SVGTextPathObserver if this <textPath> were a descendant of the
  // target <path>.
  //
  // Note that we still have to post the restyle event when we
  // change from being valid to invalid, so that mPositions on the
  // SVGTextFrame gets updated, skipping the <textPath>, ensuring
  // that nothing gets painted for that element.
  bool nowValid = TargetIsValid();
  if (!mValid && !nowValid) {
    // Just return if we were previously invalid, and are still invalid.
    return;
  }
  mValid = nowValid;

  // Repaint asynchronously in case the path frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_UpdateTextPath);
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}


class SVGMarkerObserver final: public nsSVGRenderingObserverProperty
{
public:
  SVGMarkerObserver(URLAndReferrerInfo* aURI, nsIFrame* aFrame, bool aReferenceImage)
    : nsSVGRenderingObserverProperty(aURI, aFrame, aReferenceImage)
  {}

protected:
  void OnRenderingChange() override;
};

void
SVGMarkerObserver::OnRenderingChange()
{
  nsSVGRenderingObserverProperty::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame) {
    return;
  }

  MOZ_ASSERT(frame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");

  // Don't need to request ReflowFrame if we're being reflowed.
  if (!(frame->GetStateBits() & NS_FRAME_IN_REFLOW)) {
    // XXXjwatt: We need to unify SVG into standard reflow so we can just use
    // nsChangeHint_NeedReflow | nsChangeHint_NeedDirtyReflow here.
    // XXXSDL KILL THIS!!!
    nsSVGUtils::ScheduleReflowSVG(frame);
  }
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0),
    nsChangeHint_RepaintFrame);
}


class nsSVGPaintingProperty final : public nsSVGRenderingObserverProperty
{
public:
  nsSVGPaintingProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame, bool aReferenceImage)
    : nsSVGRenderingObserverProperty(aURI, aFrame, aReferenceImage)
  {}

protected:
  void OnRenderingChange() override;
};

void
nsSVGPaintingProperty::OnRenderingChange()
{
  nsSVGRenderingObserverProperty::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame) {
    return;
  }

  if (frame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    frame->InvalidateFrameSubtree();
  } else {
    for (nsIFrame* f = frame; f;
         f = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(f)) {
      f->InvalidateFrame();
    }
  }
}


/**
 * In a filter chain, there can be multiple SVG reference filters.
 * e.g. filter: url(#svg-filter-1) blur(10px) url(#svg-filter-2);
 *
 * This class keeps track of one SVG reference filter in a filter chain.
 * e.g. url(#svg-filter-1)
 *
 * It fires invalidations when the SVG filter element's id changes or when
 * the SVG filter element's content changes.
 *
 * The SVGFilterObserverList class manages a list of SVGFilterObservers.
 */
class SVGFilterObserver final : public SVGIDRenderingObserver
{
public:
  SVGFilterObserver(URLAndReferrerInfo* aURI,
                    nsIContent* aObservingContent,
                    SVGFilterObserverList* aFilterChainObserver)
    : SVGIDRenderingObserver(aURI, aObservingContent, false)
    , mFilterObserverList(aFilterChainObserver)
  {}

  // XXXjwatt: This will return false if the reference is to a filter in an
  // external resource document that hasn't loaded yet!
  bool ReferencesValidResource() { return GetAndObserveFilterFrame(); }

  void DetachFromChainObserver() { mFilterObserverList = nullptr; }

  /**
   * @return the filter frame, or null if there is no filter frame
   */
  nsSVGFilterFrame* GetAndObserveFilterFrame();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SVGFilterObserver)

  void Invalidate() { OnRenderingChange(); };

protected:
  virtual ~SVGFilterObserver() = default; // non-public

  // SVGIDRenderingObserver
  void OnRenderingChange() override;

private:
  SVGFilterObserverList* mFilterObserverList;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGFilterObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGFilterObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGFilterObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGFilterObserver)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SVGFilterObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObservedElementTracker)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SVGFilterObserver)
  tmp->StopObserving();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObservedElementTracker);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsSVGFilterFrame*
SVGFilterObserver::GetAndObserveFilterFrame()
{
  return static_cast<nsSVGFilterFrame*>(
    GetAndObserveReferencedFrame(LayoutFrameType::SVGFilter, nullptr));
}


/**
 * This class manages a list of SVGFilterObservers, which correspond to
 * reference to SVG filters in a list of filters in a given 'filter' property.
 * e.g. filter: url(#svg-filter-1) blur(10px) url(#svg-filter-2);
 *
 * In the above example, the SVGFilterObserverList will manage two
 * SVGFilterObservers, one for each of the references to SVG filters.  CSS
 * filters like "blur(10px)" don't reference filter elements, so they don't
 * need an SVGFilterObserver.  The style system invalidates changes to CSS
 * filters.
 */
class SVGFilterObserverList : public nsISupports
{
public:
  SVGFilterObserverList(const nsTArray<nsStyleFilter>& aFilters,
                        nsIContent* aFilteredElement,
                        nsIFrame* aFiltedFrame = nullptr);

  bool ReferencesValidResources();
  void Invalidate() { OnRenderingChange(); }

  const nsTArray<RefPtr<SVGFilterObserver>>& GetObservers() const {
    return mObservers;
  }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SVGFilterObserverList)

protected:
  virtual ~SVGFilterObserverList();

  virtual void OnRenderingChange() = 0;

private:

  void DetachObservers()
  {
    for (uint32_t i = 0; i < mObservers.Length(); i++) {
      mObservers[i]->DetachFromChainObserver();
    }
  }

  nsTArray<RefPtr<SVGFilterObserver>> mObservers;
};

void
SVGFilterObserver::OnRenderingChange()
{
  SVGIDRenderingObserver::OnRenderingChange();

  if (mFilterObserverList) {
    mFilterObserverList->Invalidate();
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGFilterObserverList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGFilterObserverList)

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGFilterObserverList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SVGFilterObserverList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObservers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SVGFilterObserverList)
  tmp->DetachObservers();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObservers);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGFilterObserverList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SVGFilterObserverList::SVGFilterObserverList(const nsTArray<nsStyleFilter>& aFilters,
                                             nsIContent* aFilteredElement,
                                             nsIFrame* aFilteredFrame)
{
  for (uint32_t i = 0; i < aFilters.Length(); i++) {
    if (aFilters[i].GetType() != NS_STYLE_FILTER_URL) {
      continue;
    }

    // aFilteredFrame can be null if this filter belongs to a
    // CanvasRenderingContext2D.
    RefPtr<URLAndReferrerInfo> filterURL;
    if (aFilteredFrame) {
      filterURL = ResolveURLUsingLocalRef(aFilteredFrame, aFilters[i].GetURL());
    } else {
      nsCOMPtr<nsIURI> resolvedURI =
        aFilters[i].GetURL()->ResolveLocalRef(aFilteredElement);
      if (resolvedURI) {
        filterURL = new URLAndReferrerInfo(
          resolvedURI,
          aFilters[i].GetURL()->ExtraData());
      }
    }

    RefPtr<SVGFilterObserver> observer =
      new SVGFilterObserver(filterURL, aFilteredElement, this);
    mObservers.AppendElement(observer);
  }
}

SVGFilterObserverList::~SVGFilterObserverList()
{
  DetachObservers();
}

bool
SVGFilterObserverList::ReferencesValidResources()
{
  for (uint32_t i = 0; i < mObservers.Length(); i++) {
    if (!mObservers[i]->ReferencesValidResource()) {
      return false;
    }
  }
  return true;
}


class SVGFilterObserverListForCSSProp final : public SVGFilterObserverList
{
public:
  SVGFilterObserverListForCSSProp(const nsTArray<nsStyleFilter>& aFilters,
                                  nsIFrame* aFilteredFrame)
    : SVGFilterObserverList(aFilters, aFilteredFrame->GetContent(),
                            aFilteredFrame)
    , mFrameReference(aFilteredFrame)
  {}

  void DetachFromFrame() { mFrameReference.Detach(); }

protected:
  void OnRenderingChange() override;

  nsSVGFrameReferenceFromProperty mFrameReference;
};

void
SVGFilterObserverListForCSSProp::OnRenderingChange()
{
  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  // Repaint asynchronously in case the filter frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame);

  // Since we don't call nsSVGRenderingObserverProperty::
  // OnRenderingChange, we have to add this bit ourselves.
  if (frame->HasAllStateBits(NS_FRAME_SVG_LAYOUT)) {
    // Changes should propagate out to things that might be observing
    // the referencing frame or its ancestors.
    changeHint |= nsChangeHint_InvalidateRenderingObservers;
  }

  // Don't need to request UpdateOverflow if we're being reflowed.
  if (!(frame->GetStateBits() & NS_FRAME_IN_REFLOW)) {
    changeHint |= nsChangeHint_UpdateOverflow;
  }
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}


class SVGFilterObserverListForCanvasContext final : public SVGFilterObserverList
{
public:
  SVGFilterObserverListForCanvasContext(CanvasRenderingContext2D* aContext,
                                        Element* aCanvasElement,
                                        nsTArray<nsStyleFilter>& aFilters)
    : SVGFilterObserverList(aFilters, aCanvasElement)
    , mContext(aContext)
  {}

  void OnRenderingChange() override;
  void DetachFromContext() { mContext = nullptr; }

private:
  CanvasRenderingContext2D* mContext;
};

void
SVGFilterObserverListForCanvasContext::OnRenderingChange()
{
  if (!mContext) {
    MOZ_CRASH("GFX: This should never be called without a context");
  }
  // Refresh the cached FilterDescription in mContext->CurrentState().filter.
  // If this filter is not at the top of the state stack, we'll refresh the
  // wrong filter, but that's ok, because we'll refresh the right filter
  // when we pop the state stack in CanvasRenderingContext2D::Restore().
  RefPtr<CanvasRenderingContext2D> kungFuDeathGrip(mContext);
  kungFuDeathGrip->UpdateFilter();
}


class SVGMaskObserverList final : public nsISupports
{
public:
  explicit SVGMaskObserverList(nsIFrame* aFrame);

  // nsISupports
  NS_DECL_ISUPPORTS

  const nsTArray<RefPtr<nsSVGPaintingProperty>>& GetObservers() const
  {
    return mProperties;
  }

  void ResolveImage(uint32_t aIndex);

private:
  virtual ~SVGMaskObserverList() = default; // non-public
  nsTArray<RefPtr<nsSVGPaintingProperty>> mProperties;
  nsIFrame* mFrame;
};

NS_IMPL_ISUPPORTS(SVGMaskObserverList, nsISupports)

SVGMaskObserverList::SVGMaskObserverList(nsIFrame* aFrame)
  : mFrame(aFrame)
{
  const nsStyleSVGReset *svgReset = aFrame->StyleSVGReset();

  for (uint32_t i = 0; i < svgReset->mMask.mImageCount; i++) {
    const css::URLValue* data = svgReset->mMask.mLayers[i].mImage.GetURLValue();
    RefPtr<URLAndReferrerInfo> maskUri = ResolveURLUsingLocalRef(aFrame, data);

    bool hasRef = false;
    if (maskUri) {
      maskUri->GetURI()->GetHasRef(&hasRef);
    }

    // Accrording to maskUri, nsSVGPaintingProperty's ctor may trigger an
    // external SVG resource download, so we should pass maskUri in only if
    // maskUri has a chance pointing to an SVG mask resource.
    //
    // And, an URL may refer to an SVG mask resource if it consists of
    // a fragment.
    nsSVGPaintingProperty* prop =
      new nsSVGPaintingProperty(hasRef ? maskUri.get() : nullptr,
                                aFrame, false);
    mProperties.AppendElement(prop);
  }
}

void
SVGMaskObserverList::ResolveImage(uint32_t aIndex)
{
  const nsStyleSVGReset* svgReset = mFrame->StyleSVGReset();
  MOZ_ASSERT(aIndex < svgReset->mMask.mImageCount);

  nsStyleImage& image =
    const_cast<nsStyleImage&>(svgReset->mMask.mLayers[aIndex].mImage);

  if (!image.IsResolved()) {
    MOZ_ASSERT(image.GetType() == nsStyleImageType::eStyleImageType_Image);
    image.ResolveImage(mFrame->PresContext(), nullptr);

    mozilla::css::ImageLoader* imageLoader =
      mFrame->PresContext()->Document()->StyleImageLoader();
    if (imgRequestProxy* req = image.GetImageData()) {
      imageLoader->AssociateRequestToFrame(req, mFrame, 0);
    }
  }
}


/**
 * Used for gradient-to-gradient, pattern-to-pattern and filter-to-filter
 * references to "template" elements (specified via the 'href' attributes).
 *
 * This is a special class for the case where we know we only want to call
 * InvalidateDirectRenderingObservers (as opposed to
 * InvalidateRenderingObservers).
 *
 * TODO(jwatt): If we added a new NS_FRAME_RENDERING_OBSERVER_CONTAINER state
 * bit to clipPath, filter, gradients, marker, mask, pattern and symbol, and
 * could have InvalidateRenderingObservers stop on reaching such an element,
 * then we would no longer need this class (not to mention improving perf by
 * significantly cutting down on ancestor traversal).
 */
class SVGTemplateElementObserver : public SVGIDRenderingObserver
{
public:
  NS_DECL_ISUPPORTS

  SVGTemplateElementObserver(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
                             bool aReferenceImage)
    : SVGIDRenderingObserver(aURI, aFrame->GetContent(), aReferenceImage)
    , mFrameReference(aFrame)
  {}

protected:
  virtual ~SVGTemplateElementObserver() = default; // non-public

  void OnRenderingChange() override;

  nsSVGFrameReferenceFromProperty mFrameReference;
};

NS_IMPL_ISUPPORTS(SVGTemplateElementObserver, nsIMutationObserver)

void
SVGTemplateElementObserver::OnRenderingChange()
{
  SVGIDRenderingObserver::OnRenderingChange();

  if (nsIFrame* frame = mFrameReference.Get()) {
    // We know that we don't need to walk the parent chain notifying rendering
    // observers since changes to a gradient etc. do not affect ancestor
    // elements.  So we only invalidate *direct* rendering observers here.
    // Since we don't need to walk the parent chain, we don't need to worry
    // about coalescing multiple invalidations by using a change hint as we do
    // in nsSVGRenderingObserverProperty::OnRenderingChange.
    SVGObserverUtils::InvalidateDirectRenderingObservers(frame);
  }
}


/**
 * An instance of this class is stored on an observed frame (as a frame
 * property) whenever the frame has active rendering observers.  It is used to
 * store pointers to the SVGRenderingObserver instances belonging to any
 * observing frames, allowing invalidations from the observed frame to be sent
 * to all observing frames.
 *
 * SVGRenderingObserver instances that are added are not strongly referenced,
 * so they must remove themselves before they die.
 *
 * This class is "single-shot", which is to say that when something about the
 * observed element changes, InvalidateAll() clears our hashtable of
 * SVGRenderingObservers.  SVGRenderingObserver objects will be added back
 * again if/when the observing frame looks up our observed frame to use it.
 *
 * XXXjwatt: is this the best thing to do nowadays?  Back when that mechanism
 * landed in bug 330498 we had two pass, recursive invalidation up the frame
 * tree, and I think reference loops were a problem.  Nowadays maybe a flag
 * on the SVGRenderingObserver objects to coalesce invalidations may work
 * better?
 *
 * InvalidateAll must be called before this object is destroyed, i.e.
 * before the referenced frame is destroyed. This should normally happen
 * via nsSVGContainerFrame::RemoveFrame, since only frames in the frame
 * tree should be referenced.
 */
class SVGRenderingObserverSet
{
public:
  SVGRenderingObserverSet()
    : mObservers(4)
  {
    MOZ_COUNT_CTOR(SVGRenderingObserverSet);
  }

  ~SVGRenderingObserverSet() {
    InvalidateAll();
    MOZ_COUNT_DTOR(SVGRenderingObserverSet);
  }

  void Add(SVGRenderingObserver* aObserver) {
    mObservers.PutEntry(aObserver);
  }
  void Remove(SVGRenderingObserver* aObserver) {
    mObservers.RemoveEntry(aObserver);
  }
#ifdef DEBUG
  bool Contains(SVGRenderingObserver* aObserver) {
    return (mObservers.GetEntry(aObserver) != nullptr);
  }
#endif
  bool IsEmpty() {
    return mObservers.IsEmpty();
  }

  /**
   * Drop all our observers, and notify them that we have changed and dropped
   * our reference to them.
   */
  void InvalidateAll();

  /**
   * Drop all observers that observe reflow, and notify them that we have changed and dropped
   * our reference to them.
   */
  void InvalidateAllForReflow();

  /**
   * Drop all our observers, and notify them that we have dropped our reference
   * to them.
   */
  void RemoveAll();

private:
  nsTHashtable<nsPtrHashKey<SVGRenderingObserver>> mObservers;
};

void
SVGRenderingObserverSet::InvalidateAll()
{
  if (mObservers.IsEmpty()) {
    return;
  }

  AutoTArray<SVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    observers.AppendElement(it.Get()->GetKey());
  }
  mObservers.Clear();

  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->OnNonDOMMutationRenderingChange();
  }
}

void
SVGRenderingObserverSet::InvalidateAllForReflow()
{
  if (mObservers.IsEmpty()) {
    return;
  }

  AutoTArray<SVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    SVGRenderingObserver* obs = it.Get()->GetKey();
    if (obs->ObservesReflow()) {
      observers.AppendElement(obs);
      it.Remove();
    }
  }

  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->OnNonDOMMutationRenderingChange();
  }
}

void
SVGRenderingObserverSet::RemoveAll()
{
  AutoTArray<SVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    observers.AppendElement(it.Get()->GetKey());
  }
  mObservers.Clear();

  // Our list is now cleared.  We need to notify the observers we've removed,
  // so they can update their state & remove themselves as mutation-observers.
  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->NotifyEvictedFromRenderingObserverSet();
  }
}


static SVGRenderingObserverSet*
GetObserverSet(Element* aElement)
{
  return static_cast<SVGRenderingObserverSet*>
    (aElement->GetProperty(nsGkAtoms::renderingobserverset));
}

#ifdef DEBUG
// Defined down here because we need SVGRenderingObserverSet's definition.
void
SVGRenderingObserver::DebugObserverSet()
{
  Element* referencedElement = GetReferencedElementWithoutObserving();
  if (referencedElement) {
    SVGRenderingObserverSet* observers = GetObserverSet(referencedElement);
    bool inObserverSet = observers && observers->Contains(this);
    MOZ_ASSERT(inObserverSet == mInObserverList,
      "failed to track whether we're in our referenced element's observer set!");
  } else {
    MOZ_ASSERT(!mInObserverList, "In whose observer set are we, then?");
  }
}
#endif


typedef nsInterfaceHashtable<nsRefPtrHashKey<URLAndReferrerInfo>,
                             nsIMutationObserver> URIObserverHashtable;

using PaintingPropertyDescriptor =
  const mozilla::FramePropertyDescriptor<nsSVGPaintingProperty>*;

void DestroyFilterProperty(SVGFilterObserverListForCSSProp* aProp)
{
  // SVGFilterObserverListForCSSProp is cycle-collected, so dropping the last
  // reference doesn't necessarily destroy it. We need to tell it that the
  // frame has now become invalid.
  aProp->DetachFromFrame();

  aProp->Release();
}

NS_DECLARE_FRAME_PROPERTY_RELEASABLE(HrefToTemplateProperty,
                                     SVGTemplateElementObserver)
NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(FilterProperty,
                                    SVGFilterObserverListForCSSProp,
                                    DestroyFilterProperty)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MaskProperty, SVGMaskObserverList)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(ClipPathProperty, nsSVGPaintingProperty)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MarkerStartProperty, SVGMarkerObserver)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MarkerMidProperty, SVGMarkerObserver)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(MarkerEndProperty, SVGMarkerObserver)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(FillProperty, nsSVGPaintingProperty)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(StrokeProperty, nsSVGPaintingProperty)
NS_DECLARE_FRAME_PROPERTY_RELEASABLE(HrefAsTextPathProperty,
                                     SVGTextPathObserver)
NS_DECLARE_FRAME_PROPERTY_DELETABLE(BackgroundImageProperty,
                                    URIObserverHashtable)

template<class T>
static T*
GetEffectProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<T>* aProperty)
{
  if (!aURI)
    return nullptr;

  T* prop = aFrame->GetProperty(aProperty);
  if (prop)
    return prop;
  prop = new T(aURI, aFrame, false);
  NS_ADDREF(prop);
  aFrame->SetProperty(aProperty, prop);
  return prop;
}

static nsSVGPaintingProperty*
GetPaintingProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<nsSVGPaintingProperty>* aProperty)
{
  return GetEffectProperty(aURI, aFrame, aProperty);
}


static already_AddRefed<URLAndReferrerInfo>
GetMarkerURI(nsIFrame* aFrame, RefPtr<css::URLValue> nsStyleSVG::* aMarker)
{
  return ResolveURLUsingLocalRef(aFrame, aFrame->StyleSVG()->*aMarker);
}

bool
SVGObserverUtils::GetAndObserveMarkers(nsIFrame* aMarkedFrame,
                                       nsSVGMarkerFrame*(*aFrames)[3])
{
  MOZ_ASSERT(!aMarkedFrame->GetPrevContinuation() &&
             aMarkedFrame->IsSVGGeometryFrame() &&
             static_cast<SVGGeometryElement*>(aMarkedFrame->GetContent())->IsMarkable(),
             "Bad frame");

  bool foundMarker = false;
  RefPtr<URLAndReferrerInfo> markerURL;
  SVGMarkerObserver* observer;
  nsIFrame* marker;

#define GET_MARKER(type)                                                      \
  markerURL = GetMarkerURI(aMarkedFrame, &nsStyleSVG::mMarker##type);         \
  observer = GetEffectProperty(markerURL, aMarkedFrame,                       \
                               Marker##type##Property());                     \
  marker = observer ?                                                         \
           observer->GetAndObserveReferencedFrame(LayoutFrameType::SVGMarker, \
                                                  nullptr) :                  \
           nullptr;                                                           \
  foundMarker = foundMarker || bool(marker);                                  \
  (*aFrames)[nsSVGMark::e##type] = static_cast<nsSVGMarkerFrame*>(marker);

  GET_MARKER(Start)
  GET_MARKER(Mid)
  GET_MARKER(End)

#undef GET_MARKER

  return foundMarker;
}

// Note that the returned list will be empty in the case of a 'filter' property
// that only specifies CSS filter functions (no url()'s to SVG filters).
static SVGFilterObserverListForCSSProp*
GetOrCreateFilterObserverListForCSS(nsIFrame* aFrame)
{
  MOZ_ASSERT(!aFrame->GetPrevContinuation(), "Require first continuation");

  const nsStyleEffects* effects = aFrame->StyleEffects();
  if (!effects->HasFilters()) {
    return nullptr;
  }
  SVGFilterObserverListForCSSProp* observers =
    aFrame->GetProperty(FilterProperty());
  if (observers) {
    return observers;
  }
  observers = new SVGFilterObserverListForCSSProp(effects->mFilters, aFrame);
  NS_ADDREF(observers);
  aFrame->SetProperty(FilterProperty(), observers);
  return observers;
}

static SVGObserverUtils::ReferenceState
GetAndObserveFilters(SVGFilterObserverListForCSSProp* aObserverList,
                     nsTArray<nsSVGFilterFrame*>* aFilterFrames)
{
  if (!aObserverList) {
    return SVGObserverUtils::eHasNoRefs;
  }

  const nsTArray<RefPtr<SVGFilterObserver>>& observers =
    aObserverList->GetObservers();
  if (observers.IsEmpty()) {
    return SVGObserverUtils::eHasNoRefs;
  }

  for (uint32_t i = 0; i < observers.Length(); i++) {
    nsSVGFilterFrame* filter = observers[i]->GetAndObserveFilterFrame();
    if (!filter) {
      if (aFilterFrames) {
        aFilterFrames->Clear();
      }
      return SVGObserverUtils::eHasRefsSomeInvalid;
    }
    if (aFilterFrames) {
      aFilterFrames->AppendElement(filter);
    }
  }

  return SVGObserverUtils::eHasRefsAllValid;
}

SVGObserverUtils::ReferenceState
SVGObserverUtils::GetAndObserveFilters(nsIFrame* aFilteredFrame,
                                       nsTArray<nsSVGFilterFrame*>* aFilterFrames)
{
  SVGFilterObserverListForCSSProp* observerList =
    GetOrCreateFilterObserverListForCSS(aFilteredFrame);
  return ::GetAndObserveFilters(observerList, aFilterFrames);
}

SVGObserverUtils::ReferenceState
SVGObserverUtils::GetFiltersIfObserving(nsIFrame* aFilteredFrame,
                                        nsTArray<nsSVGFilterFrame*>* aFilterFrames)
{
  SVGFilterObserverListForCSSProp* observerList =
    aFilteredFrame->GetProperty(FilterProperty());
  return ::GetAndObserveFilters(observerList, aFilterFrames);
}

already_AddRefed<nsISupports>
SVGObserverUtils::ObserveFiltersForCanvasContext(CanvasRenderingContext2D* aContext,
                                                 Element* aCanvasElement,
                                                 nsTArray<nsStyleFilter>& aFilters)
{
  return do_AddRef(new SVGFilterObserverListForCanvasContext(aContext,
                                                             aCanvasElement,
                                                             aFilters));
}

void
SVGObserverUtils::DetachFromCanvasContext(nsISupports* aAutoObserver)
{
  static_cast<SVGFilterObserverListForCanvasContext*>(aAutoObserver)->
    DetachFromContext();
}


static nsSVGPaintingProperty*
GetOrCreateClipPathObserver(nsIFrame* aClippedFrame)
{
  MOZ_ASSERT(!aClippedFrame->GetPrevContinuation(), "Require first continuation");

  const nsStyleSVGReset* svgStyleReset = aClippedFrame->StyleSVGReset();
  if (svgStyleReset->mClipPath.GetType() != StyleShapeSourceType::URL) {
    return nullptr;
  }
  const css::URLValue& url = svgStyleReset->mClipPath.URL();
  RefPtr<URLAndReferrerInfo> pathURI = ResolveURLUsingLocalRef(aClippedFrame, &url);
  return GetPaintingProperty(pathURI, aClippedFrame, ClipPathProperty());
}

SVGObserverUtils::ReferenceState
SVGObserverUtils::GetAndObserveClipPath(nsIFrame* aClippedFrame,
                                        nsSVGClipPathFrame** aClipPathFrame)
{
  if (aClipPathFrame) {
    *aClipPathFrame = nullptr;
  }
  nsSVGPaintingProperty* observers = GetOrCreateClipPathObserver(aClippedFrame);
  if (!observers) {
    return eHasNoRefs;
  }
  bool frameTypeOK = true;
  nsSVGClipPathFrame* frame = static_cast<nsSVGClipPathFrame*>(
    observers->GetAndObserveReferencedFrame(LayoutFrameType::SVGClipPath,
                                            &frameTypeOK));
  // Note that, unlike for filters, a reference to an ID that doesn't exist
  // is not invalid for clip-path or mask.
  if (!frameTypeOK || (frame && !frame->IsValid())) {
    return eHasRefsSomeInvalid;
  }
  if (aClipPathFrame) {
    *aClipPathFrame = frame;
  }
  return frame ? eHasRefsAllValid : eHasNoRefs;
}

static SVGMaskObserverList*
GetOrCreateMaskObserverList(nsIFrame* aMaskedFrame)
{
  MOZ_ASSERT(!aMaskedFrame->GetPrevContinuation(), "Require first continuation");

  const nsStyleSVGReset* style = aMaskedFrame->StyleSVGReset();
  if (!style->HasMask()) {
    return nullptr;
  }

  MOZ_ASSERT(style->mMask.mImageCount > 0);

  SVGMaskObserverList* prop =
    aMaskedFrame->GetProperty(MaskProperty());
  if (prop) {
    return prop;
  }
  prop = new SVGMaskObserverList(aMaskedFrame);
  NS_ADDREF(prop);
  aMaskedFrame->SetProperty(MaskProperty(), prop);
  return prop;
}

SVGObserverUtils::ReferenceState
SVGObserverUtils::GetAndObserveMasks(nsIFrame* aMaskedFrame,
                                     nsTArray<nsSVGMaskFrame*>* aMaskFrames)
{
  SVGMaskObserverList* observerList = GetOrCreateMaskObserverList(aMaskedFrame);
  if (!observerList) {
    return eHasNoRefs;
  }

  const nsTArray<RefPtr<nsSVGPaintingProperty>>& observers =
    observerList->GetObservers();
  if (observers.IsEmpty()) {
    return eHasNoRefs;
  }

  ReferenceState state = eHasRefsAllValid;

  for (size_t i = 0; i < observers.Length(); i++) {
    bool frameTypeOK = true;
    nsSVGMaskFrame* maskFrame = static_cast<nsSVGMaskFrame*>(
      observers[i]->GetAndObserveReferencedFrame(LayoutFrameType::SVGMask,
                                                 &frameTypeOK));
    MOZ_ASSERT(!maskFrame || frameTypeOK);
    // XXXjwatt: this looks fishy
    if (!frameTypeOK) {
      // We can not find the specific SVG mask resource in the downloaded SVG
      // document. There are two possibilities:
      // 1. The given resource id is invalid.
      // 2. The given resource id refers to a viewbox.
      //
      // Hand it over to the style image.
      observerList->ResolveImage(i);
      state = eHasRefsSomeInvalid;
    }
    if (aMaskFrames) {
      aMaskFrames->AppendElement(maskFrame);
    }
  }

  return state;
}

SVGGeometryElement*
SVGObserverUtils::GetAndObserveTextPathsPath(nsIFrame* aTextPathFrame)
{
  SVGTextPathObserver* property =
    aTextPathFrame->GetProperty(HrefAsTextPathProperty());

  if (!property) {
    nsIContent* content = aTextPathFrame->GetContent();
    nsAutoString href;
    static_cast<SVGTextPathElement*>(content)->HrefAsString(href);
    if (href.IsEmpty()) {
      return nullptr; // no URL
    }

    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = content->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              content->GetUncomposedDoc(), base);

    // There's no clear refererer policy spec about non-CSS SVG resource references
    // Bug 1415044 to investigate which referrer we should use
    RefPtr<URLAndReferrerInfo> target =
      new URLAndReferrerInfo(targetURI,
                             content->OwnerDoc()->GetDocumentURI(),
                             content->OwnerDoc()->GetReferrerPolicy());

    property = GetEffectProperty(target, aTextPathFrame,
                                 HrefAsTextPathProperty());
    if (!property) {
      return nullptr;
    }
  }

  Element* element = property->GetAndObserveReferencedElement();
  return (element && element->IsNodeOfType(nsINode::eSHAPE)) ?
    static_cast<SVGGeometryElement*>(element) : nullptr;
}

void
SVGObserverUtils::InitiateResourceDocLoads(nsIFrame* aFrame)
{
  // We create observer objects and attach them to aFrame, but we do not
  // make aFrame start observing the referenced frames.
  Unused << GetOrCreateFilterObserverListForCSS(aFrame);
  Unused << GetOrCreateClipPathObserver(aFrame);
  Unused << GetOrCreateMaskObserverList(aFrame);
}

void
SVGObserverUtils::RemoveTextPathObserver(nsIFrame* aTextPathFrame)
{
  aTextPathFrame->DeleteProperty(HrefAsTextPathProperty());
}

nsIFrame*
SVGObserverUtils::GetAndObserveTemplate(nsIFrame* aFrame,
                                        HrefToTemplateCallback aGetHref)
{
  SVGTemplateElementObserver* observer =
    aFrame->GetProperty(HrefToTemplateProperty());

  if (!observer) {
    nsAutoString href;
    aGetHref(href);
    if (href.IsEmpty()) {
      return nullptr; // no URL
    }

    // Convert href to an nsIURI
    nsIContent* content = aFrame->GetContent();
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = content->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              content->GetUncomposedDoc(), base);

    // There's no clear refererer policy spec about non-CSS SVG resource
    // references.  Bug 1415044 to investigate which referrer we should use.
    RefPtr<URLAndReferrerInfo> target =
      new URLAndReferrerInfo(targetURI,
                             content->OwnerDoc()->GetDocumentURI(),
                             content->OwnerDoc()->GetReferrerPolicy());

    observer = GetEffectProperty(target, aFrame, HrefToTemplateProperty());
  }

  return observer ? observer->GetAndObserveReferencedFrame() : nullptr;
}

void
SVGObserverUtils::RemoveTemplateObserver(nsIFrame* aFrame)
{
  aFrame->DeleteProperty(HrefToTemplateProperty());
}

Element*
SVGObserverUtils::GetAndObserveBackgroundImage(nsIFrame* aFrame,
                                               const nsAtom* aHref)
{
  URIObserverHashtable *hashtable =
    aFrame->GetProperty(BackgroundImageProperty());
  if (!hashtable) {
    hashtable = new URIObserverHashtable();
    aFrame->SetProperty(BackgroundImageProperty(), hashtable);
  }

  nsAutoString elementId =
    NS_LITERAL_STRING("#") + nsDependentAtomString(aHref);
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> base = aFrame->GetContent()->GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(
    getter_AddRefs(targetURI),
    elementId,
    aFrame->GetContent()->GetUncomposedDoc(),
    base);
  RefPtr<URLAndReferrerInfo> url = new URLAndReferrerInfo(
    targetURI,
    aFrame->GetContent()->OwnerDoc()->GetDocumentURI(),
    aFrame->GetContent()->OwnerDoc()->GetReferrerPolicy());

  // XXXjwatt: this is broken - we're using the address of a new
  // URLAndReferrerInfo as the hash key every time!
  nsSVGPaintingProperty* observer =
    static_cast<nsSVGPaintingProperty*>(hashtable->GetWeak(url));
  if (!observer) {
    observer = new nsSVGPaintingProperty(url, aFrame, /* aWatchImage */ true);
    hashtable->Put(url, observer);
  }
  return observer->GetAndObserveReferencedElement();
}

nsSVGPaintServerFrame *
SVGObserverUtils::GetAndObservePaintServer(nsIFrame* aTargetFrame,
                                           nsStyleSVGPaint nsStyleSVG::* aPaint)
{
  // If we're looking at a frame within SVG text, then we need to look up
  // to find the right frame to get the painting property off.  We should at
  // least look up past a text frame, and if the text frame's parent is the
  // anonymous block frame, then we look up to its parent (the SVGTextFrame).
  nsIFrame* frame = aTargetFrame;
  if (frame->GetContent()->IsText()) {
    frame = frame->GetParent();
    nsIFrame* grandparent = frame->GetParent();
    if (grandparent && grandparent->IsSVGTextFrame()) {
      frame = grandparent;
    }
  }

  const nsStyleSVG* svgStyle = frame->StyleSVG();
  if ((svgStyle->*aPaint).Type() != eStyleSVGPaintType_Server)
    return nullptr;

  RefPtr<URLAndReferrerInfo> paintServerURL =
    ResolveURLUsingLocalRef(frame,
                            (svgStyle->*aPaint).GetPaintServer());

  MOZ_ASSERT(aPaint == &nsStyleSVG::mFill || aPaint == &nsStyleSVG::mStroke);
  PaintingPropertyDescriptor propDesc = (aPaint == &nsStyleSVG::mFill) ?
                                        FillProperty() : StrokeProperty();
  nsSVGPaintingProperty *property =
    GetPaintingProperty(paintServerURL, frame, propDesc);
  if (!property)
    return nullptr;
  nsIFrame* result = property->GetAndObserveReferencedFrame();
  if (!result)
    return nullptr;

  LayoutFrameType type = result->Type();
  if (type != LayoutFrameType::SVGLinearGradient &&
      type != LayoutFrameType::SVGRadialGradient &&
      type != LayoutFrameType::SVGPattern)
    return nullptr;

  return static_cast<nsSVGPaintServerFrame*>(result);
}

void
SVGObserverUtils::UpdateEffects(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame->GetContent()->IsElement(),
               "aFrame's content should be an element");

  aFrame->DeleteProperty(FilterProperty());
  aFrame->DeleteProperty(MaskProperty());
  aFrame->DeleteProperty(ClipPathProperty());
  aFrame->DeleteProperty(MarkerStartProperty());
  aFrame->DeleteProperty(MarkerMidProperty());
  aFrame->DeleteProperty(MarkerEndProperty());
  aFrame->DeleteProperty(FillProperty());
  aFrame->DeleteProperty(StrokeProperty());
  aFrame->DeleteProperty(BackgroundImageProperty());

  // Ensure that the filter is repainted correctly
  // We can't do that in OnRenderingChange as the referenced frame may
  // not be valid
  GetOrCreateFilterObserverListForCSS(aFrame);

  if (aFrame->IsSVGGeometryFrame() &&
      static_cast<SVGGeometryElement*>(aFrame->GetContent())->IsMarkable()) {
    // Set marker properties here to avoid reference loops
    RefPtr<URLAndReferrerInfo> markerURL =
      GetMarkerURI(aFrame, &nsStyleSVG::mMarkerStart);
    GetEffectProperty(markerURL, aFrame, MarkerStartProperty());
    markerURL = GetMarkerURI(aFrame, &nsStyleSVG::mMarkerMid);
    GetEffectProperty(markerURL, aFrame, MarkerMidProperty());
    markerURL = GetMarkerURI(aFrame, &nsStyleSVG::mMarkerEnd);
    GetEffectProperty(markerURL, aFrame, MarkerEndProperty());
  }
}

void
SVGObserverUtils::AddRenderingObserver(Element* aElement,
                                       SVGRenderingObserver* aObserver)
{
  SVGRenderingObserverSet* observers = GetObserverSet(aElement);
  if (!observers) {
    observers = new SVGRenderingObserverSet();
    if (!observers) {
      return;
    }
    aElement->SetProperty(nsGkAtoms::renderingobserverset, observers,
                          nsINode::DeleteProperty<SVGRenderingObserverSet>);
  }
  aElement->SetHasRenderingObservers(true);
  observers->Add(aObserver);
}

void
SVGObserverUtils::RemoveRenderingObserver(Element* aElement,
                                          SVGRenderingObserver* aObserver)
{
  SVGRenderingObserverSet* observers = GetObserverSet(aElement);
  if (observers) {
    NS_ASSERTION(observers->Contains(aObserver),
                 "removing observer from an element we're not observing?");
    observers->Remove(aObserver);
    if (observers->IsEmpty()) {
      aElement->SetHasRenderingObservers(false);
    }
  }
}

void
SVGObserverUtils::RemoveAllRenderingObservers(Element* aElement)
{
  SVGRenderingObserverSet* observers = GetObserverSet(aElement);
  if (observers) {
    observers->RemoveAll();
    aElement->SetHasRenderingObservers(false);
  }
}

void
SVGObserverUtils::InvalidateRenderingObservers(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsElement())
    return;

  // If the rendering has changed, the bounds may well have changed too:
  aFrame->DeleteProperty(nsSVGUtils::ObjectBoundingBoxProperty());

  SVGRenderingObserverSet* observers = GetObserverSet(content->AsElement());
  if (observers) {
    observers->InvalidateAll();
    return;
  }

  // Check ancestor SVG containers. The root frame cannot be of type
  // eSVGContainer so we don't have to check f for null here.
  for (nsIFrame *f = aFrame->GetParent();
       f->IsFrameOfType(nsIFrame::eSVGContainer); f = f->GetParent()) {
    if (f->GetContent()->IsElement()) {
      observers = GetObserverSet(f->GetContent()->AsElement());
      if (observers) {
        observers->InvalidateAll();
        return;
      }
    }
  }
}

void
SVGObserverUtils::InvalidateDirectRenderingObservers(Element* aElement,
                                                     uint32_t aFlags /* = 0 */)
{
  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (frame) {
    // If the rendering has changed, the bounds may well have changed too:
    frame->DeleteProperty(nsSVGUtils::ObjectBoundingBoxProperty());
  }

  if (aElement->HasRenderingObservers()) {
    SVGRenderingObserverSet* observers = GetObserverSet(aElement);
    if (observers) {
      if (aFlags & INVALIDATE_REFLOW) {
        observers->InvalidateAllForReflow();
      } else {
        observers->InvalidateAll();
      }
    }
  }
}

void
SVGObserverUtils::InvalidateDirectRenderingObservers(nsIFrame* aFrame,
                                                     uint32_t aFlags /* = 0 */)
{
  nsIContent* content = aFrame->GetContent();
  if (content && content->IsElement()) {
    InvalidateDirectRenderingObservers(content->AsElement(), aFlags);
  }
}

already_AddRefed<nsIURI>
SVGObserverUtils::GetBaseURLForLocalRef(nsIContent* content, nsIURI* aDocURI)
{
  MOZ_ASSERT(content);

  // For a local-reference URL, resolve that fragment against the current
  // document that relative URLs are resolved against.
  nsCOMPtr<nsIURI> baseURI = content->OwnerDoc()->GetDocumentURI();

  nsCOMPtr<nsIURI> originalURI;
  // Content is in a shadow tree.  If this URL was specified in the subtree
  // referenced by the <use>(or -moz-binding) element, and that subtree came
  // from a separate resource document, then we want the fragment-only URL
  // to resolve to an element from the resource document.  Otherwise, the
  // URL was specified somewhere in the document with the <use> element, and
  // we want the fragment-only URL to resolve to an element in that document.
  if (SVGUseElement* use = content->GetContainingSVGUseShadowHost()) {
    originalURI = use->GetSourceDocURI();
  } else if (content->IsInAnonymousSubtree()) {
    nsIContent* bindingParent = content->GetBindingParent();

    if (bindingParent) {
      nsXBLBinding* binding = bindingParent->GetXBLBinding();
      if (binding) {
        originalURI = binding->GetSourceDocURI();
      } else {
        MOZ_ASSERT(content->IsInNativeAnonymousSubtree(),
                   "a non-native anonymous tree which is not from "
                   "an XBL binding?");
      }
    }
  }

  if (originalURI) {
    bool isEqualsExceptRef = false;
    aDocURI->EqualsExceptRef(originalURI, &isEqualsExceptRef);
    if (isEqualsExceptRef) {
      return originalURI.forget();
    }
  }

  return baseURI.forget();
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetFilterURI(nsIFrame* aFrame, const nsStyleFilter& aFilter)
{
  MOZ_ASSERT(aFrame->StyleEffects()->mFilters.Length());
  MOZ_ASSERT(aFilter.GetType() == NS_STYLE_FILTER_URL);

  return ResolveURLUsingLocalRef(aFrame, aFilter.GetURL());
}

} // namespace mozilla

