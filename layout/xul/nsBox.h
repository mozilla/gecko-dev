/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBox_h___
#define nsBox_h___

#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "nsIFrame.h"

class nsITheme;

class nsBox : public nsIFrame {
 public:
  friend class nsIFrame;

  static void Shutdown();

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetXULFlex() override;
  virtual nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;

  virtual nsSize GetXULMinSizeForScrollArea(
      nsBoxLayoutState& aBoxLayoutState) override;

  virtual bool IsXULCollapsed() override;

  virtual void SetXULBounds(nsBoxLayoutState& aBoxLayoutState,
                            const nsRect& aRect,
                            bool aRemoveOverflowAreas = false) override;

  virtual nsresult GetXULBorder(nsMargin& aBorderAndPadding) override;
  virtual nsresult GetXULPadding(nsMargin& aBorderAndPadding) override;
  virtual nsresult GetXULMargin(nsMargin& aMargin) override;

  virtual Valignment GetXULVAlign() const override { return vAlign_Top; }
  virtual Halignment GetXULHAlign() const override { return hAlign_Left; }

  virtual nsresult XULRelayoutChildAtOrdinal(nsIFrame* aChild) override;

  nsBox(ClassID aID);
  virtual ~nsBox();

  /**
   * Returns true if this box clips its children, e.g., if this box is an
   * scrollbox.
   */
  virtual bool DoesClipChildren();
  virtual bool ComputesOwnOverflowArea() = 0;

  nsresult SyncLayout(nsBoxLayoutState& aBoxLayoutState);

  bool DoesNeedRecalc(const nsSize& aSize);
  bool DoesNeedRecalc(nscoord aCoord);
  void SizeNeedsRecalc(nsSize& aSize);
  void CoordNeedsRecalc(nscoord& aCoord);

  void AddBorderAndPadding(nsSize& aSize);

  static void AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize);
  static void AddMargin(nsIFrame* aChild, nsSize& aSize);
  static void AddMargin(nsSize& aSize, const nsMargin& aMargin);

  static nsSize BoundsCheckMinMax(const nsSize& aMinSize,
                                  const nsSize& aMaxSize);
  static nsSize BoundsCheck(const nsSize& aMinSize, const nsSize& aPrefSize,
                            const nsSize& aMaxSize);
  static nscoord BoundsCheck(nscoord aMinSize, nscoord aPrefSize,
                             nscoord aMaxSize);

  static nsIFrame* GetChildXULBox(const nsIFrame* aFrame);
  static nsIFrame* GetNextXULBox(const nsIFrame* aFrame);
  static nsIFrame* GetParentXULBox(const nsIFrame* aFrame);

 protected:
  nsresult BeginXULLayout(nsBoxLayoutState& aState);
  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState);
  nsresult EndXULLayout(nsBoxLayoutState& aState);

  static bool gGotTheme;
  static mozilla::StaticRefPtr<nsITheme> gTheme;

  enum eMouseThrough { unset, never, always };
};

#endif
