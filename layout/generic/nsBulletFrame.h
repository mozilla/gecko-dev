/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"

#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "imgIOnloadBlocker.h"

class imgIContainer;
class imgRequestProxy;

class nsBulletFrame;

class nsBulletListener final : public imgINotificationObserver,
                               public imgIOnloadBlocker
{
public:
  nsBulletListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_IMGIONLOADBLOCKER

  void SetFrame(nsBulletFrame *frame) { mFrame = frame; }

private:
  virtual ~nsBulletListener();

  nsBulletFrame *mFrame;
};

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame final : public nsFrame {
  typedef mozilla::image::DrawResult DrawResult;

public:
  NS_DECL_FRAMEARENA_HELPERS
#ifdef DEBUG
  NS_DECL_QUERYFRAME_TARGET(nsBulletFrame)
  NS_DECL_QUERYFRAME
#endif

  explicit nsBulletFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
    , mPadding(GetWritingMode())
    , mIntrinsicSize(GetWritingMode())
    , mRequestRegistered(false)
    , mBlockingOnload(false)
  { }
  virtual ~nsBulletFrame();

  NS_IMETHOD Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData);
  NS_IMETHOD BlockOnload(imgIRequest* aRequest);
  NS_IMETHOD UnblockOnload(imgIRequest* aRequest);

  // nsIFrame
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;
  virtual nsIAtom* GetType() const override;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsIHTMLReflow
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) override;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;

  // nsBulletFrame
  int32_t SetListItemOrdinal(int32_t aNextOrdinal, bool* aChanged,
                             int32_t aIncrement);

  /* get list item text, with prefix & suffix */
  void GetListItemText(nsAString& aResult);

  void GetSpokenText(nsAString& aText);
                         
  DrawResult PaintBullet(nsRenderingContext& aRenderingContext, nsPoint aPt,
                         const nsRect& aDirtyRect, uint32_t aFlags);
  
  virtual bool IsEmpty() override;
  virtual bool IsSelfEmpty() override;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

  float GetFontSizeInflation() const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & BULLET_FRAME_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  int32_t GetOrdinal() { return mOrdinal; }

  already_AddRefed<imgIContainer> GetImage() const;

protected:
  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);

  void AppendSpacingToPadding(nsFontMetrics* aFontMetrics,
                              mozilla::LogicalMargin* aPadding);
  void GetDesiredSize(nsPresContext* aPresContext,
                      nsRenderingContext *aRenderingContext,
                      nsHTMLReflowMetrics& aMetrics,
                      float aFontSizeInflation,
                      mozilla::LogicalMargin* aPadding);

  void GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup);
  nsIDocument* GetOurCurrentDoc() const;

  mozilla::LogicalMargin mPadding;
  nsRefPtr<imgRequestProxy> mImageRequest;
  nsRefPtr<nsBulletListener> mListener;

  mozilla::LogicalSize mIntrinsicSize;
  int32_t mOrdinal;

private:
  void RegisterImageRequest(bool aKnownToBeAnimated);
  void DeregisterAndCancelImageRequest();

  // This is a boolean flag indicating whether or not the current image request
  // has been registered with the refresh driver.
  bool mRequestRegistered : 1;

  // Whether we're currently blocking onload.
  bool mBlockingOnload : 1;
};

#endif /* nsBulletFrame_h___ */
