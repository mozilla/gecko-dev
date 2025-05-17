/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/NativeLayerMacSurfaceHandler.h"
#include "mozilla/layers/SurfacePoolCA.h"
#include "GLBlitHelper.h"
#ifdef XP_MACOSX
#  include "GLContextCGL.h"
#else
#  include "GLContextEAGL.h"
#endif
#include "nsRegion.h"

namespace mozilla {
namespace layers {

using gfx::DataSourceSurface;
using gfx::IntPoint;
using gfx::IntRect;
using gfx::IntRegion;
using gfx::IntSize;
using gfx::Matrix4x4;
using gfx::SurfaceFormat;
using gl::GLContext;
#ifdef XP_MACOSX
using gl::GLContextCGL;
#endif

NativeLayerMacSurfaceHandler::NativeLayerMacSurfaceHandler(
    const gfx::IntSize& aSize, SurfacePoolHandleCA* aSurfacePoolHandle)
    : mSize(aSize), mSurfacePoolHandle(aSurfacePoolHandle) {
  MOZ_RELEASE_ASSERT(mSurfacePoolHandle,
                     "Need a non-null surface pool handle.");
}

NativeLayerMacSurfaceHandler::~NativeLayerMacSurfaceHandler() {
  if (mInProgressLockedIOSurface) {
    mInProgressLockedIOSurface->Unlock(false);
    mInProgressLockedIOSurface = nullptr;
  }
  if (mInProgressSurface) {
    IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
    mSurfacePoolHandle->ReturnSurfaceToPool(mInProgressSurface->mSurface);
  }
  if (mFrontSurface) {
    mSurfacePoolHandle->ReturnSurfaceToPool(mFrontSurface->mSurface);
  }
  for (const auto& surf : mSurfaces) {
    mSurfacePoolHandle->ReturnSurfaceToPool(surf.mEntry.mSurface);
  }
}

bool NativeLayerMacSurfaceHandler::NextSurface() {
  if (mSize.IsEmpty()) {
    gfxCriticalError()
        << "NextSurface returning false because of invalid mSize ("
        << mSize.width << ", " << mSize.height << ").";
    return false;
  }

  MOZ_RELEASE_ASSERT(!mInProgressSurface,
                     "ERROR: Do not call NextSurface twice in sequence. Call "
                     "NotifySurfaceReady before the "
                     "next call to NextSurface.");

  Maybe<SurfaceWithInvalidRegion> surf = GetUnusedSurfaceAndCleanUp();
  if (!surf) {
    CFTypeRefPtr<IOSurfaceRef> newSurf =
        mSurfacePoolHandle->ObtainSurfaceFromPool(mSize);
    MOZ_RELEASE_ASSERT(
        newSurf, "NextSurface IOSurfaceCreate failed to create the surface.");
    surf = Some(SurfaceWithInvalidRegion{newSurf, IntRect({}, mSize)});
  }

  mInProgressSurface = std::move(surf);
  IOSurfaceIncrementUseCount(mInProgressSurface->mSurface.get());
  return true;
}

void NativeLayerMacSurfaceHandler::InvalidateRegionThroughoutSwapchain(
    const IntRegion& aRegion) {
  IntRegion r = aRegion;
  if (mInProgressSurface) {
    mInProgressSurface->mInvalidRegion.OrWith(r);
  }
  if (mFrontSurface) {
    mFrontSurface->mInvalidRegion.OrWith(r);
  }
  for (auto& surf : mSurfaces) {
    surf.mEntry.mInvalidRegion.OrWith(r);
  }
}

template <typename F>
void NativeLayerMacSurfaceHandler::HandlePartialUpdate(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion, F&& aCopyFn) {
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aUpdateRegion.GetBounds()),
                     "The update region should be within the surface bounds.");
  MOZ_RELEASE_ASSERT(IntRect({}, mSize).Contains(aDisplayRect),
                     "The display rect should be within the surface bounds.");

  MOZ_RELEASE_ASSERT(!mInProgressUpdateRegion);
  MOZ_RELEASE_ASSERT(!mInProgressDisplayRect);

  mInProgressUpdateRegion = Some(aUpdateRegion);
  mInProgressDisplayRect = Some(aDisplayRect);

  if (mFrontSurface) {
    // Copy not-overwritten valid content from mFrontSurface so that valid
    // content never gets lost.
    gfx::IntRegion copyRegion;
    copyRegion.Sub(mInProgressSurface->mInvalidRegion, aUpdateRegion);
    copyRegion.SubOut(mFrontSurface->mInvalidRegion);

    if (!copyRegion.IsEmpty()) {
      // Now copy the valid content, using a caller-provided copy function.
      aCopyFn(mFrontSurface->mSurface, copyRegion);
      mInProgressSurface->mInvalidRegion.SubOut(copyRegion);
    }
  }

  InvalidateRegionThroughoutSwapchain(aUpdateRegion);
}

Maybe<SurfaceWithInvalidRegion>
NativeLayerMacSurfaceHandler::GetUnusedSurfaceAndCleanUp() {
  std::vector<SurfaceWithInvalidRegionAndCheckCount> usedSurfaces;
  Maybe<SurfaceWithInvalidRegion> unusedSurface;

  // Separate mSurfaces into used and unused surfaces.
  for (auto& surf : mSurfaces) {
    if (IOSurfaceIsInUse(surf.mEntry.mSurface.get())) {
      surf.mCheckCount++;
      if (surf.mCheckCount < 10) {
        usedSurfaces.push_back(std::move(surf));
      } else {
        // The window server has been holding on to this surface for an
        // unreasonably long time. This is known to happen sometimes, for
        // example in occluded windows or after a GPU switch. In that case,
        // release our references to the surface so that it doesn't look like
        // we're trying to keep it alive.
        mSurfacePoolHandle->ReturnSurfaceToPool(
            std::move(surf.mEntry.mSurface));
      }
    } else {
      if (unusedSurface) {
        // Multiple surfaces are unused. Keep the most recent one and release
        // any earlier ones. The most recent one requires the least amount of
        // copying during partial repaints.
        mSurfacePoolHandle->ReturnSurfaceToPool(
            std::move(unusedSurface->mSurface));
      }
      unusedSurface = Some(std::move(surf.mEntry));
    }
  }

  // Put the used surfaces back into mSurfaces.
  mSurfaces = std::move(usedSurfaces);

  return unusedSurface;
}

RefPtr<gfx::DrawTarget> NativeLayerMacSurfaceHandler::NextSurfaceAsDrawTarget(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    gfx::BackendType aBackendType) {
  if (!NextSurface()) {
    return nullptr;
  }

  auto surf = MakeRefPtr<MacIOSurface>(mInProgressSurface->mSurface);
  if (NS_WARN_IF(!surf->Lock(false))) {
    gfxCriticalError() << "NextSurfaceAsDrawTarget lock surface failed.";
    return nullptr;
  }

  mInProgressLockedIOSurface = std::move(surf);
  RefPtr<gfx::DrawTarget> dt =
      mInProgressLockedIOSurface->GetAsDrawTargetLocked(aBackendType);

  HandlePartialUpdate(
      aDisplayRect, aUpdateRegion,
      [&](CFTypeRefPtr<IOSurfaceRef> validSource,
          const gfx::IntRegion& copyRegion) {
        RefPtr<MacIOSurface> source = new MacIOSurface(validSource);
        if (source->Lock(true)) {
          RefPtr<gfx::DrawTarget> sourceDT =
              source->GetAsDrawTargetLocked(aBackendType);
          RefPtr<gfx::SourceSurface> sourceSurface = sourceDT->Snapshot();

          for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
            const gfx::IntRect& r = iter.Get();
            dt->CopySurface(sourceSurface, r, r.TopLeft());
          }
          source->Unlock(true);
        } else {
          gfxCriticalError() << "HandlePartialUpdate lock surface failed.";
        }
      });

  return dt;
}

Maybe<GLuint> NativeLayerMacSurfaceHandler::NextSurfaceAsFramebuffer(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    bool aNeedsDepth) {
  bool gotNextSurface = NextSurface();
  MOZ_RELEASE_ASSERT(gotNextSurface,
                     "NextSurfaceAsFramebuffer needs a surface.");

  Maybe<GLuint> fbo = mSurfacePoolHandle->GetFramebufferForSurface(
      mInProgressSurface->mSurface, aNeedsDepth);
  MOZ_RELEASE_ASSERT(fbo, "GetFramebufferForSurface failed.");

  HandlePartialUpdate(
      aDisplayRect, aUpdateRegion,
      [&](CFTypeRefPtr<IOSurfaceRef> validSource,
          const gfx::IntRegion& copyRegion) {
        // Copy copyRegion from validSource to fbo.
        MOZ_RELEASE_ASSERT(mSurfacePoolHandle->gl());
        mSurfacePoolHandle->gl()->MakeCurrent();
        Maybe<GLuint> sourceFBO =
            mSurfacePoolHandle->GetFramebufferForSurface(validSource, false);
        MOZ_RELEASE_ASSERT(
            sourceFBO,
            "GetFramebufferForSurface failed during HandlePartialUpdate.");
        for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
          gfx::IntRect r = iter.Get();
          if (mSurfaceIsFlipped) {
            r.y = mSize.height - r.YMost();
          }
          mSurfacePoolHandle->gl()->BlitHelper()->BlitFramebufferToFramebuffer(
              *sourceFBO, *fbo, r, r, LOCAL_GL_NEAREST);
        }
      });

  return fbo;
}

bool NativeLayerMacSurfaceHandler::NotifySurfaceReady() {
  MOZ_RELEASE_ASSERT(
      mInProgressSurface,
      "NotifySurfaceReady called without preceding call to NextSurface");

  if (mInProgressLockedIOSurface) {
    mInProgressLockedIOSurface->Unlock(false);
    mInProgressLockedIOSurface = nullptr;
  }

  if (mFrontSurface) {
    mSurfaces.push_back({*mFrontSurface, 0});
    mFrontSurface = Nothing();
  }

  MOZ_RELEASE_ASSERT(mInProgressUpdateRegion);
  IOSurfaceDecrementUseCount(mInProgressSurface->mSurface.get());
  mFrontSurface = std::move(mInProgressSurface);
  mFrontSurface->mInvalidRegion.SubOut(mInProgressUpdateRegion.extract());

  bool mutatedDisplayRect = false;
  MOZ_RELEASE_ASSERT(mInProgressDisplayRect);
  if (!mDisplayRect.IsEqualInterior(*mInProgressDisplayRect)) {
    mutatedDisplayRect = true;
    mDisplayRect = *mInProgressDisplayRect;
  }
  mInProgressDisplayRect = Nothing();
  return mutatedDisplayRect;
}

void NativeLayerMacSurfaceHandler::DiscardBackbuffers() {
  for (const auto& surf : mSurfaces) {
    mSurfacePoolHandle->ReturnSurfaceToPool(surf.mEntry.mSurface);
  }
  mSurfaces.clear();
}

}  // namespace layers
}  // namespace mozilla
