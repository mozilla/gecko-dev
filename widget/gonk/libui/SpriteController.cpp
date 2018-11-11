/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Sprites"

//#define LOG_NDEBUG 0

#include "SpriteController.h"

#include "cutils_log.h"
#include <utils/String8.h>
#ifdef HAVE_ANDROID_OS
#include <gui/Surface.h>
#endif

#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkColor.h>
#include <SkPaint.h>
#include <SkXfermode.h>
#include <android/native_window.h>

namespace android {

// --- SpriteController ---

SpriteController::SpriteController(const sp<Looper>& looper, int32_t overlayLayer) :
        mLooper(looper), mOverlayLayer(overlayLayer) {
#ifdef HAVE_ANDROID_OS
    mHandler = new WeakMessageHandler(this);
#endif

    mLocked.transactionNestingCount = 0;
    mLocked.deferredSpriteUpdate = false;
}

SpriteController::~SpriteController() {
#ifdef HAVE_ANDROID_OS
    mLooper->removeMessages(mHandler);

    if (mSurfaceComposerClient != NULL) {
        mSurfaceComposerClient->dispose();
        mSurfaceComposerClient.clear();
    }
#endif
}

sp<Sprite> SpriteController::createSprite() {
    return new SpriteImpl(this);
}

void SpriteController::openTransaction() {
    AutoMutex _l(mLock);

    mLocked.transactionNestingCount += 1;
}

void SpriteController::closeTransaction() {
    AutoMutex _l(mLock);

    LOG_ALWAYS_FATAL_IF(mLocked.transactionNestingCount == 0,
            "Sprite closeTransaction() called but there is no open sprite transaction");

    mLocked.transactionNestingCount -= 1;
    if (mLocked.transactionNestingCount == 0 && mLocked.deferredSpriteUpdate) {
        mLocked.deferredSpriteUpdate = false;
#ifdef HAVE_ANDROID_OS
        mLooper->sendMessage(mHandler, Message(MSG_UPDATE_SPRITES));
#endif
    }
}

void SpriteController::invalidateSpriteLocked(const sp<SpriteImpl>& sprite) {
    bool wasEmpty = mLocked.invalidatedSprites.isEmpty();
    mLocked.invalidatedSprites.push(sprite);
    if (wasEmpty) {
        if (mLocked.transactionNestingCount != 0) {
            mLocked.deferredSpriteUpdate = true;
        } else {
#ifdef HAVE_ANDROID_OS
            mLooper->sendMessage(mHandler, Message(MSG_UPDATE_SPRITES));
#endif
        }
    }
}

#ifdef HAVE_ANDROID_OS
void SpriteController::disposeSurfaceLocked(const sp<SurfaceControl>& surfaceControl) {
    bool wasEmpty = mLocked.disposedSurfaces.isEmpty();
    mLocked.disposedSurfaces.push(surfaceControl);
    if (wasEmpty) {
        mLooper->sendMessage(mHandler, Message(MSG_DISPOSE_SURFACES));
    }
}

void SpriteController::handleMessage(const Message& message) {
    switch (message.what) {
    case MSG_UPDATE_SPRITES:
        doUpdateSprites();
        break;
    case MSG_DISPOSE_SURFACES:
        doDisposeSurfaces();
        break;
    }
}
#endif

void SpriteController::doUpdateSprites() {
    // Collect information about sprite updates.
    // Each sprite update record includes a reference to its associated sprite so we can
    // be certain the sprites will not be deleted while this function runs.  Sprites
    // may invalidate themselves again during this time but we will handle those changes
    // in the next iteration.
    Vector<SpriteUpdate> updates;
    size_t numSprites;
    { // acquire lock
        AutoMutex _l(mLock);

        numSprites = mLocked.invalidatedSprites.size();
        for (size_t i = 0; i < numSprites; i++) {
            const sp<SpriteImpl>& sprite = mLocked.invalidatedSprites.itemAt(i);

            updates.push(SpriteUpdate(sprite, sprite->getStateLocked()));
            sprite->resetDirtyLocked();
        }
        mLocked.invalidatedSprites.clear();
    } // release lock

    // Create missing surfaces.
    bool surfaceChanged = false;
#ifdef HAVE_ANDROID_OS
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);
        if (update.state.surfaceControl == NULL && update.state.wantSurfaceVisible()) {
            update.state.surfaceWidth = update.state.icon.bitmap.width();
            update.state.surfaceHeight = update.state.icon.bitmap.height();
            update.state.surfaceDrawn = false;
            update.state.surfaceVisible = false;
            update.state.surfaceControl = obtainSurface(
                    update.state.surfaceWidth, update.state.surfaceHeight);
            if (update.state.surfaceControl != NULL) {
                update.surfaceChanged = surfaceChanged = true;
            }
        }
    }
#endif

    // Resize sprites if needed, inside a global transaction.
#ifdef HAVE_ANDROID_OS
    bool haveGlobalTransaction = false;
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);
        if (update.state.surfaceControl != NULL && update.state.wantSurfaceVisible()) {
            int32_t desiredWidth = update.state.icon.bitmap.width();
            int32_t desiredHeight = update.state.icon.bitmap.height();
            if (update.state.surfaceWidth < desiredWidth
                    || update.state.surfaceHeight < desiredHeight) {
                if (!haveGlobalTransaction) {
                    SurfaceComposerClient::openGlobalTransaction();
                    haveGlobalTransaction = true;
                }

                status_t status = update.state.surfaceControl->setSize(desiredWidth, desiredHeight);
                if (status) {
                    ALOGE("Error %d resizing sprite surface from %dx%d to %dx%d",
                            status, update.state.surfaceWidth, update.state.surfaceHeight,
                            desiredWidth, desiredHeight);
                } else {
                    update.state.surfaceWidth = desiredWidth;
                    update.state.surfaceHeight = desiredHeight;
                    update.state.surfaceDrawn = false;
                    update.surfaceChanged = surfaceChanged = true;

                    if (update.state.surfaceVisible) {
                        status = update.state.surfaceControl->hide();
                        if (status) {
                            ALOGE("Error %d hiding sprite surface after resize.", status);
                        } else {
                            update.state.surfaceVisible = false;
                        }
                    }
                }
            }
        }
    }
#endif
#ifdef HAVE_ANDROID_OS
    if (haveGlobalTransaction) {
        SurfaceComposerClient::closeGlobalTransaction();
    }
#endif

    // Redraw sprites if needed.
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);

        if ((update.state.dirty & DIRTY_BITMAP) && update.state.surfaceDrawn) {
            update.state.surfaceDrawn = false;
            update.surfaceChanged = surfaceChanged = true;
        }

#ifdef HAVE_ANDROID_OS
        if (update.state.surfaceControl != NULL && !update.state.surfaceDrawn
                && update.state.wantSurfaceVisible()) {
            sp<Surface> surface = update.state.surfaceControl->getSurface();
            ANativeWindow_Buffer outBuffer;
            status_t status = surface->lock(&outBuffer, NULL);
            if (status) {
                ALOGE("Error %d locking sprite surface before drawing.", status);
            } else {
                SkBitmap surfaceBitmap;
                ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);
                surfaceBitmap.setConfig(SkBitmap::kARGB_8888_Config,
                        outBuffer.width, outBuffer.height, bpr);
                surfaceBitmap.setPixels(outBuffer.bits);

                SkCanvas surfaceCanvas(surfaceBitmap);

                SkPaint paint;
                paint.setXfermodeMode(SkXfermode::kSrc_Mode);
                surfaceCanvas.drawBitmap(update.state.icon.bitmap, 0, 0, &paint);

                if (outBuffer.width > uint32_t(update.state.icon.bitmap.width())) {
                    paint.setColor(0); // transparent fill color
                    surfaceCanvas.drawRectCoords(update.state.icon.bitmap.width(), 0,
                            outBuffer.width, update.state.icon.bitmap.height(), paint);
                }
                if (outBuffer.height > uint32_t(update.state.icon.bitmap.height())) {
                    paint.setColor(0); // transparent fill color
                    surfaceCanvas.drawRectCoords(0, update.state.icon.bitmap.height(),
                            outBuffer.width, outBuffer.height, paint);
                }

                status = surface->unlockAndPost();
                if (status) {
                    ALOGE("Error %d unlocking and posting sprite surface after drawing.", status);
                } else {
                    update.state.surfaceDrawn = true;
                    update.surfaceChanged = surfaceChanged = true;
                }
            }
        }
#endif
    }

#ifdef HAVE_ANDROID_OS
    // Set sprite surface properties and make them visible.
    bool haveTransaction = false;
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);
        bool wantSurfaceVisibleAndDrawn = update.state.wantSurfaceVisible()
                && update.state.surfaceDrawn;
        bool becomingVisible = wantSurfaceVisibleAndDrawn && !update.state.surfaceVisible;
        bool becomingHidden = !wantSurfaceVisibleAndDrawn && update.state.surfaceVisible;
        if (update.state.surfaceControl != NULL && (becomingVisible || becomingHidden
                || (wantSurfaceVisibleAndDrawn && (update.state.dirty & (DIRTY_ALPHA
                        | DIRTY_POSITION | DIRTY_TRANSFORMATION_MATRIX | DIRTY_LAYER
                        | DIRTY_VISIBILITY | DIRTY_HOTSPOT))))) {
            status_t status;
            if (!haveTransaction) {
                SurfaceComposerClient::openGlobalTransaction();
                haveTransaction = true;
            }

            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible || (update.state.dirty & DIRTY_ALPHA))) {
                status = update.state.surfaceControl->setAlpha(update.state.alpha);
                if (status) {
                    ALOGE("Error %d setting sprite surface alpha.", status);
                }
            }

            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible || (update.state.dirty & (DIRTY_POSITION
                            | DIRTY_HOTSPOT)))) {
                status = update.state.surfaceControl->setPosition(
                        update.state.positionX - update.state.icon.hotSpotX,
                        update.state.positionY - update.state.icon.hotSpotY);
                if (status) {
                    ALOGE("Error %d setting sprite surface position.", status);
                }
            }

            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible
                            || (update.state.dirty & DIRTY_TRANSFORMATION_MATRIX))) {
                status = update.state.surfaceControl->setMatrix(
                        update.state.transformationMatrix.dsdx,
                        update.state.transformationMatrix.dtdx,
                        update.state.transformationMatrix.dsdy,
                        update.state.transformationMatrix.dtdy);
                if (status) {
                    ALOGE("Error %d setting sprite surface transformation matrix.", status);
                }
            }

            int32_t surfaceLayer = mOverlayLayer + update.state.layer;
            if (wantSurfaceVisibleAndDrawn
                    && (becomingVisible || (update.state.dirty & DIRTY_LAYER))) {
                status = update.state.surfaceControl->setLayer(surfaceLayer);
                if (status) {
                    ALOGE("Error %d setting sprite surface layer.", status);
                }
            }

            if (becomingVisible) {
                status = update.state.surfaceControl->show();
                if (status) {
                    ALOGE("Error %d showing sprite surface.", status);
                } else {
                    update.state.surfaceVisible = true;
                    update.surfaceChanged = surfaceChanged = true;
                }
            } else if (becomingHidden) {
                status = update.state.surfaceControl->hide();
                if (status) {
                    ALOGE("Error %d hiding sprite surface.", status);
                } else {
                    update.state.surfaceVisible = false;
                    update.surfaceChanged = surfaceChanged = true;
                }
            }
        }
    }
#endif

#ifdef HAVE_ANDROID_OS
    if (haveTransaction) {
        SurfaceComposerClient::closeGlobalTransaction();
    }
#endif

#ifdef HAVE_ANDROID_OS
    // If any surfaces were changed, write back the new surface properties to the sprites.
    if (surfaceChanged) { // acquire lock
        AutoMutex _l(mLock);

        for (size_t i = 0; i < numSprites; i++) {
            const SpriteUpdate& update = updates.itemAt(i);

            if (update.surfaceChanged) {
                update.sprite->setSurfaceLocked(update.state.surfaceControl,
                        update.state.surfaceWidth, update.state.surfaceHeight,
                        update.state.surfaceDrawn, update.state.surfaceVisible);
            }
        }
    } // release lock
#endif

    // Clear the sprite update vector outside the lock.  It is very important that
    // we do not clear sprite references inside the lock since we could be releasing
    // the last remaining reference to the sprite here which would result in the
    // sprite being deleted and the lock being reacquired by the sprite destructor
    // while already held.
    updates.clear();
}

void SpriteController::doDisposeSurfaces() {
#ifdef HAVE_ANDROID_OS
    // Collect disposed surfaces.
    Vector<sp<SurfaceControl> > disposedSurfaces;
    { // acquire lock
        AutoMutex _l(mLock);

        disposedSurfaces = mLocked.disposedSurfaces;
        mLocked.disposedSurfaces.clear();
    } // release lock

    // Release the last reference to each surface outside of the lock.
    // We don't want the surfaces to be deleted while we are holding our lock.
    disposedSurfaces.clear();
#endif
}

void SpriteController::ensureSurfaceComposerClient() {
#ifdef HAVE_ANDROID_OS
    if (mSurfaceComposerClient == NULL) {
        mSurfaceComposerClient = new SurfaceComposerClient();
    }
#endif
}

#ifdef HAVE_ANDROID_OS
sp<SurfaceControl> SpriteController::obtainSurface(int32_t width, int32_t height) {
    ensureSurfaceComposerClient();

    sp<SurfaceControl> surfaceControl = mSurfaceComposerClient->createSurface(
            String8("Sprite"), width, height, PIXEL_FORMAT_RGBA_8888,
            ISurfaceComposerClient::eHidden);
    if (surfaceControl == NULL || !surfaceControl->isValid()) {
        ALOGE("Error creating sprite surface.");
        return NULL;
    }
    return surfaceControl;
}
#endif


// --- SpriteController::SpriteImpl ---

SpriteController::SpriteImpl::SpriteImpl(const sp<SpriteController> controller) :
        mController(controller) {
}

SpriteController::SpriteImpl::~SpriteImpl() {
    AutoMutex _m(mController->mLock);

#ifdef HAVE_ANDROID_OS
    // Let the controller take care of deleting the last reference to sprite
    // surfaces so that we do not block the caller on an IPC here.
    if (mLocked.state.surfaceControl != NULL) {
        mController->disposeSurfaceLocked(mLocked.state.surfaceControl);
        mLocked.state.surfaceControl.clear();
    }
#endif
}

void SpriteController::SpriteImpl::setIcon(const SpriteIcon& icon) {
    AutoMutex _l(mController->mLock);

#ifdef HAVE_ANDROID_OS
    uint32_t dirty;
    if (icon.isValid()) {
        icon.bitmap.copyTo(&mLocked.state.icon.bitmap, SkBitmap::kARGB_8888_Config);

        if (!mLocked.state.icon.isValid()
                || mLocked.state.icon.hotSpotX != icon.hotSpotX
                || mLocked.state.icon.hotSpotY != icon.hotSpotY) {
            mLocked.state.icon.hotSpotX = icon.hotSpotX;
            mLocked.state.icon.hotSpotY = icon.hotSpotY;
            dirty = DIRTY_BITMAP | DIRTY_HOTSPOT;
        } else {
            dirty = DIRTY_BITMAP;
        }
    } else if (mLocked.state.icon.isValid()) {
        mLocked.state.icon.bitmap.reset();
        dirty = DIRTY_BITMAP | DIRTY_HOTSPOT;
    } else {
        return; // setting to invalid icon and already invalid so nothing to do
    }

    invalidateLocked(dirty);
#endif
}

void SpriteController::SpriteImpl::setVisible(bool visible) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.visible != visible) {
        mLocked.state.visible = visible;
        invalidateLocked(DIRTY_VISIBILITY);
    }
}

void SpriteController::SpriteImpl::setPosition(float x, float y) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.positionX != x || mLocked.state.positionY != y) {
        mLocked.state.positionX = x;
        mLocked.state.positionY = y;
        invalidateLocked(DIRTY_POSITION);
    }
}

void SpriteController::SpriteImpl::setLayer(int32_t layer) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.layer != layer) {
        mLocked.state.layer = layer;
        invalidateLocked(DIRTY_LAYER);
    }
}

void SpriteController::SpriteImpl::setAlpha(float alpha) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.alpha != alpha) {
        mLocked.state.alpha = alpha;
        invalidateLocked(DIRTY_ALPHA);
    }
}

void SpriteController::SpriteImpl::setTransformationMatrix(
        const SpriteTransformationMatrix& matrix) {
    AutoMutex _l(mController->mLock);

    if (mLocked.state.transformationMatrix != matrix) {
        mLocked.state.transformationMatrix = matrix;
        invalidateLocked(DIRTY_TRANSFORMATION_MATRIX);
    }
}

void SpriteController::SpriteImpl::invalidateLocked(uint32_t dirty) {
    bool wasDirty = mLocked.state.dirty;
    mLocked.state.dirty |= dirty;

    if (!wasDirty) {
        mController->invalidateSpriteLocked(this);
    }
}

} // namespace android
