/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "gfxAndroidPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/CountingAllocatorBase.h"
#include "mozilla/Preferences.h"

#include "gfx2DGlue.h"
#include "gfxFT2FontList.h"
#include "gfxImageSurface.h"
#include "gfxTextRun.h"
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsILocaleService.h"
#include "nsServiceManagerUtils.h"
#include "gfxPrefs.h"
#include "cairo.h"
#include "VsyncSource.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include <cutils/properties.h>
#include "mozilla/layers/CompositorParent.h"
#include "HwcComposer2D.h"
#endif

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_MODULE_H

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

static FT_Library gPlatformFTLibrary = nullptr;

class FreetypeReporter final : public nsIMemoryReporter,
                               public CountingAllocatorBase<FreetypeReporter>
{
private:
    ~FreetypeReporter() {}

public:
    NS_DECL_ISUPPORTS

    static void* Malloc(FT_Memory, long size)
    {
        return CountingMalloc(size);
    }

    static void Free(FT_Memory, void* p)
    {
        return CountingFree(p);
    }

    static void*
    Realloc(FT_Memory, long cur_size, long new_size, void* p)
    {
        return CountingRealloc(p, new_size);
    }

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
    {
        return MOZ_COLLECT_REPORT(
            "explicit/freetype", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
            "Memory used by Freetype.");
    }
};

NS_IMPL_ISUPPORTS(FreetypeReporter, nsIMemoryReporter)

template<> Atomic<size_t> CountingAllocatorBase<FreetypeReporter>::sAmount(0);

static FT_MemoryRec_ sFreetypeMemoryRecord;

gfxAndroidPlatform::gfxAndroidPlatform()
{
    // A custom allocator.  It counts allocations, enabling memory reporting.
    sFreetypeMemoryRecord.user    = nullptr;
    sFreetypeMemoryRecord.alloc   = FreetypeReporter::Malloc;
    sFreetypeMemoryRecord.free    = FreetypeReporter::Free;
    sFreetypeMemoryRecord.realloc = FreetypeReporter::Realloc;

    // These two calls are equivalent to FT_Init_FreeType(), but allow us to
    // provide a custom memory allocator.
    FT_New_Library(&sFreetypeMemoryRecord, &gPlatformFTLibrary);
    FT_Add_Default_Modules(gPlatformFTLibrary);

    RegisterStrongMemoryReporter(new FreetypeReporter());

    nsCOMPtr<nsIScreenManager> screenMgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
    nsCOMPtr<nsIScreen> screen;
    screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
    mScreenDepth = 24;
    screen->GetColorDepth(&mScreenDepth);

    mOffscreenFormat = mScreenDepth == 16
                       ? gfxImageFormat::RGB16_565
                       : gfxImageFormat::RGB24;

    if (gfxPrefs::AndroidRGB16Force()) {
        mOffscreenFormat = gfxImageFormat::RGB16_565;
    }

#ifdef MOZ_WIDGET_GONK
    char propQemu[PROPERTY_VALUE_MAX];
    property_get("ro.kernel.qemu", propQemu, "");
    mIsInGonkEmulator = !strncmp(propQemu, "1", 1);
#endif
}

gfxAndroidPlatform::~gfxAndroidPlatform()
{
    FT_Done_Library(gPlatformFTLibrary);
    gPlatformFTLibrary = nullptr;
}

already_AddRefed<gfxASurface>
gfxAndroidPlatform::CreateOffscreenSurface(const IntSize& aSize,
                                           gfxImageFormat aFormat)
{
    nsRefPtr<gfxASurface> newSurface;
    newSurface = new gfxImageSurface(aSize, aFormat);

    return newSurface.forget();
}

static bool
IsJapaneseLocale()
{
    static bool sInitialized = false;
    static bool sIsJapanese = false;

    if (!sInitialized) {
        sInitialized = true;

        do { // to allow 'break' to abandon this block if a call fails
            nsresult rv;
            nsCOMPtr<nsILocaleService> ls =
                do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
            if (NS_FAILED(rv)) {
                break;
            }
            nsCOMPtr<nsILocale> appLocale;
            rv = ls->GetApplicationLocale(getter_AddRefs(appLocale));
            if (NS_FAILED(rv)) {
                break;
            }
            nsString localeStr;
            rv = appLocale->
                GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), localeStr);
            if (NS_FAILED(rv)) {
                break;
            }
            const nsAString& lang = nsDependentSubstring(localeStr, 0, 2);
            if (lang.EqualsLiteral("ja")) {
                sIsJapanese = true;
            }
        } while (false);
    }

    return sIsJapanese;
}

void
gfxAndroidPlatform::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                           int32_t aRunScript,
                                           nsTArray<const char*>& aFontList)
{
    static const char kDroidSansJapanese[] = "Droid Sans Japanese";
    static const char kMotoyaLMaru[] = "MotoyaLMaru";

    if (aNextCh == 0xfe0fu) {
        // if char is followed by VS16, try for a color emoji glyph
        aFontList.AppendElement("Noto Color Emoji");
    }

    if (IS_IN_BMP(aCh)) {
        // try language-specific "Droid Sans *" and "Noto Sans *" fonts for
        // certain blocks, as most devices probably have these
        uint8_t block = (aCh >> 8) & 0xff;
        switch (block) {
        case 0x05:
            aFontList.AppendElement("Droid Sans Hebrew");
            aFontList.AppendElement("Droid Sans Armenian");
            break;
        case 0x06:
            aFontList.AppendElement("Droid Sans Arabic");
            break;
        case 0x09:
            aFontList.AppendElement("Noto Sans Devanagari");
            aFontList.AppendElement("Droid Sans Devanagari");
            break;
        case 0x0b:
            aFontList.AppendElement("Noto Sans Tamil");
            aFontList.AppendElement("Droid Sans Tamil");
            break;
        case 0x0e:
            aFontList.AppendElement("Noto Sans Thai");
            aFontList.AppendElement("Droid Sans Thai");
            break;
        case 0x10: case 0x2d:
            aFontList.AppendElement("Droid Sans Georgian");
            break;
        case 0x12: case 0x13:
            aFontList.AppendElement("Droid Sans Ethiopic");
            break;
        case 0xf9: case 0xfa:
            if (IsJapaneseLocale()) {
                aFontList.AppendElement(kMotoyaLMaru);
                aFontList.AppendElement(kDroidSansJapanese);
            }
            break;
        default:
            if (block >= 0x2e && block <= 0x9f && IsJapaneseLocale()) {
                aFontList.AppendElement(kMotoyaLMaru);
                aFontList.AppendElement(kDroidSansJapanese);
            }
            break;
        }
    }
    // and try Droid Sans Fallback as a last resort
    aFontList.AppendElement("Droid Sans Fallback");
}

nsresult
gfxAndroidPlatform::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup,
                                                         aGenericFamily,
                                                         aListOfFonts);
    return NS_OK;
}

void
gfxAndroidPlatform::GetSystemFontList(InfallibleTArray<FontListEntry>* retValue)
{
    gfxFT2FontList::PlatformFontList()->GetSystemFontList(retValue);
}

nsresult
gfxAndroidPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    return NS_OK;
}

nsresult
gfxAndroidPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxPlatformFontList*
gfxAndroidPlatform::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFT2FontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

bool
gfxAndroidPlatform::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
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

gfxFontGroup *
gfxAndroidPlatform::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                    const gfxFontStyle *aStyle,
                                    gfxUserFontSet* aUserFontSet)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aUserFontSet);
}

FT_Library
gfxAndroidPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}

gfxFontEntry*
gfxAndroidPlatform::LookupLocalFont(const nsAString& aFontName,
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
gfxAndroidPlatform::MakePlatformFont(const nsAString& aFontName,
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

TemporaryRef<ScaledFont>
gfxAndroidPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    return GetScaledFontForFontWithCairoSkia(aTarget, aFont);
}

bool
gfxAndroidPlatform::FontHintingEnabled()
{
    // In "mobile" builds, we sometimes use non-reflow-zoom, so we
    // might not want hinting.  Let's see.

#ifdef MOZ_USING_ANDROID_JAVA_WIDGETS
    // On android-java, we currently only use gecko to render web
    // content that can always be be non-reflow-zoomed.  So turn off
    // hinting.
    // 
    // XXX when gecko-android-java is used as an "app runtime", we may
    // want to re-enable hinting for non-browser processes there.
    return false;
#endif //  MOZ_USING_ANDROID_JAVA_WIDGETS

#ifdef MOZ_WIDGET_GONK
    // On B2G, the UX preference is currently to keep hinting disabled
    // for all text (see bug 829523).
    return false;
#endif

    // Currently, we don't have any other targets, but if/when we do,
    // decide how to handle them here.

    NS_NOTREACHED("oops, what platform is this?");
    return gfxPlatform::FontHintingEnabled();
}

bool
gfxAndroidPlatform::RequiresLinearZoom()
{
#ifdef MOZ_USING_ANDROID_JAVA_WIDGETS
    // On android-java, we currently only use gecko to render web
    // content that can always be be non-reflow-zoomed.
    //
    // XXX when gecko-android-java is used as an "app runtime", we may
    // want to treat it like B2G and use linear zoom only for the web
    // browser process, not other apps.
    return true;
#endif

#ifdef MOZ_WIDGET_GONK
    // On B2G, we need linear zoom for the browser, but otherwise prefer
    // the improved glyph spacing that results from respecting the device
    // pixel resolution for glyph layout (see bug 816614).
    return XRE_GetProcessType() == GeckoProcessType_Content &&
           ContentChild::GetSingleton()->IsForBrowser();
#endif

    NS_NOTREACHED("oops, what platform is this?");
    return gfxPlatform::RequiresLinearZoom();
}

int
gfxAndroidPlatform::GetScreenDepth() const
{
    return mScreenDepth;
}

bool
gfxAndroidPlatform::UseAcceleratedSkiaCanvas()
{
    return HaveChoiceOfHWAndSWCanvas() && gfxPlatform::UseAcceleratedSkiaCanvas();
}

bool gfxAndroidPlatform::HaveChoiceOfHWAndSWCanvas()
{
#ifdef MOZ_WIDGET_ANDROID
    if (AndroidBridge::Bridge()->GetAPIVersion() < 11) {
        // It's slower than software due to not having a compositing fast path
        return false;
    }
#endif
    return gfxPlatform::HaveChoiceOfHWAndSWCanvas();
}

#ifdef MOZ_WIDGET_GONK
class GonkVsyncSource final : public VsyncSource
{
public:
  GonkVsyncSource()
  {
  }

  virtual Display& GetGlobalDisplay() override
  {
    return mGlobalDisplay;
  }

  class GonkDisplay final : public VsyncSource::Display
  {
  public:
    GonkDisplay() : mVsyncEnabled(false)
    {
    }

    ~GonkDisplay()
    {
      DisableVsync();
    }

    virtual void EnableVsync() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      if (IsVsyncEnabled()) {
        return;
      }
      mVsyncEnabled = HwcComposer2D::GetInstance()->EnableVsync(true);
    }

    virtual void DisableVsync() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      if (!IsVsyncEnabled()) {
        return;
      }
      mVsyncEnabled = HwcComposer2D::GetInstance()->EnableVsync(false);
    }

    virtual bool IsVsyncEnabled() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      return mVsyncEnabled;
    }
  private:
    bool mVsyncEnabled;
  }; // GonkDisplay

private:
  virtual ~GonkVsyncSource()
  {
  }

  GonkDisplay mGlobalDisplay;
}; // GonkVsyncSource
#endif

already_AddRefed<mozilla::gfx::VsyncSource>
gfxAndroidPlatform::CreateHardwareVsyncSource()
{
    // Only enable true hardware vsync on kit-kat and L device. Jelly Bean has
    // inaccurate hardware vsync so disable on JB. Android pre-JB doesn't have
    // hardware vsync.
    // L is android version 21, L-MR1 is 22, kit-kat is 19, 20 is kit-kat for
    // wearables.
#if defined(MOZ_WIDGET_GONK) && (ANDROID_VERSION == 19 || ANDROID_VERSION >= 21)
    nsRefPtr<GonkVsyncSource> vsyncSource = new GonkVsyncSource();
    VsyncSource::Display& display = vsyncSource->GetGlobalDisplay();
    display.EnableVsync();
    if (!display.IsVsyncEnabled()) {
        NS_WARNING("Error enabling gonk vsync. Falling back to software vsync");
        return gfxPlatform::CreateHardwareVsyncSource();
    }
    display.DisableVsync();
    return vsyncSource.forget();
#else
    return gfxPlatform::CreateHardwareVsyncSource();
#endif
}
