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

void moz_gtk_init() { moz_gtk_refresh(); }

void moz_gtk_refresh() {
  /* This will destroy all of our widgets */
  ResetWidgetCache();
}

static void moz_gtk_window_decoration_paint(cairo_t* cr,
                                            const GtkDrawingParams& aParams) {
  if (mozilla::widget::GdkIsWaylandDisplay()) {
    // Doesn't seem to be needed.
    return;
  }
  GtkStyleContext* decorationStyle = GetStyleContext(
      MOZ_GTK_WINDOW_DECORATION, aParams.image_scale, aParams.state);

  const auto& rect = aParams.rect;
  gtk_render_background(decorationStyle, cr, rect.x, rect.y, rect.width,
                        rect.height);
  gtk_render_frame(decorationStyle, cr, rect.x, rect.y, rect.width,
                   rect.height);
}

/* cairo_t *cr argument has to be a system-cairo. */
void moz_gtk_widget_paint(cairo_t* cr, const GtkDrawingParams* aParams) {
  /* A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=694086 */
  cairo_new_path(cr);
  switch (aParams->widget) {
    case MOZ_GTK_WINDOW_DECORATION:
      return moz_gtk_window_decoration_paint(cr, *aParams);
    default:
      g_warning("Unknown widget type: %d", aParams->widget);
      return;
  }
}

void moz_gtk_shutdown() {
  /* This will destroy all of our widgets */
  ResetWidgetCache();
}
