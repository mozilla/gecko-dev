/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GtkWidgets_h
#define mozilla_widget_GtkWidgets_h

#include <gtk/gtk.h>
#include <cstdint>

namespace mozilla::widget::GtkWidgets {

enum class Type : uint32_t {
  // GtkButton
  Button = 0,

  // Vertical GtkScrollbar
  Scrollbar,
  ScrollbarContents,
  ScrollbarTrough,
  ScrollbarThumb,

  // GtkTextView
  TextView,
  // The "text" window or node of a GtkTextView
  TextViewText,
  // The "selection" node of a GtkTextView.text
  TextViewTextSelection,

  // GtkToolTip
  Tooltip,
  // GtkBox from GtkToolTip
  TooltipBox,
  // GtkLabel of GtkToolTip
  TooltipBoxLabel,
  // GtkFrame (e.g. a status bar panel)
  Frame,
  // Border of a GtkFrame
  FrameBorder,
  // Expander and border of a GtkTreeView
  TreeView,
  // Paints treeheader cells
  TreeHeaderCell,
  // Background of menus, context menus.
  Menupopup,
  // Menubar for -moz-headerbar colors
  Menubar,
  // Items of popup menus
  Menuitem,
  // Menubar menuitem for foreground colors
  MenubarItem,
  // Background of a window, dialog or page.
  Window,
  // Used only as a container for HeaderBar.
  HeaderBarFixed,
  // Window container for all widgets
  WindowContainer,
  // Used for scrolled window shell.
  ScrolledWindow,
  // GtkHeaderBar
  HeaderBar,
  // Client-side window decoration node. Available on GTK 3.20+
  WindowDecoration,

  Last = WindowDecoration,
};

static constexpr size_t kTypeCount = size_t(Type::Last) + 1;

void Refresh();
void Shutdown();

// Gets a non-owning pointer to a given widget.
GtkWidget* Get(Type);

struct DrawingParams {
  // widget to paint
  Type widget;
  // bounding rectangle for the widget
  GdkRectangle rect{};
  GtkStateFlags state = GTK_STATE_FLAG_NORMAL;
  gint image_scale = 1;
};
// Paint a widget in the current theme.
void Draw(cairo_t* cr, const DrawingParams*);

/**
 * Returns a pointer to a style context for the specified node and state.
 * aStateFlags is applied only to last widget in css style path,
 * for instance GetStyleContext(MOZ_GTK_BUTTON, .., GTK_STATE_FLAG_HOVER)
 * you get "window button:hover" css selector.
 *
 * The context is static. Do not unref.
 */
GtkStyleContext* GetStyle(Type, int aScale = 1,
                          GtkStateFlags aStateFlags = GTK_STATE_FLAG_NORMAL);

/**
 * Return a new style context based on aWidget, as a child of aParentStyle.
 * If aWidget still has a floating reference, then it is sunk and released.
 */
GtkStyleContext* CreateStyleForWidget(GtkWidget* aWidget,
                                      GtkStyleContext* aParentStyle);

}  // namespace mozilla::widget::GtkWidgets

#endif
