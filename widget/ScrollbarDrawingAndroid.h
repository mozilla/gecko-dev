/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawingAndroid_h
#define mozilla_widget_ScrollbarDrawingAndroid_h

#include "nsITheme.h"
#include "ScrollbarDrawing.h"

namespace mozilla::widget {

class ScrollbarDrawingAndroid final : public ScrollbarDrawing {
 public:
  ScrollbarDrawingAndroid() : ScrollbarDrawing(Kind::Android) {}
  virtual ~ScrollbarDrawingAndroid() = default;

  LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*, StyleAppearance,
                                           nsIFrame*) override;

  template <typename PaintBackendData>
  void DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect&,
                             ScrollbarKind, nsIFrame*, const ComputedStyle&,
                             const ElementState&, const Colors&,
                             const DPIRatio&);
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&, ScrollbarKind,
                           nsIFrame*, const ComputedStyle&, const ElementState&,
                           const Colors&, const DPIRatio&) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect&,
                           ScrollbarKind, nsIFrame*, const ComputedStyle&,
                           const ElementState&, const Colors&,
                           const DPIRatio&) override;

  void RecomputeScrollbarParams() override;

  bool ShouldDrawScrollbarButtons() override { return false; }
};

}  // namespace mozilla::widget

#endif
