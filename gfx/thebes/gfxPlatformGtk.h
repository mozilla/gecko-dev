/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_GTK_H
#define GFX_PLATFORM_GTK_H

#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "nsAutoRef.h"
#include "nsTArray.h"

#if (MOZ_WIDGET_GTK == 2)
extern "C" {
    typedef struct _GdkDrawable GdkDrawable;
}
#endif

class gfxFontconfigUtils;

class gfxPlatformGtk : public gfxPlatform {
public:
    gfxPlatformGtk();
    virtual ~gfxPlatformGtk();

    static gfxPlatformGtk *GetPlatform() {
        return (gfxPlatformGtk*) gfxPlatform::GetPlatform();
    }

    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& aSize,
                             gfxImageFormat aFormat) override;

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(mozilla::gfx::DrawTarget* aTarget, gfxFont *aFont) override;

    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts) override;

    virtual nsresult UpdateFontList() override;

    virtual void
    GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                           int32_t aRunScript,
                           nsTArray<const char*>& aFontList) override;

    virtual gfxPlatformFontList* CreatePlatformFontList() override;

    virtual nsresult GetStandardFamilyName(const nsAString& aFontName,
                                           nsAString& aFamilyName) override;

    virtual gfxFontGroup* CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                                          const gfxFontStyle *aStyle,
                                          gfxUserFontSet *aUserFontSet) override;

    /**
     * Look up a local platform font using the full font face name (needed to
     * support @font-face src local() )
     */
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          bool aItalic) override;

    /**
     * Activate a platform font (needed to support @font-face src url() )
     *
     */
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           bool aItalic,
                                           const uint8_t* aFontData,
                                           uint32_t aLength) override;

    /**
     * Check whether format is supported on a platform or not (if unclear,
     * returns true).
     */
    virtual bool IsFontFormatSupported(nsIURI *aFontURI,
                                         uint32_t aFormatFlags) override;

    /**
     * Calls XFlush if xrender is enabled.
     */
    virtual void FlushContentDrawing() override;

#if (MOZ_WIDGET_GTK == 2)
    static void SetGdkDrawable(cairo_surface_t *target,
                               GdkDrawable *drawable);
    static GdkDrawable *GetGdkDrawable(cairo_surface_t *target);
#endif

    static int32_t GetDPI();
    static double  GetDPIScale();

    bool UseXRender() {
#if defined(MOZ_X11)
        if (GetContentBackend() != mozilla::gfx::BackendType::NONE &&
            GetContentBackend() != mozilla::gfx::BackendType::CAIRO)
            return false;

        return sUseXRender;
#else
        return false;
#endif
    }

    static bool UseFcFontList() { return sUseFcFontList; }

    bool UseImageOffscreenSurfaces() {
        // We want to turn on image offscreen surfaces ONLY for GTK3 builds
        // since GTK2 theme rendering still requires xlib surfaces per se.
#if (MOZ_WIDGET_GTK == 3)
        return gfxPrefs::UseImageOffscreenSurfaces();
#else
        return false;
#endif
    }

    virtual gfxImageFormat GetOffscreenFormat() override;

    virtual int GetScreenDepth() const override;

    bool SupportsApzWheelInput() const override {
      return true;
    }

protected:
    static gfxFontconfigUtils *sFontconfigUtils;

private:
    virtual void GetPlatformCMSOutputProfile(void *&mem,
                                             size_t &size) override;

#ifdef MOZ_X11
    static bool sUseXRender;
#endif

    // xxx - this will be removed once the new fontconfig platform font list
    // replaces gfxPangoFontGroup
    static bool sUseFcFontList;
};

#endif /* GFX_PLATFORM_GTK_H */
