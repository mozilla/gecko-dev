/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPlotMgr_DEFINED
#define GrPlotMgr_DEFINED

#include "GrTypes.h"
#include "SkTypes.h"

class GrPlotMgr : SkNoncopyable {
public:
    GrPlotMgr(int width, int height) {
        fDim.set(width, height);
        size_t needed = width * height;
        if (needed <= sizeof(fStorage)) {
            fBusy = fStorage;
        } else {
            fBusy = SkNEW_ARRAY(char, needed);
        }
        this->reset();
    }

    ~GrPlotMgr() {
        if (fBusy != fStorage) {
            delete[] fBusy;
        }
    }

    void reset() {
        sk_bzero(fBusy, fDim.fX * fDim.fY);
    }

    bool newPlot(SkIPoint16* loc) {
        char* busy = fBusy;
        for (int y = 0; y < fDim.fY; y++) {
            for (int x = 0; x < fDim.fX; x++) {
                if (!*busy) {
                    *busy = true;
                    loc->set(x, y);
                    return true;
                }
                busy++;
            }
        }
        return false;
    }

    bool isBusy(int x, int y) const {
        SkASSERT((unsigned)x < (unsigned)fDim.fX);
        SkASSERT((unsigned)y < (unsigned)fDim.fY);
        return fBusy[y * fDim.fX + x] != 0;
    }

    void freePlot(int x, int y) {
        SkASSERT((unsigned)x < (unsigned)fDim.fX);
        SkASSERT((unsigned)y < (unsigned)fDim.fY);
        fBusy[y * fDim.fX + x] = false;
    }

private:
    enum {
        STORAGE = 64
    };
    char fStorage[STORAGE];
    char* fBusy;
    SkIPoint16  fDim;
};

#endif
