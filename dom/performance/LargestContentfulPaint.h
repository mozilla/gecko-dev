/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LargestContentfulPaint_h___
#define mozilla_dom_LargestContentfulPaint_h___

#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PerformanceEntry.h"
#include "mozilla/dom/PerformanceLargestContentfulPaintBinding.h"

#include "imgRequestProxy.h"

class nsTextFrame;
namespace mozilla::dom {

static constexpr nsLiteralString kLargestContentfulPaintName =
    u"largest-contentful-paint"_ns;

class Performance;
class PerformanceMainThread;

struct LCPTextFrameHelper final {
  static void MaybeUnionTextFrame(nsTextFrame* aTextFrame,
                                  const nsRect& aRelativeToSelfRect);
};

class ImagePendingRendering final {
 public:
  ImagePendingRendering(Element* aElement, imgRequestProxy* aImgRequestProxy,
                        const TimeStamp& aLoadTime)
      : mElement(do_GetWeakReference(aElement)),
        mImageRequestProxy(aImgRequestProxy),
        mLoadTime(aLoadTime) {}

  Element* GetElement() const {
    nsCOMPtr<Element> element = do_QueryReferent(mElement);
    return element;
  }

  imgRequestProxy* GetImgRequestProxy() const {
    return static_cast<imgRequestProxy*>(mImageRequestProxy.get());
  }

  nsWeakPtr mElement;
  WeakPtr<PreloaderBase> mImageRequestProxy;
  TimeStamp mLoadTime;
};

class ContentIdentifierHashEntry : public PLDHashEntryHdr {
 public:
  using KeyType = const Element*;
  using KeyTypePointer = const Element*;

  explicit ContentIdentifierHashEntry(KeyTypePointer aKey) : mElement(aKey) {}

  ContentIdentifierHashEntry(ContentIdentifierHashEntry&&) = default;

  ~ContentIdentifierHashEntry() = default;

  bool KeyEquals(KeyTypePointer aKey) const { return mElement == aKey; }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashGeneric(reinterpret_cast<uintptr_t>(aKey));
  }

  // mImageRequestProxies isn't memmoveable.
  enum { ALLOW_MEMMOVE = false };

  AutoTArray<WeakPtr<PreloaderBase>, 1> mImageRequestProxies;

 private:
  // Raw pointer; Element::UnbindFromTree will delete this entry to make
  // sure mElement is always valid.
  const Element* mElement;
};

class LCPHelpers final {
 public:
  // Called when the size of the image is known.
  static void FinalizeLCPEntryForImage(Element* aContainingBlock,
                                       imgRequestProxy* aImgRequestProxy,
                                       const nsRect& aTargetRectRelativeToSelf);

  static void FinalizeLCPEntryForText(PerformanceMainThread* aPerformance,
                                      const TimeStamp& aRenderTime,
                                      Element* aContainingBlock,
                                      const nsRect& aTargetRectRelativeToSelf,
                                      const nsPresContext* aPresContext);

  static bool IsQualifiedImageRequest(imgRequest* aRequest,
                                      Element* aContainingElement);

 private:
  static bool CanFinalizeLCPEntry(const nsIFrame* aFrame);
};

// https://w3c.github.io/largest-contentful-paint/
class LargestContentfulPaint final : public PerformanceEntry {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LargestContentfulPaint,
                                           PerformanceEntry)

  LargestContentfulPaint(PerformanceMainThread* aPerformance,
                         const TimeStamp& aRenderTime,
                         const Maybe<TimeStamp>& aLoadTime,
                         const unsigned long aSize, nsIURI* aURI,
                         Element* aElement, bool aShouldExposeRenderTime);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp RenderTime() const;
  DOMHighResTimeStamp LoadTime() const;
  DOMHighResTimeStamp StartTime() const override;

  unsigned long Size() const { return mSize; }
  void GetId(nsAString& aId) const {
    if (mId) {
      mId->ToString(aId);
    }
  }
  void GetUrl(nsAString& aUrl);

  Element* GetElement() const;

  static Element* GetContainingBlockForTextFrame(const nsTextFrame* aTextFrame);

  void UpdateSize(const Element* aContainingBlock,
                  const nsRect& aTargetRectRelativeToSelf,
                  const PerformanceMainThread* aPerformance, bool aIsImage);

  void BufferEntryIfNeeded() override;

  static void MaybeProcessImageForElementTiming(imgRequestProxy* aRequest,
                                                Element* aElement);

  void QueueEntry();

 private:
  ~LargestContentfulPaint() = default;

  void ReportLCPToNavigationTimings();

  RefPtr<PerformanceMainThread> mPerformance;

  // This is always set but only exposed to web content if
  // mShouldExposeRenderTime is true.
  const TimeStamp mRenderTime;
  const Maybe<TimeStamp> mLoadTime;
  // This is set to false when for security reasons web content it not allowed
  // to see the RenderTime.
  const bool mShouldExposeRenderTime;
  unsigned long mSize;
  nsCOMPtr<nsIURI> mURI;

  nsWeakPtr mElement;
  RefPtr<nsAtom> mId;
};
}  // namespace mozilla::dom
#endif
