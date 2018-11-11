/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMtlBuffer_DEFINED
#define GrMtlBuffer_DEFINED

#include "GrBuffer.h"

#import <metal/metal.h>

class GrMtlCaps;
class GrMtlGpu;

class GrMtlBuffer: public GrBuffer {
public:
    static GrMtlBuffer* Create(GrMtlGpu*, size_t size, GrBufferType intendedType, GrAccessPattern,
                               const void* data = nullptr);

    ~GrMtlBuffer() override;

    id<MTLBuffer> mtlBuffer() const { return fMtlBuffer; }

protected:
    GrMtlBuffer(GrMtlGpu*, size_t size, GrBufferType intendedType, GrAccessPattern);

    void onAbandon() override;
    void onRelease() override;

private:
    GrMtlGpu* mtlGpu() const;

    void onMap() override;
    void onUnmap() override;
    bool onUpdateData(const void* src, size_t srcSizeInBytes) override;

    void internalMap(size_t sizeInBytes);
    void internalUnmap(size_t sizeInBytes);

#ifdef SK_DEBUG
    void validate() const;
#endif

    GrBufferType fIntendedType;
    bool fIsDynamic;
    id<MTLBuffer> fMtlBuffer;
    id<MTLBuffer> fMappedBuffer;

    typedef GrBuffer INHERITED;
};

#endif
