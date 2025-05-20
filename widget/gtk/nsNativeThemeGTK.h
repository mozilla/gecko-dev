/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GTK_NSNATIVETHEMEGTK_H_
#define _GTK_NSNATIVETHEMEGTK_H_

#include "Theme.h"

class nsNativeThemeGTK final : public mozilla::widget::Theme {
  using Theme = mozilla::widget::Theme;

 public:
  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext*, nsIFrame*, StyleAppearance,
                                  const nsRect& aRect, const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  bool CreateWebRenderCommandsForWidget(
      mozilla::wr::DisplayListBuilder&, mozilla::wr::IpcResourceUpdateQueue&,
      const mozilla::layers::StackingContextHelper&,
      mozilla::layers::RenderRootStateManager*, nsIFrame*, StyleAppearance,
      const nsRect&) override;

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext*,
                                                      nsIFrame*,
                                                      StyleAppearance) override;

  bool GetWidgetPadding(nsDeviceContext*, nsIFrame*, StyleAppearance,
                        LayoutDeviceIntMargin*) override;
  bool GetWidgetOverflow(nsDeviceContext*, nsIFrame*, StyleAppearance,
                         nsRect*) override;

  // Whether we draw a non-native widget.
  //
  // We always draw scrollbars as non-native so that all of Firefox has
  // consistent scrollbar styles both in chrome and content (plus, the
  // non-native scrollbars support scrollbar-width, auto-darkening...).
  //
  // We draw other widgets as non-native when their color-scheme doesn't match
  // the current GTK theme's color-scheme. We do that because frequently
  // switching GTK themes at runtime is prohibitively expensive. In that case
  // (`BecauseColorMismatch`) we don't call into the non-native theme for sizing
  // information (GetWidgetPadding/Border and GetMinimumWidgetSize), to avoid
  // subtle sizing changes. The non-native theme can basically draw at any size,
  // so we prefer to have consistent sizing information.
  enum class NonNative { No, Always, BecauseColorMismatch };
  static bool IsWidgetAlwaysNonNative(nsIFrame*, StyleAppearance);
  NonNative IsWidgetNonNative(nsIFrame*, StyleAppearance);

  mozilla::LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*, nsIFrame*,
                                                    StyleAppearance) override;

  bool WidgetAttributeChangeRequiresRepaint(StyleAppearance,
                                            nsAtom* aAttribute) override;
  NS_IMETHOD ThemeChanged() override;

  bool ThemeSupportsWidget(nsPresContext*, nsIFrame*, StyleAppearance) override;
  bool WidgetIsContainer(StyleAppearance) override;
  bool ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) override;
  Transparency GetWidgetTransparency(nsIFrame*, StyleAppearance) override;

  nsNativeThemeGTK();

 protected:
  virtual ~nsNativeThemeGTK();
};

#endif
