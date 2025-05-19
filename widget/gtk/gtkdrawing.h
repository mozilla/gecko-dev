/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * gtkdrawing.h: GTK widget rendering utilities
 *
 * gtkdrawing provides an API for rendering GTK widgets in the
 * current theme to a pixmap or window, without requiring an actual
 * widget instantiation, similar to the Macintosh Appearance Manager
 * or Windows XP's DrawThemeBackground() API.
 */

#ifndef _GTK_DRAWING_H_
#define _GTK_DRAWING_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <algorithm>
#include "mozilla/Span.h"

/*** type definitions ***/
typedef struct {
  guint8 active;
  guint8 focused;
  guint8 selected;
  guint8 inHover;
  guint8 disabled;
  guint8 isDefault;
  guint8 canDefault;
  /* The depressed state is for buttons which remain active for a longer period:
   * activated toggle buttons or buttons showing a popup menu. */
  guint8 depressed;
  guint8 backdrop;
  gint32 curpos; /* curpos and maxpos are used for scrollbars */
  gint32 maxpos;
  gint32 image_scale; /* image scale */
} GtkWidgetState;

/**
 * A size in the same GTK pixel units as GtkBorder and GdkRectangle.
 */
struct MozGtkSize {
  gint width;
  gint height;

  MozGtkSize& operator+=(const GtkBorder& aBorder) {
    width += aBorder.left + aBorder.right;
    height += aBorder.top + aBorder.bottom;
    return *this;
  }
  MozGtkSize operator+(const GtkBorder& aBorder) const {
    MozGtkSize result = *this;
    return result += aBorder;
  }
  bool operator<(const MozGtkSize& aOther) const {
    return (width < aOther.width && height <= aOther.height) ||
           (width <= aOther.width && height < aOther.height);
  }
  void Include(MozGtkSize aOther) {
    width = std::max(width, aOther.width);
    height = std::max(height, aOther.height);
  }
  void Rotate() {
    gint tmp = width;
    width = height;
    height = tmp;
  }
};

#define TOOLBAR_BUTTONS 3
struct ToolbarGTKMetrics {
  bool initialized = false;
  gint inlineSpacing = 0;
};

struct CSDWindowDecorationSize {
  bool initialized;
  GtkBorder decorationSize;
};

/*** result/error codes ***/
#define MOZ_GTK_SUCCESS 0
#define MOZ_GTK_UNKNOWN_WIDGET -1
#define MOZ_GTK_UNSAFE_THEME -2

/*** checkbox/radio flags ***/
#define MOZ_GTK_WIDGET_CHECKED 1
#define MOZ_GTK_WIDGET_INCONSISTENT (1 << 1)

/*** widget type constants ***/
enum WidgetNodeType : int {
  /* Paints a GtkButton. flags is a GtkReliefStyle. */
  MOZ_GTK_BUTTON,

  /* Vertical GtkScrollbar counterparts */
  MOZ_GTK_SCROLLBAR_VERTICAL,
  MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL,
  MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL,
  MOZ_GTK_SCROLLBAR_THUMB_VERTICAL,

  /* Paints a GtkTextView or gets the style context corresponding to the
     root node of a GtkTextView. */
  MOZ_GTK_TEXT_VIEW,
  /* The "text" window or node of a GtkTextView */
  MOZ_GTK_TEXT_VIEW_TEXT,
  /* The "selection" node of a GtkTextView.text */
  MOZ_GTK_TEXT_VIEW_TEXT_SELECTION,

  /* Paints a GtkToolTip */
  MOZ_GTK_TOOLTIP,
  /* Paints a GtkBox from GtkToolTip  */
  MOZ_GTK_TOOLTIP_BOX,
  /* Paints a GtkLabel of GtkToolTip */
  MOZ_GTK_TOOLTIP_BOX_LABEL,
  /* Paints a GtkFrame (e.g. a status bar panel). */
  MOZ_GTK_FRAME,
  /* Paints the border of a GtkFrame */
  MOZ_GTK_FRAME_BORDER,
  /* Paints the expander and border of a GtkTreeView */
  MOZ_GTK_TREEVIEW,
  /* Paints treeheader cells */
  MOZ_GTK_TREE_HEADER_CELL,
  /* Paints the background of menus, context menus. */
  MOZ_GTK_MENUPOPUP,
  /* Menubar for -moz-headerbar colors */
  MOZ_GTK_MENUBAR,
  /* Paints items of popup menus. */
  MOZ_GTK_MENUITEM,
  /* Menubar menuitem for foreground colors. */
  MOZ_GTK_MENUBARITEM,
  /* Paints the background of a window, dialog or page. */
  MOZ_GTK_WINDOW,
  /* Used only as a container for MOZ_GTK_HEADER_BAR. */
  MOZ_GTK_HEADERBAR_WINDOW,
  /* Used only as a container for MOZ_GTK_HEADER_BAR. */
  MOZ_GTK_HEADERBAR_FIXED,
  /* Window container for all widgets */
  MOZ_GTK_WINDOW_CONTAINER,
  /* Used for scrolled window shell. */
  MOZ_GTK_SCROLLED_WINDOW,
  /* Paints a GtkHeaderBar */
  MOZ_GTK_HEADER_BAR,

  /* Client-side window decoration node. Available on GTK 3.20+. */
  MOZ_GTK_WINDOW_DECORATION,
  MOZ_GTK_WINDOW_DECORATION_SOLID,

  MOZ_GTK_WIDGET_NODE_COUNT
};

/* ButtonLayout represents a GTK CSD button and whether its on the left or
 * right side of the tab bar */
struct ButtonLayout {
  enum class Type { Close, Minimize, Maximize };
  Type mType;
};

/*** General library functions ***/
/**
 * Initializes the drawing library.  You must call this function
 * prior to using any other functionality.
 * returns: MOZ_GTK_SUCCESS if there were no errors
 *          MOZ_GTK_UNSAFE_THEME if the current theme engine is known
 *                               to crash with gtkdrawing.
 */
gint moz_gtk_init();

/**
 * Updates the drawing library when the theme changes.
 */
void moz_gtk_refresh();

/**
 * Perform cleanup of the drawing library. You should call this function
 * when your program exits, or you no longer need the library.
 *
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_shutdown();

/*** Widget drawing ***/
/**
 * Paint a widget in the current theme.
 * widget:    a constant giving the widget to paint
 * drawable:  the drawable to paint to;
 *            it's colormap must be moz_gtk_widget_get_colormap().
 * rect:      the bounding rectangle for the widget
 * state:     the state of the widget.  ignored for some widgets.
 * flags:     widget-dependant flags; see the WidgetNodeType definition.
 * direction: the text direction, to draw the widget correctly LTR and RTL.
 */
gint moz_gtk_widget_paint(WidgetNodeType widget, cairo_t* cr,
                          GdkRectangle* rect, GtkWidgetState* state, gint flags,
                          GtkTextDirection direction);

/*** Widget metrics ***/
/**
 * Get the border size of a widget
 * left/right:  [OUT] the widget's left/right border
 * top/bottom:  [OUT] the widget's top/bottom border
 * direction:   the text direction for the widget.  Callers depend on this
 *              being used only for MOZ_GTK_DROPDOWN widgets, and cache
 *              results for other widget types across direction values.
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_widget_border(WidgetNodeType widget, gint* left, gint* top,
                               gint* right, gint* bottom,
                               GtkTextDirection direction);

gint moz_gtk_get_titlebar_button_spacing();

/**
 * Get toolbar button layout.
 * aButtonLayout:  [OUT] An array which will be filled by ButtonLayout
 *                       references to visible titlebar buttons. Must contain at
 *                       least TOOLBAR_BUTTONS entries if non-empty.
 * aReversedButtonsPlacement: [OUT] True if the buttons are placed in opposite
 *                                  titlebar corner.
 *
 * returns:    Number of returned entries at aButtonLayout.
 */
size_t GetGtkHeaderBarButtonLayout(mozilla::Span<ButtonLayout>,
                                   bool* aReversedButtonsPlacement);

#endif
