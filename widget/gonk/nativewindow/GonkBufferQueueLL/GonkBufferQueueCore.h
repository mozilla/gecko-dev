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

#ifndef NATIVEWINDOW_GONKBUFFERQUEUECORE_LL_H
#define NATIVEWINDOW_GONKBUFFERQUEUECORE_LL_H

#include "GonkBufferQueueDefs.h"
#include "GonkBufferSlot.h"

#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/NativeHandle.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>
#include <utils/Trace.h>
#include <utils/Vector.h>

#include "mozilla/layers/TextureClient.h"

#define ATRACE_BUFFER_INDEX(index)

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace android {

class GonkBufferItem;
class IConsumerListener;
class IGraphicBufferAlloc;
class IProducerListener;

class GonkBufferQueueCore : public virtual RefBase {

    friend class GonkBufferQueueProducer;
    friend class GonkBufferQueueConsumer;

public:
    // Used as a placeholder slot number when the value isn't pointing to an
    // existing buffer.
    enum { INVALID_BUFFER_SLOT = -1 }; // TODO: Extract from IGBC::BufferItem

    // We reserve two slots in order to guarantee that the producer and
    // consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = GonkBufferQueueDefs::NUM_BUFFER_SLOTS - 2 };

    // The default API number used to indicate that no producer is connected
    enum { NO_CONNECTED_API = 0 };

    typedef Vector<GonkBufferItem> Fifo;
    typedef mozilla::layers::TextureClient TextureClient;

    // GonkBufferQueueCore manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the needed
    // gralloc buffers.
    GonkBufferQueueCore(const sp<IGraphicBufferAlloc>& allocator = NULL);
    virtual ~GonkBufferQueueCore();

private:
    // Dump our state in a string
    void dump(String8& result, const char* prefix) const;

    int getSlotFromTextureClientLocked(TextureClient* client) const;

    // getMinUndequeuedBufferCountLocked returns the minimum number of buffers
    // that must remain in a state other than DEQUEUED. The async parameter
    // tells whether we're in asynchronous mode.
    int getMinUndequeuedBufferCountLocked(bool async) const;

    // getMinMaxBufferCountLocked returns the minimum number of buffers allowed
    // given the current GonkBufferQueue state. The async parameter tells whether
    // we're in asynchonous mode.
    int getMinMaxBufferCountLocked(bool async) const;

    // getMaxBufferCountLocked returns the maximum number of buffers that can be
    // allocated at once. This value depends on the following member variables:
    //
    //     mDequeueBufferCannotBlock
    //     mMaxAcquiredBufferCount
    //     mDefaultMaxBufferCount
    //     mOverrideMaxBufferCount
    //     async parameter
    //
    // Any time one of these member variables is changed while a producer is
    // connected, mDequeueCondition must be broadcast.
    int getMaxBufferCountLocked(bool async) const;

    // setDefaultMaxBufferCountLocked sets the maximum number of buffer slots
    // that will be used if the producer does not override the buffer slot
    // count. The count must be between 2 and NUM_BUFFER_SLOTS, inclusive. The
    // initial default is 2.
    status_t setDefaultMaxBufferCountLocked(int count);

    // freeBufferLocked frees the GraphicBuffer and sync resources for the
    // given slot.
    void freeBufferLocked(int slot);

    // freeAllBuffersLocked frees the GraphicBuffer and sync resources for
    // all slots.
    void freeAllBuffersLocked();

    // stillTracking returns true iff the buffer item is still being tracked
    // in one of the slots.
    bool stillTracking(const GonkBufferItem* item) const;

    // waitWhileAllocatingLocked blocks until mIsAllocating is false.
    void waitWhileAllocatingLocked() const;

    // mAllocator is the connection to SurfaceFlinger that is used to allocate
    // new GraphicBuffer objects.
    sp<IGraphicBufferAlloc> mAllocator;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of GonkBufferQueueCore objects. It must be locked whenever any
    // member variable is accessed.
    mutable Mutex mMutex;

    // mIsAbandoned indicates that the GonkBufferQueue will no longer be used to
    // consume image buffers pushed to it using the IGraphicBufferProducer
    // interface. It is initialized to false, and set to true in the
    // consumerDisconnect method. A GonkBufferQueue that is abandoned will return
    // the NO_INIT error from all IGraphicBufferProducer methods capable of
    // returning an error.
    bool mIsAbandoned;

    // mConsumerControlledByApp indicates whether the connected consumer is
    // controlled by the application.
    bool mConsumerControlledByApp;

    // mConsumerName is a string used to identify the GonkBufferQueue in log
    // messages. It is set by the IGraphicBufferConsumer::setConsumerName
    // method.
    String8 mConsumerName;

    // mConsumerListener is used to notify the connected consumer of
    // asynchronous events that it may wish to react to. It is initially
    // set to NULL and is written by consumerConnect and consumerDisconnect.
    sp<IConsumerListener> mConsumerListener;

    // mConsumerUsageBits contains flags that the consumer wants for
    // GraphicBuffers.
    uint32_t mConsumerUsageBits;

    // mConnectedApi indicates the producer API that is currently connected
    // to this GonkBufferQueue. It defaults to NO_CONNECTED_API, and gets updated
    // by the connect and disconnect methods.
    int mConnectedApi;

    // mConnectedProducerToken is used to set a binder death notification on
    // the producer.
    sp<IProducerListener> mConnectedProducerListener;

    // mSlots is an array of buffer slots that must be mirrored on the producer
    // side. This allows buffer ownership to be transferred between the producer
    // and consumer without sending a GraphicBuffer over Binder. The entire
    // array is initialized to NULL at construction time, and buffers are
    // allocated for a slot when requestBuffer is called with that slot's index.
    GonkBufferQueueDefs::SlotsType mSlots;

    // mQueue is a FIFO of queued buffers used in synchronous mode.
    Fifo mQueue;

    // mOverrideMaxBufferCount is the limit on the number of buffers that will
    // be allocated at one time. This value is set by the producer by calling
    // setBufferCount. The default is 0, which means that the producer doesn't
    // care about the number of buffers in the pool. In that case,
    // mDefaultMaxBufferCount is used as the limit.
    int mOverrideMaxBufferCount;

    // mDequeueCondition is a condition variable used for dequeueBuffer in
    // synchronous mode.
    mutable Condition mDequeueCondition;

    // mUseAsyncBuffer indicates whether an extra buffer is used in async mode
    // to prevent dequeueBuffer from blocking.
    bool mUseAsyncBuffer;

    // mDequeueBufferCannotBlock indicates whether dequeueBuffer is allowed to
    // block. This flag is set during connect when both the producer and
    // consumer are controlled by the application.
    bool mDequeueBufferCannotBlock;

    // mDefaultBufferFormat can be set so it will override the buffer format
    // when it isn't specified in dequeueBuffer.
    uint32_t mDefaultBufferFormat;

    // mDefaultWidth holds the default width of allocated buffers. It is used
    // in dequeueBuffer if a width and height of 0 are specified.
    int mDefaultWidth;

    // mDefaultHeight holds the default height of allocated buffers. It is used
    // in dequeueBuffer if a width and height of 0 are specified.
    int mDefaultHeight;

    // mDefaultMaxBufferCount is the default limit on the number of buffers that
    // will be allocated at one time. This default limit is set by the consumer.
    // The limit (as opposed to the default limit) may be overriden by the
    // producer.
    int mDefaultMaxBufferCount;

    // mMaxAcquiredBufferCount is the number of buffers that the consumer may
    // acquire at one time. It defaults to 1, and can be changed by the consumer
    // via setMaxAcquiredBufferCount, but this may only be done while no
    // producer is connected to the GonkBufferQueue. This value is used to derive
    // the value returned for the MIN_UNDEQUEUED_BUFFERS query to the producer.
    int mMaxAcquiredBufferCount;

    // mBufferHasBeenQueued is true once a buffer has been queued. It is reset
    // when something causes all buffers to be freed (e.g., changing the buffer
    // count).
    bool mBufferHasBeenQueued;

    // mFrameCounter is the free running counter, incremented on every
    // successful queueBuffer call and buffer allocation.
    uint64_t mFrameCounter;

    // mTransformHint is used to optimize for screen rotations.
    uint32_t mTransformHint;

    // mSidebandStream is a handle to the sideband buffer stream, if any
    sp<NativeHandle> mSidebandStream;

    // mIsAllocating indicates whether a producer is currently trying to allocate buffers (which
    // releases mMutex while doing the allocation proper). Producers should not modify any of the
    // FREE slots while this is true. mIsAllocatingCondition is signaled when this value changes to
    // false.
    bool mIsAllocating;

    // mIsAllocatingCondition is a condition variable used by producers to wait until mIsAllocating
    // becomes false.
    mutable Condition mIsAllocatingCondition;
}; // class GonkBufferQueueCore

} // namespace android

#endif
