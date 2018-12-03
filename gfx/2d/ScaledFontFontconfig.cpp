/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFontconfig.h"
#include "UnscaledFontFreeType.h"
#include "NativeFontResourceFreeType.h"
#include "Logging.h"
#include "StackArray.h"
#include "mozilla/webrender/WebRenderTypes.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_cairo.h"
#endif

#include <fontconfig/fcfreetype.h>

#include FT_MULTIPLE_MASTERS_H

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries,
// which is a requirement when we consider runtime switchable backends and so on
ScaledFontFontconfig::ScaledFontFontconfig(
    cairo_scaled_font_t* aScaledFont, FcPattern* aPattern,
    const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize)
    : ScaledFontBase(aUnscaledFont, aSize), mPattern(aPattern) {
  SetCairoScaledFont(aScaledFont);
  FcPatternReference(aPattern);
}

ScaledFontFontconfig::~ScaledFontFontconfig() { FcPatternDestroy(mPattern); }

#ifdef USE_SKIA
SkTypeface* ScaledFontFontconfig::CreateSkTypeface() {
  return SkCreateTypefaceFromCairoFTFontWithFontconfig(mScaledFont, mPattern);
}
#endif

ScaledFontFontconfig::InstanceData::InstanceData(
    cairo_scaled_font_t* aScaledFont, FcPattern* aPattern)
    : mFlags(0),
      mHintStyle(FC_HINT_NONE),
      mSubpixelOrder(FC_RGBA_UNKNOWN),
      mLcdFilter(FC_LCD_LEGACY) {
  // Record relevant Fontconfig properties into instance data.
  FcBool autohint;
  if (FcPatternGetBool(aPattern, FC_AUTOHINT, 0, &autohint) == FcResultMatch &&
      autohint) {
    mFlags |= AUTOHINT;
  }
  FcBool bitmap;
  if (FcPatternGetBool(aPattern, FC_EMBEDDED_BITMAP, 0, &bitmap) ==
          FcResultMatch &&
      bitmap) {
    mFlags |= EMBEDDED_BITMAP;
  }
  FcBool embolden;
  if (FcPatternGetBool(aPattern, FC_EMBOLDEN, 0, &embolden) == FcResultMatch &&
      embolden) {
    mFlags |= EMBOLDEN;
  }
  FcBool vertical;
  if (FcPatternGetBool(aPattern, FC_VERTICAL_LAYOUT, 0, &vertical) ==
          FcResultMatch &&
      vertical) {
    mFlags |= VERTICAL_LAYOUT;
  }

  FcBool antialias;
  if (FcPatternGetBool(aPattern, FC_ANTIALIAS, 0, &antialias) !=
          FcResultMatch ||
      antialias) {
    mFlags |= ANTIALIAS;

    // Only record subpixel order and lcd filtering if antialiasing is enabled.
    int rgba;
    if (FcPatternGetInteger(aPattern, FC_RGBA, 0, &rgba) == FcResultMatch) {
      mSubpixelOrder = rgba;
    }
    int filter;
    if (FcPatternGetInteger(aPattern, FC_LCD_FILTER, 0, &filter) ==
        FcResultMatch) {
      mLcdFilter = filter;
    }
  }

  cairo_font_options_t* fontOptions = cairo_font_options_create();
  cairo_scaled_font_get_font_options(aScaledFont, fontOptions);
  // For printer fonts, Cairo hint metrics and hinting will be disabled.
  // For other fonts, allow hint metrics and hinting.
  if (cairo_font_options_get_hint_metrics(fontOptions) !=
      CAIRO_HINT_METRICS_OFF) {
    mFlags |= HINT_METRICS;

    FcBool hinting;
    if (FcPatternGetBool(aPattern, FC_HINTING, 0, &hinting) != FcResultMatch ||
        hinting) {
      int hintstyle;
      if (FcPatternGetInteger(aPattern, FC_HINT_STYLE, 0, &hintstyle) !=
          FcResultMatch) {
        hintstyle = FC_HINT_FULL;
      }
      mHintStyle = hintstyle;
    }
  }
  cairo_font_options_destroy(fontOptions);
}

ScaledFontFontconfig::InstanceData::InstanceData(
    const wr::FontInstanceOptions* aOptions,
    const wr::FontInstancePlatformOptions* aPlatformOptions)
    : mFlags(HINT_METRICS),
      mHintStyle(FC_HINT_FULL),
      mSubpixelOrder(FC_RGBA_UNKNOWN),
      mLcdFilter(FC_LCD_LEGACY) {
  if (aOptions) {
    if (aOptions->flags & wr::FontInstanceFlags::FORCE_AUTOHINT) {
      mFlags |= AUTOHINT;
    }
    if (aOptions->flags & wr::FontInstanceFlags::EMBEDDED_BITMAPS) {
      mFlags |= EMBEDDED_BITMAP;
    }
    if (aOptions->flags & wr::FontInstanceFlags::SYNTHETIC_BOLD) {
      mFlags |= EMBOLDEN;
    }
    if (aOptions->flags & wr::FontInstanceFlags::VERTICAL_LAYOUT) {
      mFlags |= VERTICAL_LAYOUT;
    }
    if (aOptions->render_mode != wr::FontRenderMode::Mono) {
      mFlags |= ANTIALIAS;
      if (aOptions->render_mode == wr::FontRenderMode::Subpixel) {
        if (aOptions->flags & wr::FontInstanceFlags::SUBPIXEL_BGR) {
          mSubpixelOrder = aOptions->flags & wr::FontInstanceFlags::LCD_VERTICAL
                               ? FC_RGBA_VBGR
                               : FC_RGBA_BGR;
        } else {
          mSubpixelOrder = aOptions->flags & wr::FontInstanceFlags::LCD_VERTICAL
                               ? FC_RGBA_VRGB
                               : FC_RGBA_RGB;
        }
      }
    }
  }
  if (aPlatformOptions) {
    switch (aPlatformOptions->hinting) {
      case wr::FontHinting::None:
        mHintStyle = FC_HINT_NONE;
        break;
      case wr::FontHinting::Light:
        mHintStyle = FC_HINT_SLIGHT;
        break;
      case wr::FontHinting::Normal:
        mHintStyle = FC_HINT_MEDIUM;
        break;
      default:
        break;
    }
    switch (aPlatformOptions->lcd_filter) {
      case wr::FontLCDFilter::None:
        mLcdFilter = FC_LCD_NONE;
        break;
      case wr::FontLCDFilter::Default:
        mLcdFilter = FC_LCD_DEFAULT;
        break;
      case wr::FontLCDFilter::Light:
        mLcdFilter = FC_LCD_LIGHT;
        break;
      default:
        break;
    }
  }
}

void ScaledFontFontconfig::InstanceData::SetupPattern(
    FcPattern* aPattern) const {
  if (mFlags & AUTOHINT) {
    FcPatternAddBool(aPattern, FC_AUTOHINT, FcTrue);
  }
  if (mFlags & EMBEDDED_BITMAP) {
    FcPatternAddBool(aPattern, FC_EMBEDDED_BITMAP, FcTrue);
  }
  if (mFlags & EMBOLDEN) {
    FcPatternAddBool(aPattern, FC_EMBOLDEN, FcTrue);
  }
  if (mFlags & VERTICAL_LAYOUT) {
    FcPatternAddBool(aPattern, FC_VERTICAL_LAYOUT, FcTrue);
  }

  if (mFlags & ANTIALIAS) {
    FcPatternAddBool(aPattern, FC_ANTIALIAS, FcTrue);
    if (mSubpixelOrder != FC_RGBA_UNKNOWN) {
      FcPatternAddInteger(aPattern, FC_RGBA, mSubpixelOrder);
    }
    if (mLcdFilter != FC_LCD_LEGACY) {
      FcPatternAddInteger(aPattern, FC_LCD_FILTER, mLcdFilter);
    }
  } else {
    FcPatternAddBool(aPattern, FC_ANTIALIAS, FcFalse);
  }

  if (mHintStyle) {
    FcPatternAddBool(aPattern, FC_HINTING, FcTrue);
    FcPatternAddInteger(aPattern, FC_HINT_STYLE, mHintStyle);
  } else {
    FcPatternAddBool(aPattern, FC_HINTING, FcFalse);
  }
}

void ScaledFontFontconfig::InstanceData::SetupFontOptions(
    cairo_font_options_t* aFontOptions) const {
  // Try to build a sane initial set of Cairo font options based on the
  // Fontconfig pattern.
  if (mFlags & HINT_METRICS) {
    // For regular (non-printer) fonts, enable hint metrics as well as hinting
    // and (possibly subpixel) antialiasing.
    cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_ON);

    cairo_hint_style_t hinting;
    switch (mHintStyle) {
      case FC_HINT_NONE:
        hinting = CAIRO_HINT_STYLE_NONE;
        break;
      case FC_HINT_SLIGHT:
        hinting = CAIRO_HINT_STYLE_SLIGHT;
        break;
      case FC_HINT_MEDIUM:
      default:
        hinting = CAIRO_HINT_STYLE_MEDIUM;
        break;
      case FC_HINT_FULL:
        hinting = CAIRO_HINT_STYLE_FULL;
        break;
    }
    cairo_font_options_set_hint_style(aFontOptions, hinting);

    if (mFlags & ANTIALIAS) {
      cairo_subpixel_order_t subpixel = CAIRO_SUBPIXEL_ORDER_DEFAULT;
      switch (mSubpixelOrder) {
        case FC_RGBA_RGB:
          subpixel = CAIRO_SUBPIXEL_ORDER_RGB;
          break;
        case FC_RGBA_BGR:
          subpixel = CAIRO_SUBPIXEL_ORDER_BGR;
          break;
        case FC_RGBA_VRGB:
          subpixel = CAIRO_SUBPIXEL_ORDER_VRGB;
          break;
        case FC_RGBA_VBGR:
          subpixel = CAIRO_SUBPIXEL_ORDER_VBGR;
          break;
        default:
          break;
      }
      if (subpixel != CAIRO_SUBPIXEL_ORDER_DEFAULT) {
        cairo_font_options_set_antialias(aFontOptions,
                                         CAIRO_ANTIALIAS_SUBPIXEL);
        cairo_font_options_set_subpixel_order(aFontOptions, subpixel);
      } else {
        cairo_font_options_set_antialias(aFontOptions, CAIRO_ANTIALIAS_GRAY);
      }
    } else {
      cairo_font_options_set_antialias(aFontOptions, CAIRO_ANTIALIAS_NONE);
    }
  } else {
    // For printer fonts, disable hint metrics and hinting. Don't allow subpixel
    // antialiasing.
    cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_hint_style(aFontOptions, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_antialias(aFontOptions, mFlags & ANTIALIAS
                                                       ? CAIRO_ANTIALIAS_GRAY
                                                       : CAIRO_ANTIALIAS_NONE);
  }
}

bool ScaledFontFontconfig::GetFontInstanceData(FontInstanceDataOutput aCb,
                                               void* aBaton) {
  InstanceData instance(GetCairoScaledFont(), mPattern);

  std::vector<FontVariation> variations;
  if (HasVariationSettings()) {
    FT_Face face = nullptr;
    if (FcPatternGetFTFace(mPattern, FC_FT_FACE, 0, &face) == FcResultMatch) {
      UnscaledFontFreeType::GetVariationSettingsFromFace(&variations, face);
    }
  }

  aCb(reinterpret_cast<uint8_t*>(&instance), sizeof(instance),
      variations.data(), variations.size(), aBaton);
  return true;
}

bool ScaledFontFontconfig::GetWRFontInstanceOptions(
    Maybe<wr::FontInstanceOptions>* aOutOptions,
    Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
    std::vector<FontVariation>* aOutVariations) {
  wr::FontInstanceOptions options;
  options.render_mode = wr::FontRenderMode::Alpha;
  // FIXME: Cairo-FT metrics are not compatible with subpixel positioning.
  // options.flags = wr::FontInstanceFlags::SUBPIXEL_POSITION;
  options.flags = 0;
  options.bg_color = wr::ToColorU(Color());
  options.synthetic_italics =
      wr::DegreesToSyntheticItalics(GetSyntheticObliqueAngle());

  wr::FontInstancePlatformOptions platformOptions;
  platformOptions.lcd_filter = wr::FontLCDFilter::Legacy;
  platformOptions.hinting = wr::FontHinting::Normal;

  FcBool autohint;
  if (FcPatternGetBool(mPattern, FC_AUTOHINT, 0, &autohint) == FcResultMatch &&
      autohint) {
    options.flags |= wr::FontInstanceFlags::FORCE_AUTOHINT;
  }
  FcBool embolden;
  if (FcPatternGetBool(mPattern, FC_EMBOLDEN, 0, &embolden) == FcResultMatch &&
      embolden) {
    options.flags |= wr::FontInstanceFlags::SYNTHETIC_BOLD;
  }
  FcBool vertical;
  if (FcPatternGetBool(mPattern, FC_VERTICAL_LAYOUT, 0, &vertical) ==
          FcResultMatch &&
      vertical) {
    options.flags |= wr::FontInstanceFlags::VERTICAL_LAYOUT;
  }

  FcBool antialias;
  if (FcPatternGetBool(mPattern, FC_ANTIALIAS, 0, &antialias) !=
          FcResultMatch ||
      antialias) {
    int rgba;
    if (FcPatternGetInteger(mPattern, FC_RGBA, 0, &rgba) == FcResultMatch) {
      switch (rgba) {
        case FC_RGBA_RGB:
        case FC_RGBA_BGR:
        case FC_RGBA_VRGB:
        case FC_RGBA_VBGR:
          options.render_mode = wr::FontRenderMode::Subpixel;
          if (rgba == FC_RGBA_VRGB || rgba == FC_RGBA_VBGR) {
            options.flags |= wr::FontInstanceFlags::LCD_VERTICAL;
          }
          platformOptions.hinting = wr::FontHinting::LCD;
          if (rgba == FC_RGBA_BGR || rgba == FC_RGBA_VBGR) {
            options.flags |= wr::FontInstanceFlags::SUBPIXEL_BGR;
          }
          break;
        case FC_RGBA_NONE:
        case FC_RGBA_UNKNOWN:
        default:
          break;
      }
    }

    if (options.render_mode == wr::FontRenderMode::Subpixel) {
      int filter;
      if (FcPatternGetInteger(mPattern, FC_LCD_FILTER, 0, &filter) ==
          FcResultMatch) {
        switch (filter) {
          case FC_LCD_NONE:
            platformOptions.lcd_filter = wr::FontLCDFilter::None;
            break;
          case FC_LCD_DEFAULT:
            platformOptions.lcd_filter = wr::FontLCDFilter::Default;
            break;
          case FC_LCD_LIGHT:
            platformOptions.lcd_filter = wr::FontLCDFilter::Light;
            break;
          case FC_LCD_LEGACY:
          default:
            break;
        }
      }
    }

    // Match cairo-ft's handling of embeddedbitmap:
    // If AA is explicitly disabled, leave bitmaps enabled.
    // Otherwise, disable embedded bitmaps unless explicitly enabled.
    FcBool bitmap;
    if (FcPatternGetBool(mPattern, FC_EMBEDDED_BITMAP, 0, &bitmap) ==
            FcResultMatch &&
        bitmap) {
      options.flags |= wr::FontInstanceFlags::EMBEDDED_BITMAPS;
    }
  } else {
    options.render_mode = wr::FontRenderMode::Mono;
    platformOptions.hinting = wr::FontHinting::Mono;
    options.flags |= wr::FontInstanceFlags::EMBEDDED_BITMAPS;
  }

  FcBool hinting;
  int hintstyle;
  if (FcPatternGetBool(mPattern, FC_HINTING, 0, &hinting) != FcResultMatch ||
      hinting) {
    if (FcPatternGetInteger(mPattern, FC_HINT_STYLE, 0, &hintstyle) !=
        FcResultMatch) {
      hintstyle = FC_HINT_FULL;
    }
  } else {
    hintstyle = FC_HINT_NONE;
  }

  if (hintstyle == FC_HINT_NONE) {
    platformOptions.hinting = wr::FontHinting::None;
  } else if (options.render_mode != wr::FontRenderMode::Mono) {
    switch (hintstyle) {
      case FC_HINT_SLIGHT:
        platformOptions.hinting = wr::FontHinting::Light;
        break;
      case FC_HINT_MEDIUM:
        platformOptions.hinting = wr::FontHinting::Normal;
        break;
      case FC_HINT_FULL:
      default:
        break;
    }
  }

  *aOutOptions = Some(options);
  *aOutPlatformOptions = Some(platformOptions);

  if (HasVariationSettings()) {
    FT_Face face = nullptr;
    if (FcPatternGetFTFace(mPattern, FC_FT_FACE, 0, &face) == FcResultMatch) {
      UnscaledFontFreeType::GetVariationSettingsFromFace(aOutVariations, face);
    }
  }

  return true;
}

static cairo_user_data_key_t sNativeFontResourceKey;

static void ReleaseNativeFontResource(void* aData) {
  static_cast<NativeFontResource*>(aData)->Release();
}

static cairo_user_data_key_t sFaceKey;

static void ReleaseFace(void* aData) {
  Factory::ReleaseFTFace(static_cast<FT_Face>(aData));
}

already_AddRefed<ScaledFont> UnscaledFontFontconfig::CreateScaledFont(
    Float aSize, const uint8_t* aInstanceData, uint32_t aInstanceDataLength,
    const FontVariation* aVariations, uint32_t aNumVariations) {
  if (aInstanceDataLength < sizeof(ScaledFontFontconfig::InstanceData)) {
    gfxWarning() << "Fontconfig scaled font instance data is truncated.";
    return nullptr;
  }
  const ScaledFontFontconfig::InstanceData& instanceData =
      *reinterpret_cast<const ScaledFontFontconfig::InstanceData*>(
          aInstanceData);

  FcPattern* pattern = FcPatternCreate();
  if (!pattern) {
    gfxWarning() << "Failed initializing Fontconfig pattern for scaled font";
    return nullptr;
  }
  FT_Face face = GetFace();
  NativeFontResourceFreeType* nfr =
      static_cast<NativeFontResourceFreeType*>(mNativeFontResource.get());
  FT_Face varFace = nullptr;
  if (face) {
    if (nfr && aNumVariations > 0) {
      varFace = nfr->CloneFace();
      if (!varFace) {
        gfxWarning() << "Failed cloning face for variations";
      }
    }
    FcPatternAddFTFace(pattern, FC_FT_FACE, varFace ? varFace : face);
  } else {
    FcPatternAddString(pattern, FC_FILE,
                       reinterpret_cast<const FcChar8*>(GetFile()));
    FcPatternAddInteger(pattern, FC_INDEX, GetIndex());
  }
  FcPatternAddDouble(pattern, FC_PIXEL_SIZE, aSize);
  instanceData.SetupPattern(pattern);

  StackArray<FT_Fixed, 32> coords(aNumVariations);
  for (uint32_t i = 0; i < aNumVariations; i++) {
    coords[i] = std::round(aVariations[i].mValue * 65536.0);
  }

  cairo_font_face_t* font = cairo_ft_font_face_create_for_pattern(
      pattern, coords.data(), aNumVariations);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo font face for Fontconfig pattern";
    FcPatternDestroy(pattern);
    if (varFace) {
      Factory::ReleaseFTFace(varFace);
    }
    return nullptr;
  }

  if (nfr) {
    // Bug 1362117 - Cairo may keep the font face alive after the owning
    // NativeFontResource was freed. To prevent this, we must bind the
    // NativeFontResource to the font face so that it stays alive at least as
    // long as the font face.
    nfr->AddRef();
    cairo_status_t err = CAIRO_STATUS_SUCCESS;
    bool cleanupFace = false;
    if (varFace) {
      err =
          cairo_font_face_set_user_data(font, &sFaceKey, varFace, ReleaseFace);
    }
    if (err != CAIRO_STATUS_SUCCESS) {
      cleanupFace = true;
    } else {
      err = cairo_font_face_set_user_data(font, &sNativeFontResourceKey, nfr,
                                          ReleaseNativeFontResource);
    }
    if (err != CAIRO_STATUS_SUCCESS) {
      gfxWarning() << "Failed binding NativeFontResource to Cairo font face";
      if (varFace && cleanupFace) {
        Factory::ReleaseFTFace(varFace);
      }
      nfr->Release();
      cairo_font_face_destroy(font);
      FcPatternDestroy(pattern);
      return nullptr;
    }
  }

  cairo_matrix_t sizeMatrix;
  cairo_matrix_init(&sizeMatrix, aSize, 0, 0, aSize, 0, 0);

  cairo_matrix_t identityMatrix;
  cairo_matrix_init_identity(&identityMatrix);

  cairo_font_options_t* fontOptions = cairo_font_options_create();
  instanceData.SetupFontOptions(fontOptions);

  cairo_scaled_font_t* cairoScaledFont =
      cairo_scaled_font_create(font, &sizeMatrix, &identityMatrix, fontOptions);

  cairo_font_options_destroy(fontOptions);
  cairo_font_face_destroy(font);

  if (cairo_scaled_font_status(cairoScaledFont) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo scaled font for font face";
    FcPatternDestroy(pattern);
    return nullptr;
  }

  RefPtr<ScaledFontFontconfig> scaledFont =
      new ScaledFontFontconfig(cairoScaledFont, pattern, this, aSize);

  cairo_scaled_font_destroy(cairoScaledFont);
  FcPatternDestroy(pattern);

  // Only apply variations if we have an explicitly cloned face. Otherwise,
  // if the pattern holds the pathname, Cairo will handle setting of variations.
  if (varFace) {
    ApplyVariationsToFace(aVariations, aNumVariations, varFace);
  }

  return scaledFont.forget();
}

already_AddRefed<ScaledFont> UnscaledFontFontconfig::CreateScaledFontFromWRFont(
    Float aGlyphSize, const wr::FontInstanceOptions* aOptions,
    const wr::FontInstancePlatformOptions* aPlatformOptions,
    const FontVariation* aVariations, uint32_t aNumVariations) {
  ScaledFontFontconfig::InstanceData instanceData(aOptions, aPlatformOptions);
  return CreateScaledFont(aGlyphSize, reinterpret_cast<uint8_t*>(&instanceData),
                          sizeof(instanceData), aVariations, aNumVariations);
}

bool ScaledFontFontconfig::HasVariationSettings() {
  // Check if the FT face has been cloned.
  FT_Face face = nullptr;
  return FcPatternGetFTFace(mPattern, FC_FT_FACE, 0, &face) == FcResultMatch &&
         face && face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS &&
         face != static_cast<UnscaledFontFontconfig*>(mUnscaledFont.get())
                     ->GetFace();
}

already_AddRefed<UnscaledFont> UnscaledFontFontconfig::CreateFromFontDescriptor(
    const uint8_t* aData, uint32_t aDataLength, uint32_t aIndex) {
  if (aDataLength == 0) {
    gfxWarning() << "Fontconfig font descriptor is truncated.";
    return nullptr;
  }
  const char* path = reinterpret_cast<const char*>(aData);
  RefPtr<UnscaledFont> unscaledFont =
      new UnscaledFontFontconfig(std::string(path, aDataLength), aIndex);
  return unscaledFont.forget();
}

}  // namespace gfx
}  // namespace mozilla
