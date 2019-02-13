/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkMergeImageFilter.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkReadBuffer.h"
#include "SkWriteBuffer.h"
#include "SkValidationUtils.h"

///////////////////////////////////////////////////////////////////////////////

void SkMergeImageFilter::initAllocModes() {
    int inputCount = countInputs();
    if (inputCount) {
        size_t size = sizeof(uint8_t) * inputCount;
        if (size <= sizeof(fStorage)) {
            fModes = SkTCast<uint8_t*>(fStorage);
        } else {
            fModes = SkTCast<uint8_t*>(sk_malloc_throw(size));
        }
    } else {
        fModes = NULL;
    }
}

void SkMergeImageFilter::initModes(const SkXfermode::Mode modes[]) {
    if (modes) {
        this->initAllocModes();
        int inputCount = countInputs();
        for (int i = 0; i < inputCount; ++i) {
            fModes[i] = SkToU8(modes[i]);
        }
    } else {
        fModes = NULL;
    }
}

SkMergeImageFilter::SkMergeImageFilter(SkImageFilter* filters[], int count,
                                       const SkXfermode::Mode modes[],
                                       const CropRect* cropRect) : INHERITED(count, filters, cropRect) {
    SkASSERT(count >= 0);
    this->initModes(modes);
}

SkMergeImageFilter::~SkMergeImageFilter() {

    if (fModes != SkTCast<uint8_t*>(fStorage)) {
        sk_free(fModes);
    }
}

bool SkMergeImageFilter::onFilterImage(Proxy* proxy, const SkBitmap& src,
                                       const Context& ctx,
                                       SkBitmap* result, SkIPoint* offset) const {
    if (countInputs() < 1) {
        return false;
    }

    SkIRect bounds;
    if (!this->applyCropRect(ctx, src, SkIPoint::Make(0, 0), &bounds)) {
        return false;
    }

    const int x0 = bounds.left();
    const int y0 = bounds.top();

    SkAutoTUnref<SkBaseDevice> dst(proxy->createDevice(bounds.width(), bounds.height()));
    if (NULL == dst) {
        return false;
    }
    SkCanvas canvas(dst);
    SkPaint paint;

    int inputCount = countInputs();
    for (int i = 0; i < inputCount; ++i) {
        SkBitmap tmp;
        const SkBitmap* srcPtr;
        SkIPoint pos = SkIPoint::Make(0, 0);
        SkImageFilter* filter = getInput(i);
        if (filter) {
            if (!filter->filterImage(proxy, src, ctx, &tmp, &pos)) {
                return false;
            }
            srcPtr = &tmp;
        } else {
            srcPtr = &src;
        }

        if (fModes) {
            paint.setXfermodeMode((SkXfermode::Mode)fModes[i]);
        } else {
            paint.setXfermode(NULL);
        }
        canvas.drawSprite(*srcPtr, pos.x() - x0, pos.y() - y0, &paint);
    }

    offset->fX = bounds.left();
    offset->fY = bounds.top();
    *result = dst->accessBitmap(false);
    return true;
}

void SkMergeImageFilter::flatten(SkWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);

    buffer.writeBool(fModes != NULL);
    if (fModes) {
        buffer.writeByteArray(fModes, countInputs() * sizeof(fModes[0]));
    }
}

SkMergeImageFilter::SkMergeImageFilter(SkReadBuffer& buffer)
  : INHERITED(-1, buffer) {
    bool hasModes = buffer.readBool();
    if (hasModes) {
        this->initAllocModes();
        int nbInputs = countInputs();
        size_t size = nbInputs * sizeof(fModes[0]);
        SkASSERT(buffer.getArrayCount() == size);
        if (buffer.validate(buffer.getArrayCount() == size) &&
            buffer.readByteArray(fModes, size)) {
            for (int i = 0; i < nbInputs; ++i) {
                buffer.validate(SkIsValidMode((SkXfermode::Mode)fModes[i]));
            }
        }
    } else {
        fModes = 0;
    }
}
