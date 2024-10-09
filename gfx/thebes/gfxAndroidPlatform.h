/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_ANDROID_H
#define GFX_PLATFORM_ANDROID_H

#include "gfxPlatform.h"
#include "gfxUserFontSet.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class gfxAndroidPlatform final : public gfxPlatform {
 public:
  gfxAndroidPlatform();
  virtual ~gfxAndroidPlatform();

  static gfxAndroidPlatform* GetPlatform() {
    return (gfxAndroidPlatform*)gfxPlatform::GetPlatform();
  }

  already_AddRefed<gfxASurface> CreateOffscreenSurface(
      const IntSize& aSize, gfxImageFormat aFormat) override;

  gfxImageFormat GetOffscreenFormat() override { return mOffscreenFormat; }

  // platform implementations of font functions
  bool CreatePlatformFontList() override;

  void ReadSystemFontList(mozilla::dom::SystemFontList*) override;

  void GetCommonFallbackFonts(uint32_t aCh, Script aRunScript,
                              eFontPresentation aPresentation,
                              nsTArray<const char*>& aFontList) override;

  bool FontHintingEnabled() override;
  bool RequiresLinearZoom() override;

  already_AddRefed<mozilla::gfx::VsyncSource> CreateGlobalHardwareVsyncSource()
      override;

  static bool CheckVariationFontSupport();

  // From Android 12, Font API doesn't read XML files only. To handle updated
  // font, initializing font API causes that it analyzes all font files. So we
  // have to call this API at start up on another thread to initialize it.
  static void InitializeFontAPI();
  static void WaitForInitializeFontAPI();

  static bool IsFontAPIDisabled(bool aDontCheckPref = false);

 protected:
  void InitAcceleration() override;

  bool AccelerateLayersByDefault() override { return true; }

 private:
  static void FontAPIInitializeCallback(void*);

  gfxImageFormat mOffscreenFormat;

  static PRThread* sFontAPIInitializeThread;
  static nsCString sManufacturer;
};

#endif /* GFX_PLATFORM_ANDROID_H */
