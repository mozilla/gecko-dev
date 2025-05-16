/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayerMacSurfaceHandler_h
#define mozilla_layers_NativeLayerMacSurfaceHandler_h

#include "mozilla/Maybe.h"
#include "mozilla/gfx/Types.h"

#include "GLTypes.h"
#include "nsISupportsImpl.h"
#include "nsRegion.h"

namespace mozilla {

namespace gl {
class GLContext;
}  // namespace gl

namespace wr {
class RenderTextureHost;
class RenderMacIOSurfaceTextureHost;
}  // namespace wr

namespace layers {
class SurfacePoolHandleCA;

struct SurfaceWithInvalidRegion {
  CFTypeRefPtr<IOSurfaceRef> mSurface;
  gfx::IntRegion mInvalidRegion;
};

struct SurfaceWithInvalidRegionAndCheckCount {
  SurfaceWithInvalidRegion mEntry;
  uint32_t mCheckCount;  // The number of calls to IOSurfaceIsInUse
};

// A companion to macOS-specific subclasses of NativeLayer, this class
// handles the implementation of the surface management calls. The
// expectation is that NativeLayerMacIOSurfaceHandler is composed into
// these classes, rather than be used as a superclass/.
class NativeLayerMacSurfaceHandler {
 public:
  NativeLayerMacSurfaceHandler(const gfx::IntSize& aSize,
                                 SurfacePoolHandleCA* aSurfacePoolHandle);
  ~NativeLayerMacSurfaceHandler();

  gfx::IntSize Size() { return mSize; }

  // Returns the "display rect", in content coordinates, of the current front
  // surface. This rect acts as an extra clip and prevents invalid content from
  // getting to the screen. The display rect starts out empty before the first
  // call to NextSurface*. Note the different coordinate space from the regular
  // clip rect: the clip rect is "outside" the layer position, the display rect
  // is "inside" the layer position (moves with the layer).
  gfx::IntRect DisplayRect() { return mDisplayRect; }

  void SetSurfaceIsFlipped(bool aIsFlipped) { mSurfaceIsFlipped = aIsFlipped; }
  bool SurfaceIsFlipped() { return mSurfaceIsFlipped; }

  // Gets the next surface for drawing from our swap chain and stores it in
  // mInProgressSurface. Returns whether this was successful.
  // mInProgressSurface is guaranteed to be not in use by the window server.
  // After a call to NextSurface, NextSurface must not be called again until
  // after NotifySurfaceReady has been called. Can be called on any thread. When
  // used from multiple threads, callers need to make sure that they still only
  // call NextSurface and NotifySurfaceReady alternatingly and not in any other
  // order.
  bool NextSurface();

  // Invalidates the specified region in all surfaces that are tracked by this
  // layer.
  void InvalidateRegionThroughoutSwapchain(const gfx::IntRegion& aRegion);

  // Invalidate aUpdateRegion and make sure that mInProgressSurface retains any
  // valid content from the previous surface outside of aUpdateRegion, so that
  // only aUpdateRegion needs to be drawn. If content needs to be copied,
  // aCopyFn is called to do the copying.
  // aCopyFn: Fn(CFTypeRefPtr<IOSurfaceRef> aValidSourceIOSurface,
  //             const gfx::IntRegion& aCopyRegion) -> void
  template <typename F>
  void HandlePartialUpdate(const gfx::IntRect& aDisplayRect,
                           const gfx::IntRegion& aUpdateRegion, F&& aCopyFn);

  Maybe<SurfaceWithInvalidRegion> GetUnusedSurfaceAndCleanUp();

  // Returns a DrawTarget. The size of the DrawTarget will be the same as the
  // size of this layer. The caller should draw to that DrawTarget, then drop
  // its reference to the DrawTarget, and then call NotifySurfaceReady(). It can
  // limit its drawing to aUpdateRegion (which is in the DrawTarget's device
  // space). After a call to NextSurface*, NextSurface* must not be called again
  // until after NotifySurfaceReady has been called. Can be called on any
  // thread. When used from multiple threads, callers need to make sure that
  // they still only call NextSurface* and NotifySurfaceReady alternatingly and
  // not in any other order. aUpdateRegion and aDisplayRect are in "content
  // coordinates" and must not extend beyond the layer size. If aDisplayRect
  // contains parts that were not valid before, then those parts must be updated
  // (must be part of aUpdateRegion), so that the entirety of aDisplayRect is
  // valid after the update. The display rect determines the parts of the
  // surface that will be shown; this allows using surfaces with only
  // partially-valid content, as long as none of the invalid content is included
  // in the display rect.
  RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      const gfx::IntRect& aDisplayRect, const gfx::IntRegion& aUpdateRegion,
      gfx::BackendType aBackendType);

  // Returns a GLuint for a framebuffer that can be used for drawing to the
  // surface. The size of the framebuffer will be the same as the size of this
  // layer. If aNeedsDepth is true, the framebuffer is created with a depth
  // buffer.
  // The framebuffer's depth buffer (if present) may be shared with other
  // framebuffers of the same size, even from entirely different NativeLayer
  // objects. The caller should not assume anything about the depth buffer's
  // existing contents (i.e. it should clear it at the beginning of the draw).
  // Callers should draw to one layer at a time, such that there is no
  // interleaved drawing to different framebuffers that could be tripped up by
  // the sharing.
  // The caller should draw to the framebuffer, unbind it, and then call
  // NotifySurfaceReady(). It can limit its drawing to aUpdateRegion (which is
  // in the framebuffer's device space, possibly "upside down" if
  // SurfaceIsFlipped()).
  // The framebuffer will be created in the GLContext that this layer's
  // SurfacePoolHandle was created for.
  // After a call to NextSurface*, NextSurface* must not be called again until
  // after NotifySurfaceReady has been called. Can be called on any thread. When
  // used from multiple threads, callers need to make sure that they still only
  // call NextSurface and NotifySurfaceReady alternatingly and not in any other
  // order.
  // aUpdateRegion and aDisplayRect are in "content coordinates" and must not
  // extend beyond the layer size. If aDisplayRect contains parts that were not
  // valid before, then those parts must be updated (must be part of
  // aUpdateRegion), so that the entirety of aDisplayRect is valid after the
  // update. The display rect determines the parts of the surface that will be
  // shown; this allows using surfaces with only partially-valid content, as
  // long as none of the invalid content is included in the display rect.
  Maybe<GLuint> NextSurfaceAsFramebuffer(const gfx::IntRect& aDisplayRect,
                                         const gfx::IntRegion& aUpdateRegion,
                                         bool aNeedsDepth);

  // Indicates that the surface which has been returned from the most recent
  // call to NextSurface* is now finished being drawn to and can be displayed on
  // the screen. Resets the invalid region on the surface to the empty region.
  // Returns true if the display rect has changed.
  bool NotifySurfaceReady();

  // If you know that this layer will likely not draw any more frames, then it's
  // good to call DiscardBackbuffers in order to save memory and allow other
  // layer's to pick up the released surfaces from the pool.
  void DiscardBackbuffers();

  Maybe<SurfaceWithInvalidRegion> FrontSurface() { return mFrontSurface; }
  Maybe<SurfaceWithInvalidRegion> InProgressSurface() {
    return mInProgressSurface;
  }
  std::vector<SurfaceWithInvalidRegionAndCheckCount> Surfaces() {
    return mSurfaces;
  }

 protected:
  friend class NativeLayerCA;

  gfx::IntSize mSize;
  gfx::IntRect mDisplayRect;
  bool mSurfaceIsFlipped = false;

#ifdef NIGHTLY_BUILD
  // Track the consistency of our caller's API usage. Layers that are drawn
  // should only ever be called with NotifySurfaceReady. Layers that are
  // external should only ever be called with AttachExternalImage.
  bool mHasEverAttachExternalImage = false;
  bool mHasEverNotifySurfaceReady = false;
#endif

  // Each IOSurface is initially created inside NextSurface.
  // The surface stays alive until the recycling mechanism in NextSurface
  // determines it is no longer needed (because the swap chain has grown too
  // long) or until DiscardBackbuffers() is called or the layer is destroyed.
  // During the surface's lifetime, it will continuously move through the fields
  // mInProgressSurface, mFrontSurface, and back to front through the mSurfaces
  // queue:
  //
  //  mSurfaces.front()
  //  ------[NextSurface()]-----> mInProgressSurface
  //  --[NotifySurfaceReady()]--> mFrontSurface
  //  --[NotifySurfaceReady()]--> mSurfaces.back()  --> .... -->
  //  mSurfaces.front()
  //
  // We mark an IOSurface as "in use" as long as it is either in
  // mInProgressSurface. When it is in mFrontSurface or in the mSurfaces queue,
  // it is not marked as "in use" by us - but it can be "in use" by the window
  // server. Consequently, IOSurfaceIsInUse on a surface from mSurfaces reflects
  // whether the window server is still reading from the surface, and we can use
  // this indicator to decide when to recycle the surface.
  //
  // Users of NativeLayerCA normally proceed in this order:
  //  1. Begin a frame by calling NextSurface to get the surface.
  //  2. Draw to the surface.
  //  3. Mark the surface as done by calling NotifySurfaceReady.
  //  4. Call NativeLayerRoot::CommitToScreen(), which calls ApplyChanges()
  //     during a CATransaction.

  // The surface we returned from the most recent call to NextSurface, before
  // the matching call to NotifySurfaceReady.
  // Will only be Some() between calls to NextSurface and NotifySurfaceReady.
  Maybe<SurfaceWithInvalidRegion> mInProgressSurface;
  Maybe<gfx::IntRegion> mInProgressUpdateRegion;
  Maybe<gfx::IntRect> mInProgressDisplayRect;

  // The surface that the most recent call to NotifySurfaceReady was for.
  // Will be Some() after the first call to NotifySurfaceReady, for the rest of
  // the layer's life time.
  Maybe<SurfaceWithInvalidRegion> mFrontSurface;

  // The queue of surfaces which make up the rest of our "swap chain".
  // mSurfaces.front() is the next surface we'll attempt to use.
  // mSurfaces.back() is the one that was used most recently.
  std::vector<SurfaceWithInvalidRegionAndCheckCount> mSurfaces;

  // Non-null between calls to NextSurfaceAsDrawTarget and NotifySurfaceReady.
  RefPtr<MacIOSurface> mInProgressLockedIOSurface;

  RefPtr<SurfacePoolHandleCA> mSurfacePoolHandle;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayerMacSurfaceHandler_h
