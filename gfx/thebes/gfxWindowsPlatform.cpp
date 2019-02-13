/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxWindowsPlatform.h"

#include "cairo.h"
#include "mozilla/ArrayUtils.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

#include "nsUnicharUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/WindowsVersion.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "mozilla/Telemetry.h"

#include "nsIWindowsRegKey.h"
#include "nsIFile.h"
#include "plbase64.h"
#include "nsIXULRuntime.h"
#include "imgLoader.h"

#include "nsIGfxInfo.h"
#include "GfxDriverInfo.h"

#include "gfxCrashReporterUtils.h"

#include "gfxGDIFontList.h"
#include "gfxGDIFont.h"

#include "mozilla/layers/CompositorParent.h"   // for CompositorParent::IsInCompositorThread
#include "DeviceManagerD3D9.h"
#include "mozilla/layers/ReadbackManagerD3D11.h"

#include "WinUtils.h"

#ifdef CAIRO_HAS_DWRITE_FONT
#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "gfxDWriteCommon.h"
#include <dwrite.h>
#endif

#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "nsWindowsHelpers.h"
#include "gfx2DGlue.h"

#include <string>

#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"

#include <d3d10_1.h>

#include "mozilla/gfx/2D.h"

#include "nsMemory.h"
#endif

#include <d3d11.h>

#include "nsIMemoryReporter.h"
#include <winternl.h>
#include "d3dkmtQueryStatistics.h"

#include "SurfaceCache.h"
#include "gfxPrefs.h"

#include "VsyncSource.h"
#include "DriverInitCrashDetection.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::image;

DCFromDrawTarget::DCFromDrawTarget(DrawTarget& aDrawTarget)
{
  mDC = nullptr;
  if (aDrawTarget.GetBackendType() == BackendType::CAIRO) {
    cairo_surface_t *surf = (cairo_surface_t*)
        aDrawTarget.GetNativeSurface(NativeSurfaceType::CAIRO_SURFACE);
    if (surf) {
      cairo_surface_type_t surfaceType = cairo_surface_get_type(surf);
      if (surfaceType == CAIRO_SURFACE_TYPE_WIN32 ||
          surfaceType == CAIRO_SURFACE_TYPE_WIN32_PRINTING) {
        mDC = cairo_win32_surface_get_dc(surf);
        mNeedsRelease = false;
        SaveDC(mDC);
        cairo_t* ctx = (cairo_t*)
            aDrawTarget.GetNativeSurface(NativeSurfaceType::CAIRO_CONTEXT);
        cairo_scaled_font_t* scaled = cairo_get_scaled_font(ctx);
        cairo_win32_scaled_font_select_font(scaled, mDC);
      }
    }
    if (!mDC) {
      mDC = GetDC(nullptr);
      SetGraphicsMode(mDC, GM_ADVANCED);
      mNeedsRelease = true;
    }
  }
}

#ifdef CAIRO_HAS_D2D_SURFACE

static const char *kFeatureLevelPref =
  "gfx.direct3d.last_used_feature_level_idx";
static const int kSupportedFeatureLevels[] =
  { D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0 };

class GfxD2DSurfaceReporter final : public nsIMemoryReporter
{
    ~GfxD2DSurfaceReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
    {
        nsresult rv;

        int64_t amount = cairo_d2d_get_image_surface_cache_usage();
        rv = MOZ_COLLECT_REPORT(
            "gfx-d2d-surface-cache", KIND_OTHER, UNITS_BYTES, amount,
            "Memory used by the Direct2D internal surface cache.");
        NS_ENSURE_SUCCESS(rv, rv);

        cairo_device_t *device =
            gfxWindowsPlatform::GetPlatform()->GetD2DDevice();
        amount = device ? cairo_d2d_get_surface_vram_usage(device) : 0;
        rv = MOZ_COLLECT_REPORT(
            "gfx-d2d-surface-vram", KIND_OTHER, UNITS_BYTES, amount,
            "Video memory used by D2D surfaces.");
        NS_ENSURE_SUCCESS(rv, rv);

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(GfxD2DSurfaceReporter, nsIMemoryReporter)

#endif

class GfxD2DVramReporter final : public nsIMemoryReporter
{
    ~GfxD2DVramReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
    {
        nsresult rv;

        rv = MOZ_COLLECT_REPORT(
            "gfx-d2d-vram-draw-target", KIND_OTHER, UNITS_BYTES,
            Factory::GetD2DVRAMUsageDrawTarget(),
            "Video memory used by D2D DrawTargets.");
        NS_ENSURE_SUCCESS(rv, rv);

        rv = MOZ_COLLECT_REPORT(
            "gfx-d2d-vram-source-surface", KIND_OTHER, UNITS_BYTES,
            Factory::GetD2DVRAMUsageSourceSurface(),
            "Video memory used by D2D SourceSurfaces.");
        NS_ENSURE_SUCCESS(rv, rv);

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(GfxD2DVramReporter, nsIMemoryReporter)

#define GFX_USE_CLEARTYPE_ALWAYS "gfx.font_rendering.cleartype.always_use_for_content"
#define GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE "gfx.font_rendering.cleartype.use_for_downloadable_fonts"

#define GFX_CLEARTYPE_PARAMS           "gfx.font_rendering.cleartype_params."
#define GFX_CLEARTYPE_PARAMS_GAMMA     "gfx.font_rendering.cleartype_params.gamma"
#define GFX_CLEARTYPE_PARAMS_CONTRAST  "gfx.font_rendering.cleartype_params.enhanced_contrast"
#define GFX_CLEARTYPE_PARAMS_LEVEL     "gfx.font_rendering.cleartype_params.cleartype_level"
#define GFX_CLEARTYPE_PARAMS_STRUCTURE "gfx.font_rendering.cleartype_params.pixel_structure"
#define GFX_CLEARTYPE_PARAMS_MODE      "gfx.font_rendering.cleartype_params.rendering_mode"

class GPUAdapterReporter final : public nsIMemoryReporter
{
    // Callers must Release the DXGIAdapter after use or risk mem-leak
    static bool GetDXGIAdapter(IDXGIAdapter **DXGIAdapter)
    {
        ID3D10Device1 *D2D10Device;
        IDXGIDevice *DXGIDevice;
        bool result = false;

        if ((D2D10Device = mozilla::gfx::Factory::GetDirect3D10Device())) {
            if (D2D10Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice) == S_OK) {
                result = (DXGIDevice->GetAdapter(DXGIAdapter) == S_OK);
                DXGIDevice->Release();
            }
        }

        return result;
    }

    ~GPUAdapterReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD
    CollectReports(nsIMemoryReporterCallback* aCb,
                   nsISupports* aClosure, bool aAnonymize)
    {
        HANDLE ProcessHandle = GetCurrentProcess();

        int64_t dedicatedBytesUsed = 0;
        int64_t sharedBytesUsed = 0;
        int64_t committedBytesUsed = 0;
        IDXGIAdapter *DXGIAdapter;

        HMODULE gdi32Handle;
        PFND3DKMTQS queryD3DKMTStatistics;

        // GPU memory reporting is not available before Windows 7
        if (!IsWin7OrLater())
            return NS_OK;

        if ((gdi32Handle = LoadLibrary(TEXT("gdi32.dll"))))
            queryD3DKMTStatistics = (PFND3DKMTQS)GetProcAddress(gdi32Handle, "D3DKMTQueryStatistics");

        if (queryD3DKMTStatistics && GetDXGIAdapter(&DXGIAdapter)) {
            // Most of this block is understood thanks to wj32's work on Process Hacker

            DXGI_ADAPTER_DESC adapterDesc;
            D3DKMTQS queryStatistics;

            DXGIAdapter->GetDesc(&adapterDesc);
            DXGIAdapter->Release();

            memset(&queryStatistics, 0, sizeof(D3DKMTQS));
            queryStatistics.Type = D3DKMTQS_PROCESS;
            queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
            queryStatistics.hProcess = ProcessHandle;
            if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                committedBytesUsed = queryStatistics.QueryResult.ProcessInfo.SystemMemory.BytesAllocated;
            }

            memset(&queryStatistics, 0, sizeof(D3DKMTQS));
            queryStatistics.Type = D3DKMTQS_ADAPTER;
            queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
            if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                ULONG i;
                ULONG segmentCount = queryStatistics.QueryResult.AdapterInfo.NbSegments;

                for (i = 0; i < segmentCount; i++) {
                    memset(&queryStatistics, 0, sizeof(D3DKMTQS));
                    queryStatistics.Type = D3DKMTQS_SEGMENT;
                    queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
                    queryStatistics.QuerySegment.SegmentId = i;

                    if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                        bool aperture;

                        // SegmentInformation has a different definition in Win7 than later versions
                        if (!IsWin8OrLater())
                            aperture = queryStatistics.QueryResult.SegmentInfoWin7.Aperture;
                        else
                            aperture = queryStatistics.QueryResult.SegmentInfoWin8.Aperture;

                        memset(&queryStatistics, 0, sizeof(D3DKMTQS));
                        queryStatistics.Type = D3DKMTQS_PROCESS_SEGMENT;
                        queryStatistics.AdapterLuid = adapterDesc.AdapterLuid;
                        queryStatistics.hProcess = ProcessHandle;
                        queryStatistics.QueryProcessSegment.SegmentId = i;
                        if (NT_SUCCESS(queryD3DKMTStatistics(&queryStatistics))) {
                            ULONGLONG bytesCommitted;
                            if (!IsWin8OrLater())
                                bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInfo.Win7.BytesCommitted;
                            else
                                bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInfo.Win8.BytesCommitted;
                            if (aperture)
                                sharedBytesUsed += bytesCommitted;
                            else
                                dedicatedBytesUsed += bytesCommitted;
                        }
                    }
                }
            }
        }

        FreeLibrary(gdi32Handle);

#define REPORT(_path, _amount, _desc)                                         \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aCb->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path),           \
                         KIND_OTHER, UNITS_BYTES, _amount,                    \
                         NS_LITERAL_CSTRING(_desc), aClosure);                \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
    } while (0)

        REPORT("gpu-committed", committedBytesUsed,
               "Memory committed by the Windows graphics system.");

        REPORT("gpu-dedicated", dedicatedBytesUsed,
               "Out-of-process memory allocated for this process in a "
               "physical GPU adapter's memory.");

        REPORT("gpu-shared", sharedBytesUsed,
               "In-process memory that is shared with the GPU.");

#undef REPORT

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(GPUAdapterReporter, nsIMemoryReporter)


Atomic<size_t> gfxWindowsPlatform::sD3D11MemoryUsed;

class D3D11TextureReporter final : public nsIMemoryReporter
{
  ~D3D11TextureReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
      return MOZ_COLLECT_REPORT("d3d11-shared-textures", KIND_OTHER, UNITS_BYTES,
                                gfxWindowsPlatform::sD3D11MemoryUsed,
                                "Memory used for D3D11 shared textures");
  }
};

NS_IMPL_ISUPPORTS(D3D11TextureReporter, nsIMemoryReporter)

Atomic<size_t> gfxWindowsPlatform::sD3D9MemoryUsed;

class D3D9TextureReporter final : public nsIMemoryReporter
{
  ~D3D9TextureReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT("d3d9-shared-textures", KIND_OTHER, UNITS_BYTES,
                              gfxWindowsPlatform::sD3D9MemoryUsed,
                              "Memory used for D3D9 shared textures");
  }
};

NS_IMPL_ISUPPORTS(D3D9TextureReporter, nsIMemoryReporter)

Atomic<size_t> gfxWindowsPlatform::sD3D9SurfaceImageUsed;

class D3D9SurfaceImageReporter final : public nsIMemoryReporter
{
  ~D3D9SurfaceImageReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT("d3d9-surface-image", KIND_OTHER, UNITS_BYTES,
                              gfxWindowsPlatform::sD3D9SurfaceImageUsed,
                              "Memory used for D3D9 surface images");
  }
};

NS_IMPL_ISUPPORTS(D3D9SurfaceImageReporter, nsIMemoryReporter)

Atomic<size_t> gfxWindowsPlatform::sD3D9SharedTextureUsed;

class D3D9SharedTextureReporter final : public nsIMemoryReporter
{
  ~D3D9SharedTextureReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback *aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT("d3d9-shared-texture", KIND_OTHER, UNITS_BYTES,
                              gfxWindowsPlatform::sD3D9SharedTextureUsed,
                              "Memory used for D3D9 shared textures");
  }
};

NS_IMPL_ISUPPORTS(D3D9SharedTextureReporter, nsIMemoryReporter)

gfxWindowsPlatform::gfxWindowsPlatform()
  : mUseDirectWrite(false)
  , mD3D11DeviceInitialized(false)
  , mIsWARP(false)
  , mHasDeviceReset(false)
  , mDoesD3D11TextureSharingWork(false)
{
    mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
    mUseClearTypeAlways = UNINITIALIZED_VALUE;

    mUsingGDIFonts = false;

    /* 
     * Initialize COM 
     */ 
    CoInitialize(nullptr); 

#ifdef CAIRO_HAS_D2D_SURFACE
    RegisterStrongMemoryReporter(new GfxD2DSurfaceReporter());
    mD2DDevice = nullptr;
#endif
    RegisterStrongMemoryReporter(new GfxD2DVramReporter());

    if (gfxPrefs::Direct2DUse1_1()) {
      InitD3D11Devices();
    }

    UpdateRenderMode();

    RegisterStrongMemoryReporter(new GPUAdapterReporter());
    RegisterStrongMemoryReporter(new D3D11TextureReporter());
    RegisterStrongMemoryReporter(new D3D9TextureReporter());
    RegisterStrongMemoryReporter(new D3D9SurfaceImageReporter());
    RegisterStrongMemoryReporter(new D3D9SharedTextureReporter());
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
    mDeviceManager = nullptr;
    mD3D11Device = nullptr;
    mD3D11ContentDevice = nullptr;
    mD3D11ImageBridgeDevice = nullptr;

    // not calling FT_Done_FreeType because cairo may still hold references to
    // these FT_Faces.  See bug 458169.
#ifdef CAIRO_HAS_D2D_SURFACE
    if (mD2DDevice) {
        cairo_release_device(mD2DDevice);
    }
#endif

    mozilla::gfx::Factory::D2DCleanup();

    mAdapter = nullptr;

    /* 
     * Uninitialize COM 
     */ 
    CoUninitialize();
}

double
gfxWindowsPlatform::GetDPIScale()
{
  return WinUtils::LogToPhysFactor();
}

bool
gfxWindowsPlatform::CanUseHardwareVideoDecoding()
{
    if (!gfxPrefs::LayersPreferD3D9() && !mDoesD3D11TextureSharingWork) {
        return false;
    }
    return !IsWARP() && gfxPlatform::CanUseHardwareVideoDecoding();
}

void
gfxWindowsPlatform::UpdateRenderMode()
{
/* Pick the default render mode for
 * desktop.
 */
    bool didReset = false;
    DeviceResetReason resetReason = DeviceResetReason::OK;
    if (DidRenderingDeviceReset(&resetReason)) {
      Telemetry::Accumulate(Telemetry::DEVICE_RESET_REASON, uint32_t(resetReason));
      mD3D11DeviceInitialized = false;
      mD3D11Device = nullptr;
      mD3D11ContentDevice = nullptr;
      mAdapter = nullptr;
      mDeviceResetReason = DeviceResetReason::OK;
      mHasDeviceReset = false;

      imgLoader::Singleton()->ClearCache(true);
      imgLoader::Singleton()->ClearCache(false);
      Factory::SetDirect3D11Device(nullptr);

      didReset = true;
    }

    mRenderMode = RENDER_GDI;

    bool isVistaOrHigher = IsVistaOrLater();

    mUseDirectWrite = false;

#ifdef CAIRO_HAS_D2D_SURFACE
    bool d2dDisabled = false;
    bool d2dForceEnabled = false;
    bool d2dBlocked = false;

    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo) {
        int32_t status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT2D, &status))) {
            if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
                d2dBlocked = true;
            }
        }
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, &status))) {
            if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
                d2dBlocked = true;
            }
        }
    }

    // These will only be evaluated once, and any subsequent changes to
    // the preferences will be ignored until restart.
    d2dDisabled = gfxPrefs::Direct2DDisabled();
    d2dForceEnabled = gfxPrefs::Direct2DForceEnabled();

    bool tryD2D = d2dForceEnabled || (!d2dBlocked && !gfxPrefs::LayersPreferD3D9());

    // Do not ever try if d2d is explicitly disabled,
    // or if we're not using DWrite fonts.
    if (d2dDisabled || mUsingGDIFonts) {
        tryD2D = false;
    }

    ID3D11Device *device = GetD3D11Device();
    if (isVistaOrHigher && !InSafeMode() && tryD2D && device &&
        mDoesD3D11TextureSharingWork) {

        VerifyD2DDevice(d2dForceEnabled);
        if (mD2DDevice && GetD3D11Device()) {
            mRenderMode = RENDER_DIRECT2D;
            mUseDirectWrite = true;
        }
    } else {
        mD2DDevice = nullptr;
    }
#endif

#ifdef CAIRO_HAS_DWRITE_FONT
    // Enable when it's preffed on -and- we're using Vista or higher. Or when
    // we're going to use D2D.
    if (!mDWriteFactory && (mUseDirectWrite && isVistaOrHigher)) {
        mozilla::ScopedGfxFeatureReporter reporter("DWrite");
        decltype(DWriteCreateFactory)* createDWriteFactory = (decltype(DWriteCreateFactory)*)
            GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory");

        if (createDWriteFactory) {
            /**
             * I need a direct pointer to be able to cast to IUnknown**, I also
             * need to remember to release this because the nsRefPtr will
             * AddRef it.
             */
            IDWriteFactory *factory;
            HRESULT hr = createDWriteFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&factory));

            if (SUCCEEDED(hr) && factory) {
                mDWriteFactory = factory;
                factory->Release();
                hr = mDWriteFactory->CreateTextAnalyzer(
                    getter_AddRefs(mDWriteAnalyzer));
            }

            SetupClearTypeParams();

            if (hr == S_OK)
              reporter.SetSuccessful();
        }
    }
#endif

    uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO);
    uint32_t contentMask = BackendTypeBit(BackendType::CAIRO);
    BackendType defaultBackend = BackendType::CAIRO;
    if (mRenderMode == RENDER_DIRECT2D) {
      canvasMask |= BackendTypeBit(BackendType::DIRECT2D);
      contentMask |= BackendTypeBit(BackendType::DIRECT2D);
      if (gfxPrefs::Direct2DUse1_1() && Factory::SupportsD2D1() &&
          GetD3D11ContentDevice()) {
        contentMask |= BackendTypeBit(BackendType::DIRECT2D1_1);
        canvasMask |= BackendTypeBit(BackendType::DIRECT2D1_1);
        defaultBackend = BackendType::DIRECT2D1_1;
      } else {
        defaultBackend = BackendType::DIRECT2D;
      }
    } else {
      canvasMask |= BackendTypeBit(BackendType::SKIA);
    }
    contentMask |= BackendTypeBit(BackendType::SKIA);
    InitBackendPrefs(canvasMask, defaultBackend,
                     contentMask, defaultBackend);

    if (didReset) {
      mScreenReferenceDrawTarget = CreateOffscreenContentDrawTarget(IntSize(1, 1), SurfaceFormat::B8G8R8A8);
    }
}

#ifdef CAIRO_HAS_D2D_SURFACE
HRESULT
gfxWindowsPlatform::CreateDevice(nsRefPtr<IDXGIAdapter1> &adapter1,
                                 int featureLevelIndex)
{
  nsModuleHandle d3d10module(LoadLibrarySystem32(L"d3d10_1.dll"));
  if (!d3d10module)
    return E_FAIL;
  decltype(D3D10CreateDevice1)* createD3DDevice =
    (decltype(D3D10CreateDevice1)*) GetProcAddress(d3d10module, "D3D10CreateDevice1");
  if (!createD3DDevice)
    return E_FAIL;

  nsRefPtr<ID3D10Device1> device;
  HRESULT hr =
    createD3DDevice(adapter1, D3D10_DRIVER_TYPE_HARDWARE, nullptr,
#ifdef DEBUG
                    // This isn't set because of bug 1078411
                    // D3D10_CREATE_DEVICE_DEBUG |
#endif
                    D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                    D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                    static_cast<D3D10_FEATURE_LEVEL1>(kSupportedFeatureLevels[featureLevelIndex]),
                    D3D10_1_SDK_VERSION, getter_AddRefs(device));

  // If we fail here, the DirectX version or video card probably
  // changed.  We previously could use 10.1 but now we can't
  // anymore.  Revert back to doing a 10.0 check first before
  // the 10.1 check.
  if (device) {
    mD2DDevice = cairo_d2d_create_device_from_d3d10device(device);

    // Setup a pref for future launch optimizaitons when in main process.
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      Preferences::SetInt(kFeatureLevelPref, featureLevelIndex);
    }
  }

  return device ? S_OK : hr;
}
#endif

void
gfxWindowsPlatform::VerifyD2DDevice(bool aAttemptForce)
{
#ifdef CAIRO_HAS_D2D_SURFACE
    DriverInitCrashDetection detectCrashes;
    if (detectCrashes.DisableAcceleration()) {
      return;
    }

    if (mD2DDevice) {
        ID3D10Device1 *device = cairo_d2d_device_get_device(mD2DDevice);

        if (SUCCEEDED(device->GetDeviceRemovedReason())) {
            return;
        }
        mD2DDevice = nullptr;

        // Surface cache needs to be invalidated since it may contain vector
        // images rendered with our old, broken D2D device.
        SurfaceCache::DiscardAll();
    }

    mozilla::ScopedGfxFeatureReporter reporter("D2D", aAttemptForce);

    nsRefPtr<ID3D10Device1> device;

    int supportedFeatureLevelsCount = ArrayLength(kSupportedFeatureLevels);

    nsRefPtr<IDXGIAdapter1> adapter1 = GetDXGIAdapter();

    if (!adapter1) {
      // Unable to create adapter, abort acceleration.
      return;
    }

    // It takes a lot of time (5-10% of startup time or ~100ms) to do both
    // a createD3DDevice on D3D10_FEATURE_LEVEL_10_0.  We therefore store
    // the last used feature level to go direct to that.
    int featureLevelIndex = Preferences::GetInt(kFeatureLevelPref, 0);
    if (featureLevelIndex >= supportedFeatureLevelsCount || featureLevelIndex < 0)
      featureLevelIndex = 0;

    // Start with the last used feature level, and move to lower DX versions
    // until we find one that works.
    HRESULT hr = E_FAIL;
    for (int i = featureLevelIndex; i < supportedFeatureLevelsCount; i++) {
      hr = CreateDevice(adapter1, i);
      // If it succeeded we found the first available feature level
      if (SUCCEEDED(hr))
        break;
    }

    // If we succeeded in creating a device, try for a newer device
    // that we haven't tried yet.
    if (SUCCEEDED(hr)) {
      for (int i = featureLevelIndex - 1; i >= 0; i--) {
        hr = CreateDevice(adapter1, i);
        // If it failed then we don't have new hardware
        if (FAILED(hr)) {
          break;
        }
      }
    }

    if (!mD2DDevice && aAttemptForce) {
        mD2DDevice = cairo_d2d_create_device();
    }

    if (mD2DDevice) {
        reporter.SetSuccessful();
        mozilla::gfx::Factory::SetDirect3D10Device(cairo_d2d_device_get_device(mD2DDevice));
    }

    ScopedGfxFeatureReporter reporter1_1("D2D1.1");

    if (Factory::SupportsD2D1()) {
      reporter1_1.SetSuccessful();
    }
#endif
}

gfxPlatformFontList*
gfxWindowsPlatform::CreatePlatformFontList()
{
    mUsingGDIFonts = false;
    gfxPlatformFontList *pfl;
#ifdef CAIRO_HAS_DWRITE_FONT
    // bug 630201 - older pre-RTM versions of Direct2D/DirectWrite cause odd
    // crashers so blacklist them altogether
    if (IsNotWin7PreRTM() && GetDWriteFactory()) {
        pfl = new gfxDWriteFontList();
        if (NS_SUCCEEDED(pfl->InitFontList())) {
            return pfl;
        }
        // DWrite font initialization failed! Don't know why this would happen,
        // but apparently it can - see bug 594865.
        // So we're going to fall back to GDI fonts & rendering.
        gfxPlatformFontList::Shutdown();
        SetRenderMode(RENDER_GDI);
    }
#endif
    pfl = new gfxGDIFontList();
    mUsingGDIFonts = true;

    if (NS_SUCCEEDED(pfl->InitFontList())) {
        return pfl;
    }

    gfxPlatformFontList::Shutdown();
    return nullptr;
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(const IntSize& aSize,
                                           gfxImageFormat aFormat)
{
    nsRefPtr<gfxASurface> surf = nullptr;

#ifdef CAIRO_HAS_WIN32_SURFACE
    if (mRenderMode == RENDER_GDI)
        surf = new gfxWindowsSurface(aSize, aFormat);
#endif

#ifdef CAIRO_HAS_D2D_SURFACE
    if (mRenderMode == RENDER_DIRECT2D)
        surf = new gfxD2DSurface(aSize, aFormat);
#endif

    if (!surf || surf->CairoStatus()) {
        surf = new gfxImageSurface(aSize, aFormat);
    }

    return surf.forget();
}

TemporaryRef<ScaledFont>
gfxWindowsPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    if (aFont->GetType() == gfxFont::FONT_TYPE_DWRITE) {
        gfxDWriteFont *font = static_cast<gfxDWriteFont*>(aFont);

        NativeFont nativeFont;
        nativeFont.mType = NativeFontType::DWRITE_FONT_FACE;
        nativeFont.mFont = font->GetFontFace();

        if (aTarget->GetBackendType() == BackendType::CAIRO) {
          return Factory::CreateScaledFontWithCairo(nativeFont,
                                                    font->GetAdjustedSize(),
                                                    font->GetCairoScaledFont());
        }

        return Factory::CreateScaledFontForNativeFont(nativeFont,
                                                      font->GetAdjustedSize());
    }

    NS_ASSERTION(aFont->GetType() == gfxFont::FONT_TYPE_GDI,
        "Fonts on windows should be GDI or DWrite!");

    NativeFont nativeFont;
    nativeFont.mType = NativeFontType::GDI_FONT_FACE;
    LOGFONT lf;
    GetObject(static_cast<gfxGDIFont*>(aFont)->GetHFONT(), sizeof(LOGFONT), &lf);
    nativeFont.mFont = &lf;

    if (aTarget->GetBackendType() == BackendType::CAIRO) {
      return Factory::CreateScaledFontWithCairo(nativeFont,
                                                aFont->GetAdjustedSize(),
                                                aFont->GetCairoScaledFont());
    }

    return Factory::CreateScaledFontForNativeFont(nativeFont, aFont->GetAdjustedSize());
}

nsresult
gfxWindowsPlatform::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup, aGenericFamily, aListOfFonts);

    return NS_OK;
}

nsresult
gfxWindowsPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();

    return NS_OK;
}

static const char kFontAparajita[] = "Aparajita";
static const char kFontArabicTypesetting[] = "Arabic Typesetting";
static const char kFontArial[] = "Arial";
static const char kFontArialUnicodeMS[] = "Arial Unicode MS";
static const char kFontCambria[] = "Cambria";
static const char kFontCambriaMath[] = "Cambria Math";
static const char kFontEbrima[] = "Ebrima";
static const char kFontEstrangeloEdessa[] = "Estrangelo Edessa";
static const char kFontEuphemia[] = "Euphemia";
static const char kFontGabriola[] = "Gabriola";
static const char kFontJavaneseText[] = "Javanese Text";
static const char kFontKhmerUI[] = "Khmer UI";
static const char kFontLaoUI[] = "Lao UI";
static const char kFontLeelawadeeUI[] = "Leelawadee UI";
static const char kFontLucidaSansUnicode[] = "Lucida Sans Unicode";
static const char kFontMVBoli[] = "MV Boli";
static const char kFontMalgunGothic[] = "Malgun Gothic";
static const char kFontMicrosoftJhengHei[] = "Microsoft JhengHei";
static const char kFontMicrosoftNewTaiLue[] = "Microsoft New Tai Lue";
static const char kFontMicrosoftPhagsPa[] = "Microsoft PhagsPa";
static const char kFontMicrosoftTaiLe[] = "Microsoft Tai Le";
static const char kFontMicrosoftUighur[] = "Microsoft Uighur";
static const char kFontMicrosoftYaHei[] = "Microsoft YaHei";
static const char kFontMicrosoftYiBaiti[] = "Microsoft Yi Baiti";
static const char kFontMeiryo[] = "Meiryo";
static const char kFontMongolianBaiti[] = "Mongolian Baiti";
static const char kFontMyanmarText[] = "Myanmar Text";
static const char kFontNirmalaUI[] = "Nirmala UI";
static const char kFontNyala[] = "Nyala";
static const char kFontPlantagenetCherokee[] = "Plantagenet Cherokee";
static const char kFontSegoeUI[] = "Segoe UI";
static const char kFontSegoeUIEmoji[] = "Segoe UI Emoji";
static const char kFontSegoeUISymbol[] = "Segoe UI Symbol";
static const char kFontSylfaen[] = "Sylfaen";
static const char kFontTraditionalArabic[] = "Traditional Arabic";
static const char kFontUtsaah[] = "Utsaah";
static const char kFontYuGothic[] = "Yu Gothic";

void
gfxWindowsPlatform::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                           int32_t aRunScript,
                                           nsTArray<const char*>& aFontList)
{
    if (aNextCh == 0xfe0fu) {
        aFontList.AppendElement(kFontSegoeUIEmoji);
    }

    // Arial is used as the default fallback for system fallback
    aFontList.AppendElement(kFontArial);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) { // SMP plane
            if (aNextCh == 0xfe0eu) {
                aFontList.AppendElement(kFontSegoeUISymbol);
                aFontList.AppendElement(kFontSegoeUIEmoji);
            } else {
                if (aNextCh != 0xfe0fu) {
                    aFontList.AppendElement(kFontSegoeUIEmoji);
                }
                aFontList.AppendElement(kFontSegoeUISymbol);
            }
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontNirmalaUI);
            aFontList.AppendElement(kFontCambriaMath);
        }
    } else {
        uint32_t b = (aCh >> 8) & 0xff;

        switch (b) {
        case 0x05:
            aFontList.AppendElement(kFontEstrangeloEdessa);
            aFontList.AppendElement(kFontCambria);
            break;
        case 0x06:
            aFontList.AppendElement(kFontMicrosoftUighur);
            break;
        case 0x07:
            aFontList.AppendElement(kFontEstrangeloEdessa);
            aFontList.AppendElement(kFontMVBoli);
            aFontList.AppendElement(kFontEbrima);
            break;
        case 0x09:
            aFontList.AppendElement(kFontNirmalaUI);
            aFontList.AppendElement(kFontUtsaah);
            aFontList.AppendElement(kFontAparajita);
            break;
        case 0x0e:
            aFontList.AppendElement(kFontLaoUI);
            break;
        case 0x10:
            aFontList.AppendElement(kFontMyanmarText);
            break;
        case 0x11:
            aFontList.AppendElement(kFontMalgunGothic);
            break;
        case 0x12:
        case 0x13:
            aFontList.AppendElement(kFontNyala);
            aFontList.AppendElement(kFontPlantagenetCherokee);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
            aFontList.AppendElement(kFontEuphemia);
            aFontList.AppendElement(kFontSegoeUISymbol);
            break;
        case 0x17:
            aFontList.AppendElement(kFontKhmerUI);
            break;
        case 0x18:  // Mongolian
            aFontList.AppendElement(kFontMongolianBaiti);
            aFontList.AppendElement(kFontEuphemia);
            break;
        case 0x19:
            aFontList.AppendElement(kFontMicrosoftTaiLe);
            aFontList.AppendElement(kFontMicrosoftNewTaiLue);
            aFontList.AppendElement(kFontKhmerUI);
            break;
            break;
        case 0x1a:
            aFontList.AppendElement(kFontLeelawadeeUI);
            break;
        case 0x1c:
            aFontList.AppendElement(kFontNirmalaUI);
            break;
        case 0x20:  // Symbol ranges
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
            aFontList.AppendElement(kFontSegoeUI);
            aFontList.AppendElement(kFontSegoeUISymbol);
            aFontList.AppendElement(kFontCambria);
            aFontList.AppendElement(kFontMeiryo);
            aFontList.AppendElement(kFontArial);
            aFontList.AppendElement(kFontLucidaSansUnicode);
            aFontList.AppendElement(kFontEbrima);
            break;
        case 0x2d:
        case 0x2e:
        case 0x2f:
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontNyala);
            aFontList.AppendElement(kFontSegoeUI);
            aFontList.AppendElement(kFontSegoeUISymbol);
            aFontList.AppendElement(kFontMeiryo);
            break;
        case 0x28:  // Braille
            aFontList.AppendElement(kFontSegoeUISymbol);
            break;
        case 0x30:
        case 0x31:
            aFontList.AppendElement(kFontMicrosoftYaHei);
            break;
        case 0x32:
            aFontList.AppendElement(kFontMalgunGothic);
            break;
        case 0x4d:
            aFontList.AppendElement(kFontSegoeUISymbol);
            break;
        case 0x9f:
            aFontList.AppendElement(kFontMicrosoftYaHei);
            aFontList.AppendElement(kFontYuGothic);
            break;
        case 0xa0:  // Yi
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
            aFontList.AppendElement(kFontMicrosoftYiBaiti);
            aFontList.AppendElement(kFontSegoeUI);
            break;
        case 0xa5:
        case 0xa6:
        case 0xa7:
            aFontList.AppendElement(kFontEbrima);
            aFontList.AppendElement(kFontSegoeUI);
            aFontList.AppendElement(kFontCambriaMath);
            break;
        case 0xa8:
             aFontList.AppendElement(kFontMicrosoftPhagsPa);
             aFontList.AppendElement(kFontNirmalaUI);
             break;
        case 0xa9:
             aFontList.AppendElement(kFontMalgunGothic);
             aFontList.AppendElement(kFontJavaneseText);
             break;
        case 0xaa:
             aFontList.AppendElement(kFontMyanmarText);
             break;
        case 0xab:
             aFontList.AppendElement(kFontEbrima);
             aFontList.AppendElement(kFontNyala);
             break;
        case 0xd7:
             aFontList.AppendElement(kFontMalgunGothic);
             break;
        case 0xfb:
            aFontList.AppendElement(kFontMicrosoftUighur);
            aFontList.AppendElement(kFontGabriola);
            aFontList.AppendElement(kFontSylfaen);
            break;
        case 0xfc:
        case 0xfd:
            aFontList.AppendElement(kFontTraditionalArabic);
            aFontList.AppendElement(kFontArabicTypesetting);
            break;
        case 0xfe:
            aFontList.AppendElement(kFontTraditionalArabic);
            aFontList.AppendElement(kFontMicrosoftJhengHei);
           break;
       case 0xff:
            aFontList.AppendElement(kFontMicrosoftJhengHei);
            break;
        default:
            break;
        }
    }

    // Arial Unicode MS has lots of glyphs for obscure characters,
    // use it as a last resort
    aFontList.AppendElement(kFontArialUnicodeMS);
}

nsresult
gfxWindowsPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxWindowsPlatform::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                    const gfxFontStyle *aStyle,
                                    gfxUserFontSet *aUserFontSet)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aUserFontSet);
}

gfxFontEntry* 
gfxWindowsPlatform::LookupLocalFont(const nsAString& aFontName,
                                    uint16_t aWeight,
                                    int16_t aStretch,
                                    bool aItalic)
{
    return gfxPlatformFontList::PlatformFontList()->LookupLocalFont(aFontName,
                                                                    aWeight,
                                                                    aStretch,
                                                                    aItalic);
}

gfxFontEntry* 
gfxWindowsPlatform::MakePlatformFont(const nsAString& aFontName,
                                     uint16_t aWeight,
                                     int16_t aStretch,
                                     bool aItalic,
                                     const uint8_t* aFontData,
                                     uint32_t aLength)
{
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aFontName,
                                                                     aWeight,
                                                                     aStretch,
                                                                     aItalic,
                                                                     aFontData,
                                                                     aLength);
}

bool
gfxWindowsPlatform::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & gfxUserFontSet::FLAG_FORMATS_COMMON) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

static DeviceResetReason HResultToResetReason(HRESULT hr)
{
  switch (hr) {
  case DXGI_ERROR_DEVICE_HUNG:
    return DeviceResetReason::HUNG;
  case DXGI_ERROR_DEVICE_REMOVED:
    return DeviceResetReason::REMOVED;
  case DXGI_ERROR_DEVICE_RESET:
    return DeviceResetReason::RESET;
  case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
    return DeviceResetReason::DRIVER_ERROR;
  case DXGI_ERROR_INVALID_CALL:
    return DeviceResetReason::INVALID_CALL;
  case E_OUTOFMEMORY:
    return DeviceResetReason::OUT_OF_MEMORY;
  default:
    MOZ_ASSERT(false);
  }
  return DeviceResetReason::UNKNOWN;
}

bool
gfxWindowsPlatform::IsDeviceReset(HRESULT hr, DeviceResetReason* aResetReason)
{
  if (hr != S_OK) {
    mDeviceResetReason = HResultToResetReason(hr);
    mHasDeviceReset = true;
    if (aResetReason) {
      *aResetReason = mDeviceResetReason;
    }
    return true;
  }
  return false;
}

bool
gfxWindowsPlatform::DidRenderingDeviceReset(DeviceResetReason* aResetReason)
{
  if (mHasDeviceReset) {
    if (aResetReason) {
      *aResetReason = mDeviceResetReason;
    }
    return true;
  }
  if (aResetReason) {
    *aResetReason = DeviceResetReason::OK;
  }

  if (mD3D11Device) {
    HRESULT hr = mD3D11Device->GetDeviceRemovedReason();
    if (IsDeviceReset(hr, aResetReason)) {
      return true;
    }
  }
  if (mD3D11ContentDevice) {
    HRESULT hr = mD3D11ContentDevice->GetDeviceRemovedReason();
    if (IsDeviceReset(hr, aResetReason)) {
      return true;
    }
  }
  if (GetD3D10Device()) {
    HRESULT hr = GetD3D10Device()->GetDeviceRemovedReason();
    if (IsDeviceReset(hr, aResetReason)) {
      return true;
    }
  }
  return false;
}

void
gfxWindowsPlatform::GetPlatformCMSOutputProfile(void* &mem, size_t &mem_size)
{
    WCHAR str[MAX_PATH];
    DWORD size = MAX_PATH;
    BOOL res;

    mem = nullptr;
    mem_size = 0;

    HDC dc = GetDC(nullptr);
    if (!dc)
        return;

    MOZ_SEH_TRY {
        res = GetICMProfileW(dc, &size, (LPWSTR)&str);
    } MOZ_SEH_EXCEPT(GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION) {
        res = FALSE;
    }

    ReleaseDC(nullptr, dc);
    if (!res)
        return;

#ifdef _WIN32
    qcms_data_from_unicode_path(str, &mem, &mem_size);

#ifdef DEBUG_tor
    if (mem_size > 0)
        fprintf(stderr,
                "ICM profile read from %s successfully\n",
                NS_ConvertUTF16toUTF8(str).get());
#endif // DEBUG_tor
#endif // _WIN32
}

bool
gfxWindowsPlatform::UseClearTypeForDownloadableFonts()
{
    if (mUseClearTypeForDownloadableFonts == UNINITIALIZED_VALUE) {
        mUseClearTypeForDownloadableFonts = Preferences::GetBool(GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE, true);
    }

    return mUseClearTypeForDownloadableFonts;
}

bool
gfxWindowsPlatform::UseClearTypeAlways()
{
    if (mUseClearTypeAlways == UNINITIALIZED_VALUE) {
        mUseClearTypeAlways = Preferences::GetBool(GFX_USE_CLEARTYPE_ALWAYS, false);
    }

    return mUseClearTypeAlways;
}

void 
gfxWindowsPlatform::GetDLLVersion(char16ptr_t aDLLPath, nsAString& aVersion)
{
    DWORD versInfoSize, vers[4] = {0};
    // version info not available case
    aVersion.AssignLiteral(MOZ_UTF16("0.0.0.0"));
    versInfoSize = GetFileVersionInfoSizeW(aDLLPath, nullptr);
    nsAutoTArray<BYTE,512> versionInfo;
    
    if (versInfoSize == 0 ||
        !versionInfo.AppendElements(uint32_t(versInfoSize)))
    {
        return;
    }

    if (!GetFileVersionInfoW(aDLLPath, 0, versInfoSize, 
           LPBYTE(versionInfo.Elements())))
    {
        return;
    } 

    UINT len = 0;
    VS_FIXEDFILEINFO *fileInfo = nullptr;
    if (!VerQueryValue(LPBYTE(versionInfo.Elements()), TEXT("\\"),
           (LPVOID *)&fileInfo, &len) ||
        len == 0 ||
        fileInfo == nullptr)
    {
        return;
    }

    DWORD fileVersMS = fileInfo->dwFileVersionMS; 
    DWORD fileVersLS = fileInfo->dwFileVersionLS;

    vers[0] = HIWORD(fileVersMS);
    vers[1] = LOWORD(fileVersMS);
    vers[2] = HIWORD(fileVersLS);
    vers[3] = LOWORD(fileVersLS);

    char buf[256];
    sprintf(buf, "%d.%d.%d.%d", vers[0], vers[1], vers[2], vers[3]);
    aVersion.Assign(NS_ConvertUTF8toUTF16(buf));
}

void 
gfxWindowsPlatform::GetCleartypeParams(nsTArray<ClearTypeParameterInfo>& aParams)
{
    HKEY  hKey, subKey;
    DWORD i, rv, size, type;
    WCHAR displayName[256], subkeyName[256];

    aParams.Clear();

    // construct subkeys based on HKLM subkeys, assume they are same for HKCU
    rv = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft\\Avalon.Graphics",
                       0, KEY_READ, &hKey);

    if (rv != ERROR_SUCCESS) {
        return;
    }

    // enumerate over subkeys
    for (i = 0, rv = ERROR_SUCCESS; rv != ERROR_NO_MORE_ITEMS; i++) {
        size = ArrayLength(displayName);
        rv = RegEnumKeyExW(hKey, i, displayName, &size,
                           nullptr, nullptr, nullptr, nullptr);
        if (rv != ERROR_SUCCESS) {
            continue;
        }

        ClearTypeParameterInfo ctinfo;
        ctinfo.displayName.Assign(displayName);

        DWORD subrv, value;
        bool foundData = false;

        swprintf_s(subkeyName, ArrayLength(subkeyName),
                   L"Software\\Microsoft\\Avalon.Graphics\\%s", displayName);

        // subkey for gamma, pixel structure
        subrv = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                              subkeyName, 0, KEY_QUERY_VALUE, &subKey);

        if (subrv == ERROR_SUCCESS) {
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"GammaLevel", nullptr, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.gamma = value;
            }

            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"PixelStructure", nullptr, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.pixelStructure = value;
            }

            RegCloseKey(subKey);
        }

        // subkey for cleartype level, enhanced contrast
        subrv = RegOpenKeyExW(HKEY_CURRENT_USER,
                              subkeyName, 0, KEY_QUERY_VALUE, &subKey);

        if (subrv == ERROR_SUCCESS) {
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"ClearTypeLevel", nullptr, &type,
                                     (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.clearTypeLevel = value;
            }
      
            size = sizeof(value);
            subrv = RegQueryValueExW(subKey, L"EnhancedContrastLevel",
                                     nullptr, &type, (LPBYTE)&value, &size);
            if (subrv == ERROR_SUCCESS && type == REG_DWORD) {
                foundData = true;
                ctinfo.enhancedContrast = value;
            }

            RegCloseKey(subKey);
        }

        if (foundData) {
            aParams.AppendElement(ctinfo);
        }
    }

    RegCloseKey(hKey);
}

void
gfxWindowsPlatform::FontsPrefsChanged(const char *aPref)
{
    bool clearTextFontCaches = true;

    gfxPlatform::FontsPrefsChanged(aPref);

    if (!aPref) {
        mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
        mUseClearTypeAlways = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_DOWNLOADABLE_FONTS_USE_CLEARTYPE, aPref)) {
        mUseClearTypeForDownloadableFonts = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_USE_CLEARTYPE_ALWAYS, aPref)) {
        mUseClearTypeAlways = UNINITIALIZED_VALUE;
    } else if (!strncmp(GFX_CLEARTYPE_PARAMS, aPref, strlen(GFX_CLEARTYPE_PARAMS))) {
        SetupClearTypeParams();
    } else {
        clearTextFontCaches = false;
    }

    if (clearTextFontCaches) {    
        gfxFontCache *fc = gfxFontCache::GetCache();
        if (fc) {
            fc->Flush();
        }
    }
}

#define ENHANCED_CONTRAST_REGISTRY_KEY \
    HKEY_CURRENT_USER, "Software\\Microsoft\\Avalon.Graphics\\DISPLAY1\\EnhancedContrastLevel"

void
gfxWindowsPlatform::SetupClearTypeParams()
{
#if CAIRO_HAS_DWRITE_FONT
    if (GetDWriteFactory()) {
        // any missing prefs will default to invalid (-1) and be ignored;
        // out-of-range values will also be ignored
        FLOAT gamma = -1.0;
        FLOAT contrast = -1.0;
        FLOAT level = -1.0;
        int geometry = -1;
        int mode = -1;
        int32_t value;
        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_GAMMA, &value))) {
            if (value >= 1000 && value <= 2200) {
                gamma = FLOAT(value / 1000.0);
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_CONTRAST, &value))) {
            if (value >= 0 && value <= 1000) {
                contrast = FLOAT(value / 100.0);
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_LEVEL, &value))) {
            if (value >= 0 && value <= 100) {
                level = FLOAT(value / 100.0);
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_STRUCTURE, &value))) {
            if (value >= 0 && value <= 2) {
                geometry = value;
            }
        }

        if (NS_SUCCEEDED(Preferences::GetInt(GFX_CLEARTYPE_PARAMS_MODE, &value))) {
            if (value >= 0 && value <= 5) {
                mode = value;
            }
        }

        cairo_dwrite_set_cleartype_params(gamma, contrast, level, geometry, mode);

        switch (mode) {
        case DWRITE_RENDERING_MODE_ALIASED:
        case DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC:
            mMeasuringMode = DWRITE_MEASURING_MODE_GDI_CLASSIC;
            break;
        case DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL:
            mMeasuringMode = DWRITE_MEASURING_MODE_GDI_NATURAL;
            break;
        default:
            mMeasuringMode = DWRITE_MEASURING_MODE_NATURAL;
            break;
        }

        nsRefPtr<IDWriteRenderingParams> defaultRenderingParams;
        GetDWriteFactory()->CreateRenderingParams(getter_AddRefs(defaultRenderingParams));
        // For EnhancedContrast, we override the default if the user has not set it
        // in the registry (by using the ClearType Tuner).
        if (contrast >= 0.0 && contrast <= 10.0) {
            contrast = contrast;
        } else {
            HKEY hKey;
            if (RegOpenKeyExA(ENHANCED_CONTRAST_REGISTRY_KEY,
                              0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                contrast = defaultRenderingParams->GetEnhancedContrast();
                RegCloseKey(hKey);
            } else {
                contrast = 1.0;
            }
        }

        // For parameters that have not been explicitly set,
        // we copy values from default params (or our overridden value for contrast)
        if (gamma < 1.0 || gamma > 2.2) {
            gamma = defaultRenderingParams->GetGamma();
        }

        if (level < 0.0 || level > 1.0) {
            level = defaultRenderingParams->GetClearTypeLevel();
        }

        DWRITE_PIXEL_GEOMETRY dwriteGeometry =
          static_cast<DWRITE_PIXEL_GEOMETRY>(geometry);
        DWRITE_RENDERING_MODE renderMode =
          static_cast<DWRITE_RENDERING_MODE>(mode);

        if (dwriteGeometry < DWRITE_PIXEL_GEOMETRY_FLAT ||
            dwriteGeometry > DWRITE_PIXEL_GEOMETRY_BGR) {
            dwriteGeometry = defaultRenderingParams->GetPixelGeometry();
        }

        if (renderMode < DWRITE_RENDERING_MODE_DEFAULT ||
            renderMode > DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC) {
            renderMode = defaultRenderingParams->GetRenderingMode();
        }

        mRenderingParams[TEXT_RENDERING_NO_CLEARTYPE] = defaultRenderingParams;

        GetDWriteFactory()->CreateCustomRenderingParams(gamma, contrast, level,
            dwriteGeometry, renderMode,
            getter_AddRefs(mRenderingParams[TEXT_RENDERING_NORMAL]));

        GetDWriteFactory()->CreateCustomRenderingParams(gamma, contrast, level,
            dwriteGeometry, DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC,
            getter_AddRefs(mRenderingParams[TEXT_RENDERING_GDI_CLASSIC]));
    }
#endif
}

void
gfxWindowsPlatform::OnDeviceManagerDestroy(DeviceManagerD3D9* aDeviceManager)
{
  if (aDeviceManager == mDeviceManager) {
    mDeviceManager = nullptr;
  }
}

IDirect3DDevice9*
gfxWindowsPlatform::GetD3D9Device()
{
  DeviceManagerD3D9* manager = GetD3D9DeviceManager();
  return manager ? manager->device() : nullptr;
}

DeviceManagerD3D9*
gfxWindowsPlatform::GetD3D9DeviceManager()
{
  // We should only create the d3d9 device on the compositor thread
  // or we don't have a compositor thread.
  if (!mDeviceManager &&
      (!gfxPlatform::UsesOffMainThreadCompositing() ||
       CompositorParent::IsInCompositorThread())) {
    mDeviceManager = new DeviceManagerD3D9();
    if (!mDeviceManager->Init()) {
      gfxCriticalError() << "[D3D9] Could not Initialize the DeviceManagerD3D9";
      mDeviceManager = nullptr;
    }
  }

  return mDeviceManager;
}

ID3D11Device*
gfxWindowsPlatform::GetD3D11Device()
{
  if (mD3D11DeviceInitialized) {
    return mD3D11Device;
  }

  InitD3D11Devices();

  return mD3D11Device;
}

ID3D11Device*
gfxWindowsPlatform::GetD3D11ContentDevice()
{
  if (mD3D11DeviceInitialized) {
    return mD3D11ContentDevice;
  }

  InitD3D11Devices();

  return mD3D11ContentDevice;
}

ID3D11Device*
gfxWindowsPlatform::GetD3D11ImageBridgeDevice()
{
  if (mD3D11DeviceInitialized) {
    return mD3D11ImageBridgeDevice;
  }

  InitD3D11Devices();

  return mD3D11ImageBridgeDevice;
}


ReadbackManagerD3D11*
gfxWindowsPlatform::GetReadbackManager()
{
  if (!mD3D11ReadbackManager) {
    mD3D11ReadbackManager = new ReadbackManagerD3D11();
  }

  return mD3D11ReadbackManager;
}

bool
gfxWindowsPlatform::IsOptimus()
{
    static int knowIsOptimus = -1;
    if (knowIsOptimus == -1) {
        // other potential optimus -- nvd3d9wrapx.dll & nvdxgiwrap.dll
        if (GetModuleHandleA("nvumdshim.dll") ||
            GetModuleHandleA("nvumdshimx.dll"))
        {
            knowIsOptimus = 1;
        } else {
            knowIsOptimus = 0;
        }
    }
    return knowIsOptimus;
}

int
gfxWindowsPlatform::GetScreenDepth() const
{
    // if the system doesn't have all displays with the same
    // pixel format, just return 24 and move on with life.
    if (!GetSystemMetrics(SM_SAMEDISPLAYFORMAT))
        return 24;

    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return 24;

    int depth = GetDeviceCaps(hdc, BITSPIXEL) *
                GetDeviceCaps(hdc, PLANES);

    ReleaseDC(nullptr, hdc);

    return depth;
}

IDXGIAdapter1*
gfxWindowsPlatform::GetDXGIAdapter()
{
  if (mAdapter) {
    return mAdapter;
  }

  nsModuleHandle dxgiModule(LoadLibrarySystem32(L"dxgi.dll"));
  decltype(CreateDXGIFactory1)* createDXGIFactory1 = (decltype(CreateDXGIFactory1)*)
    GetProcAddress(dxgiModule, "CreateDXGIFactory1");

  // Try to use a DXGI 1.1 adapter in order to share resources
  // across processes.
  if (createDXGIFactory1) {
    nsRefPtr<IDXGIFactory1> factory1;
    HRESULT hr = createDXGIFactory1(__uuidof(IDXGIFactory1),
                                    getter_AddRefs(factory1));

    if (FAILED(hr) || !factory1) {
      // This seems to happen with some people running the iZ3D driver.
      // They won't get acceleration.
      return nullptr;
    }

    hr = factory1->EnumAdapters1(0, byRef(mAdapter));
    if (FAILED(hr)) {
      // We should return and not accelerate if we can't obtain
      // an adapter.
      return nullptr;
    }
  }

  // We leak this module everywhere, we might as well do so here as well.
  dxgiModule.disown();

  return mAdapter;
}

bool DoesD3D11DeviceWork()
{
  static bool checked = false;
  static bool result = false;

  if (checked)
      return result;
  checked = true;

  if (gfxPrefs::Direct2DForceEnabled() ||
      gfxPrefs::LayersAccelerationForceEnabled())
  {
    result = true;
    return true;
  }

  if (GetModuleHandleW(L"igd10umd32.dll")) {
    const wchar_t* checkModules[] = {L"dlumd32.dll",
                                     L"dlumd11.dll",
                                     L"dlumd10.dll"};
    for (int i=0; i<PR_ARRAY_SIZE(checkModules); i+=1) {
      if (GetModuleHandleW(checkModules[i])) {
        nsString displayLinkModuleVersionString;
        gfxWindowsPlatform::GetDLLVersion(checkModules[i],
                                          displayLinkModuleVersionString);
        uint64_t displayLinkModuleVersion;
        if (!ParseDriverVersion(displayLinkModuleVersionString,
                                &displayLinkModuleVersion)) {
          gfxCriticalError() << "DisplayLink: could not parse version "
                             << checkModules[i];
          gANGLESupportsD3D11 = false;
          return false;
        }
        if (displayLinkModuleVersion <= V(8,6,1,36484)) {
          gfxCriticalError(CriticalLog::DefaultOptions(false)) << "DisplayLink: too old version " << displayLinkModuleVersionString.get();
          gANGLESupportsD3D11 = false;
          return false;
        }
      }
    }
  }
  result = true;
  return true;
}

// See bug 1083071. On some drivers, Direct3D 11 CreateShaderResourceView fails
// with E_OUTOFMEMORY.
bool DoesD3D11TextureSharingWorkInternal(ID3D11Device *device, DXGI_FORMAT format, UINT bindflags)
{
  // CreateTexture2D is known to crash on lower feature levels, see bugs
  // 1170211 and 1089413.
  if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
    return false;
  }

  if (gfxPrefs::Direct2DForceEnabled() ||
      gfxPrefs::LayersAccelerationForceEnabled())
  {
    return true;
  }

  if (GetModuleHandleW(L"atidxx32.dll")) {
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo) {
      nsString vendorID, vendorID2;
      gfxInfo->GetAdapterVendorID(vendorID);
      gfxInfo->GetAdapterVendorID2(vendorID2);
      if (vendorID.EqualsLiteral("0x8086") && vendorID2.IsEmpty()) {
        gfxCriticalError(CriticalLog::DefaultOptions(false)) << "Unexpected Intel/AMD dual-GPU setup";
        return false;
      }
    }
  }

  RefPtr<ID3D11Texture2D> texture;
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = 32;
  desc.Height = 32;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  desc.BindFlags = bindflags;
  if (FAILED(device->CreateTexture2D(&desc, NULL, byRef(texture)))) {
    return false;
  }

  HANDLE shareHandle;
  nsRefPtr<IDXGIResource> otherResource;
  if (FAILED(texture->QueryInterface(__uuidof(IDXGIResource),
                                     getter_AddRefs(otherResource))))
  {
    return false;
  }

  if (FAILED(otherResource->GetSharedHandle(&shareHandle))) {
    return false;
  }

  nsRefPtr<ID3D11Resource> sharedResource;
  nsRefPtr<ID3D11Texture2D> sharedTexture;
  if (FAILED(device->OpenSharedResource(shareHandle, __uuidof(ID3D11Resource),
                                        getter_AddRefs(sharedResource))))
  {
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "OpenSharedResource failed for format " << format;
    return false;
  }

  if (FAILED(sharedResource->QueryInterface(__uuidof(ID3D11Texture2D),
                                            getter_AddRefs(sharedTexture))))
  {
    return false;
  }

  RefPtr<ID3D11ShaderResourceView> sharedView;

  // This if(FAILED()) is the one that actually fails on systems affected by bug 1083071.
  if (FAILED(device->CreateShaderResourceView(sharedTexture, NULL, byRef(sharedView)))) {
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "CreateShaderResourceView failed for format" << format;
    return false;
  }

  return true;
}

bool DoesD3D11TextureSharingWork(ID3D11Device *device)
{
  static bool checked;
  static bool result;

  if (checked)
    return result;
  checked = true;

  result = DoesD3D11TextureSharingWorkInternal(device, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
  return result;
}

bool DoesD3D11AlphaTextureSharingWork(ID3D11Device *device)
{
  return DoesD3D11TextureSharingWorkInternal(device, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE);
}

void
gfxWindowsPlatform::InitD3D11Devices()
{
  // This function attempts to initialize our D3D11 devices. If the hardware
  // is not blacklisted for D3D11 layers. This will first attempt to create a
  // hardware accelerated device. If this creation fails or the hardware is
  // blacklisted, then this function will abort if WARP is disabled, causing us
  // to fallback to D3D9 or Basic layers. If WARP is not disabled it will use
  // a WARP device which should always be available on Windows 7 and higher.

  mD3D11DeviceInitialized = true;
  mDoesD3D11TextureSharingWork = false;

  MOZ_ASSERT(!mD3D11Device); 

  DriverInitCrashDetection detectCrashes;
  if (InSafeMode() || detectCrashes.DisableAcceleration()) {
    return;
  }

  bool useWARP = false;
  bool allowWARP = false;

  if (IsWin8OrLater()) {
    allowWARP = true;
  }

  nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
  if (gfxInfo) {
    int32_t status;
    if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS, &status))) {
      if (status != nsIGfxInfo::FEATURE_STATUS_OK) {

        // It seems like nvdxgiwrap makes a mess of WARP. See bug 1154703 for more.
        if (gfxPrefs::LayersD3D11DisableWARP() || GetModuleHandleA("nvdxgiwrap.dll")) {
          return;
        }

        if (!IsWin8OrLater()) {
          /* We don't trust Windows 7 enough yet to use WARP */
          return;
        }

        useWARP = allowWARP;
      }
    }
  }

  if (gfxPrefs::LayersD3D11ForceWARP()) {
    useWARP = true;
  }

  nsModuleHandle d3d11Module(LoadLibrarySystem32(L"d3d11.dll"));
  decltype(D3D11CreateDevice)* d3d11CreateDevice = (decltype(D3D11CreateDevice)*)
    GetProcAddress(d3d11Module, "D3D11CreateDevice");

  if (!d3d11CreateDevice) {
    // We should just be on Windows Vista or XP in this case.
    return;
  }

  nsTArray<D3D_FEATURE_LEVEL> featureLevels;
  if (IsWin8OrLater()) {
    featureLevels.AppendElement(D3D_FEATURE_LEVEL_11_1);
  }
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_11_0);
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_10_1);
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_10_0);
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_9_3);

  RefPtr<IDXGIAdapter1> adapter;

  if (!useWARP) {
    adapter = GetDXGIAdapter();

    if (!adapter) {
      if (!gfxPrefs::LayersD3D11DisableWARP()) {
        return;
      }
      useWARP = allowWARP;
    }
  }

  HRESULT hr = E_INVALIDARG;

  if (!useWARP) {
    MOZ_SEH_TRY {
      hr = d3d11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                             // Use
                             // D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
                             // to prevent bug 1092260. IE 11 also uses this flag.
                             D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                             featureLevels.Elements(), featureLevels.Length(),
                             D3D11_SDK_VERSION, byRef(mD3D11Device), nullptr, nullptr);
    } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
      gfxCriticalError() << "Crash during D3D11 device creation";

      if (gfxPrefs::LayersD3D11DisableWARP()) {
        return;
      }

      useWARP = allowWARP;
      adapter = nullptr;
    }

    if (FAILED(hr) || !DoesD3D11DeviceWork()) {
      gfxCriticalError() << "D3D11 device creation failed" << hexa(hr);
      mD3D11Device = nullptr;
      if (gfxPrefs::LayersD3D11DisableWARP()) {
        return;
      }

      useWARP = allowWARP;
      adapter = nullptr;
    }

    if (mD3D11Device) {
      // Only test this when not using WARP since it can fail and cause GetDeviceRemovedReason to return
      // weird values.
      mDoesD3D11TextureSharingWork = ::DoesD3D11TextureSharingWork(mD3D11Device);
    }
  }

  if (useWARP) {
    MOZ_ASSERT(!gfxPrefs::LayersD3D11DisableWARP());
    MOZ_ASSERT(!mD3D11Device);
    MOZ_ASSERT(!adapter);

    ScopedGfxFeatureReporter reporterWARP("D3D11-WARP", gfxPrefs::LayersD3D11ForceWARP());

    MOZ_SEH_TRY {
      hr = d3d11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                             // Use
                             // D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
                             // to prevent bug 1092260. IE 11 also uses this flag.
                             D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                             featureLevels.Elements(), featureLevels.Length(),
                             D3D11_SDK_VERSION, byRef(mD3D11Device), nullptr, nullptr);

      if (FAILED(hr)) {
        // This should always succeed... in theory.
        gfxCriticalError() << "Failed to initialize WARP D3D11 device! " << hexa(hr);
        return;
      }

      mIsWARP = true;
      reporterWARP.SetSuccessful();
    } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
      gfxCriticalError() << "Exception occurred initializing WARP D3D11 device!";
      return;
    }
  }

  if (!mD3D11Device)
    return;

  mD3D11Device->SetExceptionMode(0);

  // We create our device for D2D content drawing here. Normally we don't use
  // D2D content drawing when using WARP. However when WARP is forced by
  // default we will let Direct2D use WARP as well.
  if (Factory::SupportsD2D1() && (!useWARP || gfxPrefs::LayersD3D11ForceWARP())) {
    MOZ_ASSERT((useWARP && !adapter) || !useWARP);

    hr = E_INVALIDARG;
    MOZ_SEH_TRY {
      hr = d3d11CreateDevice(adapter, useWARP ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                             D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                             featureLevels.Elements(), featureLevels.Length(),
                             D3D11_SDK_VERSION, byRef(mD3D11ContentDevice), nullptr, nullptr);
    } MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
      mD3D11ContentDevice = nullptr;
    }

    if (FAILED(hr)) {
      d3d11Module.disown();
      return;
    }

    mD3D11ContentDevice->SetExceptionMode(0);

    nsRefPtr<ID3D10Multithread> multi;
    mD3D11ContentDevice->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));
    multi->SetMultithreadProtected(TRUE);

    Factory::SetDirect3D11Device(mD3D11ContentDevice);
  }

  if (!useWARP) {
    hr = E_INVALIDARG;

    MOZ_SEH_TRY{
      hr = d3d11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                             D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                             featureLevels.Elements(), featureLevels.Length(),
                             D3D11_SDK_VERSION, byRef(mD3D11ImageBridgeDevice), nullptr, nullptr);
    } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
      mD3D11ImageBridgeDevice = nullptr;
    }

    if (FAILED(hr)) {
      d3d11Module.disown();
      return;
    }

    mD3D11ImageBridgeDevice->SetExceptionMode(0);

    if (!DoesD3D11AlphaTextureSharingWork(mD3D11ImageBridgeDevice)) {
      mD3D11ImageBridgeDevice = nullptr;
    }
  }

  // We leak these everywhere and we need them our entire runtime anyway, let's
  // leak it here as well.
  d3d11Module.disown();
}

TemporaryRef<ID3D11Device>
gfxWindowsPlatform::CreateD3D11DecoderDevice()
{
  nsModuleHandle d3d11Module(LoadLibrarySystem32(L"d3d11.dll"));
  decltype(D3D11CreateDevice)* d3d11CreateDevice = (decltype(D3D11CreateDevice)*)
    GetProcAddress(d3d11Module, "D3D11CreateDevice");

   if (!d3d11CreateDevice) {
    // We should just be on Windows Vista or XP in this case.
    return nullptr;
  }

  nsTArray<D3D_FEATURE_LEVEL> featureLevels;
  if (IsWin8OrLater()) {
    featureLevels.AppendElement(D3D_FEATURE_LEVEL_11_1);
  }
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_11_0);
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_10_1);
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_10_0);
  featureLevels.AppendElement(D3D_FEATURE_LEVEL_9_3);

  RefPtr<IDXGIAdapter1> adapter = GetDXGIAdapter();

  if (!adapter) {
    return nullptr;
  }

  HRESULT hr = E_INVALIDARG;

  RefPtr<ID3D11Device> device;

  MOZ_SEH_TRY{
    hr = d3d11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                           D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                           featureLevels.Elements(), featureLevels.Length(),
                           D3D11_SDK_VERSION, byRef(device), nullptr, nullptr);
  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }

  if (FAILED(hr) || !DoesD3D11DeviceWork()) {
    return nullptr;
  }

  nsRefPtr<ID3D10Multithread> multi;
  device->QueryInterface(__uuidof(ID3D10Multithread), getter_AddRefs(multi));

  multi->SetMultithreadProtected(TRUE);

  return device.forget();
}

static bool
DwmCompositionEnabled()
{
  MOZ_ASSERT(WinUtils::dwmIsCompositionEnabledPtr);
  BOOL dwmEnabled = false;
  WinUtils::dwmIsCompositionEnabledPtr(&dwmEnabled);
  return dwmEnabled;
}

class D3DVsyncSource final : public VsyncSource
{
public:

  class D3DVsyncDisplay final : public VsyncSource::Display
  {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(D3DVsyncDisplay)
    public:
      D3DVsyncDisplay()
        : mVsyncEnabledLock("D3DVsyncEnabledLock")
        , mPrevVsync(TimeStamp::Now())
        , mVsyncEnabled(false)
      {
        mVsyncThread = new base::Thread("WindowsVsyncThread");
        const double rate = 1000 / 60.0;
        mSoftwareVsyncRate = TimeDuration::FromMilliseconds(rate);
        MOZ_RELEASE_ASSERT(mVsyncThread->Start(), "Could not start Windows vsync thread");
      }

      virtual void EnableVsync() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mVsyncThread->IsRunning());
        { // scope lock
          MonitorAutoLock lock(mVsyncEnabledLock);
          if (mVsyncEnabled) {
            return;
          }
          mVsyncEnabled = true;
        }

        CancelableTask* vsyncStart = NewRunnableMethod(this,
            &D3DVsyncDisplay::VBlankLoop);
        mVsyncThread->message_loop()->PostTask(FROM_HERE, vsyncStart);
      }

      virtual void DisableVsync() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mVsyncThread->IsRunning());
        MonitorAutoLock lock(mVsyncEnabledLock);
        if (!mVsyncEnabled) {
          return;
        }
        mVsyncEnabled = false;
      }

      virtual bool IsVsyncEnabled() override
      {
        MOZ_ASSERT(NS_IsMainThread());
        MonitorAutoLock lock(mVsyncEnabledLock);
        return mVsyncEnabled;
      }

      void ScheduleSoftwareVsync(TimeStamp aVsyncTimestamp)
      {
        MOZ_ASSERT(IsInVsyncThread());
        NS_WARNING("DwmComposition dynamically disabled, falling back to software timers");

        TimeStamp nextVsync = aVsyncTimestamp + mSoftwareVsyncRate;
        TimeDuration delay = nextVsync - TimeStamp::Now();
        if (delay.ToMilliseconds() < 0) {
          delay = mozilla::TimeDuration::FromMilliseconds(0);
        }

        mVsyncThread->message_loop()->PostDelayedTask(FROM_HERE,
            NewRunnableMethod(this, &D3DVsyncDisplay::VBlankLoop),
            delay.ToMilliseconds());
      }

      TimeStamp GetAdjustedVsyncTimeStamp(LARGE_INTEGER& aFrequency,
                                          QPC_TIME& aQpcVblankTime)
      {
        TimeStamp vsync = TimeStamp::Now();
        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);

        const int microseconds = 1000000;
        int64_t adjust = qpcNow.QuadPart - aQpcVblankTime;
        int64_t usAdjust = (adjust * microseconds) / aFrequency.QuadPart;
        vsync -= TimeDuration::FromMicroseconds((double) usAdjust);

        if (IsWin10OrLater()) {
          // On Windows 10 and on, DWMGetCompositionTimingInfo, mostly
          // reports the upcoming vsync time, which is in the future.
          // It can also sometimes report a vblank time in the past.
          // Since large parts of Gecko assume TimeStamps can't be in future,
          // use the previous vsync.

          // Windows 10 and Intel HD vsync timestamps are messy and
          // all over the place once in a while. Most of the time,
          // it reports the upcoming vsync. Sometimes, that upcoming
          // vsync is in the past. Sometimes that upcoming vsync is before
          // the previously seen vsync. Sometimes, the previous vsync
          // is still in the future. In these error cases,
          // we try to normalize to Now().
          TimeStamp upcomingVsync = vsync;
          if (upcomingVsync < mPrevVsync) {
            // Windows can report a vsync that's before
            // the previous one. So update it to sometime in the future.
            upcomingVsync = TimeStamp::Now() + TimeDuration::FromMilliseconds(1);
          }

          vsync = mPrevVsync;
          mPrevVsync = upcomingVsync;
        }
        // On Windows 7 and 8, DwmFlush wakes up AFTER qpcVBlankTime
        // from DWMGetCompositionTimingInfo. We can return the adjusted vsync.

        // Once in a while, the reported vsync timestamp can be in the future.
        // Normalize the reported timestamp to now.
        if (vsync >= TimeStamp::Now()) {
          vsync = TimeStamp::Now();
        }
        return vsync;
      }

      void VBlankLoop()
      {
        MOZ_ASSERT(IsInVsyncThread());
        MOZ_ASSERT(sizeof(int64_t) == sizeof(QPC_TIME));

        DWM_TIMING_INFO vblankTime;
        // Make sure to init the cbSize, otherwise GetCompositionTiming will fail
        vblankTime.cbSize = sizeof(DWM_TIMING_INFO);

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        TimeStamp vsync = TimeStamp::Now();

        for (;;) {
          { // scope lock
            MonitorAutoLock lock(mVsyncEnabledLock);
            if (!mVsyncEnabled) return;
          }

          // Large parts of gecko assume that the refresh driver timestamp
          // must be <= Now() and cannot be in the future.
          MOZ_ASSERT(vsync <= TimeStamp::Now());
          Display::NotifyVsync(vsync);

          // DwmComposition can be dynamically enabled/disabled
          // so we have to check every time that it's available.
          // When it is unavailable, we fallback to software but will try
          // to get back to dwm rendering once it's re-enabled
          if (!DwmCompositionEnabled()) {
            ScheduleSoftwareVsync(vsync);
            return;
          }

          // Use a combination of DwmFlush + DwmGetCompositionTimingInfoPtr
          // Using WaitForVBlank, the whole system dies :/
          WinUtils::dwmFlushProcPtr();
          HRESULT hr = WinUtils::dwmGetCompositionTimingInfoPtr(0, &vblankTime);
          vsync = TimeStamp::Now();
          if (SUCCEEDED(hr)) {
            vsync = GetAdjustedVsyncTimeStamp(frequency, vblankTime.qpcVBlank);
          }
        } // end for
      }

    private:
      virtual ~D3DVsyncDisplay()
      {
        MOZ_ASSERT(NS_IsMainThread());
        DisableVsync();
        mVsyncThread->Stop();
        delete mVsyncThread;
      }

      bool IsInVsyncThread()
      {
        return mVsyncThread->thread_id() == PlatformThread::CurrentId();
      }

      TimeDuration mSoftwareVsyncRate;
      TimeStamp mPrevVsync; // Only used on Windows 10
      Monitor mVsyncEnabledLock;
      base::Thread* mVsyncThread;
      bool mVsyncEnabled;
  }; // end d3dvsyncdisplay

  D3DVsyncSource()
  {
    mPrimaryDisplay = new D3DVsyncDisplay();
  }

  virtual Display& GetGlobalDisplay() override
  {
    return *mPrimaryDisplay;
  }

private:
  virtual ~D3DVsyncSource()
  {
  }
  nsRefPtr<D3DVsyncDisplay> mPrimaryDisplay;
}; // end D3DVsyncSource

already_AddRefed<mozilla::gfx::VsyncSource>
gfxWindowsPlatform::CreateHardwareVsyncSource()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!WinUtils::dwmIsCompositionEnabledPtr) {
    NS_WARNING("Dwm composition not available, falling back to software vsync");
    return gfxPlatform::CreateHardwareVsyncSource();
  }

  BOOL dwmEnabled = false;
  WinUtils::dwmIsCompositionEnabledPtr(&dwmEnabled);
  if (!dwmEnabled) {
    NS_WARNING("DWM not enabled, falling back to software vsync");
    return gfxPlatform::CreateHardwareVsyncSource();
  }

  nsRefPtr<VsyncSource> d3dVsyncSource = new D3DVsyncSource();
  return d3dVsyncSource.forget();
}

bool
gfxWindowsPlatform::SupportsApzTouchInput() const
{
  int value = Preferences::GetInt("dom.w3c_touch_events.enabled", 0);
  return value == 1 || value == 2;
}
