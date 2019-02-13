/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameContainer.h"

#include "mozilla/dom/HTMLMediaElement.h"
#include "nsIFrame.h"
#include "nsDisplayList.h"
#include "nsSVGEffects.h"
#include "ImageContainer.h"

using namespace mozilla::layers;

namespace mozilla {

VideoFrameContainer::VideoFrameContainer(dom::HTMLMediaElement* aElement,
                                         already_AddRefed<ImageContainer> aContainer)
  : mElement(aElement),
    mImageContainer(aContainer), mMutex("nsVideoFrameContainer"),
    mIntrinsicSizeChanged(false), mImageSizeChanged(false)
{
  NS_ASSERTION(aElement, "aElement must not be null");
  NS_ASSERTION(mImageContainer, "aContainer must not be null");
}

VideoFrameContainer::~VideoFrameContainer()
{}

void VideoFrameContainer::SetCurrentFrame(const gfxIntSize& aIntrinsicSize,
                                          Image* aImage,
                                          TimeStamp aTargetTime)
{
  MutexAutoLock lock(mMutex);

  if (aIntrinsicSize != mIntrinsicSize) {
    mIntrinsicSize = aIntrinsicSize;
    mIntrinsicSizeChanged = true;
  }

  gfx::IntSize oldFrameSize = mImageContainer->GetCurrentSize();
  TimeStamp lastPaintTime = mImageContainer->GetPaintTime();
  if (!lastPaintTime.IsNull() && !mPaintTarget.IsNull()) {
    mPaintDelay = lastPaintTime - mPaintTarget;
  }

  // When using the OMX decoder, destruction of the current image can indirectly
  //  block on main thread I/O. If we let this happen while holding onto
  //  |mImageContainer|'s lock, then when the main thread then tries to
  //  composite it can then block on |mImageContainer|'s lock, causing a
  //  deadlock. We use this hack to defer the destruction of the current image
  //  until it is safe.
  nsRefPtr<Image> kungFuDeathGrip;
  kungFuDeathGrip = mImageContainer->LockCurrentImage();
  mImageContainer->UnlockCurrentImage();

  mImageContainer->SetCurrentImage(aImage);
  gfx::IntSize newFrameSize = mImageContainer->GetCurrentSize();
  if (oldFrameSize != newFrameSize) {
    mImageSizeChanged = true;
  }

  mPaintTarget = aTargetTime;
}

void VideoFrameContainer::Reset()
{
  ClearCurrentFrame(true);
  Invalidate();
  mIntrinsicSize = gfxIntSize(-1, -1);
  mPaintDelay = mozilla::TimeDuration();
  mPaintTarget = mozilla::TimeStamp();
  mImageContainer->ResetPaintCount();
}

void VideoFrameContainer::ClearCurrentFrame(bool aResetSize)
{
  MutexAutoLock lock(mMutex);

  // See comment in SetCurrentFrame for the reasoning behind
  // using a kungFuDeathGrip here.
  nsRefPtr<Image> kungFuDeathGrip;
  kungFuDeathGrip = mImageContainer->LockCurrentImage();
  mImageContainer->UnlockCurrentImage();

  mImageContainer->ClearAllImages();
  mImageSizeChanged = aResetSize;
}

ImageContainer* VideoFrameContainer::GetImageContainer() {
  return mImageContainer;
}


double VideoFrameContainer::GetFrameDelay()
{
  MutexAutoLock lock(mMutex);
  return mPaintDelay.ToSeconds();
}

void VideoFrameContainer::InvalidateWithFlags(uint32_t aFlags)
{
  NS_ASSERTION(NS_IsMainThread(), "Must call on main thread");

  if (!mElement) {
    // Element has been destroyed
    return;
  }

  nsIFrame* frame = mElement->GetPrimaryFrame();
  bool invalidateFrame = false;

  {
    MutexAutoLock lock(mMutex);

    // Get mImageContainerSizeChanged while holding the lock.
    invalidateFrame = mImageSizeChanged;
    mImageSizeChanged = false;

    if (mIntrinsicSizeChanged) {
      mElement->UpdateMediaSize(mIntrinsicSize);
      mIntrinsicSizeChanged = false;

      if (frame) {
        nsPresContext* presContext = frame->PresContext();
        nsIPresShell *presShell = presContext->PresShell();
        presShell->FrameNeedsReflow(frame,
                                    nsIPresShell::eStyleChange,
                                    NS_FRAME_IS_DIRTY);
      }
    }
  }

  bool asyncInvalidate = mImageContainer &&
                         mImageContainer->IsAsync() &&
                         !(aFlags & INVALIDATE_FORCE);

  if (frame) {
    if (invalidateFrame) {
      frame->InvalidateFrame();
    } else {
      frame->InvalidateLayer(nsDisplayItem::TYPE_VIDEO, nullptr, nullptr,
                             asyncInvalidate ? nsIFrame::UPDATE_IS_ASYNC : 0);
    }
  }

  nsSVGEffects::InvalidateDirectRenderingObservers(mElement);
}

}
