/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrBackendSurface_DEFINED
#define GrBackendSurface_DEFINED

#include "GrTypes.h"
#include "gl/GrGLTypes.h"
#include "mock/GrMockTypes.h"
#include "vk/GrVkTypes.h"
#include "../private/GrVkTypesPriv.h"

class GrVkImageLayout;

#ifdef SK_METAL
#include "mtl/GrMtlTypes.h"
#endif

#if !SK_SUPPORT_GPU

// SkSurface and SkImage rely on a minimal version of these always being available
class SK_API GrBackendTexture {
public:
    GrBackendTexture() {}

    bool isValid() const { return false; }
};

class SK_API GrBackendRenderTarget {
public:
    GrBackendRenderTarget() {}

    bool isValid() const { return false; }
};
#else

class SK_API GrBackendFormat {
public:
    // Creates an invalid backend format.
    GrBackendFormat() : fValid(false) {}

    static GrBackendFormat MakeGL(GrGLenum format, GrGLenum target) {
        return GrBackendFormat(format, target);
    }

    static GrBackendFormat MakeVk(VkFormat format) {
        return GrBackendFormat(format);
    }

#ifdef SK_METAL
    static GrBackendFormat MakeMtl(GrMTLPixelFormat format) {
        return GrBackendFormat(format);
    }
#endif

    static GrBackendFormat MakeMock(GrPixelConfig config) {
        return GrBackendFormat(config);
    }

    GrBackend backend() const {return fBackend; }

    // If the backend API is GL, these return a pointer to the format and target. Otherwise
    // it returns nullptr.
    const GrGLenum* getGLFormat() const;
    const GrGLenum* getGLTarget() const;

    // If the backend API is Vulkan, this returns a pointer to a VkFormat. Otherwise
    // it returns nullptr
    const VkFormat* getVkFormat() const;

#ifdef SK_METAL
    // If the backend API is Metal, this returns a pointer to a GrMTLPixelFormat. Otherwise
    // it returns nullptr
    const GrMTLPixelFormat* getMtlFormat() const;
#endif

    // If the backend API is Mock, this returns a pointer to a GrPixelConfig. Otherwise
    // it returns nullptr.
    const GrPixelConfig* getMockFormat() const;

    // Returns true if the backend format has been initialized.
    bool isValid() const { return fValid; }

private:
    GrBackendFormat(GrGLenum format, GrGLenum target);

    GrBackendFormat(const VkFormat vkFormat);

#ifdef SK_METAL
    GrBackendFormat(const GrMTLPixelFormat mtlFormat);
#endif

    GrBackendFormat(const GrPixelConfig config);

    GrBackend fBackend;
    bool      fValid;

    union {
        struct {
            GrGLenum fTarget; // GL_TEXTURE_2D, GL_TEXTURE_EXTERNAL or GL_TEXTURE_RECTANGLE
            GrGLenum fFormat; // the sized, internal format of the GL resource
        } fGL;
        VkFormat         fVkFormat;
#ifdef SK_METAL
        GrMTLPixelFormat fMtlFormat;
#endif
        GrPixelConfig    fMockFormat;
    };
};

class SK_API GrBackendTexture {
public:
    // Creates an invalid backend texture.
    GrBackendTexture() : fIsValid(false) {}

    // The GrGLTextureInfo must have a valid fFormat.
    GrBackendTexture(int width,
                     int height,
                     GrMipMapped,
                     const GrGLTextureInfo& glInfo);

    GrBackendTexture(int width,
                     int height,
                     const GrVkImageInfo& vkInfo);

#ifdef SK_METAL
    GrBackendTexture(int width,
                     int height,
                     GrMipMapped,
                     const GrMtlTextureInfo& mtlInfo);
#endif

    GrBackendTexture(int width,
                     int height,
                     GrMipMapped,
                     const GrMockTextureInfo& mockInfo);

    GrBackendTexture(const GrBackendTexture& that);

    ~GrBackendTexture();

    GrBackendTexture& operator=(const GrBackendTexture& that);

    int width() const { return fWidth; }
    int height() const { return fHeight; }
    bool hasMipMaps() const { return GrMipMapped::kYes == fMipMapped; }
    GrBackend backend() const {return fBackend; }

    // If the backend API is GL, copies a snapshot of the GrGLTextureInfo struct into the passed in
    // pointer and returns true. Otherwise returns false if the backend API is not GL.
    bool getGLTextureInfo(GrGLTextureInfo*) const;

    // If the backend API is Vulkan, copies a snapshot of the GrVkImageInfo struct into the passed
    // in pointer and returns true. This snapshot will set the fImageLayout to the current layout
    // state. Otherwise returns false if the backend API is not Vulkan.
    bool getVkImageInfo(GrVkImageInfo*) const;

    // Anytime the client changes the VkImageLayout of the VkImage captured by this
    // GrBackendTexture, they must call this function to notify Skia of the changed layout.
    void setVkImageLayout(VkImageLayout);

#ifdef SK_METAL
    // If the backend API is Metal, copies a snapshot of the GrMtlTextureInfo struct into the passed
    // in pointer and returns true. Otherwise returns false if the backend API is not Metal.
    bool getMtlTextureInfo(GrMtlTextureInfo*) const;
#endif

    // If the backend API is Mock, copies a snapshot of the GrMockTextureInfo struct into the passed
    // in pointer and returns true. Otherwise returns false if the backend API is not Mock.
    bool getMockTextureInfo(GrMockTextureInfo*) const;

    // Returns true if the backend texture has been initialized.
    bool isValid() const { return fIsValid; }

#if GR_TEST_UTILS
    // We can remove the pixelConfig getter and setter once we remove the GrPixelConfig from the
    // GrBackendTexture and plumb the GrPixelconfig manually throughout our code (or remove all use
    // of GrPixelConfig in general).
    GrPixelConfig pixelConfig() const { return fConfig; }
    void setPixelConfig(GrPixelConfig config) { fConfig = config; }

    static bool TestingOnly_Equals(const GrBackendTexture& , const GrBackendTexture&);
#endif

private:
    // Friending for access to the GrPixelConfig
    friend class SkImage;
    friend class SkImage_Gpu;
    friend class SkImage_GpuBase;
    friend class SkImage_GpuYUVA;
    friend class SkPromiseImageHelper;
    friend class SkSurface;
    friend class GrAHardwareBufferImageGenerator;
    friend class GrBackendTextureImageGenerator;
    friend class GrProxyProvider;
    friend class GrGpu;
    friend class GrGLGpu;
    friend class GrVkGpu;
    friend class GrMtlGpu;
    friend class PromiseImageHelper;

    GrPixelConfig config() const { return fConfig; }

   // Requires friending of GrVkGpu (done above already)
   sk_sp<GrVkImageLayout> getGrVkImageLayout() const;

   friend class GrVkTexture;
#ifdef SK_VULKAN
   GrBackendTexture(int width,
                    int height,
                    const GrVkImageInfo& vkInfo,
                    sk_sp<GrVkImageLayout> layout);
#endif

    // Free and release and resources being held by the GrBackendTexture.
    void cleanup();

    bool fIsValid;
    int fWidth;         //<! width in pixels
    int fHeight;        //<! height in pixels
    GrPixelConfig fConfig;
    GrMipMapped fMipMapped;
    GrBackend fBackend;

    union {
        GrGLTextureInfo fGLInfo;
        GrVkBackendSurfaceInfo fVkInfo;
#ifdef SK_METAL
        GrMtlTextureInfo fMtlInfo;
#endif
        GrMockTextureInfo fMockInfo;
    };
};

class SK_API GrBackendRenderTarget {
public:
    // Creates an invalid backend texture.
    GrBackendRenderTarget() : fIsValid(false) {}

    // The GrGLTextureInfo must have a valid fFormat.
    GrBackendRenderTarget(int width,
                          int height,
                          int sampleCnt,
                          int stencilBits,
                          const GrGLFramebufferInfo& glInfo);

    /** Deprecated, use version that does not take stencil bits. */
    GrBackendRenderTarget(int width,
                          int height,
                          int sampleCnt,
                          int stencilBits,
                          const GrVkImageInfo& vkInfo);
    GrBackendRenderTarget(int width, int height, int sampleCnt, const GrVkImageInfo& vkInfo);

#ifdef SK_METAL
    GrBackendRenderTarget(int width,
                          int height,
                          int sampleCnt,
                          const GrMtlTextureInfo& mtlInfo);
#endif

    GrBackendRenderTarget(int width,
                          int height,
                          int sampleCnt,
                          int stencilBits,
                          const GrMockRenderTargetInfo& mockInfo);

    ~GrBackendRenderTarget();

    GrBackendRenderTarget(const GrBackendRenderTarget& that);
    GrBackendRenderTarget& operator=(const GrBackendRenderTarget&);

    int width() const { return fWidth; }
    int height() const { return fHeight; }
    int sampleCnt() const { return fSampleCnt; }
    int stencilBits() const { return fStencilBits; }
    GrBackend backend() const {return fBackend; }

    // If the backend API is GL, copies a snapshot of the GrGLFramebufferInfo struct into the passed
    // in pointer and returns true. Otherwise returns false if the backend API is not GL.
    bool getGLFramebufferInfo(GrGLFramebufferInfo*) const;

    // If the backend API is Vulkan, copies a snapshot of the GrVkImageInfo struct into the passed
    // in pointer and returns true. This snapshot will set the fImageLayout to the current layout
    // state. Otherwise returns false if the backend API is not Vulkan.
    bool getVkImageInfo(GrVkImageInfo*) const;

    // Anytime the client changes the VkImageLayout of the VkImage captured by this
    // GrBackendRenderTarget, they must call this function to notify Skia of the changed layout.
    void setVkImageLayout(VkImageLayout);

#ifdef SK_METAL
    // If the backend API is Metal, copies a snapshot of the GrMtlTextureInfo struct into the passed
    // in pointer and returns true. Otherwise returns false if the backend API is not Metal.
    bool getMtlTextureInfo(GrMtlTextureInfo*) const;
#endif

    // If the backend API is Mock, copies a snapshot of the GrMockTextureInfo struct into the passed
    // in pointer and returns true. Otherwise returns false if the backend API is not Mock.
    bool getMockRenderTargetInfo(GrMockRenderTargetInfo*) const;

    // Returns true if the backend texture has been initialized.
    bool isValid() const { return fIsValid; }


#if GR_TEST_UTILS
    // We can remove the pixelConfig getter and setter once we remove the pixel config from the
    // GrBackendRenderTarget and plumb the pixel config manually throughout our code (or remove all
    // use of GrPixelConfig in general).
    GrPixelConfig pixelConfig() const { return fConfig; }
    void setPixelConfig(GrPixelConfig config) { fConfig = config; }

    static bool TestingOnly_Equals(const GrBackendRenderTarget&, const GrBackendRenderTarget&);
#endif

private:
    // Friending for access to the GrPixelConfig
    friend class SkSurface;
    friend class SkSurface_Gpu;
    friend class SkImage_Gpu;
    friend class GrGpu;
    friend class GrGLGpu;
    friend class GrProxyProvider;
    friend class GrVkGpu;
    friend class GrMtlGpu;
    GrPixelConfig config() const { return fConfig; }

   // Requires friending of GrVkGpu (done above already)
   sk_sp<GrVkImageLayout> getGrVkImageLayout() const;

   friend class GrVkRenderTarget;
   GrBackendRenderTarget(int width, int height, int sampleCnt, const GrVkImageInfo& vkInfo,
                         sk_sp<GrVkImageLayout> layout);

    // Free and release and resources being held by the GrBackendTexture.
    void cleanup();

    bool fIsValid;
    int fWidth;         //<! width in pixels
    int fHeight;        //<! height in pixels

    int fSampleCnt;
    int fStencilBits;
    GrPixelConfig fConfig;

    GrBackend fBackend;

    union {
        GrGLFramebufferInfo fGLInfo;
        GrVkBackendSurfaceInfo fVkInfo;
#ifdef SK_METAL
        GrMtlTextureInfo fMtlInfo;
#endif
        GrMockRenderTargetInfo fMockInfo;
    };
};

#endif

#endif

