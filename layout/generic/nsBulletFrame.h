/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"

#include "imgINotificationObserver.h"

class imgIContainer;
class imgRequestProxy;

class nsBulletFrame;

class nsBulletListener : public imgINotificationObserver
{
public:
  nsBulletListener();
  virtual ~nsBulletListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(nsBulletFrame *frame) { mFrame = frame; }

private:
  nsBulletFrame *mFrame;
};

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsBulletFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
  {
  }
  virtual ~nsBulletFrame();

  NS_IMETHOD Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData);

  // nsIFrame
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;
#ifdef DEBUG_FRAME_DUMP
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus) MOZ_OVERRIDE;
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  // nsBulletFrame
  int32_t SetListItemOrdinal(int32_t aNextOrdinal, bool* aChanged,
                             int32_t aIncrement);


  /* get list item text, without '.' */
  static void AppendCounterText(int32_t aListStyleType,
                                int32_t aOrdinal,
                                nsString& aResult,
                                bool& aIsRTL);

  /* get suffix of list item */
  static void GetListItemSuffix(int32_t aListStyleType,
                                nsString& aResult,
                                bool& aSuppressPadding);

  /* get list item text, with '.' */
  void GetListItemText(const nsStyleList& aStyleList, nsString& aResult);
                         
  void PaintBullet(nsRenderingContext& aRenderingContext, nsPoint aPt,
                   const nsRect& aDirtyRect, uint32_t aFlags);
  
  virtual bool IsEmpty() MOZ_OVERRIDE;
  virtual bool IsSelfEmpty() MOZ_OVERRIDE;
  virtual nscoord GetBaseline() const MOZ_OVERRIDE;

  float GetFontSizeInflation() const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & BULLET_FRAME_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  int32_t GetOrdinal() { return mOrdinal; }

  already_AddRefed<imgIContainer> GetImage() const;

protected:
  nsresult OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);

  void GetDesiredSize(nsPresContext* aPresContext,
                      nsRenderingContext *aRenderingContext,
                      nsHTMLReflowMetrics& aMetrics,
                      float aFontSizeInflation);

  void GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup);

  nsMargin mPadding;
  nsRefPtr<imgRequestProxy> mImageRequest;
  nsRefPtr<nsBulletListener> mListener;

  nsSize mIntrinsicSize;
  int32_t mOrdinal;
  bool mTextIsRTL;

  // If set to true, any padding of bullet defined in the UA style sheet will
  // be suppressed.  This is used for some CJK numbering styles where extra
  // space after the suffix is not desired.  Note that, any author-specified
  // padding overriding the default style will NOT be suppressed.
  bool mSuppressPadding;

private:

  // This is a boolean flag indicating whether or not the current image request
  // has been registered with the refresh driver.
  bool mRequestRegistered;
};

#endif /* nsBulletFrame_h___ */
