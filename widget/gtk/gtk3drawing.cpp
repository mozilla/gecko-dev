/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains painting functions for each of the gtk2 widgets.
 * Adapted from the gtkdrawing.c, and gtk+2.0 source.
 */

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <string.h>
#include "gdk/gdk.h"
#include "gtkdrawing.h"
#include "mozilla/Assertions.h"
#include "mozilla/ScopeExit.h"
#include "prinrval.h"
#include "WidgetStyleCache.h"
#include "nsString.h"
#include "nsDebug.h"
#include "WidgetUtilsGtk.h"

#include <math.h>
#include <dlfcn.h>

static ToolbarGTKMetrics sToolbarMetrics;

using mozilla::Span;

#if 0
// It's used for debugging only to compare Gecko widget style with
// the ones used by Gtk+ applications.
static void
style_path_print(GtkStyleContext *context)
{
    const GtkWidgetPath* path = gtk_style_context_get_path(context);

    static auto sGtkWidgetPathToStringPtr =
        (char * (*)(const GtkWidgetPath *))
        dlsym(RTLD_DEFAULT, "gtk_widget_path_to_string");

    fprintf(stderr, "Style path:\n%s\n\n", sGtkWidgetPathToStringPtr(path));
}
#endif

// GetStateFlagsFromGtkWidgetState() can be safely used for the specific
// GtkWidgets that set both prelight and active flags.  For other widgets,
// either the GtkStateFlags or Gecko's GtkWidgetState need to be carefully
// adjusted to match GTK behavior.  Although GTK sets insensitive and focus
// flags in the generic GtkWidget base class, GTK adds prelight and active
// flags only to widgets that are expected to demonstrate prelight or active
// states.  This contrasts with HTML where any element may have :active and
// :hover states, and so Gecko's GtkStateFlags do not necessarily map to GTK
// flags.  Failure to restrict the flags in the same way as GTK can cause
// generic CSS selectors from some themes to unintentionally match elements
// that are not expected to change appearance on hover or mouse-down.
static GtkStateFlags GetStateFlagsFromGtkWidgetState(GtkWidgetState* state) {
  GtkStateFlags stateFlags = GTK_STATE_FLAG_NORMAL;

  if (state->disabled)
    stateFlags = GTK_STATE_FLAG_INSENSITIVE;
  else {
    if (state->depressed || state->active)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_ACTIVE);
    if (state->inHover)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_PRELIGHT);
    if (state->focused)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_FOCUSED);
    if (state->backdrop)
      stateFlags =
          static_cast<GtkStateFlags>(stateFlags | GTK_STATE_FLAG_BACKDROP);
  }

  return stateFlags;
}

gint moz_gtk_init() {
  moz_gtk_refresh();

  return MOZ_GTK_SUCCESS;
}

void moz_gtk_refresh() {
  sToolbarMetrics.initialized = false;

  /* This will destroy all of our widgets */
  ResetWidgetCache();
}

size_t GetGtkHeaderBarButtonLayout(Span<ButtonLayout> aButtonLayout,
                                   bool* aReversedButtonsPlacement) {
  gchar* decorationLayoutSetting = nullptr;
  GtkSettings* settings = gtk_settings_get_default();
  g_object_get(settings, "gtk-decoration-layout", &decorationLayoutSetting,
               nullptr);
  auto free = mozilla::MakeScopeExit([&] { g_free(decorationLayoutSetting); });

  // Use a default layout
  const gchar* decorationLayout = "menu:minimize,maximize,close";
  if (decorationLayoutSetting) {
    decorationLayout = decorationLayoutSetting;
  }

  // "minimize,maximize,close:" layout means buttons are on the opposite
  // titlebar side. close button is always there.
  if (aReversedButtonsPlacement) {
    const char* closeButton = strstr(decorationLayout, "close");
    const char* separator = strchr(decorationLayout, ':');
    *aReversedButtonsPlacement =
        closeButton && separator && closeButton < separator;
  }

  // We check what position a button string is stored in decorationLayout.
  //
  // decorationLayout gets its value from the GNOME preference:
  // org.gnome.desktop.vm.preferences.button-layout via the
  // gtk-decoration-layout property.
  //
  // Documentation of the gtk-decoration-layout property can be found here:
  // https://developer.gnome.org/gtk3/stable/GtkSettings.html#GtkSettings--gtk-decoration-layout
  if (aButtonLayout.IsEmpty()) {
    return 0;
  }

  nsDependentCSubstring layout(decorationLayout, strlen(decorationLayout));

  size_t activeButtons = 0;
  for (const auto& part : layout.Split(':')) {
    for (const auto& button : part.Split(',')) {
      if (button.EqualsLiteral("close")) {
        aButtonLayout[activeButtons++] = {ButtonLayout::Type::Close};
      } else if (button.EqualsLiteral("minimize")) {
        aButtonLayout[activeButtons++] = {ButtonLayout::Type::Minimize};
      } else if (button.EqualsLiteral("maximize")) {
        aButtonLayout[activeButtons++] = {ButtonLayout::Type::Maximize};
      }
      if (activeButtons == aButtonLayout.Length()) {
        return activeButtons;
      }
    }
  }
  return activeButtons;
}

static void EnsureToolbarMetrics() {
  if (sToolbarMetrics.initialized) {
    return;
  }
  sToolbarMetrics = {};

  // Account for the spacing property in the header bar.
  // Default to 6 pixels (gtk/gtkheaderbar.c)
  gint spacing = 6;
  g_object_get(GetWidget(MOZ_GTK_HEADER_BAR), "spacing", &spacing, nullptr);
  sToolbarMetrics.inlineSpacing += spacing;
  sToolbarMetrics.initialized = true;
}

gint moz_gtk_get_titlebar_button_spacing() {
  EnsureToolbarMetrics();
  return sToolbarMetrics.inlineSpacing;
}

static gint moz_gtk_window_decoration_paint(cairo_t* cr,
                                            const GdkRectangle* rect,
                                            GtkWidgetState* state,
                                            GtkTextDirection direction) {
  if (mozilla::widget::GdkIsWaylandDisplay()) {
    // Doesn't seem to be needed.
    return MOZ_GTK_SUCCESS;
  }
  GtkStateFlags state_flags = GetStateFlagsFromGtkWidgetState(state);
  GtkStyleContext* windowStyle =
      GetStyleContext(MOZ_GTK_HEADERBAR_WINDOW, state->image_scale);
  const bool solidDecorations =
      gtk_style_context_has_class(windowStyle, "solid-csd");
  GtkStyleContext* decorationStyle =
      GetStyleContext(solidDecorations ? MOZ_GTK_WINDOW_DECORATION_SOLID
                                       : MOZ_GTK_WINDOW_DECORATION,
                      state->image_scale, GTK_TEXT_DIR_LTR, state_flags);

  gtk_render_background(decorationStyle, cr, rect->x, rect->y, rect->width,
                        rect->height);
  gtk_render_frame(decorationStyle, cr, rect->x, rect->y, rect->width,
                   rect->height);
  return MOZ_GTK_SUCCESS;
}

gint moz_gtk_get_widget_border(WidgetNodeType widget, gint* left, gint* top,
                               gint* right, gint* bottom,
                               // NOTE: callers depend on direction being used
                               // only for MOZ_GTK_DROPDOWN widgets.
                               GtkTextDirection direction) {
  *left = *top = *right = *bottom = 0;
  return MOZ_GTK_SUCCESS;
}

/* cairo_t *cr argument has to be a system-cairo. */
gint moz_gtk_widget_paint(WidgetNodeType widget, cairo_t* cr,
                          GdkRectangle* rect, GtkWidgetState* state, gint flags,
                          GtkTextDirection direction) {
  /* A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=694086
   */
  cairo_new_path(cr);

  switch (widget) {
    case MOZ_GTK_WINDOW_DECORATION:
      return moz_gtk_window_decoration_paint(cr, rect, state, direction);
    default:
      g_warning("Unknown widget type: %d", widget);
  }

  return MOZ_GTK_UNKNOWN_WIDGET;
}

gint moz_gtk_shutdown() {
  /* This will destroy all of our widgets */
  ResetWidgetCache();

  return MOZ_GTK_SUCCESS;
}
