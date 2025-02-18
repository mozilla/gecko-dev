/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperUIKit.h"

#import <UIKit/UIScreen.h>

#include "mozilla/Logging.h"
#include "nsObjCExceptions.h"

using namespace mozilla;

static LazyLogModule sScreenLog("WidgetScreen");

namespace mozilla {
namespace widget {

ScreenHelperUIKit::ScreenHelperUIKit() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  MOZ_LOG(sScreenLog, LogLevel::Debug, ("ScreenHelperUIKit created"));

  RefreshScreens();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static already_AddRefed<Screen> MakeScreen(UIScreen* aScreen) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  DesktopToLayoutDeviceScale contentsScaleFactor([aScreen scale]);
  CSSToLayoutDeviceScale defaultCssScaleFactor(contentsScaleFactor.scale);
  CGRect bounds = [[aScreen fixedCoordinateSpace] bounds];
  LayoutDeviceIntRect rect;
  rect.x = NSToIntRound(bounds.origin.x * contentsScaleFactor.scale);
  rect.y = NSToIntRound(bounds.origin.y * contentsScaleFactor.scale);
  rect.width = NSToIntRound((bounds.origin.x + bounds.size.width) *
                            contentsScaleFactor.scale) -
               rect.x;
  rect.height = NSToIntRound((bounds.size.height - bounds.origin.y) *
                             contentsScaleFactor.scale) -
                rect.y;

  // Modern iOS does support HDR, but it's not straightforward to get it given a
  // screen.
  uint32_t pixelDepth = 24;

  float dpi = 96.0f;
  MOZ_LOG(sScreenLog, LogLevel::Debug,
          ("New screen [%d %d %d %d (%d %d %d %d) %d %f %f %f]", rect.x, rect.y,
           rect.width, rect.height, rect.x, rect.y, rect.width, rect.height,
           pixelDepth, contentsScaleFactor.scale, defaultCssScaleFactor.scale,
           dpi));

  NSInteger fps = [aScreen maximumFramesPerSecond];
  RefPtr<Screen> screen =
      new Screen(rect, rect, pixelDepth, pixelDepth, fps, contentsScaleFactor,
                 defaultCssScaleFactor, dpi, Screen::IsPseudoDisplay::No,
                 Screen::IsHDR::No);
  return screen.forget();

  NS_OBJC_END_TRY_BLOCK_RETURN(nullptr);
}

void ScreenHelperUIKit::RefreshScreens() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refreshing screens"));

  AutoTArray<RefPtr<Screen>, 1> screens;
  screens.AppendElement(MakeScreen([UIScreen mainScreen]));

  ScreenManager::Refresh(std::move(screens));

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

}  // namespace widget
}  // namespace mozilla
