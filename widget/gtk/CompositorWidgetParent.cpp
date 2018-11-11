/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetParent.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace widget {

CompositorWidgetParent::CompositorWidgetParent(const CompositorWidgetInitData& aInitData)
 : X11CompositorWidget(aInitData)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
}

CompositorWidgetParent::~CompositorWidgetParent()
{
}

void
CompositorWidgetParent::ObserveVsync(VsyncObserver* aObserver)
{
  if (aObserver) {
    Unused << SendObserveVsync();
  } else {
    Unused << SendUnobserveVsync();
  }
  mVsyncObserver = aObserver;
}

RefPtr<VsyncObserver>
CompositorWidgetParent::GetVsyncObserver() const
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  return mVsyncObserver;
}

bool
CompositorWidgetParent::RecvNotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize)
{
  NotifyClientSizeChanged(aClientSize);
  return true;
}

} // namespace widget
} // namespace mozilla
