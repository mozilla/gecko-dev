/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef GrSurface_DEFINED
#define GrSurface_DEFINED

#include "GrTypes.h"
#include "GrGpuResource.h"
#include "SkRect.h"

class GrTexture;
class GrRenderTarget;
struct SkImageInfo;

class GrSurface : public GrGpuResource {
public:
    SK_DECLARE_INST_COUNT(GrSurface);

    /**
     * Retrieves the width of the surface.
     *
     * @return the width in texels
     */
    int width() const { return fDesc.fWidth; }

    /**
     * Retrieves the height of the surface.
     *
     * @return the height in texels
     */
    int height() const { return fDesc.fHeight; }

    /**
     * Helper that gets the width and height of the surface as a bounding rectangle.
     */
    void getBoundsRect(SkRect* rect) const { rect->setWH(SkIntToScalar(this->width()),
                                                         SkIntToScalar(this->height())); }

    GrSurfaceOrigin origin() const {
        SkASSERT(kTopLeft_GrSurfaceOrigin == fDesc.fOrigin || kBottomLeft_GrSurfaceOrigin == fDesc.fOrigin);
        return fDesc.fOrigin;
    }

    /**
     * Retrieves the pixel config specified when the surface was created.
     * For render targets this can be kUnknown_GrPixelConfig
     * if client asked us to render to a target that has a pixel
     * config that isn't equivalent with one of our configs.
     */
    GrPixelConfig config() const { return fDesc.fConfig; }

    /**
     * Return the descriptor describing the surface
     */
    const GrTextureDesc& desc() const { return fDesc; }

    SkImageInfo info() const;

    /**
     * @return the texture associated with the surface, may be NULL.
     */
    virtual GrTexture* asTexture() = 0;
    virtual const GrTexture* asTexture() const = 0;

    /**
     * @return the render target underlying this surface, may be NULL.
     */
    virtual GrRenderTarget* asRenderTarget() = 0;
    virtual const GrRenderTarget* asRenderTarget() const = 0;

    /**
     * Checks whether this GrSurface refers to the same GPU object as other. This
     * catches the case where a GrTexture and GrRenderTarget refer to the same
     * GPU memory.
     */
    bool isSameAs(const GrSurface* other) const {
        const GrRenderTarget* thisRT = this->asRenderTarget();
        if (NULL != thisRT) {
            return thisRT == other->asRenderTarget();
        } else {
            const GrTexture* thisTex = this->asTexture();
            SkASSERT(NULL != thisTex); // We must be one or the other
            return thisTex == other->asTexture();
        }
    }

    /**
     * Reads a rectangle of pixels from the surface.
     * @param left          left edge of the rectangle to read (inclusive)
     * @param top           top edge of the rectangle to read (inclusive)
     * @param width         width of rectangle to read in pixels.
     * @param height        height of rectangle to read in pixels.
     * @param config        the pixel config of the destination buffer
     * @param buffer        memory to read the rectangle into.
     * @param rowBytes      number of bytes between consecutive rows. Zero means rows are tightly
     *                      packed.
     * @param pixelOpsFlags See the GrContext::PixelOpsFlags enum.
     *
     * @return true if the read succeeded, false if not. The read can fail because of an unsupported
     *              pixel config.
     */
    virtual bool readPixels(int left, int top, int width, int height,
                            GrPixelConfig config,
                            void* buffer,
                            size_t rowBytes = 0,
                            uint32_t pixelOpsFlags = 0) = 0;

    /**
     * Copy the src pixels [buffer, rowbytes, pixelconfig] into the surface at the specified
     * rectangle.
     * @param left          left edge of the rectangle to write (inclusive)
     * @param top           top edge of the rectangle to write (inclusive)
     * @param width         width of rectangle to write in pixels.
     * @param height        height of rectangle to write in pixels.
     * @param config        the pixel config of the source buffer
     * @param buffer        memory to read the rectangle from.
     * @param rowBytes      number of bytes between consecutive rows. Zero means rows are tightly
     *                      packed.
     * @param pixelOpsFlags See the GrContext::PixelOpsFlags enum.
     */
    virtual void writePixels(int left, int top, int width, int height,
                             GrPixelConfig config,
                             const void* buffer,
                             size_t rowBytes = 0,
                             uint32_t pixelOpsFlags = 0) = 0;

    /**
     * Write the contents of the surface to a PNG. Returns true if successful.
     * @param filename      Full path to desired file
     */
    bool savePixels(const char* filename);

protected:
    GrSurface(GrGpu* gpu, bool isWrapped, const GrTextureDesc& desc)
    : INHERITED(gpu, isWrapped)
    , fDesc(desc) {
    }

    GrTextureDesc fDesc;

private:
    typedef GrGpuResource INHERITED;
};

#endif // GrSurface_DEFINED
