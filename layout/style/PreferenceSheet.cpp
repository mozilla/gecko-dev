/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreferenceSheet.h"

#include "ServoCSSParser.h"
#include "MainThreadUtils.h"
#include "mozilla/Encoding.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/glean/AccessibleMetrics.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

#define AVG2(a, b) (((a) + (b) + 1) >> 1)

namespace mozilla {

using dom::Document;

bool PreferenceSheet::sInitialized;
PreferenceSheet::Prefs PreferenceSheet::sContentPrefs;
PreferenceSheet::Prefs PreferenceSheet::sChromePrefs;
PreferenceSheet::Prefs PreferenceSheet::sPrintPrefs;

static void GetColor(const char* aPrefName, ColorScheme aColorScheme,
                     nscolor& aColor) {
  nsAutoCString darkPrefName;
  if (aColorScheme == ColorScheme::Dark) {
    darkPrefName.Append(aPrefName);
    darkPrefName.AppendLiteral(".dark");
    aPrefName = darkPrefName.get();
  }

  nsAutoCString value;
  Preferences::GetCString(aPrefName, value);
  if (value.IsEmpty() || Encoding::UTF8ValidUpTo(value) != value.Length()) {
    return;
  }
  nscolor result;
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), value, &result)) {
    return;
  }
  aColor = result;
}

auto PreferenceSheet::PrefsKindFor(const Document& aDoc) -> PrefsKind {
  if (aDoc.IsInChromeDocShell()) {
    return PrefsKind::Chrome;
  }

  if (aDoc.IsBeingUsedAsImage() && aDoc.ChromeRulesEnabled()) {
    return PrefsKind::Chrome;
  }

  if (aDoc.IsStaticDocument()) {
    return PrefsKind::Print;
  }

  return PrefsKind::Content;
}

static bool UseStandinsForNativeColors() {
  return nsContentUtils::ShouldResistFingerprinting(
             "we want to have consistent colors across the browser if RFP is "
             "enabled, so we check the global preference"
             "not excluding chrome browsers or webpages, so we call the legacy "
             "RFP function to prevent that",
             RFPTarget::UseStandinsForNativeColors) ||
         StaticPrefs::ui_use_standins_for_native_colors();
}

void PreferenceSheet::Prefs::LoadColors(bool aIsLight) {
  auto& colors = aIsLight ? mLightColors : mDarkColors;

  if (!aIsLight) {
    // Initialize the dark-color-scheme foreground/background colors as being
    // the reverse of these members' default values, for ~reasonable fallback if
    // the user configures broken pref values.
    std::swap(colors.mDefault, colors.mDefaultBackground);
  }

  const auto scheme = aIsLight ? ColorScheme::Light : ColorScheme::Dark;
  using ColorID = LookAndFeel::ColorID;

  if (!mIsChrome && (mUseDocumentColors || mUseStandins)) {
    // Tab content not in HCM, or we need to use standins.
    auto GetStandinColor = [&scheme](ColorID aColorID, nscolor& aColor) {
      aColor = LookAndFeel::Color(aColorID, scheme,
                                  LookAndFeel::UseStandins::Yes, aColor);
    };

    GetStandinColor(ColorID::Windowtext, colors.mDefault);
    GetStandinColor(ColorID::Window, colors.mDefaultBackground);
    GetStandinColor(ColorID::Linktext, colors.mLink);
    GetStandinColor(ColorID::Visitedtext, colors.mVisitedLink);
    GetStandinColor(ColorID::Activetext, colors.mActiveLink);
  } else if (!mIsChrome && mUsePrefColors) {
    // Tab content with explicit browser HCM, use our prefs for colors.
    GetColor("browser.display.background_color", scheme,
             colors.mDefaultBackground);
    GetColor("browser.display.foreground_color", scheme, colors.mDefault);
    GetColor("browser.anchor_color", scheme, colors.mLink);
    GetColor("browser.active_color", scheme, colors.mActiveLink);
    GetColor("browser.visited_color", scheme, colors.mVisitedLink);
  } else {
    // Browser UI or OS HCM, use system colors.
    auto GetSystemColor = [&scheme](ColorID aColorID, nscolor& aColor) {
      aColor = LookAndFeel::Color(aColorID, scheme,
                                  LookAndFeel::UseStandins::No, aColor);
    };

    GetSystemColor(ColorID::Windowtext, colors.mDefault);
    GetSystemColor(ColorID::Window, colors.mDefaultBackground);
    GetSystemColor(ColorID::Linktext, colors.mLink);
    GetSystemColor(ColorID::Visitedtext, colors.mVisitedLink);
    GetSystemColor(ColorID::Activetext, colors.mActiveLink);
  }

  // Wherever we got the default background color from, ensure it is opaque.
  colors.mDefaultBackground =
      NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF), colors.mDefaultBackground);
}

bool PreferenceSheet::Prefs::NonNativeThemeShouldBeHighContrast() const {
  // We only do that if we are overriding the document colors. Otherwise it
  // causes issues when pages only override some of the system colors,
  // specially in dark themes mode.
  return StaticPrefs::widget_non_native_theme_always_high_contrast() ||
         !mUseDocumentColors;
}

auto PreferenceSheet::ColorSchemeSettingForChrome()
    -> ChromeColorSchemeSetting {
  switch (StaticPrefs::browser_theme_toolbar_theme()) {
    case 0:  // Dark
      return ChromeColorSchemeSetting::Dark;
    case 1:  // Light
      return ChromeColorSchemeSetting::Light;
    default:
      return ChromeColorSchemeSetting::System;
  }
}

ColorScheme PreferenceSheet::ThemeDerivedColorSchemeForContent() {
  switch (StaticPrefs::browser_theme_content_theme()) {
    case 0:  // Dark
      return ColorScheme::Dark;
    case 1:  // Light
      return ColorScheme::Light;
    default:
      return LookAndFeel::SystemColorScheme();
  }
}

void PreferenceSheet::Prefs::Load(bool aIsChrome) {
  *this = {};

  mIsChrome = aIsChrome;
  mUseAccessibilityTheme =
      LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme);
  // Chrome documents always use system colors, not stand-ins, not forced, etc.
  if (!aIsChrome) {
    switch (StaticPrefs::browser_display_document_color_use()) {
      case 1:
        // Never High Contrast
        mUsePrefColors = false;
        mUseDocumentColors = true;
        break;
      case 2:
        // Always High Contrast
        mUsePrefColors = true;
        mUseDocumentColors = false;
        break;
      default:
        // Only with OS HCM
        mUsePrefColors = false;
        mUseDocumentColors = !mUseAccessibilityTheme;
        break;
    }
    mUseStandins = UseStandinsForNativeColors();
  }

  LoadColors(true);
  LoadColors(false);

  // When forcing the pref colors, we need to forcibly use the light color-set,
  // as those are the colors exposed to the user in the colors dialog.
  mMustUseLightColorSet = mUsePrefColors && !mUseDocumentColors;
#ifdef XP_WIN
  if (mUseAccessibilityTheme && (mIsChrome || !mUseDocumentColors)) {
    // Windows overrides the light colors with the HCM colors when HCM is
    // active, so make sure to always use the light system colors in that case,
    // and also make sure that we always use the light color set for the same
    // reason.
    mMustUseLightSystemColors = mMustUseLightColorSet = true;
  }
#endif

  mColorScheme = [&] {
    if (aIsChrome) {
      switch (ColorSchemeSettingForChrome()) {
        case ChromeColorSchemeSetting::Light:
          return ColorScheme::Light;
        case ChromeColorSchemeSetting::Dark:
          return ColorScheme::Dark;
        case ChromeColorSchemeSetting::System:
          break;
      }
      return LookAndFeel::SystemColorScheme();
    }
    if (mMustUseLightColorSet) {
      // When forcing colors in a way such as color-scheme isn't respected, we
      // compute a preference based on the darkness of
      // our background.
      return LookAndFeel::IsDarkColor(mLightColors.mDefaultBackground)
                 ? ColorScheme::Dark
                 : ColorScheme::Light;
    }
    switch (StaticPrefs::layout_css_prefers_color_scheme_content_override()) {
      case 0:
        return ColorScheme::Dark;
      case 1:
        return ColorScheme::Light;
      default:
        return ThemeDerivedColorSchemeForContent();
    }
  }();
}

void PreferenceSheet::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInitialized);

  sInitialized = true;

  sContentPrefs.Load(false);
  sChromePrefs.Load(true);
  sPrintPrefs = sContentPrefs;
  {
    // For printing, we always use a preferred-light color scheme.
    sPrintPrefs.mColorScheme = ColorScheme::Light;
    if (!sPrintPrefs.mUseDocumentColors) {
      // When overriding document colors, we ignore the `color-scheme` property,
      // but we still don't want to use the system colors (which might be dark,
      // despite having made it into mLightColors), because it both wastes ink
      // and it might interact poorly with the color adjustments we do while
      // printing.
      //
      // So we override the light colors with our hardcoded default colors, and
      // force the use of stand-ins.
      sPrintPrefs.mLightColors = Prefs().mLightColors;
      sPrintPrefs.mUseStandins = true;
    }
  }

  // Telemetry for these preferences is only collected on the parent process.
  if (!XRE_IsParentProcess()) {
    return;
  }

  glean::a11y::ThemeLabel gleanLabel;
  switch (StaticPrefs::browser_display_document_color_use()) {
    case 1:
      gleanLabel = glean::a11y::ThemeLabel::eAlways;
      break;
    case 2:
      gleanLabel = glean::a11y::ThemeLabel::eNever;
      break;
    default:
      gleanLabel = glean::a11y::ThemeLabel::eDefault;
      break;
  }

  glean::a11y::theme.EnumGet(gleanLabel)
      .Set(sContentPrefs.mUseAccessibilityTheme);
  if (!sContentPrefs.mUseDocumentColors) {
    // If a user has chosen to override doc colors through OS HCM or our HCM,
    // we should log the user's current foreground (text) color and background
    // color. Note, the document color use pref is the inverse of the HCM
    // dropdown option in preferences.
    //
    // Note that we only look at light colors because that's the color set we
    // use when forcing colors (since color-scheme is ignored when colors are
    // forced).
    //
    // The light color set is the one that potentially contains the Windows HCM
    // theme color/background (if we're using system colors and the user is
    // using a High Contrast theme), and also the colors that as of today we
    // allow setting in about:preferences.
    glean::a11y::hcm_foreground.Set(sContentPrefs.mLightColors.mDefault);
    glean::a11y::hcm_background.Set(
        sContentPrefs.mLightColors.mDefaultBackground);
  }

  glean::a11y::backplate.Set(StaticPrefs::browser_display_permit_backplate());
  glean::a11y::always_underline_links.Set(
      StaticPrefs::layout_css_always_underline_links());
}

bool PreferenceSheet::AffectedByPref(const nsACString& aPref) {
  const char* prefNames[] = {
      StaticPrefs::GetPrefName_privacy_resistFingerprinting(),
      StaticPrefs::GetPrefName_ui_use_standins_for_native_colors(),
      "browser.anchor_color",
      "browser.active_color",
      "browser.visited_color",
  };

  if (StringBeginsWith(aPref, "browser.display."_ns)) {
    return true;
  }

  for (const char* pref : prefNames) {
    if (aPref.Equals(pref)) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla

#undef AVG2
