/*
 * Copyright 2014 The Android Open Source Project
 * Copyright (C) 2014 Mozilla Foundation
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

#ifndef NATIVEWINDOW_GONKBUFFERQUEUECONSUMER_LL_H
#define NATIVEWINDOW_GONKBUFFERQUEUECONSUMER_LL_H

#include "GonkBufferQueueDefs.h"
#include "IGonkGraphicBufferConsumerLL.h"

namespace android {

class GonkBufferQueueCore;

class GonkBufferQueueConsumer : public BnGonkGraphicBufferConsumer {

public:
    GonkBufferQueueConsumer(const sp<GonkBufferQueueCore>& core);
    virtual ~GonkBufferQueueConsumer();

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the GonkBufferQueue. If no buffer is pending then it returns
    // NO_BUFFER_AVAILABLE. If a buffer is successfully acquired, the
    // information about the buffer is returned in BufferItem.  If the buffer
    // returned had previously been acquired then the BufferItem::mGraphicBuffer
    // field of buffer is set to NULL and it is assumed that the consumer still
    // holds a reference to the buffer.
    //
    // If expectedPresent is nonzero, it indicates the time when the buffer
    // will be displayed on screen. If the buffer's timestamp is farther in the
    // future, the buffer won't be acquired, and PRESENT_LATER will be
    // returned.  The presentation time is in nanoseconds, and the time base
    // is CLOCK_MONOTONIC.
    virtual status_t acquireBuffer(BufferItem* outBuffer,
            nsecs_t expectedPresent);

    // See IGonkGraphicBufferConsumer::detachBuffer
    virtual status_t detachBuffer(int slot);

    // See IGonkGraphicBufferConsumer::attachBuffer
    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer);

    // releaseBuffer releases a buffer slot from the consumer back to the
    // GonkBufferQueue.  This may be done while the buffer's contents are still
    // being accessed.  The fence will signal when the buffer is no longer
    // in use. frameNumber is used to indentify the exact buffer returned.
    //
    // If releaseBuffer returns STALE_BUFFER_SLOT, then the consumer must free
    // any references to the just-released buffer that it might have, as if it
    // had received a onBuffersReleased() call with a mask set for the released
    // buffer.
    virtual status_t releaseBuffer(int slot, uint64_t frameNumber,
            const sp<Fence>& releaseFence);

    // connect connects a consumer to the GonkBufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // GonkBufferQueue is placed into the "abandoned" state, causing most
    // interactions with the GonkBufferQueue by the producer to fail.
    // controlledByApp indicates whether the consumer is controlled by
    // the application.
    //
    // consumerListener may not be NULL.
    virtual status_t connect(const sp<IConsumerListener>& consumerListener,
            bool controlledByApp);

    // disconnect disconnects a consumer from the GonkBufferQueue. All
    // buffers will be freed and the GonkBufferQueue is placed in the "abandoned"
    // state, causing most interactions with the GonkBufferQueue by the producer to
    // fail.
    virtual status_t disconnect();

    // getReleasedBuffers sets the value pointed to by outSlotMask to a bit mask
    // indicating which buffer slots have been released by the GonkBufferQueue
    // but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    virtual status_t getReleasedBuffers(uint64_t* outSlotMask);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    virtual status_t setDefaultBufferSize(uint32_t width, uint32_t height);

    // setDefaultMaxBufferCount sets the default value for the maximum buffer
    // count (the initial default is 2). If the producer has requested a
    // buffer count using setBufferCount, the default buffer count will only
    // take effect if the producer sets the count back to zero.
    //
    // The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    virtual status_t setDefaultMaxBufferCount(int bufferCount);

    // disableAsyncBuffer disables the extra buffer used in async mode
    // (when both producer and consumer have set their "isControlledByApp"
    // flag) and has dequeueBuffer() return WOULD_BLOCK instead.
    //
    // This can only be called before connect().
    virtual status_t disableAsyncBuffer();

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1).  This call will
    // fail if a producer is connected to the GonkBufferQueue.
    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers);

    // setConsumerName sets the name used in logging
    virtual void setConsumerName(const String8& name);

    // setDefaultBufferFormat allows the GonkBufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer.  Formats are enumerated in graphics.h; the
    // initial default is HAL_PIXEL_FORMAT_RGBA_8888.
    virtual status_t setDefaultBufferFormat(uint32_t defaultFormat);

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    virtual status_t setConsumerUsageBits(uint32_t usage);

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    virtual status_t setTransformHint(uint32_t hint);

    // Retrieve the sideband buffer stream, if any.
    virtual sp<NativeHandle> getSidebandStream() const;

    // dump our state in a String
    virtual void dumpToString(String8& result, const char* prefix) const;

    // Added by mozilla
    virtual already_AddRefed<GonkBufferSlot::TextureClient> getTextureClientFromBuffer(ANativeWindowBuffer* buffer);

    virtual int getSlotFromTextureClientLocked(GonkBufferSlot::TextureClient* client) const;

    // Functions required for backwards compatibility.
    // These will be modified/renamed in IGonkGraphicBufferConsumer and will be
    // removed from this class at that time. See b/13306289.
    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer,
            bool controlledByApp) {
        return connect(consumer, controlledByApp);
    }

    virtual status_t consumerDisconnect() { return disconnect(); }

    // End functions required for backwards compatibility

private:
    sp<GonkBufferQueueCore> mCore;

    // This references mCore->mSlots. Lock mCore->mMutex while accessing.
    GonkBufferQueueDefs::SlotsType& mSlots;

    // This is a cached copy of the name stored in the GonkBufferQueueCore.
    // It's updated during setConsumerName.
    String8 mConsumerName;

}; // class GonkBufferQueueConsumer

} // namespace android

#endif
