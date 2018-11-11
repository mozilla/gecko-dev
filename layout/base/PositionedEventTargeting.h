/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PositionedEventTargeting_h
#define mozilla_PositionedEventTargeting_h

#include <stdint.h>
#include "mozilla/EventForwards.h"

class nsIFrame;
struct nsPoint;

namespace mozilla {

enum {
  INPUT_IGNORE_ROOT_SCROLL_FRAME = 0x01
};
/**
 * Finds the target frame for a pointer event given the event type and location.
 * This can look for frames within a rectangle surrounding the actual location
 * that are suitable targets, to account for inaccurate pointing devices.
 */
nsIFrame*
FindFrameTargetedByInputEvent(WidgetGUIEvent* aEvent,
                              nsIFrame* aRootFrame,
                              const nsPoint& aPointRelativeToRootFrame,
                              uint32_t aFlags = 0);

} // namespace mozilla

#endif /* mozilla_PositionedEventTargeting_h */
