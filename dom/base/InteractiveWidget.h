/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INTERACTIVE_WIDGET_H_
#define DOM_INTERACTIVE_WIDGET_H_

#include "mozilla/StaticPrefs_dom.h"

namespace mozilla::dom {
// https://drafts.csswg.org/css-viewport/#interactive-widget-section
enum class InteractiveWidget : uint8_t {
  OverlaysContent,
  ResizesContent,
  ResizesVisual,
};

class InteractiveWidgetUtils {
 public:
  static InteractiveWidget DefaultInteractiveWidgetMode() {
    return StaticPrefs::dom_interactive_widget_default_resizes_visual()
               ? InteractiveWidget::ResizesVisual
               : InteractiveWidget::ResizesContent;
  }
};

}  // namespace mozilla::dom

#endif  // DOM_INTERACTIVE_WIDGET_H_
