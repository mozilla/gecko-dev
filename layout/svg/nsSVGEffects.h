/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGEFFECTS_H_
#define NSSVGEFFECTS_H_

#include "mozilla/Attributes.h"
#include "FramePropertyTable.h"
#include "mozilla/dom/Element.h"
#include "nsHashKeys.h"
#include "nsID.h"
#include "nsIFrame.h"
#include "nsIMutationObserver.h"
#include "nsInterfaceHashtable.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsReferencedElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGUtils.h"
#include "nsTHashtable.h"
#include "nsURIHashKey.h"

class nsIAtom;
class nsIPresShell;
class nsIURI;
class nsSVGClipPathFrame;
class nsSVGPaintServerFrame;
class nsSVGFilterFrame;
class nsSVGMaskFrame;

/*
 * This interface allows us to be notified when a piece of SVG content is
 * re-rendered.
 *
 * Concrete implementations of this interface need to implement
 * "GetTarget()" to specify the piece of SVG content that they'd like to
 * monitor, and they need to implement "DoUpdate" to specify how we'll react
 * when that content gets re-rendered. They also need to implement a
 * constructor and destructor, which should call StartListening and
 * StopListening, respectively.
 */
class nsSVGRenderingObserver : public nsStubMutationObserver {

protected:
  virtual ~nsSVGRenderingObserver()
    {}

public:
  typedef mozilla::dom::Element Element;
  nsSVGRenderingObserver()
    : mInObserverList(false)
    {}

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  void InvalidateViaReferencedElement();

  // When a nsSVGRenderingObserver list gets forcibly cleared, it uses this
  // callback to notify every observer that's cleared from it, so they can
  // react.
  void NotifyEvictedFromRenderingObserverList();

  bool IsInObserverList() const { return mInObserverList; }

  nsIFrame* GetReferencedFrame();
  /**
   * @param aOK this is only for the convenience of callers. We set *aOK to false
   * if the frame is the wrong type
   */
  nsIFrame* GetReferencedFrame(nsIAtom* aFrameType, bool* aOK);

  Element* GetReferencedElement();

  virtual bool ObservesReflow() { return true; }

protected:
  // Non-virtual protected methods
  void StartListening();
  void StopListening();

  // Virtual protected methods
  virtual void DoUpdate() = 0; // called when the referenced resource changes.

  // This is an internally-used version of GetReferencedElement that doesn't
  // forcibly add us as an observer. (whereas GetReferencedElement does)
  virtual Element* GetTarget() = 0;

  // Whether we're in our referenced element's observer list at this time.
  bool mInObserverList;
};


/*
 * SVG elements reference supporting resources by element ID. We need to
 * track when those resources change and when the DOM changes in ways
 * that affect which element is referenced by a given ID (e.g., when
 * element IDs change). The code here is responsible for that.
 *
 * When a frame references a supporting resource, we create a property
 * object derived from nsSVGIDRenderingObserver to manage the relationship. The
 * property object is attached to the referencing frame.
 */
class nsSVGIDRenderingObserver : public nsSVGRenderingObserver {
public:
  typedef mozilla::dom::Element Element;
  nsSVGIDRenderingObserver(nsIURI* aURI, nsIFrame *aFrame,
                         bool aReferenceImage);
  virtual ~nsSVGIDRenderingObserver();

protected:
  Element* GetTarget() MOZ_OVERRIDE { return mElement.get(); }

  // This is called when the referenced resource changes.
  virtual void DoUpdate() MOZ_OVERRIDE;

  class SourceReference : public nsReferencedElement {
  public:
    SourceReference(nsSVGIDRenderingObserver* aContainer) : mContainer(aContainer) {}
  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) MOZ_OVERRIDE {
      mContainer->StopListening();
      nsReferencedElement::ElementChanged(aFrom, aTo);
      mContainer->StartListening();
      mContainer->DoUpdate();
    }
    /**
     * Override IsPersistent because we want to keep tracking the element
     * for the ID even when it changes.
     */
    virtual bool IsPersistent() MOZ_OVERRIDE { return true; }
  private:
    nsSVGIDRenderingObserver* mContainer;
  };

  SourceReference mElement;
  // The frame that this property is attached to
  nsIFrame *mFrame;
  // When a presshell is torn down, we don't delete the properties for
  // each frame until after the frames are destroyed. So here we remember
  // the presshell for the frames we care about and, before we use the frame,
  // we test the presshell to see if it's destroying itself. If it is,
  // then the frame pointer is not valid and we know the frame has gone away.
  nsIPresShell *mFramePresShell;
};

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
 * The nsSVGFilterProperty class manages a list of nsSVGFilterReferences.
 */
class nsSVGFilterReference :
  public nsSVGIDRenderingObserver, public nsISVGFilterReference {
public:
  nsSVGFilterReference(nsIURI *aURI, nsIFrame *aFilteredFrame)
    : nsSVGIDRenderingObserver(aURI, aFilteredFrame, false) {}

  bool ReferencesValidResource() { return GetFilterFrame(); }

  /**
   * @return the filter frame, or null if there is no filter frame
   */
  nsSVGFilterFrame *GetFilterFrame();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsISVGFilterReference
  virtual void Invalidate() MOZ_OVERRIDE { DoUpdate(); };

protected:
  virtual ~nsSVGFilterReference() {}

private:
  // nsSVGIDRenderingObserver
  virtual void DoUpdate() MOZ_OVERRIDE;
};

/**
 * This class manages a list of nsSVGFilterReferences, which represent SVG
 * reference filters in a filter chain.
 * e.g. filter: url(#svg-filter-1) blur(10px) url(#svg-filter-2);
 *
 * In the above example, the nsSVGFilterProperty will manage two
 * nsSVGFilterReferences, one for each SVG reference filter. CSS filters like
 * "blur(10px)" don't reference filter elements, so they don't need an
 * nsSVGFilterReference. The style system invalidates changes to CSS filters.
 */
class nsSVGFilterProperty : public nsISupports {
public:
  nsSVGFilterProperty(const nsTArray<nsStyleFilter> &aFilters,
                      nsIFrame *aFilteredFrame);

  const nsTArray<nsStyleFilter>& GetFilters() { return mFilters; }
  bool ReferencesValidResources();
  bool IsInObserverLists() const;
  void Invalidate();

  // nsISupports
  NS_DECL_ISUPPORTS

protected:
  virtual ~nsSVGFilterProperty();

private:
  nsTArray<nsSVGFilterReference*> mReferences;
  nsTArray<nsStyleFilter> mFilters;
};

class nsSVGMarkerProperty : public nsSVGIDRenderingObserver {
public:
  nsSVGMarkerProperty(nsIURI *aURI, nsIFrame *aFrame, bool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void DoUpdate() MOZ_OVERRIDE;
};

class nsSVGTextPathProperty : public nsSVGIDRenderingObserver {
public:
  nsSVGTextPathProperty(nsIURI *aURI, nsIFrame *aFrame, bool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFrame, aReferenceImage)
    , mValid(true) {}

  virtual bool ObservesReflow() MOZ_OVERRIDE { return false; }

protected:
  virtual void DoUpdate() MOZ_OVERRIDE;

private:
  /**
   * Returns true if the target of the textPath is the frame of a 'path' element.
   */
  bool TargetIsValid();

  bool mValid;
};

class nsSVGPaintingProperty : public nsSVGIDRenderingObserver {
public:
  nsSVGPaintingProperty(nsIURI *aURI, nsIFrame *aFrame, bool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void DoUpdate() MOZ_OVERRIDE;
};

/**
 * A manager for one-shot nsSVGRenderingObserver tracking.
 * nsSVGRenderingObservers can be added or removed. They are not strongly
 * referenced so an observer must be removed before it dies.
 * When InvalidateAll is called, all outstanding references get
 * InvalidateViaReferencedElement()
 * called on them and the list is cleared. The intent is that
 * the observer will force repainting of whatever part of the document
 * is needed, and then at paint time the observer will do a clean lookup
 * of the referenced element and [re-]add itself to the element's observer list.
 *
 * InvalidateAll must be called before this object is destroyed, i.e.
 * before the referenced frame is destroyed. This should normally happen
 * via nsSVGContainerFrame::RemoveFrame, since only frames in the frame
 * tree should be referenced.
 */
class nsSVGRenderingObserverList {
public:
  nsSVGRenderingObserverList()
    : mObservers(5)
  {
    MOZ_COUNT_CTOR(nsSVGRenderingObserverList);
  }

  ~nsSVGRenderingObserverList() {
    InvalidateAll();
    MOZ_COUNT_DTOR(nsSVGRenderingObserverList);
  }

  void Add(nsSVGRenderingObserver* aObserver)
  { mObservers.PutEntry(aObserver); }
  void Remove(nsSVGRenderingObserver* aObserver)
  { mObservers.RemoveEntry(aObserver); }
#ifdef DEBUG
  bool Contains(nsSVGRenderingObserver* aObserver)
  { return (mObservers.GetEntry(aObserver) != nullptr); }
#endif
  bool IsEmpty()
  { return mObservers.Count() == 0; }

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
  nsTHashtable<nsPtrHashKey<nsSVGRenderingObserver> > mObservers;
};

class nsSVGEffects {
public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::FramePropertyDescriptor FramePropertyDescriptor;
  typedef nsInterfaceHashtable<nsURIHashKey, nsIMutationObserver>
    URIObserverHashtable;

  static void DestroySupports(void* aPropertyValue)
  {
    (static_cast<nsISupports*>(aPropertyValue))->Release();
  }

  static void DestroyHashtable(void* aPropertyValue)
  {
    delete static_cast<URIObserverHashtable*> (aPropertyValue);
  }

  NS_DECLARE_FRAME_PROPERTY(FilterProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MaskProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(ClipPathProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MarkerBeginProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MarkerMiddleProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MarkerEndProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(FillProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(StrokeProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(HrefProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(BackgroundImageProperty, DestroyHashtable)

  /**
   * Get the paint server for a aTargetFrame.
   */
  static nsSVGPaintServerFrame *GetPaintServer(nsIFrame *aTargetFrame,
                                               const nsStyleSVGPaint *aPaint,
                                               const FramePropertyDescriptor *aProperty);

  struct EffectProperties {
    nsSVGFilterProperty*   mFilter;
    nsSVGPaintingProperty* mMask;
    nsSVGPaintingProperty* mClipPath;

    /**
     * @return the clip-path frame, or null if there is no clip-path frame
     * @param aOK if a clip-path was specified and the designated element
     * exists but is an element of the wrong type, *aOK is set to false.
     * Otherwise *aOK is untouched.
     */
    nsSVGClipPathFrame *GetClipPathFrame(bool *aOK);
    /**
     * @return the mask frame, or null if there is no mask frame
     * @param aOK if a mask was specified and the designated element
     * exists but is an element of the wrong type, *aOK is set to false.
     * Otherwise *aOK is untouched.
     */
    nsSVGMaskFrame *GetMaskFrame(bool *aOK);

    bool HasValidFilter() {
      return mFilter && mFilter->ReferencesValidResources();
    }

    bool HasNoFilterOrHasValidFilter() {
      return !mFilter || mFilter->ReferencesValidResources();
    }
  };

  /**
   * @param aFrame should be the first continuation
   */
  static EffectProperties GetEffectProperties(nsIFrame *aFrame);

  /**
   * Called when changes to an element (e.g. CSS property changes) cause its
   * frame to start/stop referencing (or reference different) SVG resource
   * elements. (_Not_ called for changes to referenced resource elements.)
   *
   * This function handles such changes by discarding _all_ the frame's SVG
   * effects frame properties (causing those properties to stop watching their
   * target element). It also synchronously (re)creates the filter and marker
   * frame properties (XXX why not the other properties?), which makes it
   * useful for initializing those properties during first reflow.
   *
   * XXX rename to something more meaningful like RefreshResourceReferences?
   */
  static void UpdateEffects(nsIFrame *aFrame);

  /**
   * @param aFrame should be the first continuation
   */
  static nsSVGFilterProperty *GetFilterProperty(nsIFrame *aFrame);

  /**
   * @param aFrame must be a first-continuation.
   */
  static void AddRenderingObserver(Element *aElement, nsSVGRenderingObserver *aObserver);
  /**
   * @param aFrame must be a first-continuation.
   */
  static void RemoveRenderingObserver(Element *aElement, nsSVGRenderingObserver *aObserver);

  /**
   * Removes all rendering observers from aElement.
   */
  static void RemoveAllRenderingObservers(Element *aElement);

  /**
   * This can be called on any frame. We invalidate the observers of aFrame's
   * element, if any, or else walk up to the nearest observable SVG parent
   * frame with observers and invalidate them instead.
   *
   * Note that this method is very different to e.g.
   * nsNodeUtils::AttributeChanged which walks up the content node tree all the
   * way to the root node (not stopping if it encounters a non-container SVG
   * node) invalidating all mutation observers (not just
   * nsSVGRenderingObservers) on all nodes along the way (not just the first
   * node it finds with observers). In other words, by doing all the
   * things in parentheses in the preceding sentence, this method uses
   * knowledge about our implementation and what can be affected by SVG effects
   * to make invalidation relatively lightweight when an SVG effect changes.
   */
  static void InvalidateRenderingObservers(nsIFrame *aFrame);

  enum {
    INVALIDATE_REFLOW = 1
  };

  /**
   * This can be called on any element or frame. Only direct observers of this
   * (frame's) element, if any, are invalidated.
   */
  static void InvalidateDirectRenderingObservers(Element *aElement, uint32_t aFlags = 0);
  static void InvalidateDirectRenderingObservers(nsIFrame *aFrame, uint32_t aFlags = 0);

  /**
   * Get an nsSVGMarkerProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGMarkerProperty *
  GetMarkerProperty(nsIURI *aURI, nsIFrame *aFrame,
                    const FramePropertyDescriptor *aProperty);
  /**
   * Get an nsSVGTextPathProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGTextPathProperty *
  GetTextPathProperty(nsIURI *aURI, nsIFrame *aFrame,
                      const FramePropertyDescriptor *aProperty);
  /**
   * Get an nsSVGPaintingProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGPaintingProperty *
  GetPaintingProperty(nsIURI *aURI, nsIFrame *aFrame,
                      const FramePropertyDescriptor *aProperty);
  /**
   * Get an nsSVGPaintingProperty for the frame for that URI, creating a fresh
   * one if necessary
   */
  static nsSVGPaintingProperty *
  GetPaintingPropertyForURI(nsIURI *aURI, nsIFrame *aFrame,
                            const FramePropertyDescriptor *aProp);
};

#endif /*NSSVGEFFECTS_H_*/
