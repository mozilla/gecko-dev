/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ISurfaceAllocator.h"     // for GfxMemoryImageReporter

#include "prlog.h"

#include "gfxPlatform.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "nsXULAppAPI.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"

#if defined(XP_WIN)
#include "gfxWindowsPlatform.h"
#include "gfxD2DSurface.h"
#elif defined(XP_MACOSX)
#include "gfxPlatformMac.h"
#include "gfxQuartzSurface.h"
#elif defined(MOZ_WIDGET_GTK)
#include "gfxPlatformGtk.h"
#elif defined(MOZ_WIDGET_QT)
#include "gfxQtPlatform.h"
#elif defined(XP_OS2)
#include "gfxOS2Platform.h"
#elif defined(ANDROID)
#include "gfxAndroidPlatform.h"
#endif

#include "nsGkAtoms.h"
#include "gfxPlatformFontList.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsUnicodeProperties.h"
#include "harfbuzz/hb.h"
#include "gfxGraphiteShaper.h"
#include "gfx2DGlue.h"
#include "gfxGradientCache.h"

#include "nsUnicodeRange.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsILocaleService.h"
#include "nsIObserverService.h"
#include "MainThreadUtils.h"

#include "nsWeakReference.h"

#include "cairo.h"
#include "qcms.h"

#include "plstr.h"
#include "nsCRT.h"
#include "GLContext.h"
#include "GLContextProvider.h"

#ifdef MOZ_WIDGET_ANDROID
#include "TexturePoolOGL.h"
#endif

#ifdef USE_SKIA
#include "mozilla/Hal.h"
#include "skia/SkGraphics.h"
#endif

#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

#include "nsIGfxInfo.h"
#include "nsIXULRuntime.h"

using namespace mozilla;
using namespace mozilla::layers;

gfxPlatform *gPlatform = nullptr;
static bool gEverInitialized = false;

static Mutex* gGfxPlatformPrefsLock = nullptr;

// These two may point to the same profile
static qcms_profile *gCMSOutputProfile = nullptr;
static qcms_profile *gCMSsRGBProfile = nullptr;

static qcms_transform *gCMSRGBTransform = nullptr;
static qcms_transform *gCMSInverseRGBTransform = nullptr;
static qcms_transform *gCMSRGBATransform = nullptr;

static bool gCMSInitialized = false;
static eCMSMode gCMSMode = eCMSMode_Off;
static int gCMSIntent = -2;

static void ShutdownCMS();
static void MigratePrefs();

static bool sDrawFrameCounter = false;

#include "mozilla/gfx/2D.h"
using namespace mozilla::gfx;

// logs shared across gfx
#ifdef PR_LOGGING
static PRLogModuleInfo *sFontlistLog = nullptr;
static PRLogModuleInfo *sFontInitLog = nullptr;
static PRLogModuleInfo *sTextrunLog = nullptr;
static PRLogModuleInfo *sTextrunuiLog = nullptr;
static PRLogModuleInfo *sCmapDataLog = nullptr;
static PRLogModuleInfo *sTextPerfLog = nullptr;
#endif

/* Class to listen for pref changes so that chrome code can dynamically
   force sRGB as an output profile. See Bug #452125. */
class SRGBOverrideObserver MOZ_FINAL : public nsIObserver,
                                       public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS2(SRGBOverrideObserver, nsIObserver, nsISupportsWeakReference)

#define GFX_DOWNLOADABLE_FONTS_ENABLED "gfx.downloadable_fonts.enabled"

#define GFX_PREF_HARFBUZZ_SCRIPTS "gfx.font_rendering.harfbuzz.scripts"
#define HARFBUZZ_SCRIPTS_DEFAULT  mozilla::unicode::SHAPING_DEFAULT
#define GFX_PREF_FALLBACK_USE_CMAPS  "gfx.font_rendering.fallback.always_use_cmaps"

#define GFX_PREF_OPENTYPE_SVG "gfx.font_rendering.opentype_svg.enabled"

#define GFX_PREF_WORD_CACHE_CHARLIMIT "gfx.font_rendering.wordcache.charlimit"
#define GFX_PREF_WORD_CACHE_MAXENTRIES "gfx.font_rendering.wordcache.maxentries"

#define GFX_PREF_GRAPHITE_SHAPING "gfx.font_rendering.graphite.enabled"

#define BIDI_NUMERAL_PREF "bidi.numeral"

#define GFX_PREF_CMS_RENDERING_INTENT "gfx.color_management.rendering_intent"
#define GFX_PREF_CMS_DISPLAY_PROFILE "gfx.color_management.display_profile"
#define GFX_PREF_CMS_ENABLED_OBSOLETE "gfx.color_management.enabled"
#define GFX_PREF_CMS_FORCE_SRGB "gfx.color_management.force_srgb"
#define GFX_PREF_CMS_ENABLEV4 "gfx.color_management.enablev4"
#define GFX_PREF_CMS_MODE "gfx.color_management.mode"

NS_IMETHODIMP
SRGBOverrideObserver::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const char16_t* someData)
{
    NS_ASSERTION(NS_strcmp(someData,
                           MOZ_UTF16(GFX_PREF_CMS_FORCE_SRGB)) == 0,
                 "Restarting CMS on wrong pref!");
    ShutdownCMS();
    return NS_OK;
}

static const char* kObservedPrefs[] = {
    "gfx.downloadable_fonts.",
    "gfx.font_rendering.",
    BIDI_NUMERAL_PREF,
    nullptr
};

class FontPrefsObserver MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(FontPrefsObserver, nsIObserver)

NS_IMETHODIMP
FontPrefsObserver::Observe(nsISupports *aSubject,
                           const char *aTopic,
                           const char16_t *someData)
{
    if (!someData) {
        NS_ERROR("font pref observer code broken");
        return NS_ERROR_UNEXPECTED;
    }
    NS_ASSERTION(gfxPlatform::GetPlatform(), "the singleton instance has gone");
    gfxPlatform::GetPlatform()->FontsPrefsChanged(NS_ConvertUTF16toUTF8(someData).get());

    return NS_OK;
}

class OrientationSyncPrefsObserver MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(OrientationSyncPrefsObserver, nsIObserver)

NS_IMETHODIMP
OrientationSyncPrefsObserver::Observe(nsISupports *aSubject,
                                      const char *aTopic,
                                      const char16_t *someData)
{
    if (!someData) {
        NS_ERROR("orientation sync pref observer broken");
        return NS_ERROR_UNEXPECTED;
    }
    NS_ASSERTION(gfxPlatform::GetPlatform(), "the singleton instance has gone");
    gfxPlatform::GetPlatform()->OrientationSyncPrefsObserverChanged();

    return NS_OK;
}

class MemoryPressureObserver MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(MemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
MemoryPressureObserver::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const char16_t *someData)
{
    NS_ASSERTION(strcmp(aTopic, "memory-pressure") == 0, "unexpected event topic");
    Factory::PurgeAllCaches();
    return NS_OK;
}

// this needs to match the list of pref font.default.xx entries listed in all.js!
// the order *must* match the order in eFontPrefLang
static const char *gPrefLangNames[] = {
    "x-western",
    "x-central-euro",
    "ja",
    "zh-TW",
    "zh-CN",
    "zh-HK",
    "ko",
    "x-cyrillic",
    "x-baltic",
    "el",
    "tr",
    "th",
    "he",
    "ar",
    "x-devanagari",
    "x-tamil",
    "x-armn",
    "x-beng",
    "x-cans",
    "x-ethi",
    "x-geor",
    "x-gujr",
    "x-guru",
    "x-khmr",
    "x-mlym",
    "x-orya",
    "x-telu",
    "x-knda",
    "x-sinh",
    "x-tibt",
    "x-unicode",
};

gfxPlatform::gfxPlatform()
  : mAzureCanvasBackendCollector(MOZ_THIS_IN_INITIALIZER_LIST(),
                                 &gfxPlatform::GetAzureBackendInfo)
  , mDrawLayerBorders(false)
  , mDrawTileBorders(false)
  , mDrawBigImageBorders(false)
{
    mUseHarfBuzzScripts = UNINITIALIZED_VALUE;
    mAllowDownloadableFonts = UNINITIALIZED_VALUE;
    mFallbackUsesCmaps = UNINITIALIZED_VALUE;

    mWordCacheCharLimit = UNINITIALIZED_VALUE;
    mWordCacheMaxEntries = UNINITIALIZED_VALUE;
    mGraphiteShapingEnabled = UNINITIALIZED_VALUE;
    mOpenTypeSVGEnabled = UNINITIALIZED_VALUE;
    mBidiNumeralOption = UNINITIALIZED_VALUE;

    mLayersPreferMemoryOverShmem = XRE_GetProcessType() == GeckoProcessType_Default;

#ifdef XP_WIN
    // XXX - When 957560 is fixed, the pref can go away entirely
    mLayersUseDeprecated =
        Preferences::GetBool("layers.use-deprecated-textures", true)
        && !Preferences::GetBool("layers.prefer-opengl", false);
#else
    mLayersUseDeprecated = false;
#endif

    Preferences::AddBoolVarCache(&mDrawLayerBorders,
                                 "layers.draw-borders",
                                 false);
    Preferences::AddBoolVarCache(&mDrawTileBorders,
                                 "layers.draw-tile-borders",
                                 false);
    Preferences::AddBoolVarCache(&mDrawBigImageBorders,
                                 "layers.draw-bigimage-borders",
                                 false);

    uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO) | BackendTypeBit(BackendType::SKIA);
    uint32_t contentMask = BackendTypeBit(BackendType::CAIRO);
    InitBackendPrefs(canvasMask, BackendType::CAIRO,
                     contentMask, BackendType::CAIRO);
}

gfxPlatform*
gfxPlatform::GetPlatform()
{
    if (!gPlatform) {
        Init();
    }
    return gPlatform;
}

void RecordingPrefChanged(const char *aPrefName, void *aClosure)
{
  if (Preferences::GetBool("gfx.2d.recording", false)) {
    nsAutoCString fileName;
    nsAdoptingString prefFileName = Preferences::GetString("gfx.2d.recordingfile");

    if (prefFileName) {
      fileName.Append(NS_ConvertUTF16toUTF8(prefFileName));
    } else {
      nsCOMPtr<nsIFile> tmpFile;
      if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile)))) {
        return;
      }
      fileName.AppendPrintf("moz2drec_%i_%i.aer", XRE_GetProcessType(), getpid());

      nsresult rv = tmpFile->AppendNative(fileName);
      if (NS_FAILED(rv))
        return;

      rv = tmpFile->GetNativePath(fileName);
      if (NS_FAILED(rv))
        return;
    }

    gPlatform->mRecorder = Factory::CreateEventRecorderForFile(fileName.BeginReading());
    printf_stderr("Recording to %s\n", fileName.get());
    Factory::SetGlobalEventRecorder(gPlatform->mRecorder);
  } else {
    Factory::SetGlobalEventRecorder(nullptr);
  }
}

void
gfxPlatform::Init()
{
    if (gEverInitialized) {
        NS_RUNTIMEABORT("Already started???");
    }
    gEverInitialized = true;

#ifdef PR_LOGGING
    sFontlistLog = PR_NewLogModule("fontlist");
    sFontInitLog = PR_NewLogModule("fontinit");
    sTextrunLog = PR_NewLogModule("textrun");
    sTextrunuiLog = PR_NewLogModule("textrunui");
    sCmapDataLog = PR_NewLogModule("cmapdata");
    sTextPerfLog = PR_NewLogModule("textperf");
#endif

    gGfxPlatformPrefsLock = new Mutex("gfxPlatform::gGfxPlatformPrefsLock");

    /* Initialize the GfxInfo service.
     * Note: we can't call functions on GfxInfo that depend
     * on gPlatform until after it has been initialized
     * below. GfxInfo initialization annotates our
     * crash reports so we want to do it before
     * we try to load any drivers and do device detection
     * incase that code crashes. See bug #591561. */
    nsCOMPtr<nsIGfxInfo> gfxInfo;
    /* this currently will only succeed on Windows */
    gfxInfo = do_GetService("@mozilla.org/gfx/info;1");

#if defined(XP_WIN)
    gPlatform = new gfxWindowsPlatform;
#elif defined(XP_MACOSX)
    gPlatform = new gfxPlatformMac;
#elif defined(MOZ_WIDGET_GTK)
    gPlatform = new gfxPlatformGtk;
#elif defined(MOZ_WIDGET_QT)
    gPlatform = new gfxQtPlatform;
#elif defined(XP_OS2)
    gPlatform = new gfxOS2Platform;
#elif defined(ANDROID)
    gPlatform = new gfxAndroidPlatform;
#else
    #error "No gfxPlatform implementation available"
#endif

#ifdef DEBUG
    mozilla::gl::GLContext::StaticInit();
#endif

    bool useOffMainThreadCompositing = OffMainThreadCompositionRequired() ||
                                       GetPrefLayersOffMainThreadCompositionEnabled();

    if (!OffMainThreadCompositionRequired()) {
      useOffMainThreadCompositing &= GetPlatform()->SupportsOffMainThreadCompositing();
    }

    if (useOffMainThreadCompositing && (XRE_GetProcessType() == GeckoProcessType_Default)) {
        CompositorParent::StartUp();
        if (AsyncVideoEnabled()) {
            ImageBridgeChild::StartUp();
        }
    }

    nsresult rv;

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(ANDROID) // temporary, until this is implemented on others
    rv = gfxPlatformFontList::Init();
    if (NS_FAILED(rv)) {
        NS_RUNTIMEABORT("Could not initialize gfxPlatformFontList");
    }
#endif

    gPlatform->mScreenReferenceSurface =
        gPlatform->CreateOffscreenSurface(gfxIntSize(1,1),
                                          gfxContentType::COLOR_ALPHA);
    if (!gPlatform->mScreenReferenceSurface) {
        NS_RUNTIMEABORT("Could not initialize mScreenReferenceSurface");
    }

    if (gPlatform->SupportsAzureContent()) {
        gPlatform->mScreenReferenceDrawTarget =
            gPlatform->CreateOffscreenContentDrawTarget(IntSize(1, 1),
                                                        SurfaceFormat::B8G8R8A8);
      if (!gPlatform->mScreenReferenceDrawTarget) {
        NS_RUNTIMEABORT("Could not initialize mScreenReferenceDrawTarget");
      }
    }

    rv = gfxFontCache::Init();
    if (NS_FAILED(rv)) {
        NS_RUNTIMEABORT("Could not initialize gfxFontCache");
    }

    /* Pref migration hook. */
    MigratePrefs();

    /* Create and register our CMS Override observer. */
    gPlatform->mSRGBOverrideObserver = new SRGBOverrideObserver();
    Preferences::AddWeakObserver(gPlatform->mSRGBOverrideObserver, GFX_PREF_CMS_FORCE_SRGB);

    gPlatform->mFontPrefsObserver = new FontPrefsObserver();
    Preferences::AddStrongObservers(gPlatform->mFontPrefsObserver, kObservedPrefs);

    gPlatform->mOrientationSyncPrefsObserver = new OrientationSyncPrefsObserver();
    Preferences::AddStrongObserver(gPlatform->mOrientationSyncPrefsObserver, "layers.orientation.sync.timeout");

    gPlatform->mWorkAroundDriverBugs = Preferences::GetBool("gfx.work-around-driver-bugs", true);

    mozilla::Preferences::AddBoolVarCache(&gPlatform->mWidgetUpdateFlashing,
                                          "nglayout.debug.widget_update_flashing");

    mozilla::gl::GLContext::PlatformStartup();

#ifdef MOZ_WIDGET_ANDROID
    // Texture pool init
    mozilla::gl::TexturePoolOGL::Init();
#endif

    // Force registration of the gfx component, thus arranging for
    // ::Shutdown to be called.
    nsCOMPtr<nsISupports> forceReg
        = do_CreateInstance("@mozilla.org/gfx/init;1");

    Preferences::RegisterCallbackAndCall(RecordingPrefChanged, "gfx.2d.recording", nullptr);

    gPlatform->mOrientationSyncMillis = Preferences::GetUint("layers.orientation.sync.timeout", (uint32_t)0);

    mozilla::Preferences::AddBoolVarCache(&sDrawFrameCounter,
                                          "layers.frame-counter",
                                          false);

    CreateCMSOutputProfile();

#ifdef USE_SKIA
    gPlatform->InitializeSkiaCaches();
#endif

    // Listen to memory pressure event so we can purge DrawTarget caches
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        gPlatform->mMemoryPressureObserver = new MemoryPressureObserver();
        obs->AddObserver(gPlatform->mMemoryPressureObserver, "memory-pressure", false);
    }

    RegisterStrongMemoryReporter(new GfxMemoryImageReporter());
}

void
gfxPlatform::Shutdown()
{
    // These may be called before the corresponding subsystems have actually
    // started up. That's OK, they can handle it.
    gfxFontCache::Shutdown();
    gfxFontGroup::Shutdown();
    gfxGradientCache::Shutdown();
    gfxGraphiteShaper::Shutdown();
#if defined(XP_MACOSX) || defined(XP_WIN) // temporary, until this is implemented on others
    gfxPlatformFontList::Shutdown();
#endif

    // Free the various non-null transforms and loaded profiles
    ShutdownCMS();

    // In some cases, gPlatform may not be created but Shutdown() called,
    // e.g., during xpcshell tests.
    if (gPlatform) {
        /* Unregister our CMS Override callback. */
        NS_ASSERTION(gPlatform->mSRGBOverrideObserver, "mSRGBOverrideObserver has alreay gone");
        Preferences::RemoveObserver(gPlatform->mSRGBOverrideObserver, GFX_PREF_CMS_FORCE_SRGB);
        gPlatform->mSRGBOverrideObserver = nullptr;

        NS_ASSERTION(gPlatform->mFontPrefsObserver, "mFontPrefsObserver has alreay gone");
        Preferences::RemoveObservers(gPlatform->mFontPrefsObserver, kObservedPrefs);
        gPlatform->mFontPrefsObserver = nullptr;

        NS_ASSERTION(gPlatform->mMemoryPressureObserver, "mMemoryPressureObserver has already gone");
        nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
        if (obs) {
            obs->RemoveObserver(gPlatform->mMemoryPressureObserver, "memory-pressure");
        }

        gPlatform->mMemoryPressureObserver = nullptr;
    }

#ifdef MOZ_WIDGET_ANDROID
    // Shut down the texture pool
    mozilla::gl::TexturePoolOGL::Shutdown();
#endif

    // Shut down the default GL context provider.
    mozilla::gl::GLContextProvider::Shutdown();

#if defined(XP_WIN)
    // The above shutdown calls operate on the available context providers on
    // most platforms.  Windows is a "special snowflake", though, and has three
    // context providers available, so we have to shut all of them down.
    // We should only support the default GL provider on Windows; then, this
    // could go away. Unfortunately, we currently support WGL (the default) for
    // WebGL on Optimus.
    mozilla::gl::GLContextProviderEGL::Shutdown();
#endif

    // This will block this thread untill the ImageBridge protocol is completely
    // deleted.
    ImageBridgeChild::ShutDown();

    CompositorParent::ShutDown();

    delete gGfxPlatformPrefsLock;

    delete gPlatform;
    gPlatform = nullptr;
}

gfxPlatform::~gfxPlatform()
{
    mScreenReferenceSurface = nullptr;
    mScreenReferenceDrawTarget = nullptr;

    // The cairo folks think we should only clean up in debug builds,
    // but we're generally in the habit of trying to shut down as
    // cleanly as possible even in production code, so call this
    // cairo_debug_* function unconditionally.
    //
    // because cairo can assert and thus crash on shutdown, don't do this in release builds
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING) || defined(NS_TRACE_MALLOC) || defined(MOZ_VALGRIND)
#ifdef USE_SKIA
    // must do Skia cleanup before Cairo cleanup, because Skia may be referencing
    // Cairo objects e.g. through SkCairoFTTypeface
    SkGraphics::Term();
#endif

#if MOZ_TREE_CAIRO
    cairo_debug_reset_static_data();
#endif
#endif

#if 0
    // It would be nice to do this (although it might need to be after
    // the cairo shutdown that happens in ~gfxPlatform).  It even looks
    // idempotent.  But it has fatal assertions that fire if stuff is
    // leaked, and we hit them.
    FcFini();
#endif
}

bool
gfxPlatform::PreferMemoryOverShmem() const {
  MOZ_ASSERT(!CompositorParent::IsInCompositorThread());
  return mLayersPreferMemoryOverShmem;
}

already_AddRefed<gfxASurface>
gfxPlatform::CreateOffscreenImageSurface(const gfxIntSize& aSize,
                                         gfxContentType aContentType)
{
  nsRefPtr<gfxASurface> newSurface;
  newSurface = new gfxImageSurface(aSize, OptimalFormatForContent(aContentType));

  return newSurface.forget();
}

already_AddRefed<gfxASurface>
gfxPlatform::OptimizeImage(gfxImageSurface *aSurface,
                           gfxImageFormat format)
{
    const gfxIntSize& surfaceSize = aSurface->GetSize();

#ifdef XP_WIN
    if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
        gfxWindowsPlatform::RENDER_DIRECT2D) {
        return nullptr;
    }
#endif
    nsRefPtr<gfxASurface> optSurface = CreateOffscreenSurface(surfaceSize, gfxASurface::ContentFromFormat(format));
    if (!optSurface || optSurface->CairoStatus() != 0)
        return nullptr;

    gfxContext tmpCtx(optSurface);
    tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx.SetSource(aSurface);
    tmpCtx.Paint();

    return optSurface.forget();
}

cairo_user_data_key_t kDrawTarget;

RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForSurface(gfxASurface *aSurface, const IntSize& aSize)
{
  RefPtr<DrawTarget> drawTarget = Factory::CreateDrawTargetForCairoSurface(aSurface->CairoSurface(), aSize);
  aSurface->SetData(&kDrawTarget, drawTarget, nullptr);
  return drawTarget;
}

// This is a temporary function used by ContentClient to build a DrawTarget
// around the gfxASurface. This should eventually be replaced by plumbing
// the DrawTarget through directly
RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForUpdateSurface(gfxASurface *aSurface, const IntSize& aSize)
{
#ifdef XP_MACOSX
  // this is a bit of a hack that assumes that the buffer associated with the CGContext
  // will live around long enough that nothing bad will happen.
  if (aSurface->GetType() == gfxSurfaceType::Quartz) {
    return Factory::CreateDrawTargetForCairoCGContext(static_cast<gfxQuartzSurface*>(aSurface)->GetCGContext(), aSize);
  }
#endif
  MOZ_CRASH();
  return nullptr;
}


cairo_user_data_key_t kSourceSurface;

/**
 * Record the backend that was used to construct the SourceSurface.
 * When getting the cached SourceSurface for a gfxASurface/DrawTarget pair,
 * we check to make sure the DrawTarget's backend matches the backend
 * for the cached SourceSurface, and only use it if they match. This
 * can avoid expensive and unnecessary readbacks.
 */
struct SourceSurfaceUserData
{
  RefPtr<SourceSurface> mSrcSurface;
  BackendType mBackendType;
};

void SourceBufferDestroy(void *srcSurfUD)
{
  delete static_cast<SourceSurfaceUserData*>(srcSurfUD);
}

#if MOZ_TREE_CAIRO
void SourceSnapshotDetached(cairo_surface_t *nullSurf)
{
  gfxImageSurface* origSurf =
    static_cast<gfxImageSurface*>(cairo_surface_get_user_data(nullSurf, &kSourceSurface));

  origSurf->SetData(&kSourceSurface, nullptr, nullptr);
}
#else
void SourceSnapshotDetached(void *nullSurf)
{
  gfxImageSurface* origSurf = static_cast<gfxImageSurface*>(nullSurf);
  origSurf->SetData(&kSourceSurface, nullptr, nullptr);
}
#endif

void
gfxPlatform::ClearSourceSurfaceForSurface(gfxASurface *aSurface)
{
  aSurface->SetData(&kSourceSurface, nullptr, nullptr);
}

RefPtr<SourceSurface>
gfxPlatform::GetSourceSurfaceForSurface(DrawTarget *aTarget, gfxASurface *aSurface)
{
  if (!aSurface->CairoSurface() || aSurface->CairoStatus()) {
    return nullptr;
  }

  if (!aTarget) {
    if (ScreenReferenceDrawTarget()) {
      aTarget = ScreenReferenceDrawTarget();
    } else {
      return nullptr;
    }
  }

  void *userData = aSurface->GetData(&kSourceSurface);

  if (userData) {
    SourceSurfaceUserData *surf = static_cast<SourceSurfaceUserData*>(userData);

    if (surf->mSrcSurface->IsValid() && surf->mBackendType == aTarget->GetType()) {
      return surf->mSrcSurface;
    }
    // We can just continue here as when setting new user data the destroy
    // function will be called for the old user data.
  }

  SurfaceFormat format;
  if (aSurface->GetContentType() == gfxContentType::ALPHA) {
    format = SurfaceFormat::A8;
  } else if (aSurface->GetContentType() == gfxContentType::COLOR) {
    format = SurfaceFormat::B8G8R8X8;
  } else {
    format = SurfaceFormat::B8G8R8A8;
  }

  RefPtr<SourceSurface> srcBuffer;

#ifdef XP_WIN
  if (aSurface->GetType() == gfxSurfaceType::D2D &&
      format != SurfaceFormat::A8) {
    NativeSurface surf;
    surf.mFormat = format;
    surf.mType = NativeSurfaceType::D3D10_TEXTURE;
    surf.mSurface = static_cast<gfxD2DSurface*>(aSurface)->GetTexture();
    mozilla::gfx::DrawTarget *dt = static_cast<mozilla::gfx::DrawTarget*>(aSurface->GetData(&kDrawTarget));
    if (dt) {
      dt->Flush();
    }
    srcBuffer = aTarget->CreateSourceSurfaceFromNativeSurface(surf);
  } else
#endif
  if (aSurface->CairoSurface() && aTarget->GetType() == BackendType::CAIRO) {
    // If this is an xlib cairo surface we don't want to fetch it into memory
    // because this is a major slow down.
    NativeSurface surf;
    surf.mFormat = format;
    surf.mType = NativeSurfaceType::CAIRO_SURFACE;
    surf.mSurface = aSurface->CairoSurface();
    srcBuffer = aTarget->CreateSourceSurfaceFromNativeSurface(surf);

    if (srcBuffer) {
      // It's cheap enough to make a new one so we won't keep it around and
      // keeping it creates a cycle.
      return srcBuffer;
    }
  }

  if (!srcBuffer) {
    nsRefPtr<gfxImageSurface> imgSurface = aSurface->GetAsImageSurface();

    bool isWin32ImageSurf = imgSurface &&
                            aSurface->GetType() == gfxSurfaceType::Win32;

    if (!imgSurface) {
      imgSurface = new gfxImageSurface(aSurface->GetSize(), OptimalFormatForContent(aSurface->GetContentType()));
      nsRefPtr<gfxContext> ctx = new gfxContext(imgSurface);
      ctx->SetSource(aSurface);
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->Paint();
    }

    gfxImageFormat cairoFormat = imgSurface->Format();
    switch(cairoFormat) {
      case gfxImageFormat::ARGB32:
        format = SurfaceFormat::B8G8R8A8;
        break;
      case gfxImageFormat::RGB24:
        format = SurfaceFormat::B8G8R8X8;
        break;
      case gfxImageFormat::A8:
        format = SurfaceFormat::A8;
        break;
      case gfxImageFormat::RGB16_565:
        format = SurfaceFormat::R5G6B5;
        break;
      default:
        NS_RUNTIMEABORT("Invalid surface format!");
    }

    IntSize size = IntSize(imgSurface->GetSize().width, imgSurface->GetSize().height);
    srcBuffer = aTarget->CreateSourceSurfaceFromData(imgSurface->Data(),
                                                     size,
                                                     imgSurface->Stride(),
                                                     format);

    if (!srcBuffer) {
      // We need to check if our gfxASurface will keep the underlying data
      // alive. This is true if gfxASurface actually -is- an ImageSurface or
      // if it is a gfxWindowsSurface which supports GetAsImageSurface.
      if (imgSurface != aSurface && !isWin32ImageSurf) {
        return nullptr;
      }

      srcBuffer = Factory::CreateWrappingDataSourceSurface(imgSurface->Data(),
                                                           imgSurface->Stride(),
                                                           size, format);

    }

#if MOZ_TREE_CAIRO
    cairo_surface_t *nullSurf =
	cairo_null_surface_create(CAIRO_CONTENT_COLOR_ALPHA);
    cairo_surface_set_user_data(nullSurf,
                                &kSourceSurface,
                                imgSurface,
                                nullptr);
    cairo_surface_attach_snapshot(imgSurface->CairoSurface(), nullSurf, SourceSnapshotDetached);
    cairo_surface_destroy(nullSurf);
#else
    cairo_surface_set_mime_data(imgSurface->CairoSurface(), "mozilla/magic", (const unsigned char*) "data", 4, SourceSnapshotDetached, imgSurface.get());
#endif
  }

  SourceSurfaceUserData *srcSurfUD = new SourceSurfaceUserData;
  srcSurfUD->mBackendType = aTarget->GetType();
  srcSurfUD->mSrcSurface = srcBuffer;
  aSurface->SetData(&kSourceSurface, srcSurfUD, SourceBufferDestroy);

  return srcBuffer;
}

TemporaryRef<ScaledFont>
gfxPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
  NativeFont nativeFont;
  nativeFont.mType = NativeFontType::CAIRO_FONT_FACE;
  nativeFont.mFont = aFont->GetCairoScaledFont();
  RefPtr<ScaledFont> scaledFont =
    Factory::CreateScaledFontForNativeFont(nativeFont,
                                           aFont->GetAdjustedSize());
  return scaledFont;
}

cairo_user_data_key_t kDrawSourceSurface;
static void
DataSourceSurfaceDestroy(void *dataSourceSurface)
{
  static_cast<DataSourceSurface*>(dataSourceSurface)->Release();
}

cairo_user_data_key_t kDrawTargetForSurface;
static void
DataDrawTargetDestroy(void *aTarget)
{
  static_cast<DrawTarget*>(aTarget)->Release();
}

bool
gfxPlatform::SupportsAzureContentForDrawTarget(DrawTarget* aTarget)
{
  if (!aTarget) {
    return false;
  }

  return SupportsAzureContentForType(aTarget->GetType());
}

bool
gfxPlatform::UseAcceleratedSkiaCanvas()
{
  return Preferences::GetBool("gfx.canvas.azure.accelerated", false) &&
         mPreferredCanvasBackend == BackendType::SKIA;
}

void
gfxPlatform::InitializeSkiaCaches()
{
#ifdef USE_SKIA_GPU
  if (UseAcceleratedSkiaCanvas()) {
    bool usingDynamicCache = Preferences::GetBool("gfx.canvas.skiagl.dynamic-cache", false);

    int cacheItemLimit = Preferences::GetInt("gfx.canvas.skiagl.cache-items", 256);
    int cacheSizeLimit = Preferences::GetInt("gfx.canvas.skiagl.cache-size", 96);

    // Prefs are in megabytes, but we want the sizes in bytes
    cacheSizeLimit *= 1024*1024;

    if (usingDynamicCache) {
      uint32_t totalMemory = mozilla::hal::GetTotalSystemMemory();

      if (totalMemory <= 256*1024*1024) {
        // We need a very minimal cache on 256 meg devices
        cacheSizeLimit = 2*1024*1024;
      } else if (totalMemory > 0) {
        cacheSizeLimit = totalMemory / 16;
      }
    }

#ifdef DEBUG
    printf_stderr("Determined SkiaGL cache limits: Size %i, Items: %i\n", cacheSizeLimit, cacheItemLimit);
#endif

    Factory::SetGlobalSkiaCacheLimits(cacheItemLimit, cacheSizeLimit);
  }
#endif
}

already_AddRefed<gfxASurface>
gfxPlatform::GetThebesSurfaceForDrawTarget(DrawTarget *aTarget)
{
  if (aTarget->GetType() == BackendType::CAIRO) {
    cairo_surface_t* csurf =
      static_cast<cairo_surface_t*>(aTarget->GetNativeSurface(NativeSurfaceType::CAIRO_SURFACE));
    if (csurf) {
      return gfxASurface::Wrap(csurf);
    }
  }

  // The semantics of this part of the function are sort of weird. If we
  // don't have direct support for the backend, we snapshot the first time
  // and then return the snapshotted surface for the lifetime of the draw
  // target. Sometimes it seems like this works out, but it seems like it
  // might result in no updates ever.
  RefPtr<SourceSurface> source = aTarget->Snapshot();
  RefPtr<DataSourceSurface> data = source->GetDataSurface();

  if (!data) {
    return nullptr;
  }

  IntSize size = data->GetSize();
  gfxImageFormat format = SurfaceFormatToImageFormat(data->GetFormat());


  nsRefPtr<gfxASurface> surf =
    new gfxImageSurface(data->GetData(), gfxIntSize(size.width, size.height),
                        data->Stride(), format);

  surf->SetData(&kDrawSourceSurface, data.forget().drop(), DataSourceSurfaceDestroy);
  // keep the draw target alive as long as we need its data
  aTarget->AddRef();
  surf->SetData(&kDrawTargetForSurface, aTarget, DataDrawTargetDestroy);

  return surf.forget();
}

RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForBackend(BackendType aBackend, const IntSize& aSize, SurfaceFormat aFormat)
{
  // There is a bunch of knowledge in the gfxPlatform heirarchy about how to
  // create the best offscreen surface for the current system and situation. We
  // can easily take advantage of this for the Cairo backend, so that's what we
  // do.
  // mozilla::gfx::Factory can get away without having all this knowledge for
  // now, but this might need to change in the future (using
  // CreateOffscreenSurface() and CreateDrawTargetForSurface() for all
  // backends).
  if (aBackend == BackendType::CAIRO) {
    nsRefPtr<gfxASurface> surf = CreateOffscreenSurface(ThebesIntSize(aSize),
                                                        ContentForFormat(aFormat));
    if (!surf || surf->CairoStatus()) {
      return nullptr;
    }

    return CreateDrawTargetForSurface(surf, aSize);
  } else {
    return Factory::CreateDrawTarget(aBackend, aSize, aFormat);
  }
}

RefPtr<DrawTarget>
gfxPlatform::CreateOffscreenCanvasDrawTarget(const IntSize& aSize, SurfaceFormat aFormat)
{
  NS_ASSERTION(mPreferredCanvasBackend != BackendType::NONE, "No backend.");
  RefPtr<DrawTarget> target = CreateDrawTargetForBackend(mPreferredCanvasBackend, aSize, aFormat);
  if (target ||
      mFallbackCanvasBackend == BackendType::NONE) {
    return target;
  }

  return CreateDrawTargetForBackend(mFallbackCanvasBackend, aSize, aFormat);
}

RefPtr<DrawTarget>
gfxPlatform::CreateOffscreenContentDrawTarget(const IntSize& aSize, SurfaceFormat aFormat)
{
  NS_ASSERTION(mPreferredCanvasBackend != BackendType::NONE, "No backend.");
  return CreateDrawTargetForBackend(mContentBackend, aSize, aFormat);
}

RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForData(unsigned char* aData, const IntSize& aSize, int32_t aStride, SurfaceFormat aFormat)
{
  NS_ASSERTION(mContentBackend != BackendType::NONE, "No backend.");
  if (mContentBackend == BackendType::CAIRO) {
    nsRefPtr<gfxImageSurface> image = new gfxImageSurface(aData, gfxIntSize(aSize.width, aSize.height), aStride, SurfaceFormatToImageFormat(aFormat)); 
    return Factory::CreateDrawTargetForCairoSurface(image->CairoSurface(), aSize);
  }
  return Factory::CreateDrawTargetForData(mContentBackend, aData, aSize, aStride, aFormat);
}

/* static */ BackendType
gfxPlatform::BackendTypeForName(const nsCString& aName)
{
  if (aName.EqualsLiteral("cairo"))
    return BackendType::CAIRO;
  if (aName.EqualsLiteral("skia"))
    return BackendType::SKIA;
  if (aName.EqualsLiteral("direct2d"))
    return BackendType::DIRECT2D;
  if (aName.EqualsLiteral("cg"))
    return BackendType::COREGRAPHICS;
  return BackendType::NONE;
}

nsresult
gfxPlatform::GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
gfxPlatform::UpdateFontList()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

bool
gfxPlatform::DownloadableFontsEnabled()
{
    if (mAllowDownloadableFonts == UNINITIALIZED_VALUE) {
        mAllowDownloadableFonts =
            Preferences::GetBool(GFX_DOWNLOADABLE_FONTS_ENABLED, false);
    }

    return mAllowDownloadableFonts;
}

bool
gfxPlatform::UseCmapsDuringSystemFallback()
{
    if (mFallbackUsesCmaps == UNINITIALIZED_VALUE) {
        mFallbackUsesCmaps =
            Preferences::GetBool(GFX_PREF_FALLBACK_USE_CMAPS, false);
    }

    return mFallbackUsesCmaps;
}

bool
gfxPlatform::OpenTypeSVGEnabled()
{
    if (mOpenTypeSVGEnabled == UNINITIALIZED_VALUE) {
        mOpenTypeSVGEnabled =
            Preferences::GetBool(GFX_PREF_OPENTYPE_SVG, false);
    }

    return mOpenTypeSVGEnabled > 0;
}

uint32_t
gfxPlatform::WordCacheCharLimit()
{
    if (mWordCacheCharLimit == UNINITIALIZED_VALUE) {
        mWordCacheCharLimit =
            Preferences::GetInt(GFX_PREF_WORD_CACHE_CHARLIMIT, 32);
        if (mWordCacheCharLimit < 0) {
            mWordCacheCharLimit = 32;
        }
    }

    return uint32_t(mWordCacheCharLimit);
}

uint32_t
gfxPlatform::WordCacheMaxEntries()
{
    if (mWordCacheMaxEntries == UNINITIALIZED_VALUE) {
        mWordCacheMaxEntries =
            Preferences::GetInt(GFX_PREF_WORD_CACHE_MAXENTRIES, 10000);
        if (mWordCacheMaxEntries < 0) {
            mWordCacheMaxEntries = 10000;
        }
    }

    return uint32_t(mWordCacheMaxEntries);
}

bool
gfxPlatform::UseGraphiteShaping()
{
    if (mGraphiteShapingEnabled == UNINITIALIZED_VALUE) {
        mGraphiteShapingEnabled =
            Preferences::GetBool(GFX_PREF_GRAPHITE_SHAPING, false);
    }

    return mGraphiteShapingEnabled;
}

bool
gfxPlatform::UseHarfBuzzForScript(int32_t aScriptCode)
{
    if (mUseHarfBuzzScripts == UNINITIALIZED_VALUE) {
        mUseHarfBuzzScripts = Preferences::GetInt(GFX_PREF_HARFBUZZ_SCRIPTS, HARFBUZZ_SCRIPTS_DEFAULT);
    }

    int32_t shapingType = mozilla::unicode::ScriptShapingType(aScriptCode);

    return (mUseHarfBuzzScripts & shapingType) != 0;
}

gfxFontEntry*
gfxPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                              const uint8_t *aFontData,
                              uint32_t aLength)
{
    // Default implementation does not handle activating downloaded fonts;
    // just free the data and return.
    // Platforms that support @font-face must override this,
    // using the data to instantiate the font, and taking responsibility
    // for freeing it when no longer required.
    if (aFontData) {
        NS_Free((void*)aFontData);
    }
    return nullptr;
}

static void
AppendGenericFontFromPref(nsString& aFonts, nsIAtom *aLangGroup, const char *aGenericName)
{
    NS_ENSURE_TRUE_VOID(Preferences::GetRootBranch());

    nsAutoCString prefName, langGroupString;

    aLangGroup->ToUTF8String(langGroupString);

    nsAutoCString genericDotLang;
    if (aGenericName) {
        genericDotLang.Assign(aGenericName);
    } else {
        prefName.AssignLiteral("font.default.");
        prefName.Append(langGroupString);
        genericDotLang = Preferences::GetCString(prefName.get());
    }

    genericDotLang.AppendLiteral(".");
    genericDotLang.Append(langGroupString);

    // fetch font.name.xxx value
    prefName.AssignLiteral("font.name.");
    prefName.Append(genericDotLang);
    nsAdoptingString nameValue = Preferences::GetString(prefName.get());
    if (nameValue) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts += nameValue;
    }

    // fetch font.name-list.xxx value
    prefName.AssignLiteral("font.name-list.");
    prefName.Append(genericDotLang);
    nsAdoptingString nameListValue = Preferences::GetString(prefName.get());
    if (nameListValue && !nameListValue.Equals(nameValue)) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts += nameListValue;
    }
}

void
gfxPlatform::GetPrefFonts(nsIAtom *aLanguage, nsString& aFonts, bool aAppendUnicode)
{
    aFonts.Truncate();

    AppendGenericFontFromPref(aFonts, aLanguage, nullptr);
    if (aAppendUnicode)
        AppendGenericFontFromPref(aFonts, nsGkAtoms::Unicode, nullptr);
}

bool gfxPlatform::ForEachPrefFont(eFontPrefLang aLangArray[], uint32_t aLangArrayLen, PrefFontCallback aCallback,
                                    void *aClosure)
{
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), false);

    uint32_t    i;
    for (i = 0; i < aLangArrayLen; i++) {
        eFontPrefLang prefLang = aLangArray[i];
        const char *langGroup = GetPrefLangName(prefLang);

        nsAutoCString prefName;

        prefName.AssignLiteral("font.default.");
        prefName.Append(langGroup);
        nsAdoptingCString genericDotLang = Preferences::GetCString(prefName.get());

        genericDotLang.AppendLiteral(".");
        genericDotLang.Append(langGroup);

        // fetch font.name.xxx value
        prefName.AssignLiteral("font.name.");
        prefName.Append(genericDotLang);
        nsAdoptingCString nameValue = Preferences::GetCString(prefName.get());
        if (nameValue) {
            if (!aCallback(prefLang, NS_ConvertUTF8toUTF16(nameValue), aClosure))
                return false;
        }

        // fetch font.name-list.xxx value
        prefName.AssignLiteral("font.name-list.");
        prefName.Append(genericDotLang);
        nsAdoptingCString nameListValue = Preferences::GetCString(prefName.get());
        if (nameListValue && !nameListValue.Equals(nameValue)) {
            const char kComma = ',';
            const char *p, *p_end;
            nsAutoCString list(nameListValue);
            list.BeginReading(p);
            list.EndReading(p_end);
            while (p < p_end) {
                while (nsCRT::IsAsciiSpace(*p)) {
                    if (++p == p_end)
                        break;
                }
                if (p == p_end)
                    break;
                const char *start = p;
                while (++p != p_end && *p != kComma)
                    /* nothing */ ;
                nsAutoCString fontName(Substring(start, p));
                fontName.CompressWhitespace(false, true);
                if (!aCallback(prefLang, NS_ConvertUTF8toUTF16(fontName), aClosure))
                    return false;
                p++;
            }
        }
    }

    return true;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(const char* aLang)
{
    if (!aLang || !aLang[0])
        return eFontPrefLang_Others;
    for (uint32_t i = 0; i < uint32_t(eFontPrefLang_LangCount); ++i) {
        if (!PL_strcasecmp(gPrefLangNames[i], aLang))
            return eFontPrefLang(i);
    }
    return eFontPrefLang_Others;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(nsIAtom *aLang)
{
    if (!aLang)
        return eFontPrefLang_Others;
    nsAutoCString lang;
    aLang->ToUTF8String(lang);
    return GetFontPrefLangFor(lang.get());
}

const char*
gfxPlatform::GetPrefLangName(eFontPrefLang aLang)
{
    if (uint32_t(aLang) < uint32_t(eFontPrefLang_AllCount))
        return gPrefLangNames[uint32_t(aLang)];
    return nullptr;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(uint8_t aUnicodeRange)
{
    switch (aUnicodeRange) {
        case kRangeSetLatin:   return eFontPrefLang_Western;
        case kRangeCyrillic:   return eFontPrefLang_Cyrillic;
        case kRangeGreek:      return eFontPrefLang_Greek;
        case kRangeTurkish:    return eFontPrefLang_Turkish;
        case kRangeHebrew:     return eFontPrefLang_Hebrew;
        case kRangeArabic:     return eFontPrefLang_Arabic;
        case kRangeBaltic:     return eFontPrefLang_Baltic;
        case kRangeThai:       return eFontPrefLang_Thai;
        case kRangeKorean:     return eFontPrefLang_Korean;
        case kRangeJapanese:   return eFontPrefLang_Japanese;
        case kRangeSChinese:   return eFontPrefLang_ChineseCN;
        case kRangeTChinese:   return eFontPrefLang_ChineseTW;
        case kRangeDevanagari: return eFontPrefLang_Devanagari;
        case kRangeTamil:      return eFontPrefLang_Tamil;
        case kRangeArmenian:   return eFontPrefLang_Armenian;
        case kRangeBengali:    return eFontPrefLang_Bengali;
        case kRangeCanadian:   return eFontPrefLang_Canadian;
        case kRangeEthiopic:   return eFontPrefLang_Ethiopic;
        case kRangeGeorgian:   return eFontPrefLang_Georgian;
        case kRangeGujarati:   return eFontPrefLang_Gujarati;
        case kRangeGurmukhi:   return eFontPrefLang_Gurmukhi;
        case kRangeKhmer:      return eFontPrefLang_Khmer;
        case kRangeMalayalam:  return eFontPrefLang_Malayalam;
        case kRangeOriya:      return eFontPrefLang_Oriya;
        case kRangeTelugu:     return eFontPrefLang_Telugu;
        case kRangeKannada:    return eFontPrefLang_Kannada;
        case kRangeSinhala:    return eFontPrefLang_Sinhala;
        case kRangeTibetan:    return eFontPrefLang_Tibetan;
        case kRangeSetCJK:     return eFontPrefLang_CJKSet;
        default:               return eFontPrefLang_Others;
    }
}

bool
gfxPlatform::IsLangCJK(eFontPrefLang aLang)
{
    switch (aLang) {
        case eFontPrefLang_Japanese:
        case eFontPrefLang_ChineseTW:
        case eFontPrefLang_ChineseCN:
        case eFontPrefLang_ChineseHK:
        case eFontPrefLang_Korean:
        case eFontPrefLang_CJKSet:
            return true;
        default:
            return false;
    }
}

mozilla::layers::DiagnosticTypes
gfxPlatform::GetLayerDiagnosticTypes()
{
  mozilla::layers::DiagnosticTypes type = DIAGNOSTIC_NONE;
  if (mDrawLayerBorders) {
    type |= mozilla::layers::DIAGNOSTIC_LAYER_BORDERS;
  }
  if (mDrawTileBorders) {
    type |= mozilla::layers::DIAGNOSTIC_TILE_BORDERS;
  }
  if (mDrawBigImageBorders) {
    type |= mozilla::layers::DIAGNOSTIC_BIGIMAGE_BORDERS;
  }
  return type;
}

bool
gfxPlatform::DrawFrameCounter()
{
    return sDrawFrameCounter;
}

void
gfxPlatform::GetLangPrefs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang)
{
    if (IsLangCJK(aCharLang)) {
        AppendCJKPrefLangs(aPrefLangs, aLen, aCharLang, aPageLang);
    } else {
        AppendPrefLang(aPrefLangs, aLen, aCharLang);
    }

    AppendPrefLang(aPrefLangs, aLen, eFontPrefLang_Others);
}

void
gfxPlatform::AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang)
{
    // prefer the lang specified by the page *if* CJK
    if (IsLangCJK(aPageLang)) {
        AppendPrefLang(aPrefLangs, aLen, aPageLang);
    }

    // if not set up, set up the default CJK order, based on accept lang settings and locale
    if (mCJKPrefLangs.Length() == 0) {

        // temp array
        eFontPrefLang tempPrefLangs[kMaxLenPrefLangList];
        uint32_t tempLen = 0;

        // Add the CJK pref fonts from accept languages, the order should be same order
        nsAdoptingCString list = Preferences::GetLocalizedCString("intl.accept_languages");
        if (!list.IsEmpty()) {
            const char kComma = ',';
            const char *p, *p_end;
            list.BeginReading(p);
            list.EndReading(p_end);
            while (p < p_end) {
                while (nsCRT::IsAsciiSpace(*p)) {
                    if (++p == p_end)
                        break;
                }
                if (p == p_end)
                    break;
                const char *start = p;
                while (++p != p_end && *p != kComma)
                    /* nothing */ ;
                nsAutoCString lang(Substring(start, p));
                lang.CompressWhitespace(false, true);
                eFontPrefLang fpl = gfxPlatform::GetFontPrefLangFor(lang.get());
                switch (fpl) {
                    case eFontPrefLang_Japanese:
                    case eFontPrefLang_Korean:
                    case eFontPrefLang_ChineseCN:
                    case eFontPrefLang_ChineseHK:
                    case eFontPrefLang_ChineseTW:
                        AppendPrefLang(tempPrefLangs, tempLen, fpl);
                        break;
                    default:
                        break;
                }
                p++;
            }
        }

        do { // to allow 'break' to abort this block if a call fails
            nsresult rv;
            nsCOMPtr<nsILocaleService> ls =
                do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
            if (NS_FAILED(rv))
                break;

            nsCOMPtr<nsILocale> appLocale;
            rv = ls->GetApplicationLocale(getter_AddRefs(appLocale));
            if (NS_FAILED(rv))
                break;

            nsString localeStr;
            rv = appLocale->
                GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), localeStr);
            if (NS_FAILED(rv))
                break;

            const nsAString& lang = Substring(localeStr, 0, 2);
            if (lang.EqualsLiteral("ja")) {
                AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
            } else if (lang.EqualsLiteral("zh")) {
                const nsAString& region = Substring(localeStr, 3, 2);
                if (region.EqualsLiteral("CN")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
                } else if (region.EqualsLiteral("TW")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);
                } else if (region.EqualsLiteral("HK")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
                }
            } else if (lang.EqualsLiteral("ko")) {
                AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
            }
        } while (0);

        // last resort... (the order is same as old gfx.)
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);

        // copy into the cached array
        uint32_t j;
        for (j = 0; j < tempLen; j++) {
            mCJKPrefLangs.AppendElement(tempPrefLangs[j]);
        }
    }

    // append in cached CJK langs
    uint32_t  i, numCJKlangs = mCJKPrefLangs.Length();

    for (i = 0; i < numCJKlangs; i++) {
        AppendPrefLang(aPrefLangs, aLen, (eFontPrefLang) (mCJKPrefLangs[i]));
    }

}

void
gfxPlatform::AppendPrefLang(eFontPrefLang aPrefLangs[], uint32_t& aLen, eFontPrefLang aAddLang)
{
    if (aLen >= kMaxLenPrefLangList) return;

    // make sure
    uint32_t  i = 0;
    while (i < aLen && aPrefLangs[i] != aAddLang) {
        i++;
    }

    if (i == aLen) {
        aPrefLangs[aLen] = aAddLang;
        aLen++;
    }
}

void
gfxPlatform::InitBackendPrefs(uint32_t aCanvasBitmask, BackendType aCanvasDefault,
                              uint32_t aContentBitmask, BackendType aContentDefault)
{
    mPreferredCanvasBackend = GetCanvasBackendPref(aCanvasBitmask);
    if (mPreferredCanvasBackend == BackendType::NONE) {
        mPreferredCanvasBackend = aCanvasDefault;
    }
    mFallbackCanvasBackend =
        GetCanvasBackendPref(aCanvasBitmask & ~BackendTypeBit(mPreferredCanvasBackend));

    mContentBackendBitmask = aContentBitmask;
    mContentBackend = GetContentBackendPref(mContentBackendBitmask);
    if (mContentBackend == BackendType::NONE) {
        mContentBackend = aContentDefault;
        // mContentBackendBitmask is our canonical reference for supported
        // backends so we need to add the default if we are using it and
        // overriding the prefs.
        mContentBackendBitmask |= BackendTypeBit(aContentDefault);
    }
}

/* static */ BackendType
gfxPlatform::GetCanvasBackendPref(uint32_t aBackendBitmask)
{
    return GetBackendPref("gfx.canvas.azure.backends", aBackendBitmask);
}

/* static */ BackendType
gfxPlatform::GetContentBackendPref(uint32_t &aBackendBitmask)
{
    return GetBackendPref("gfx.content.azure.backends", aBackendBitmask);
}

/* static */ BackendType
gfxPlatform::GetBackendPref(const char* aBackendPrefName, uint32_t &aBackendBitmask)
{
    nsTArray<nsCString> backendList;
    nsCString prefString;
    if (NS_SUCCEEDED(Preferences::GetCString(aBackendPrefName, &prefString))) {
        ParseString(prefString, ',', backendList);
    }

    uint32_t allowedBackends = 0;
    BackendType result = BackendType::NONE;
    for (uint32_t i = 0; i < backendList.Length(); ++i) {
        BackendType type = BackendTypeForName(backendList[i]);
        if (BackendTypeBit(type) & aBackendBitmask) {
            allowedBackends |= BackendTypeBit(type);
            if (result == BackendType::NONE) {
                result = type;
            }
        }
    }

    aBackendBitmask = allowedBackends;
    return result;
}

bool
gfxPlatform::UseProgressiveTilePainting()
{
    static bool sUseProgressiveTilePainting;
    static bool sUseProgressiveTilePaintingPrefCached = false;

    if (!sUseProgressiveTilePaintingPrefCached) {
        sUseProgressiveTilePaintingPrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sUseProgressiveTilePainting,
                                              "layers.progressive-paint",
                                              false);
    }

    return sUseProgressiveTilePainting;
}

bool
gfxPlatform::UseLowPrecisionBuffer()
{
    static bool sUseLowPrecisionBuffer;
    static bool sUseLowPrecisionBufferPrefCached = false;

    if (!sUseLowPrecisionBufferPrefCached) {
        sUseLowPrecisionBufferPrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sUseLowPrecisionBuffer,
                                              "layers.low-precision-buffer",
                                              false);
    }

    return sUseLowPrecisionBuffer;
}

float
gfxPlatform::GetLowPrecisionResolution()
{
    static float sLowPrecisionResolution;
    static bool sLowPrecisionResolutionPrefCached = false;

    if (!sLowPrecisionResolutionPrefCached) {
        int32_t lowPrecisionResolution = 250;
        sLowPrecisionResolutionPrefCached = true;
        mozilla::Preferences::AddIntVarCache(&lowPrecisionResolution,
                                             "layers.low-precision-resolution",
                                             250);
        sLowPrecisionResolution = lowPrecisionResolution / 1000.f;
    }

    return sLowPrecisionResolution;
}

bool
gfxPlatform::OffMainThreadCompositingEnabled()
{
  return XRE_GetProcessType() == GeckoProcessType_Default ?
    CompositorParent::CompositorLoop() != nullptr :
    CompositorChild::ChildProcessHasCompositor();
}

eCMSMode
gfxPlatform::GetCMSMode()
{
    if (gCMSInitialized == false) {
        gCMSInitialized = true;
        nsresult rv;

        int32_t mode;
        rv = Preferences::GetInt(GFX_PREF_CMS_MODE, &mode);
        if (NS_SUCCEEDED(rv) && (mode >= 0) && (mode < eCMSMode_AllCount)) {
            gCMSMode = static_cast<eCMSMode>(mode);
        }

        bool enableV4;
        rv = Preferences::GetBool(GFX_PREF_CMS_ENABLEV4, &enableV4);
        if (NS_SUCCEEDED(rv) && enableV4) {
            qcms_enable_iccv4();
        }
    }
    return gCMSMode;
}

int
gfxPlatform::GetRenderingIntent()
{
    if (gCMSIntent == -2) {

        /* Try to query the pref system for a rendering intent. */
        int32_t pIntent;
        if (NS_SUCCEEDED(Preferences::GetInt(GFX_PREF_CMS_RENDERING_INTENT, &pIntent))) {
            /* If the pref is within range, use it as an override. */
            if ((pIntent >= QCMS_INTENT_MIN) && (pIntent <= QCMS_INTENT_MAX)) {
                gCMSIntent = pIntent;
            }
            /* If the pref is out of range, use embedded profile. */
            else {
                gCMSIntent = -1;
            }
        }
        /* If we didn't get a valid intent from prefs, use the default. */
        else {
            gCMSIntent = QCMS_INTENT_DEFAULT;
        }
    }
    return gCMSIntent;
}

void
gfxPlatform::TransformPixel(const gfxRGBA& in, gfxRGBA& out, qcms_transform *transform)
{

    if (transform) {
        /* we want the bytes in RGB order */
#ifdef IS_LITTLE_ENDIAN
        /* ABGR puts the bytes in |RGBA| order on little endian */
        uint32_t packed = in.Packed(gfxRGBA::PACKED_ABGR);
        qcms_transform_data(transform,
                       (uint8_t *)&packed, (uint8_t *)&packed,
                       1);
        out.~gfxRGBA();
        new (&out) gfxRGBA(packed, gfxRGBA::PACKED_ABGR);
#else
        /* ARGB puts the bytes in |ARGB| order on big endian */
        uint32_t packed = in.Packed(gfxRGBA::PACKED_ARGB);
        /* add one to move past the alpha byte */
        qcms_transform_data(transform,
                       (uint8_t *)&packed + 1, (uint8_t *)&packed + 1,
                       1);
        out.~gfxRGBA();
        new (&out) gfxRGBA(packed, gfxRGBA::PACKED_ARGB);
#endif
    }

    else if (&out != &in)
        out = in;
}

void
gfxPlatform::GetPlatformCMSOutputProfile(void *&mem, size_t &size)
{
    mem = nullptr;
    size = 0;
}

void
gfxPlatform::GetCMSOutputProfileData(void *&mem, size_t &size)
{
    nsAdoptingCString fname = Preferences::GetCString("gfx.color_management.display_profile");
    if (!fname.IsEmpty()) {
        qcms_data_from_path(fname, &mem, &size);
    }
    else {
        gfxPlatform::GetPlatform()->GetPlatformCMSOutputProfile(mem, size);
    }
}

void
gfxPlatform::CreateCMSOutputProfile()
{
    if (!gCMSOutputProfile) {
        /* Determine if we're using the internal override to force sRGB as
           an output profile for reftests. See Bug 452125.

           Note that we don't normally (outside of tests) set a
           default value of this preference, which means nsIPrefBranch::GetBoolPref
           will typically throw (and leave its out-param untouched).
         */
        if (Preferences::GetBool(GFX_PREF_CMS_FORCE_SRGB, false)) {
            gCMSOutputProfile = GetCMSsRGBProfile();
        }

        if (!gCMSOutputProfile) {
            void* mem = nullptr;
            size_t size = 0;

            GetCMSOutputProfileData(mem, size);
            if ((mem != nullptr) && (size > 0)) {
                gCMSOutputProfile = qcms_profile_from_memory(mem, size);
                free(mem);
            }
        }

        /* Determine if the profile looks bogus. If so, close the profile
         * and use sRGB instead. See bug 460629, */
        if (gCMSOutputProfile && qcms_profile_is_bogus(gCMSOutputProfile)) {
            NS_ASSERTION(gCMSOutputProfile != GetCMSsRGBProfile(),
                         "Builtin sRGB profile tagged as bogus!!!");
            qcms_profile_release(gCMSOutputProfile);
            gCMSOutputProfile = nullptr;
        }

        if (!gCMSOutputProfile) {
            gCMSOutputProfile = GetCMSsRGBProfile();
        }
        /* Precache the LUT16 Interpolations for the output profile. See
           bug 444661 for details. */
        qcms_profile_precache_output_transform(gCMSOutputProfile);
    }
}

qcms_profile *
gfxPlatform::GetCMSOutputProfile()
{
    return gCMSOutputProfile;
}

qcms_profile *
gfxPlatform::GetCMSsRGBProfile()
{
    if (!gCMSsRGBProfile) {

        /* Create the profile using qcms. */
        gCMSsRGBProfile = qcms_profile_sRGB();
    }
    return gCMSsRGBProfile;
}

qcms_transform *
gfxPlatform::GetCMSRGBTransform()
{
    if (!gCMSRGBTransform) {
        qcms_profile *inProfile, *outProfile;
        outProfile = GetCMSOutputProfile();
        inProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nullptr;

        gCMSRGBTransform = qcms_transform_create(inProfile, QCMS_DATA_RGB_8,
                                              outProfile, QCMS_DATA_RGB_8,
                                             QCMS_INTENT_PERCEPTUAL);
    }

    return gCMSRGBTransform;
}

qcms_transform *
gfxPlatform::GetCMSInverseRGBTransform()
{
    if (!gCMSInverseRGBTransform) {
        qcms_profile *inProfile, *outProfile;
        inProfile = GetCMSOutputProfile();
        outProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nullptr;

        gCMSInverseRGBTransform = qcms_transform_create(inProfile, QCMS_DATA_RGB_8,
                                                     outProfile, QCMS_DATA_RGB_8,
                                                     QCMS_INTENT_PERCEPTUAL);
    }

    return gCMSInverseRGBTransform;
}

qcms_transform *
gfxPlatform::GetCMSRGBATransform()
{
    if (!gCMSRGBATransform) {
        qcms_profile *inProfile, *outProfile;
        outProfile = GetCMSOutputProfile();
        inProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nullptr;

        gCMSRGBATransform = qcms_transform_create(inProfile, QCMS_DATA_RGBA_8,
                                               outProfile, QCMS_DATA_RGBA_8,
                                               QCMS_INTENT_PERCEPTUAL);
    }

    return gCMSRGBATransform;
}

/* Shuts down various transforms and profiles for CMS. */
static void ShutdownCMS()
{

    if (gCMSRGBTransform) {
        qcms_transform_release(gCMSRGBTransform);
        gCMSRGBTransform = nullptr;
    }
    if (gCMSInverseRGBTransform) {
        qcms_transform_release(gCMSInverseRGBTransform);
        gCMSInverseRGBTransform = nullptr;
    }
    if (gCMSRGBATransform) {
        qcms_transform_release(gCMSRGBATransform);
        gCMSRGBATransform = nullptr;
    }
    if (gCMSOutputProfile) {
        qcms_profile_release(gCMSOutputProfile);

        // handle the aliased case
        if (gCMSsRGBProfile == gCMSOutputProfile)
            gCMSsRGBProfile = nullptr;
        gCMSOutputProfile = nullptr;
    }
    if (gCMSsRGBProfile) {
        qcms_profile_release(gCMSsRGBProfile);
        gCMSsRGBProfile = nullptr;
    }

    // Reset the state variables
    gCMSIntent = -2;
    gCMSMode = eCMSMode_Off;
    gCMSInitialized = false;
}

static void MigratePrefs()
{
    /* Migrate from the boolean color_management.enabled pref - we now use
       color_management.mode. */
    if (Preferences::HasUserValue(GFX_PREF_CMS_ENABLED_OBSOLETE)) {
        if (Preferences::GetBool(GFX_PREF_CMS_ENABLED_OBSOLETE, false)) {
            Preferences::SetInt(GFX_PREF_CMS_MODE, static_cast<int32_t>(eCMSMode_All));
        }
        Preferences::ClearUser(GFX_PREF_CMS_ENABLED_OBSOLETE);
    }
}

// default SetupClusterBoundaries, based on Unicode properties;
// platform subclasses may override if they wish
void
gfxPlatform::SetupClusterBoundaries(gfxTextRun *aTextRun, const char16_t *aString)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) {
        // 8-bit text doesn't have clusters.
        // XXX is this true in all languages???
        // behdad: don't think so.  Czech for example IIRC has a
        // 'ch' grapheme.
        // jfkthame: but that's not expected to behave as a grapheme cluster
        // for selection/editing/etc.
        return;
    }

    aTextRun->SetupClusterBoundaries(0, aString, aTextRun->GetLength());
}

int32_t
gfxPlatform::GetBidiNumeralOption()
{
    if (mBidiNumeralOption == UNINITIALIZED_VALUE) {
        mBidiNumeralOption = Preferences::GetInt(BIDI_NUMERAL_PREF, 0);
    }
    return mBidiNumeralOption;
}

static void
FlushFontAndWordCaches()
{
    gfxFontCache *fontCache = gfxFontCache::GetCache();
    if (fontCache) {
        fontCache->AgeAllGenerations();
        fontCache->FlushShapedWordCaches();
    }
}

void
gfxPlatform::FontsPrefsChanged(const char *aPref)
{
    NS_ASSERTION(aPref != nullptr, "null preference");
    if (!strcmp(GFX_DOWNLOADABLE_FONTS_ENABLED, aPref)) {
        mAllowDownloadableFonts = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_PREF_FALLBACK_USE_CMAPS, aPref)) {
        mFallbackUsesCmaps = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_PREF_WORD_CACHE_CHARLIMIT, aPref)) {
        mWordCacheCharLimit = UNINITIALIZED_VALUE;
        FlushFontAndWordCaches();
    } else if (!strcmp(GFX_PREF_WORD_CACHE_MAXENTRIES, aPref)) {
        mWordCacheMaxEntries = UNINITIALIZED_VALUE;
        FlushFontAndWordCaches();
    } else if (!strcmp(GFX_PREF_GRAPHITE_SHAPING, aPref)) {
        mGraphiteShapingEnabled = UNINITIALIZED_VALUE;
        FlushFontAndWordCaches();
    } else if (!strcmp(GFX_PREF_HARFBUZZ_SCRIPTS, aPref)) {
        mUseHarfBuzzScripts = UNINITIALIZED_VALUE;
        FlushFontAndWordCaches();
    } else if (!strcmp(BIDI_NUMERAL_PREF, aPref)) {
        mBidiNumeralOption = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_PREF_OPENTYPE_SVG, aPref)) {
        mOpenTypeSVGEnabled = UNINITIALIZED_VALUE;
        gfxFontCache::GetCache()->AgeAllGenerations();
    }
}


PRLogModuleInfo*
gfxPlatform::GetLog(eGfxLog aWhichLog)
{
#ifdef PR_LOGGING
    switch (aWhichLog) {
    case eGfxLog_fontlist:
        return sFontlistLog;
        break;
    case eGfxLog_fontinit:
        return sFontInitLog;
        break;
    case eGfxLog_textrun:
        return sTextrunLog;
        break;
    case eGfxLog_textrunui:
        return sTextrunuiLog;
        break;
    case eGfxLog_cmapdata:
        return sCmapDataLog;
        break;
    case eGfxLog_textperf:
        return sTextPerfLog;
        break;
    default:
        break;
    }

    return nullptr;
#else
    return nullptr;
#endif
}

int
gfxPlatform::GetScreenDepth() const
{
    NS_WARNING("GetScreenDepth not implemented on this platform -- returning 0!");
    return 0;
}

mozilla::gfx::SurfaceFormat
gfxPlatform::Optimal2DFormatForContent(gfxContentType aContent)
{
  switch (aContent) {
  case gfxContentType::COLOR:
    switch (GetOffscreenFormat()) {
    case gfxImageFormat::ARGB32:
      return mozilla::gfx::SurfaceFormat::B8G8R8A8;
    case gfxImageFormat::RGB24:
      return mozilla::gfx::SurfaceFormat::B8G8R8X8;
    case gfxImageFormat::RGB16_565:
      return mozilla::gfx::SurfaceFormat::R5G6B5;
    default:
      NS_NOTREACHED("unknown gfxImageFormat for gfxContentType::COLOR");
      return mozilla::gfx::SurfaceFormat::B8G8R8A8;
    }
  case gfxContentType::ALPHA:
    return mozilla::gfx::SurfaceFormat::A8;
  case gfxContentType::COLOR_ALPHA:
    return mozilla::gfx::SurfaceFormat::B8G8R8A8;
  default:
    NS_NOTREACHED("unknown gfxContentType");
    return mozilla::gfx::SurfaceFormat::B8G8R8A8;
  }
}

gfxImageFormat
gfxPlatform::OptimalFormatForContent(gfxContentType aContent)
{
  switch (aContent) {
  case gfxContentType::COLOR:
    return GetOffscreenFormat();
  case gfxContentType::ALPHA:
    return gfxImageFormat::A8;
  case gfxContentType::COLOR_ALPHA:
    return gfxImageFormat::ARGB32;
  default:
    NS_NOTREACHED("unknown gfxContentType");
    return gfxImageFormat::ARGB32;
  }
}

void
gfxPlatform::OrientationSyncPrefsObserverChanged()
{
  mOrientationSyncMillis = Preferences::GetUint("layers.orientation.sync.timeout", (uint32_t)0);
}

uint32_t
gfxPlatform::GetOrientationSyncMillis() const
{
  return mOrientationSyncMillis;
}

/**
 * There are a number of layers acceleration (or layers in general) preferences
 * that should be consistent for the lifetime of the application (bug 840967).
 * As such, we will evaluate them all as soon as one of them is evaluated
 * and remember the values.  Changing these preferences during the run will
 * not have any effect until we restart.
 */
static bool sPrefLayersOffMainThreadCompositionEnabled = false;
static bool sPrefLayersOffMainThreadCompositionTestingEnabled = false;
static bool sPrefLayersOffMainThreadCompositionForceEnabled = false;
static bool sPrefLayersAccelerationForceEnabled = false;
static bool sPrefLayersAccelerationDisabled = false;
static bool sPrefLayersPreferOpenGL = false;
static bool sPrefLayersPreferD3D9 = false;
static bool sPrefLayersDump = false;
static bool sPrefLayersScrollGraph = false;
static bool sPrefLayersEnableTiles = false;
static bool sLayersSupportsD3D9 = false;
static int  sPrefLayoutFrameRate = -1;
static int  sPrefLayersCompositionFrameRate = -1;
static bool sBufferRotationEnabled = false;
static bool sComponentAlphaEnabled = true;
static bool sPrefBrowserTabsRemote = false;

static bool sLayersAccelerationPrefsInitialized = false;

void
InitLayersAccelerationPrefs()
{
  if (!sLayersAccelerationPrefsInitialized)
  {
    // If this is called for the first time on a non-main thread, we're screwed.
    // At the moment there's no explicit guarantee that the main thread calls
    // this before the compositor thread, but let's at least make the assumption
    // explicit.
    MOZ_ASSERT(NS_IsMainThread(), "can only initialize prefs on the main thread");

    sPrefLayersOffMainThreadCompositionEnabled = Preferences::GetBool("layers.offmainthreadcomposition.enabled", false);
    sPrefLayersOffMainThreadCompositionTestingEnabled = Preferences::GetBool("layers.offmainthreadcomposition.testing.enabled", false);
    sPrefLayersOffMainThreadCompositionForceEnabled = Preferences::GetBool("layers.offmainthreadcomposition.force-enabled", false);
    sPrefLayersAccelerationForceEnabled = Preferences::GetBool("layers.acceleration.force-enabled", false);
    sPrefLayersAccelerationDisabled = Preferences::GetBool("layers.acceleration.disabled", false);
    sPrefLayersPreferOpenGL = Preferences::GetBool("layers.prefer-opengl", false);
    sPrefLayersPreferD3D9 = Preferences::GetBool("layers.prefer-d3d9", false);
    sPrefLayersDump = Preferences::GetBool("layers.dump", false);
    sPrefLayersScrollGraph = Preferences::GetBool("layers.scroll-graph", false);
    sPrefLayersEnableTiles = Preferences::GetBool("layers.enable-tiles", false);
    sPrefLayoutFrameRate = Preferences::GetInt("layout.frame_rate", -1);
    sPrefLayersCompositionFrameRate = Preferences::GetInt("layers.offmainthreadcomposition.frame-rate", -1);
    sBufferRotationEnabled = Preferences::GetBool("layers.bufferrotation.enabled", true);
    sComponentAlphaEnabled = Preferences::GetBool("layers.componentalpha.enabled", true);
    sPrefBrowserTabsRemote = BrowserTabsRemote();

#ifdef XP_WIN
    if (sPrefLayersAccelerationForceEnabled) {
      sLayersSupportsD3D9 = true;
    } else {
      nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
      if (gfxInfo) {
        int32_t status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, &status))) {
          if (status == nsIGfxInfo::FEATURE_NO_INFO) {
            sLayersSupportsD3D9 = true;
          }
        }
      }
    }
#endif

    sLayersAccelerationPrefsInitialized = true;
  }
}

bool
gfxPlatform::GetPrefLayersOffMainThreadCompositionEnabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersOffMainThreadCompositionEnabled ||
         sPrefLayersOffMainThreadCompositionForceEnabled ||
         sPrefLayersOffMainThreadCompositionTestingEnabled;
}

bool
gfxPlatform::GetPrefLayersOffMainThreadCompositionForceEnabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersOffMainThreadCompositionForceEnabled;
}

bool
gfxPlatform::GetPrefLayersAccelerationForceEnabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersAccelerationForceEnabled;
}

bool gfxPlatform::OffMainThreadCompositionRequired()
{
  InitLayersAccelerationPrefs();
#if defined(MOZ_WIDGET_GTK) && defined(NIGHTLY_BUILD)
  // Linux users who chose OpenGL are being grandfathered in to OMTC
  return sPrefBrowserTabsRemote ||
         sPrefLayersAccelerationForceEnabled;
#else
  return sPrefBrowserTabsRemote;
#endif
}

bool
gfxPlatform::GetPrefLayersAccelerationDisabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersAccelerationDisabled;
}

bool
gfxPlatform::GetPrefLayersPreferOpenGL()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersPreferOpenGL;
}

bool
gfxPlatform::GetPrefLayersPreferD3D9()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersPreferD3D9;
}

bool
gfxPlatform::CanUseDirect3D9()
{
  // this function is called from the compositor thread, so it is not
  // safe to init the prefs etc. from here.
  MOZ_ASSERT(sLayersAccelerationPrefsInitialized);
  return sLayersSupportsD3D9;
}

int
gfxPlatform::GetPrefLayoutFrameRate()
{
  InitLayersAccelerationPrefs();
  return sPrefLayoutFrameRate;
}

bool
gfxPlatform::GetPrefLayersDump()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersDump;
}

bool
gfxPlatform::GetPrefLayersScrollGraph()
{
  // this function is called from the compositor thread, so it is not
  // safe to init the prefs etc. from here.
  MOZ_ASSERT(sLayersAccelerationPrefsInitialized);
  return sPrefLayersScrollGraph;
}

bool
gfxPlatform::GetPrefLayersEnableTiles()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersEnableTiles;
}

int
gfxPlatform::GetPrefLayersCompositionFrameRate()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersCompositionFrameRate;
}

bool
gfxPlatform::BufferRotationEnabled()
{
  MutexAutoLock autoLock(*gGfxPlatformPrefsLock);

  InitLayersAccelerationPrefs();
  return sBufferRotationEnabled;
}

void
gfxPlatform::DisableBufferRotation()
{
  MutexAutoLock autoLock(*gGfxPlatformPrefsLock);

  sBufferRotationEnabled = false;
}

bool
gfxPlatform::ComponentAlphaEnabled()
{
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  return false;
#endif

  InitLayersAccelerationPrefs();
  return sComponentAlphaEnabled;
}

bool
gfxPlatform::AsyncVideoEnabled()
{
#ifdef XP_WIN
  return false;
#else
  return Preferences::GetBool("layers.async-video.enabled", false);
#endif
}

TemporaryRef<ScaledFont>
gfxPlatform::GetScaledFontForFontWithCairoSkia(DrawTarget* aTarget, gfxFont* aFont)
{
    NativeFont nativeFont;
    if (aTarget->GetType() == BackendType::CAIRO || aTarget->GetType() == BackendType::SKIA) {
        nativeFont.mType = NativeFontType::CAIRO_FONT_FACE;
        nativeFont.mFont = aFont->GetCairoScaledFont();
        return Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());
    }

    return nullptr;
}
