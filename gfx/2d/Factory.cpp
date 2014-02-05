/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"

#ifdef USE_CAIRO
#include "DrawTargetCairo.h"
#include "ScaledFontCairo.h"
#endif

#ifdef USE_SKIA
#include "DrawTargetSkia.h"
#include "ScaledFontBase.h"
#ifdef MOZ_ENABLE_FREETYPE
#define USE_SKIA_FREETYPE
#include "ScaledFontCairo.h"
#endif
#endif

#if defined(WIN32) && defined(USE_SKIA)
#include "ScaledFontWin.h"
#endif

#ifdef XP_MACOSX
#include "ScaledFontMac.h"
#endif


#ifdef XP_MACOSX
#include "DrawTargetCG.h"
#endif

#ifdef WIN32
#include "DrawTargetD2D.h"
#ifdef USE_D2D1_1
#include "DrawTargetD2D1.h"
#endif
#include "ScaledFontDWrite.h"
#include <d3d10_1.h>
#include "HelpersD2D.h"
#endif

#include "DrawTargetDual.h"
#include "DrawTargetRecording.h"

#include "SourceSurfaceRawData.h"

#include "DrawEventRecorder.h"

#include "Logging.h"

#include "mozilla/CheckedInt.h"

#ifdef PR_LOGGING
PRLogModuleInfo *
GetGFX2DLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("gfx2d");
  return sLog;
}
#endif

// The following code was largely taken from xpcom/glue/SSE.cpp and
// made a little simpler.
enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

#ifdef HAVE_CPUID_H

#if !(defined(__SSE2__) || defined(_M_X64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
// cpuid.h is available on gcc 4.3 and higher on i386 and x86_64
#include <cpuid.h>

static inline bool
HasCPUIDBit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  unsigned int regs[4];
  return __get_cpuid(level, &regs[0], &regs[1], &regs[2], &regs[3]) &&
         (regs[reg] & bit);
}
#endif

#define HAVE_CPU_DETECTION
#else

#if defined(_MSC_VER) && _MSC_VER >= 1600 && (defined(_M_IX86) || defined(_M_AMD64))
// MSVC 2005 or later supports __cpuid by intrin.h
// But it does't work on MSVC 2005 with SDK 7.1 (Bug 753772)
#include <intrin.h>

#define HAVE_CPU_DETECTION
#elif defined(__SUNPRO_CC) && (defined(__i386) || defined(__x86_64__))

// Define a function identical to MSVC function.
#ifdef __i386
static void
__cpuid(int CPUInfo[4], int InfoType)
{
  asm (
    "xchg %esi, %ebx\n"
    "cpuid\n"
    "movl %eax, (%edi)\n"
    "movl %ebx, 4(%edi)\n"
    "movl %ecx, 8(%edi)\n"
    "movl %edx, 12(%edi)\n"
    "xchg %esi, %ebx\n"
    :
    : "a"(InfoType), // %eax
      "D"(CPUInfo) // %edi
    : "%ecx", "%edx", "%esi"
  );
}
#else
static void
__cpuid(int CPUInfo[4], int InfoType)
{
  asm (
    "xchg %rsi, %rbx\n"
    "cpuid\n"
    "movl %eax, (%rdi)\n"
    "movl %ebx, 4(%rdi)\n"
    "movl %ecx, 8(%rdi)\n"
    "movl %edx, 12(%rdi)\n"
    "xchg %rsi, %rbx\n"
    :
    : "a"(InfoType), // %eax
      "D"(CPUInfo) // %rdi
    : "%ecx", "%edx", "%rsi"
  );
}

#define HAVE_CPU_DETECTION
#endif
#endif

#ifdef HAVE_CPU_DETECTION
static inline bool
HasCPUIDBit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  // Check that the level in question is supported.
  volatile int regs[4];
  __cpuid((int *)regs, level & 0x80000000u);
  if (unsigned(regs[0]) < level)
    return false;
  __cpuid((int *)regs, level);
  return !!(unsigned(regs[reg]) & bit);
}
#endif
#endif

namespace mozilla {
namespace gfx {

// XXX - Need to define an API to set this.
int sGfxLogLevel = LOG_DEBUG;

#ifdef WIN32
ID3D10Device1 *Factory::mD3D10Device;
#ifdef USE_D2D1_1
ID3D11Device *Factory::mD3D11Device;
ID2D1Device *Factory::mD2D1Device;
#endif
#endif

DrawEventRecorder *Factory::mRecorder;

bool
Factory::HasSSE2()
{
#if defined(__SSE2__) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  // gcc with -msse2 (default on OSX and x86-64)
  // cl.exe with -arch:SSE2 (default on x64 compiler)
  return true;
#elif defined(HAVE_CPU_DETECTION)
  return HasCPUIDBit(1u, edx, (1u<<26));
#else
  return false;
#endif
}

bool
Factory::CheckSurfaceSize(const IntSize &sz, int32_t limit)
{
  if (sz.width < 0 || sz.height < 0) {
    gfxDebug() << "Surface width or height < 0!";
    return false;
  }

  // reject images with sides bigger than limit
  if (limit && (sz.width > limit || sz.height > limit)) {
    gfxDebug() << "Surface size too large (exceeds caller's limit)!";
    return false;
  }

  // make sure the surface area doesn't overflow a int32_t
  CheckedInt<int32_t> tmp = sz.width;
  tmp *= sz.height;
  if (!tmp.isValid()) {
    gfxDebug() << "Surface size too large (would overflow)!";
    return false;
  }

  // assuming 4 bytes per pixel, make sure the allocation size
  // doesn't overflow a int32_t either
  CheckedInt<int32_t> stride = sz.width;
  stride *= 4;

  // When aligning the stride to 16 bytes, it can grow by up to 15 bytes.
  stride += 16 - 1;

  if (!stride.isValid()) {
    gfxDebug() << "Surface size too large (stride overflows int32_t)!";
    return false;
  }

  CheckedInt<int32_t> numBytes = GetAlignedStride<16>(sz.width * 4);
  numBytes *= sz.height;
  if (!numBytes.isValid()) {
    gfxDebug() << "Surface size too large (allocation size would overflow int32_t)!";
    return false;
  }

  return true;
}

TemporaryRef<DrawTarget>
Factory::CreateDrawTarget(BackendType aBackend, const IntSize &aSize, SurfaceFormat aFormat)
{
  if (!CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<DrawTarget> retVal;
  switch (aBackend) {
#ifdef WIN32
  case BackendType::DIRECT2D:
    {
      RefPtr<DrawTargetD2D> newTarget;
      newTarget = new DrawTargetD2D();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#ifdef USE_D2D1_1
  case BackendType::DIRECT2D1_1:
    {
      RefPtr<DrawTargetD2D1> newTarget;
      newTarget = new DrawTargetD2D1();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
#elif defined XP_MACOSX
  case BackendType::COREGRAPHICS:
  case BackendType::COREGRAPHICS_ACCELERATED:
    {
      RefPtr<DrawTargetCG> newTarget;
      newTarget = new DrawTargetCG();
      if (newTarget->Init(aBackend, aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
#ifdef USE_SKIA
  case BackendType::SKIA:
    {
      RefPtr<DrawTargetSkia> newTarget;
      newTarget = new DrawTargetSkia();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
#ifdef USE_CAIRO
  case BackendType::CAIRO:
    {
      RefPtr<DrawTargetCairo> newTarget;
      newTarget = new DrawTargetCairo();
      if (newTarget->Init(aSize, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
  default:
    gfxDebug() << "Invalid draw target type specified.";
    return nullptr;
  }

  if (mRecorder && retVal) {
    RefPtr<DrawTarget> recordDT;
    recordDT = new DrawTargetRecording(mRecorder, retVal);
    return recordDT;
  }

  if (!retVal) {
    // Failed
    gfxDebug() << "Failed to create DrawTarget, Type: " << int(aBackend) << " Size: " << aSize;
  }
  
  return retVal;
}

TemporaryRef<DrawTarget>
Factory::CreateRecordingDrawTarget(DrawEventRecorder *aRecorder, DrawTarget *aDT)
{
  return new DrawTargetRecording(aRecorder, aDT);
}

TemporaryRef<DrawTarget>
Factory::CreateDrawTargetForData(BackendType aBackend, 
                                 unsigned char *aData, 
                                 const IntSize &aSize, 
                                 int32_t aStride, 
                                 SurfaceFormat aFormat)
{
  if (!CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<DrawTarget> retVal;

  switch (aBackend) {
#ifdef USE_SKIA
  case BackendType::SKIA:
    {
      RefPtr<DrawTargetSkia> newTarget;
      newTarget = new DrawTargetSkia();
      newTarget->Init(aData, aSize, aStride, aFormat);
      retVal = newTarget;
    }
#endif
#ifdef XP_MACOSX
  case BackendType::COREGRAPHICS:
    {
      RefPtr<DrawTargetCG> newTarget = new DrawTargetCG();
      if (newTarget->Init(aBackend, aData, aSize, aStride, aFormat))
        return newTarget;
      break;
    }
#endif
#ifdef USE_CAIRO
  case BackendType::CAIRO:
    {
      RefPtr<DrawTargetCairo> newTarget;
      newTarget = new DrawTargetCairo();
      if (newTarget->Init(aData, aSize, aStride, aFormat)) {
        retVal = newTarget;
      }
      break;
    }
#endif
  default:
    gfxDebug() << "Invalid draw target type specified.";
    return nullptr;
  }

  if (mRecorder && retVal) {
    RefPtr<DrawTarget> recordDT = new DrawTargetRecording(mRecorder, retVal, true);
    return recordDT;
  }

  if (!retVal) {
    gfxDebug() << "Failed to create DrawTarget, Type: " << int(aBackend) << " Size: " << aSize;
  }

  return retVal;
}

TemporaryRef<ScaledFont>
Factory::CreateScaledFontForNativeFont(const NativeFont &aNativeFont, Float aSize)
{
  switch (aNativeFont.mType) {
#ifdef WIN32
  case NativeFontType::DWRITE_FONT_FACE:
    {
      return new ScaledFontDWrite(static_cast<IDWriteFontFace*>(aNativeFont.mFont), aSize);
    }
#if defined(USE_CAIRO) || defined(USE_SKIA)
  case NativeFontType::GDI_FONT_FACE:
    {
      return new ScaledFontWin(static_cast<LOGFONT*>(aNativeFont.mFont), aSize);
    }
#endif
#endif
#ifdef XP_MACOSX
  case NativeFontType::MAC_FONT_FACE:
    {
      return new ScaledFontMac(static_cast<CGFontRef>(aNativeFont.mFont), aSize);
    }
#endif
#if defined(USE_CAIRO) || defined(USE_SKIA_FREETYPE)
  case NativeFontType::CAIRO_FONT_FACE:
    {
      return new ScaledFontCairo(static_cast<cairo_scaled_font_t*>(aNativeFont.mFont), aSize);
    }
#endif
  default:
    gfxWarning() << "Invalid native font type specified.";
    return nullptr;
  }
}

TemporaryRef<ScaledFont>
Factory::CreateScaledFontForTrueTypeData(uint8_t *aData, uint32_t aSize,
                                         uint32_t aFaceIndex, Float aGlyphSize,
                                         FontType aType)
{
  switch (aType) {
#ifdef WIN32
  case FontType::DWRITE:
    {
      return new ScaledFontDWrite(aData, aSize, aFaceIndex, aGlyphSize);
    }
#endif
  default:
    gfxWarning() << "Unable to create requested font type from truetype data";
    return nullptr;
  }
}

TemporaryRef<ScaledFont>
Factory::CreateScaledFontWithCairo(const NativeFont& aNativeFont, Float aSize, cairo_scaled_font_t* aScaledFont)
{
#ifdef USE_CAIRO
  // In theory, we could pull the NativeFont out of the cairo_scaled_font_t*,
  // but that would require a lot of code that would be otherwise repeated in
  // various backends.
  // Therefore, we just reuse CreateScaledFontForNativeFont's implementation.
  RefPtr<ScaledFont> font = CreateScaledFontForNativeFont(aNativeFont, aSize);
  static_cast<ScaledFontBase*>(font.get())->SetCairoScaledFont(aScaledFont);
  return font;
#else
  return nullptr;
#endif
}

TemporaryRef<DrawTarget>
Factory::CreateDualDrawTarget(DrawTarget *targetA, DrawTarget *targetB)
{
  RefPtr<DrawTarget> newTarget =
    new DrawTargetDual(targetA, targetB);

  RefPtr<DrawTarget> retVal = newTarget;

  if (mRecorder) {
    retVal = new DrawTargetRecording(mRecorder, retVal);
  }

  return retVal;
}


#ifdef WIN32
TemporaryRef<DrawTarget>
Factory::CreateDrawTargetForD3D10Texture(ID3D10Texture2D *aTexture, SurfaceFormat aFormat)
{
  RefPtr<DrawTargetD2D> newTarget;

  newTarget = new DrawTargetD2D();
  if (newTarget->Init(aTexture, aFormat)) {
    RefPtr<DrawTarget> retVal = newTarget;

    if (mRecorder) {
      retVal = new DrawTargetRecording(mRecorder, retVal, true);
    }

    return retVal;
  }

  gfxWarning() << "Failed to create draw target for D3D10 texture.";

  // Failed
  return nullptr;
}

TemporaryRef<DrawTarget>
Factory::CreateDualDrawTargetForD3D10Textures(ID3D10Texture2D *aTextureA,
                                              ID3D10Texture2D *aTextureB,
                                              SurfaceFormat aFormat)
{
  RefPtr<DrawTargetD2D> newTargetA;
  RefPtr<DrawTargetD2D> newTargetB;

  newTargetA = new DrawTargetD2D();
  if (!newTargetA->Init(aTextureA, aFormat)) {
    gfxWarning() << "Failed to create draw target for D3D10 texture.";
    return nullptr;
  }

  newTargetB = new DrawTargetD2D();
  if (!newTargetB->Init(aTextureB, aFormat)) {
    gfxWarning() << "Failed to create draw target for D3D10 texture.";
    return nullptr;
  }

  RefPtr<DrawTarget> newTarget =
    new DrawTargetDual(newTargetA, newTargetB);

  RefPtr<DrawTarget> retVal = newTarget;

  if (mRecorder) {
    retVal = new DrawTargetRecording(mRecorder, retVal);
  }

  return retVal;
}

void
Factory::SetDirect3D10Device(ID3D10Device1 *aDevice)
{
  mD3D10Device = aDevice;
}

ID3D10Device1*
Factory::GetDirect3D10Device()
{
  return mD3D10Device;
}

#ifdef USE_D2D1_1
void
Factory::SetDirect3D11Device(ID3D11Device *aDevice)
{
  mD3D11Device = aDevice;

  RefPtr<ID2D1Factory1> factory = D2DFactory1();

  RefPtr<IDXGIDevice> device;
  aDevice->QueryInterface((IDXGIDevice**)byRef(device));
  factory->CreateDevice(device, &mD2D1Device);
}

ID3D11Device*
Factory::GetDirect3D11Device()
{
  return mD3D11Device;
}

ID2D1Device*
Factory::GetD2D1Device()
{
  return mD2D1Device;
}
#endif

TemporaryRef<GlyphRenderingOptions>
Factory::CreateDWriteGlyphRenderingOptions(IDWriteRenderingParams *aParams)
{
  RefPtr<GlyphRenderingOptions> options =
    new GlyphRenderingOptionsDWrite(aParams);

  return options;
}

uint64_t
Factory::GetD2DVRAMUsageDrawTarget()
{
  return DrawTargetD2D::mVRAMUsageDT;
}

uint64_t
Factory::GetD2DVRAMUsageSourceSurface()
{
  return DrawTargetD2D::mVRAMUsageSS;
}

void
Factory::D2DCleanup()
{
  DrawTargetD2D::CleanupD2D();
}

#endif // XP_WIN

#ifdef USE_SKIA_GPU
TemporaryRef<DrawTarget>
Factory::CreateDrawTargetSkiaWithGLContextAndGrGLInterface(GenericRefCountedBase* aGLContext,
                                                           GrGLInterface* aGrGLInterface,
                                                           const IntSize &aSize,
                                                           SurfaceFormat aFormat)
{
  DrawTargetSkia* newDrawTargetSkia = new DrawTargetSkia();
  newDrawTargetSkia->InitWithGLContextAndGrGLInterface(aGLContext, aGrGLInterface, aSize, aFormat);
  RefPtr<DrawTarget> newTarget = newDrawTargetSkia;
  return newTarget;
}

void
Factory::SetGlobalSkiaCacheLimits(int aCount, int aSizeInBytes)
{
    DrawTargetSkia::SetGlobalCacheLimits(aCount, aSizeInBytes);
}
#endif // USE_SKIA_GPU

void
Factory::PurgeAllCaches()
{
#ifdef USE_SKIA_GPU
  DrawTargetSkia::PurgeAllCaches();
#endif
}

#ifdef USE_SKIA_FREETYPE
TemporaryRef<GlyphRenderingOptions>
Factory::CreateCairoGlyphRenderingOptions(FontHinting aHinting, bool aAutoHinting)
{
  RefPtr<GlyphRenderingOptionsCairo> options =
    new GlyphRenderingOptionsCairo();

  options->SetHinting(aHinting);
  options->SetAutoHinting(aAutoHinting);
  return options;
}
#endif

TemporaryRef<DrawTarget>
Factory::CreateDrawTargetForCairoSurface(cairo_surface_t* aSurface, const IntSize& aSize)
{
  RefPtr<DrawTarget> retVal;

#ifdef USE_CAIRO
  RefPtr<DrawTargetCairo> newTarget = new DrawTargetCairo();

  if (newTarget->Init(aSurface, aSize)) {
    retVal = newTarget;
  }

  if (mRecorder && retVal) {
    RefPtr<DrawTarget> recordDT = new DrawTargetRecording(mRecorder, retVal, true);
    return recordDT;
  }
#endif
  return retVal;
}

TemporaryRef<SourceSurface>
Factory::CreateSourceSurfaceForCairoSurface(cairo_surface_t* aSurface,
                                            SurfaceFormat aFormat)
{
  RefPtr<SourceSurface> retVal;

#ifdef USE_CAIRO
  retVal = DrawTargetCairo::CreateSourceSurfaceForCairoSurface(aSurface, aFormat);
#endif
  return retVal;
}

#ifdef XP_MACOSX
TemporaryRef<DrawTarget>
Factory::CreateDrawTargetForCairoCGContext(CGContextRef cg, const IntSize& aSize)
{
  RefPtr<DrawTarget> retVal;

  RefPtr<DrawTargetCG> newTarget = new DrawTargetCG();

  if (newTarget->Init(cg, aSize)) {
    retVal = newTarget;
  }

  if (mRecorder && retVal) {
    RefPtr<DrawTarget> recordDT = new DrawTargetRecording(mRecorder, retVal);
    return recordDT;
  }
  return retVal;
}
#endif

TemporaryRef<DataSourceSurface>
Factory::CreateWrappingDataSourceSurface(uint8_t *aData, int32_t aStride,
                                         const IntSize &aSize,
                                         SurfaceFormat aFormat)
{
  if (aSize.width <= 0 || aSize.height <= 0) {
    return nullptr;
  }

  RefPtr<SourceSurfaceRawData> newSurf = new SourceSurfaceRawData();

  if (newSurf->InitWrappingData(aData, aSize, aStride, aFormat, false)) {
    return newSurf;
  }

  return nullptr;
}

TemporaryRef<DataSourceSurface>
Factory::CreateDataSourceSurface(const IntSize &aSize,
                                 SurfaceFormat aFormat)
{
  if (!CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<SourceSurfaceAlignedRawData> newSurf = new SourceSurfaceAlignedRawData();
  if (newSurf->Init(aSize, aFormat)) {
    return newSurf;
  }

  return nullptr;
}

TemporaryRef<DataSourceSurface>
Factory::CreateDataSourceSurfaceWithStride(const IntSize &aSize,
                                           SurfaceFormat aFormat,
                                           int32_t aStride)
{
  if (aStride < aSize.width * BytesPerPixel(aFormat)) {
    return nullptr;
  }

  RefPtr<SourceSurfaceAlignedRawData> newSurf = new SourceSurfaceAlignedRawData();
  if (newSurf->InitWithStride(aSize, aFormat, aStride)) {
    return newSurf;
  }

  return nullptr;
}

TemporaryRef<DrawEventRecorder>
Factory::CreateEventRecorderForFile(const char *aFilename)
{
  return new DrawEventRecorderFile(aFilename);
}

void
Factory::SetGlobalEventRecorder(DrawEventRecorder *aRecorder)
{
  mRecorder = aRecorder;
}

}
}
