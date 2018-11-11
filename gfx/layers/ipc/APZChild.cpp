/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/GeckoContentController.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"

#include "InputData.h" // for InputData

namespace mozilla {
namespace layers {

APZChild::APZChild(RefPtr<GeckoContentController> aController)
  : mController(aController)
{
  MOZ_ASSERT(mController);
}

APZChild::~APZChild()
{
  if (mController) {
    mController->Destroy();
    mController = nullptr;
  }
}

bool
APZChild::RecvRequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(mController->IsRepaintThread());

  mController->RequestContentRepaint(aFrameMetrics);
  return true;
}

bool
APZChild::RecvUpdateOverscrollVelocity(const float& aX, const float& aY, const bool& aIsRootContent)
{
  mController->UpdateOverscrollVelocity(aX, aY, aIsRootContent);
  return true;
}

bool
APZChild::RecvUpdateOverscrollOffset(const float& aX, const float& aY, const bool& aIsRootContent)
{
  mController->UpdateOverscrollOffset(aX, aY, aIsRootContent);
  return true;
}

bool
APZChild::RecvSetScrollingRootContent(const bool& aIsRootContent)
{
  mController->SetScrollingRootContent(aIsRootContent);
  return true;
}

bool
APZChild::RecvNotifyMozMouseScrollEvent(const ViewID& aScrollId,
                                        const nsString& aEvent)
{
  mController->NotifyMozMouseScrollEvent(aScrollId, aEvent);
  return true;
}

bool
APZChild::RecvNotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                   const APZStateChange& aChange,
                                   const int& aArg)
{
  mController->NotifyAPZStateChange(aGuid, aChange, aArg);
  return true;
}

bool
APZChild::RecvNotifyFlushComplete()
{
  MOZ_ASSERT(mController->IsRepaintThread());

  mController->NotifyFlushComplete();
  return true;
}

bool
APZChild::RecvDestroy()
{
  // mController->Destroy will be called in the destructor
  PAPZChild::Send__delete__(this);
  return true;
}


} // namespace layers
} // namespace mozilla
