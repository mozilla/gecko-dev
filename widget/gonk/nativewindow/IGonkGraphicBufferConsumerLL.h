/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef NATIVEWINDOW_IGONKGRAPHICBUFFERCONSUMER_LL_H
#define NATIVEWINDOW_IGONKGRAPHICBUFFERCONSUMER_LL_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Timers.h>

#include <binder/IInterface.h>
#include <ui/Rect.h>

#include "mozilla/RefPtr.h"

class ANativeWindowBuffer;

namespace mozilla {
namespace layers {
class TextureClient;
}
}

namespace android {
// ----------------------------------------------------------------------------

class Fence;
class GraphicBuffer;
class IConsumerListener;
class NativeHandle;

class IGonkGraphicBufferConsumer : public IInterface {
public:

    // public facing structure for BufferSlot
    class BufferItem : public Flattenable<BufferItem> {
        friend class Flattenable<BufferItem>;
        size_t getPodSize() const;
        size_t getFlattenedSize() const;
        size_t getFdCount() const;
        status_t flatten(void*& buffer, size_t& size, int*& fds, size_t& count) const;
        status_t unflatten(void const*& buffer, size_t& size, int const*& fds, size_t& count);

    public:
        // The default value of mBuf, used to indicate this doesn't correspond to a slot.
        enum { INVALID_BUFFER_SLOT = -1 };
        BufferItem();

        // mGraphicBuffer points to the buffer allocated for this slot, or is NULL
        // if the buffer in this slot has been acquired in the past (see
        // BufferSlot.mAcquireCalled).
        sp<GraphicBuffer> mGraphicBuffer;

        // mFence is a fence that will signal when the buffer is idle.
        sp<Fence> mFence;

        // mCrop is the current crop rectangle for this buffer slot.
        Rect mCrop;

        // mTransform is the current transform flags for this buffer slot.
        // refer to NATIVE_WINDOW_TRANSFORM_* in <window.h>
        uint32_t mTransform;

        // mScalingMode is the current scaling mode for this buffer slot.
        // refer to NATIVE_WINDOW_SCALING_* in <window.h>
        uint32_t mScalingMode;

        // mTimestamp is the current timestamp for this buffer slot. This gets
        // to set by queueBuffer each time this slot is queued. This value
        // is guaranteed to be monotonically increasing for each newly
        // acquired buffer.
        int64_t mTimestamp;

        // mIsAutoTimestamp indicates whether mTimestamp was generated
        // automatically when the buffer was queued.
        bool mIsAutoTimestamp;

        // mFrameNumber is the number of the queued frame for this slot.
        uint64_t mFrameNumber;

        // mBuf is the slot index of this buffer (default INVALID_BUFFER_SLOT).
        int mBuf;

        // mIsDroppable whether this buffer was queued with the
        // property that it can be replaced by a new buffer for the purpose of
        // making sure dequeueBuffer() won't block.
        // i.e.: was the BufferQueue in "mDequeueBufferCannotBlock" when this buffer
        // was queued.
        bool mIsDroppable;

        // Indicates whether this buffer has been seen by a consumer yet
        bool mAcquireCalled;

        // Indicates this buffer must be transformed by the inverse transform of the screen
        // it is displayed onto. This is applied after mTransform.
        bool mTransformToDisplayInverse;
    };

    enum {
        // Returned by releaseBuffer, after which the consumer must
        // free any references to the just-released buffer that it might have.
        STALE_BUFFER_SLOT = 1,
        // Returned by dequeueBuffer if there are no pending buffers available.
        NO_BUFFER_AVAILABLE,
        // Returned by dequeueBuffer if it's too early for the buffer to be acquired.
        PRESENT_LATER,
    };

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the BufferQueue.  If no buffer is pending then it returns
    // NO_BUFFER_AVAILABLE.  If a buffer is successfully acquired, the
    // information about the buffer is returned in BufferItem.
    //
    // If the buffer returned had previously been
    // acquired then the BufferItem::mGraphicBuffer field of buffer is set to
    // NULL and it is assumed that the consumer still holds a reference to the
    // buffer.
    //
    // If presentWhen is non-zero, it indicates the time when the buffer will
    // be displayed on screen.  If the buffer's timestamp is farther in the
    // future, the buffer won't be acquired, and PRESENT_LATER will be
    // returned.  The presentation time is in nanoseconds, and the time base
    // is CLOCK_MONOTONIC.
    //
    // Return of NO_ERROR means the operation completed as normal.
    //
    // Return of a positive value means the operation could not be completed
    //    at this time, but the user should try again later:
    // * NO_BUFFER_AVAILABLE - no buffer is pending (nothing queued by producer)
    // * PRESENT_LATER - the buffer's timestamp is farther in the future
    //
    // Return of a negative value means an error has occurred:
    // * INVALID_OPERATION - too many buffers have been acquired
    virtual status_t acquireBuffer(BufferItem* buffer, nsecs_t presentWhen) = 0;

    // detachBuffer attempts to remove all ownership of the buffer in the given
    // slot from the buffer queue. If this call succeeds, the slot will be
    // freed, and there will be no way to obtain the buffer from this interface.
    // The freed slot will remain unallocated until either it is selected to
    // hold a freshly allocated buffer in dequeueBuffer or a buffer is attached
    // to the slot. The buffer must have already been acquired.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - the given slot number is invalid, either because it is
    //               out of the range [0, NUM_BUFFER_SLOTS) or because the slot
    //               it refers to is not currently acquired.
    virtual status_t detachBuffer(int slot) = 0;

    // attachBuffer attempts to transfer ownership of a buffer to the buffer
    // queue. If this call succeeds, it will be as if this buffer was acquired
    // from the returned slot number. As such, this call will fail if attaching
    // this buffer would cause too many buffers to be simultaneously acquired.
    //
    // If the buffer is successfully attached, its frameNumber is initialized
    // to 0. This must be passed into the releaseBuffer call or else the buffer
    // will be deallocated as stale.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - outSlot or buffer were NULL
    // * INVALID_OPERATION - cannot attach the buffer because it would cause too
    //                       many buffers to be acquired.
    // * NO_MEMORY - no free slots available
    virtual status_t attachBuffer(int *outSlot,
            const sp<GraphicBuffer>& buffer) = 0;

    // releaseBuffer releases a buffer slot from the consumer back to the
    // BufferQueue.  This may be done while the buffer's contents are still
    // being accessed.  The fence will signal when the buffer is no longer
    // in use. frameNumber is used to indentify the exact buffer returned.
    //
    // If releaseBuffer returns STALE_BUFFER_SLOT, then the consumer must free
    // any references to the just-released buffer that it might have, as if it
    // had received a onBuffersReleased() call with a mask set for the released
    // buffer.
    //
    // Note that the dependencies on EGL will be removed once we switch to using
    // the Android HW Sync HAL.
    //
    // Return of NO_ERROR means the operation completed as normal.
    //
    // Return of a positive value means the operation could not be completed
    //    at this time, but the user should try again later:
    // * STALE_BUFFER_SLOT - see above (second paragraph)
    //
    // Return of a negative value means an error has occurred:
    // * BAD_VALUE - one of the following could've happened:
    //               * the buffer slot was invalid
    //               * the fence was NULL
    //               * the buffer slot specified is not in the acquired state
    virtual status_t releaseBuffer(int buf, uint64_t frameNumber, const sp<Fence>& releaseFence) = 0;

    // consumerConnect connects a consumer to the BufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // BufferQueue is placed into the "abandoned" state, causing most
    // interactions with the BufferQueue by the producer to fail.
    // controlledByApp indicates whether the consumer is controlled by
    // the application.
    //
    // consumer may not be NULL.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned
    // * BAD_VALUE - a NULL consumer was provided
    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer, bool controlledByApp) = 0;

    // consumerDisconnect disconnects a consumer from the BufferQueue. All
    // buffers will be freed and the BufferQueue is placed in the "abandoned"
    // state, causing most interactions with the BufferQueue by the producer to
    // fail.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - no consumer is currently connected
    virtual status_t consumerDisconnect() = 0;

    // getReleasedBuffers sets the value pointed to by slotMask to a bit set.
    // Each bit index with a 1 corresponds to a released buffer slot with that
    // index value.  In particular, a released buffer is one that has
    // been released by the BufferQueue but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned.
    virtual status_t getReleasedBuffers(uint64_t* slotMask) = 0;

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - either w or h was zero
    virtual status_t setDefaultBufferSize(uint32_t w, uint32_t h) = 0;

    // setDefaultMaxBufferCount sets the default value for the maximum buffer
    // count (the initial default is 2). If the producer has requested a
    // buffer count using setBufferCount, the default buffer count will only
    // take effect if the producer sets the count back to zero.
    //
    // The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - bufferCount was out of range (see above).
    virtual status_t setDefaultMaxBufferCount(int bufferCount) = 0;

    // disableAsyncBuffer disables the extra buffer used in async mode
    // (when both producer and consumer have set their "isControlledByApp"
    // flag) and has dequeueBuffer() return WOULD_BLOCK instead.
    //
    // This can only be called before consumerConnect().
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * INVALID_OPERATION - attempting to call this after consumerConnect.
    virtual status_t disableAsyncBuffer() = 0;

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1).  This call will
    // fail if a producer is connected to the BufferQueue.
    //
    // maxAcquiredBuffers must be (inclusive) between 1 and MAX_MAX_ACQUIRED_BUFFERS.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - maxAcquiredBuffers was out of range (see above).
    // * INVALID_OPERATION - attempting to call this after a producer connected.
    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers) = 0;

    // setConsumerName sets the name used in logging
    virtual void setConsumerName(const String8& name) = 0;

    // setDefaultBufferFormat allows the BufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer.  Formats are enumerated in graphics.h; the
    // initial default is HAL_PIXEL_FORMAT_RGBA_8888.
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    virtual status_t setDefaultBufferFormat(uint32_t defaultFormat) = 0;

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    virtual status_t setConsumerUsageBits(uint32_t usage) = 0;

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    virtual status_t setTransformHint(uint32_t hint) = 0;

    // Retrieve the sideband buffer stream, if any.
    virtual sp<NativeHandle> getSidebandStream() const = 0;

    // dump state into a string
    virtual void dumpToString(String8& result, const char* prefix) const = 0;

    // Added by mozilla
    virtual mozilla::TemporaryRef<mozilla::layers::TextureClient>
        getTextureClientFromBuffer(ANativeWindowBuffer* buffer) = 0;

    virtual int getSlotFromTextureClientLocked(mozilla::layers::TextureClient* client) const = 0;

public:
    DECLARE_META_INTERFACE(GonkGraphicBufferConsumer);
};

// ----------------------------------------------------------------------------

class BnGonkGraphicBufferConsumer : public BnInterface<IGonkGraphicBufferConsumer>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_IGONKGRAPHICBUFFERCONSUMER_H
