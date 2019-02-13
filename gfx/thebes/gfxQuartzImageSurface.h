/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QUARTZIMAGESURFACE_H
#define GFX_QUARTZIMAGESURFACE_H

#include "gfxASurface.h"
#include "nsSize.h"

class gfxImageSurface;

class gfxQuartzImageSurface : public gfxASurface {
public:
    explicit gfxQuartzImageSurface(gfxImageSurface *imageSurface);
    explicit gfxQuartzImageSurface(cairo_surface_t *csurf);

    virtual ~gfxQuartzImageSurface();

    already_AddRefed<gfxImageSurface> GetAsImageSurface();
    virtual int32_t KnownMemoryUsed();
    virtual const mozilla::gfx::IntSize GetSize() const { return mSize; }

protected:
    mozilla::gfx::IntSize mSize;

private:
    mozilla::gfx::IntSize ComputeSize();
};

#endif /* GFX_QUARTZIMAGESURFACE_H */
