/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrContextOptions_DEFINED
#define GrContextOptions_DEFINED

#include "SkData.h"
#include "SkTypes.h"
#include "GrTypes.h"
#include "../private/GrTypesPriv.h"
#include "GrDriverBugWorkarounds.h"

#include <vector>

class SkExecutor;

#if SK_SUPPORT_GPU
struct GrContextOptions {
    enum class Enable {
        /** Forces an option to be disabled. */
        kNo,
        /** Forces an option to be enabled. */
        kYes,
        /**
         * Uses Skia's default behavior, which may use runtime properties (e.g. driver version).
         */
        kDefault
    };

    /**
     * Abstract class which stores Skia data in a cache that persists between sessions. Currently,
     * Skia stores compiled shader binaries (only when glProgramBinary / glGetProgramBinary are
     * supported) when provided a persistent cache, but this may extend to other data in the future.
     */
    class PersistentCache {
    public:
        virtual ~PersistentCache() {}

        /**
         * Returns the data for the key if it exists in the cache, otherwise returns null.
         */
        virtual sk_sp<SkData> load(const SkData& key) = 0;

        virtual void store(const SkData& key, const SkData& data) = 0;
    };

    GrContextOptions() {}

    // Suppress prints for the GrContext.
    bool fSuppressPrints = false;

    /** Overrides: These options override feature detection using backend API queries. These
        overrides can only reduce the feature set or limits, never increase them beyond the
        detected values. */

    int  fMaxTextureSizeOverride = SK_MaxS32;

    /** the threshold in bytes above which we will use a buffer mapping API to map vertex and index
        buffers to CPU memory in order to update them.  A value of -1 means the GrContext should
        deduce the optimal value for this platform. */
    int  fBufferMapThreshold = -1;

    /**
     * Executor to handle threaded work within Ganesh. If this is nullptr, then all work will be
     * done serially on the main thread. To have worker threads assist with various tasks, set this
     * to a valid SkExecutor instance. Currently, used for software path rendering, but may be used
     * for other tasks.
     */
    SkExecutor* fExecutor = nullptr;

    /** Construct mipmaps manually, via repeated downsampling draw-calls. This is used when
        the driver's implementation (glGenerateMipmap) contains bugs. This requires mipmap
        level and LOD control (ie desktop or ES3). */
    bool fDoManualMipmapping = false;

    /**
     * Disables the coverage counting path renderer. Coverage counting can sometimes cause new
     * rendering artifacts along shared edges if care isn't taken to ensure both contours wind in
     * the same direction.
     */
    bool fDisableCoverageCountingPaths = false;

    /**
     * Disables distance field rendering for paths. Distance field computation can be expensive,
     * and yields no benefit if a path is not rendered multiple times with different transforms.
     */
    bool fDisableDistanceFieldPaths = false;

    /**
     * If true this allows path mask textures to be cached. This is only really useful if paths
     * are commonly rendered at the same scale and fractional translation.
     */
    bool fAllowPathMaskCaching = true;

    /**
     * If true, the GPU will not be used to perform YUV -> RGB conversion when generating
     * textures from codec-backed images.
     */
    bool fDisableGpuYUVConversion = false;

    /**
     * The maximum size of cache textures used for Skia's Glyph cache.
     */
    size_t fGlyphCacheTextureMaximumBytes = 2048 * 1024 * 4;

    /**
     * Below this threshold size in device space distance field fonts won't be used. Distance field
     * fonts don't support hinting which is more important at smaller sizes. A negative value means
     * use the default threshold.
     */
    float fMinDistanceFieldFontSize = -1.f;

    /**
     * Above this threshold size in device space glyphs are drawn as individual paths. A negative
     * value means use the default threshold.
     */
    float fGlyphsAsPathsFontSize = -1.f;

    /**
     * Can the glyph atlas use multiple textures. If allowed, the each texture's size is bound by
     * fGlypheCacheTextureMaximumBytes.
     */
    Enable fAllowMultipleGlyphCacheTextures = Enable::kDefault;

    /**
     * Bugs on certain drivers cause stencil buffers to leak. This flag causes Skia to avoid
     * allocating stencil buffers and use alternate rasterization paths, avoiding the leak.
     */
    bool fAvoidStencilBuffers = false;

    /**
     * When specifing new data for a vertex/index buffer that replaces old data Ganesh can give
     * a hint to the driver that the previous data will not be used in future draws like this:
     *  glBufferData(GL_..._BUFFER, size, NULL, usage);       //<--hint, NULL means
     *  glBufferSubData(GL_..._BUFFER, 0, lessThanSize, data) //   old data can't be
     *                                                        //   used again.
     * However, this can be an unoptimization on some platforms, esp. Chrome.
     * Chrome's cmd buffer will create a new allocation and memset the whole thing
     * to zero (for security reasons).
     * Defaults to the value of GR_GL_USE_BUFFER_DATA_NULL_HINT #define (which is, by default, 1).
     */
    Enable fUseGLBufferDataNullHint = Enable::kDefault;

    /**
     * If true, texture fetches from mip-mapped textures will be biased to read larger MIP levels.
     * This has the effect of sharpening those textures, at the cost of some aliasing, and possible
     * performance impact.
     */
    bool fSharpenMipmappedTextures = false;

    /**
     * Enables driver workaround to use draws instead of glClear. This only applies to
     * kOpenGL_GrBackend.
     */
    Enable fUseDrawInsteadOfGLClear = Enable::kDefault;

    /**
     * Allow Ganesh to explicitly allocate resources at flush time rather than incrementally while
     * drawing. This will eventually just be the way it is but, for now, it is optional.
     */
    Enable fExplicitlyAllocateGPUResources = Enable::kDefault;

    /**
     * Allow Ganesh to sort the opLists prior to allocating resources. This is an optional
     * behavior that is only relevant when 'fExplicitlyAllocateGPUResources' is enabled.
     * Eventually this will just be what is done and will not be optional.
     */
    Enable fSortRenderTargets = Enable::kDefault;

    /**
     * Allow Ganesh to more aggressively reorder operations. This is an optional
     * behavior that is only relevant when 'fSortRenderTargets' is enabled.
     * Eventually this will just be what is done and will not be optional.
     */
    Enable fReduceOpListSplitting = Enable::kDefault;

    /**
     * Some ES3 contexts report the ES2 external image extension, but not the ES3 version.
     * If support for external images is critical, enabling this option will cause Ganesh to limit
     * shaders to the ES2 shading language in that situation.
     */
    bool fPreferExternalImagesOverES3 = false;

    /**
     * Disables correctness workarounds that are enabled for particular GPUs, OSes, or drivers.
     * This does not affect code path choices that are made for perfomance reasons nor does it
     * override other GrContextOption settings.
     */
    bool fDisableDriverCorrectnessWorkarounds = false;

    /**
     * Cache in which to store compiled shader binaries between runs.
     */
    PersistentCache* fPersistentCache = nullptr;

#if GR_TEST_UTILS
    /**
     * Private options that are only meant for testing within Skia's tools.
     */

    /**
     * If non-zero, overrides the maximum size of a tile for sw-backed images and bitmaps rendered
     * by SkGpuDevice.
     */
    int  fMaxTileSizeOverride = 0;

    /**
     * Prevents use of dual source blending, to test that all xfer modes work correctly without it.
     */
    bool fSuppressDualSourceBlending = false;

    /**
     * If true, the caps will never report driver support for path rendering.
     */
    bool fSuppressPathRendering = false;

    /**
     * If true, the caps will never support geometry shaders.
     */
    bool fSuppressGeometryShaders = false;

    /**
     * Render everything in wireframe
     */
    bool fWireframeMode = false;

    /**
     * Include or exclude specific GPU path renderers.
     */
    GpuPathRenderers fGpuPathRenderers = GpuPathRenderers::kAll;

    /**
     * Disables using multiple texture units to batch multiple images into a single draw on
     * supported GPUs.
     */
    bool fDisableImageMultitexturing = false;
#endif

#if SK_SUPPORT_ATLAS_TEXT
    /**
     * Controls whether distance field glyph vertices always have 3 components even when the view
     * matrix does not have perspective.
     */
    Enable fDistanceFieldGlyphVerticesAlwaysHaveW = Enable::kDefault;
#endif

    GrDriverBugWorkarounds fDriverBugWorkarounds;
};
#else
struct GrContextOptions {
    struct PersistentCache {};
};
#endif

#endif
