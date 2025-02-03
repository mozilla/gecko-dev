/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessWinCompositorWidget.h"

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "gfxPlatform.h"
#include "HeadlessCompositorWidget.h"
#include "HeadlessWidget.h"
#include "nsIWidget.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"
#include "WinCompositorWindowThread.h"

#include <ddraw.h>

namespace mozilla::widget {

using namespace mozilla::gfx;
using namespace mozilla;

/* static */
RefPtr<CompositorWidget> CompositorWidget::CreateLocal(
    const CompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsIWidget* aWidget) {
  if (aInitData.type() ==
      CompositorWidgetInitData::THeadlessCompositorWidgetInitData) {
    return new HeadlessCompositorWidget(
        aInitData.get_HeadlessCompositorWidgetInitData(), aOptions,
        static_cast<HeadlessWidget*>(aWidget));
  }
  return new InProcessWinCompositorWidget(
      aInitData.get_WinCompositorWidgetInitData(), aOptions,
      static_cast<nsWindow*>(aWidget));
}

InProcessWinCompositorWidget::InProcessWinCompositorWidget(
    const WinCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsWindow* aWindow)
    : WinCompositorWidget(aInitData, aOptions),
      mWindow(aWindow),
      mWnd(reinterpret_cast<HWND>(aInitData.hWnd())),
      mCompositeDC(nullptr),
      mLockedBackBufferData(nullptr) {
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));
}

void InProcessWinCompositorWidget::OnDestroyWindow() {
  gfx::CriticalSectionAutoEnter presentLock(&mPresentLock);
}

bool InProcessWinCompositorWidget::OnWindowResize(
    const LayoutDeviceIntSize& aSize) {
  return true;
}

bool InProcessWinCompositorWidget::PreRender(WidgetRenderingContext* aContext) {
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void InProcessWinCompositorWidget::PostRender(
    WidgetRenderingContext* aContext) {
  mPresentLock.Leave();
}

LayoutDeviceIntSize InProcessWinCompositorWidget::GetClientSize() {
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return LayoutDeviceIntSize();
  }
  return LayoutDeviceIntSize(r.right - r.left, r.bottom - r.top);
}

already_AddRefed<gfx::DrawTarget>
InProcessWinCompositorWidget::StartRemoteDrawing() {
  MOZ_ASSERT(!mCompositeDC);

  // Must call this after EnsureTransparentSurface(), since it could update
  // the DC.
  HDC dc = GetWindowSurface();
  if (!dc) {
    return nullptr;
  }
  uint32_t flags = TransparencyModeIs(TransparencyMode::Opaque)
                       ? 0
                       : gfxWindowsSurface::FLAG_IS_TRANSPARENT;
  RefPtr<gfxASurface> surf = new gfxWindowsSurface(dc, flags);
  IntSize size = surf->GetSize();
  if (size.width <= 0 || size.height <= 0) {
    if (dc) {
      FreeWindowSurface(dc);
    }
    return nullptr;
  }

  RefPtr<DrawTarget> dt =
      mozilla::gfx::Factory::CreateDrawTargetForCairoSurface(
          surf->CairoSurface(), size);
  if (dt) {
    mCompositeDC = dc;
  } else {
    FreeWindowSurface(dc);
  }

  return dt.forget();
}

void InProcessWinCompositorWidget::EndRemoteDrawing() {
  MOZ_ASSERT(!mLockedBackBufferData);
  if (mCompositeDC) {
    FreeWindowSurface(mCompositeDC);
  }
  mCompositeDC = nullptr;
}

already_AddRefed<gfx::DrawTarget>
InProcessWinCompositorWidget::GetBackBufferDrawTarget(
    gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
    bool* aOutIsCleared) {
  MOZ_ASSERT(!mLockedBackBufferData);

  RefPtr<gfx::DrawTarget> target = CompositorWidget::GetBackBufferDrawTarget(
      aScreenTarget, aRect, aOutIsCleared);
  if (!target) {
    return nullptr;
  }

  MOZ_ASSERT(target->GetBackendType() == BackendType::CAIRO);

  uint8_t* destData;
  IntSize destSize;
  int32_t destStride;
  SurfaceFormat destFormat;
  if (!target->LockBits(&destData, &destSize, &destStride, &destFormat)) {
    // LockBits is not supported. Use original DrawTarget.
    return target.forget();
  }

  RefPtr<gfx::DrawTarget> dataTarget = Factory::CreateDrawTargetForData(
      BackendType::CAIRO, destData, destSize, destStride, destFormat);
  mLockedBackBufferData = destData;

  return dataTarget.forget();
}

already_AddRefed<gfx::SourceSurface>
InProcessWinCompositorWidget::EndBackBufferDrawing() {
  if (mLockedBackBufferData) {
    MOZ_ASSERT(mLastBackBuffer);
    mLastBackBuffer->ReleaseBits(mLockedBackBufferData);
    mLockedBackBufferData = nullptr;
  }
  return CompositorWidget::EndBackBufferDrawing();
}

bool InProcessWinCompositorWidget::InitCompositor(
    layers::Compositor* aCompositor) {
  return true;
}

void InProcessWinCompositorWidget::EnterPresentLock() { mPresentLock.Enter(); }

void InProcessWinCompositorWidget::LeavePresentLock() { mPresentLock.Leave(); }

void InProcessWinCompositorWidget::UpdateTransparency(TransparencyMode aMode) {
  gfx::CriticalSectionAutoEnter presentLock(&mPresentLock);
  SetTransparencyMode(aMode);
}

void InProcessWinCompositorWidget::NotifyVisibilityUpdated(
    bool aIsFullyOccluded) {
  mIsFullyOccluded = aIsFullyOccluded;
}

bool InProcessWinCompositorWidget::GetWindowIsFullyOccluded() const {
  bool isFullyOccluded = mIsFullyOccluded;
  return isFullyOccluded;
}

HDC InProcessWinCompositorWidget::GetWindowSurface() { return ::GetDC(mWnd); }

void InProcessWinCompositorWidget::FreeWindowSurface(HDC dc) {
  ::ReleaseDC(mWnd, dc);
}

bool InProcessWinCompositorWidget::IsHidden() const { return ::IsIconic(mWnd); }

nsIWidget* InProcessWinCompositorWidget::RealWidget() { return mWindow; }

void InProcessWinCompositorWidget::ObserveVsync(VsyncObserver* aObserver) {
  if (RefPtr<CompositorVsyncDispatcher> cvd =
          mWindow->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

void InProcessWinCompositorWidget::UpdateCompositorWnd(
    const HWND aCompositorWnd, const HWND aParentWnd) {
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aCompositorWnd && aParentWnd);
  MOZ_ASSERT(aParentWnd == mWnd);

  // Since we're in the parent process anyway, we can just call SetParent
  // directly.
  ::SetParent(aCompositorWnd, aParentWnd);
  mSetParentCompleted = true;
}
}  // namespace mozilla::widget
