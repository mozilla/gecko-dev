/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <gtk/gtk.h>
#include "WidgetStyleCache.h"
#include "gtkdrawing.h"
#include "mozilla/Assertions.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsString.h"

#define STATE_FLAG_DIR_LTR (1U << 7)
#define STATE_FLAG_DIR_RTL (1U << 8)
static_assert(GTK_STATE_FLAG_DIR_LTR == STATE_FLAG_DIR_LTR &&
                  GTK_STATE_FLAG_DIR_RTL == STATE_FLAG_DIR_RTL,
              "incorrect direction state flags");

enum class CSDStyle {
  Unknown,
  Solid,
  Normal,
};

static bool gHeaderBarShouldDrawContainer = false;
static CSDStyle gCSDStyle = CSDStyle::Unknown;
static GtkWidget* sWidgetStorage[MOZ_GTK_WIDGET_NODE_COUNT];
static GtkStyleContext* sStyleStorage[MOZ_GTK_WIDGET_NODE_COUNT];

static GtkStyleContext* GetWidgetRootStyle(WidgetNodeType aNodeType);
static GtkStyleContext* GetCssNodeStyleInternal(WidgetNodeType aNodeType);

static GtkWidget* CreateWindowContainerWidget() {
  GtkWidget* widget = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_WINDOW)), widget);
  return widget;
}

static void AddToWindowContainer(GtkWidget* widget) {
  gtk_container_add(GTK_CONTAINER(GetWidget(MOZ_GTK_WINDOW_CONTAINER)), widget);
}

static GtkWidget* CreateScrollbarWidget(WidgetNodeType aAppearance,
                                        GtkOrientation aOrientation) {
  GtkWidget* widget = gtk_scrollbar_new(aOrientation, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateMenuPopupWidget() {
  GtkWidget* widget = gtk_menu_new();
  GtkStyleContext* style = gtk_widget_get_style_context(widget);
  gtk_style_context_add_class(style, GTK_STYLE_CLASS_POPUP);
  gtk_menu_attach_to_widget(GTK_MENU(widget), GetWidget(MOZ_GTK_WINDOW),
                            nullptr);
  return widget;
}

static GtkWidget* CreateMenuBarWidget() {
  GtkWidget* widget = gtk_menu_bar_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateFrameWidget() {
  GtkWidget* widget = gtk_frame_new(nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateButtonWidget() {
  GtkWidget* widget = gtk_button_new_with_label("M");
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateScrolledWindowWidget() {
  GtkWidget* widget = gtk_scrolled_window_new(nullptr, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateTreeViewWidget() {
  GtkWidget* widget = gtk_tree_view_new();
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateTreeHeaderCellWidget() {
  /*
   * Some GTK engines paint the first and last cell
   * of a TreeView header with a highlight.
   * Since we do not know where our widget will be relative
   * to the other buttons in the TreeView header, we must
   * paint it as a button that is between two others,
   * thus ensuring it is neither the first or last button
   * in the header.
   * GTK doesn't give us a way to do this explicitly,
   * so we must paint with a button that is between two
   * others.
   */
  GtkTreeViewColumn* firstTreeViewColumn;
  GtkTreeViewColumn* middleTreeViewColumn;
  GtkTreeViewColumn* lastTreeViewColumn;

  GtkWidget* treeView = GetWidget(MOZ_GTK_TREEVIEW);

  /* Create and append our three columns */
  firstTreeViewColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(firstTreeViewColumn, "M");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), firstTreeViewColumn);

  middleTreeViewColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(middleTreeViewColumn, "M");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), middleTreeViewColumn);

  lastTreeViewColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(lastTreeViewColumn, "M");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), lastTreeViewColumn);

  /* Use the middle column's header for our button */
  return gtk_tree_view_column_get_button(middleTreeViewColumn);
}

static bool HasBackground(GtkStyleContext* aStyle) {
  GdkRGBA gdkColor;
  gtk_style_context_get_background_color(aStyle, GTK_STATE_FLAG_NORMAL,
                                         &gdkColor);
  if (gdkColor.alpha != 0.0) {
    return true;
  }

  GValue value = G_VALUE_INIT;
  gtk_style_context_get_property(aStyle, "background-image",
                                 GTK_STATE_FLAG_NORMAL, &value);
  auto cleanup = mozilla::MakeScopeExit([&] { g_value_unset(&value); });
  return g_value_get_boxed(&value);
}

static void CreateWindowAndHeaderBar() {
  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(window, "MozillaGtkWidget");
  GtkStyleContext* windowStyle = gtk_widget_get_style_context(window);

  // Headerbar has to be placed to window with csd or solid-csd style
  // to properly draw the decorated.
  gtk_style_context_add_class(windowStyle,
                              IsSolidCSDStyleUsed() ? "solid-csd" : "csd");

  GtkWidget* fixed = gtk_fixed_new();
  GtkStyleContext* fixedStyle = gtk_widget_get_style_context(fixed);
  gtk_style_context_add_class(fixedStyle, "titlebar");

  GtkWidget* headerBar = gtk_header_bar_new();
  // From create_headerbar in gtkwindow.c
  g_object_set(headerBar, "title", "Title", "has-subtitle", FALSE,
               "show-close-button", TRUE, NULL);

  // Emulate what create_titlebar() at gtkwindow.c does.
  GtkStyleContext* headerBarStyle = gtk_widget_get_style_context(headerBar);
  gtk_style_context_add_class(headerBarStyle, GTK_STYLE_CLASS_TITLEBAR);

  // TODO: Define default-decoration titlebar style as workaround
  // to ensure the titlebar buttons does not overflow outside. Recently the
  // titlebar size is calculated as tab size + titlebar border/padding
  // (default-decoration has 6px padding at default Adwaita theme).
  // We need to fix titlebar size calculation to also include titlebar button
  // sizes. (Bug 1419442)
  gtk_style_context_add_class(headerBarStyle, "default-decoration");

  sWidgetStorage[MOZ_GTK_HEADER_BAR] = headerBar;
  MOZ_ASSERT(!sWidgetStorage[MOZ_GTK_HEADERBAR_WINDOW],
             "Window widget is already created!");
  MOZ_ASSERT(!sWidgetStorage[MOZ_GTK_HEADERBAR_FIXED],
             "Fixed widget is already created!");
  sWidgetStorage[MOZ_GTK_WINDOW] = window;
  sWidgetStorage[MOZ_GTK_HEADERBAR_FIXED] = fixed;

  gtk_container_add(GTK_CONTAINER(window), fixed);
  gtk_container_add(GTK_CONTAINER(fixed), headerBar);

  gtk_widget_show_all(headerBar);

  // Some themes like Elementary's style the container of the headerbar rather
  // than the header bar itself.
  gHeaderBarShouldDrawContainer = [&] {
    const bool headerBarHasBackground = HasBackground(headerBarStyle);
    if (headerBarHasBackground && GetBorderRadius(headerBarStyle)) {
      return false;
    }
    if (HasBackground(fixedStyle) &&
        (GetBorderRadius(fixedStyle) || !headerBarHasBackground)) {
      return true;
    }
    return false;
  }();
}

bool IsSolidCSDStyleUsed() {
  if (gCSDStyle == CSDStyle::Unknown) {
    bool solid;
    {
      GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_titlebar(GTK_WINDOW(window), gtk_header_bar_new());
      gtk_widget_realize(window);
      GtkStyleContext* windowStyle = gtk_widget_get_style_context(window);
      solid = gtk_style_context_has_class(windowStyle, "solid-csd");
      gtk_widget_destroy(window);
    }
    gCSDStyle = solid ? CSDStyle::Solid : CSDStyle::Normal;
  }
  return gCSDStyle == CSDStyle::Solid;
}

static GtkWidget* CreateWidget(WidgetNodeType aAppearance) {
  switch (aAppearance) {
    case MOZ_GTK_WINDOW:
    case MOZ_GTK_HEADERBAR_FIXED:
    case MOZ_GTK_HEADER_BAR:
      /* Create header bar widgets once and fill with child elements as we need
         the header bar fully configured to get a correct style */
      CreateWindowAndHeaderBar();
      return sWidgetStorage[aAppearance];
    case MOZ_GTK_WINDOW_CONTAINER:
      return CreateWindowContainerWidget();
    case MOZ_GTK_SCROLLBAR_VERTICAL:
      return CreateScrollbarWidget(aAppearance, GTK_ORIENTATION_VERTICAL);
    case MOZ_GTK_MENUPOPUP:
      return CreateMenuPopupWidget();
    case MOZ_GTK_MENUBAR:
      return CreateMenuBarWidget();
    case MOZ_GTK_FRAME:
      return CreateFrameWidget();
    case MOZ_GTK_BUTTON:
      return CreateButtonWidget();
    case MOZ_GTK_SCROLLED_WINDOW:
      return CreateScrolledWindowWidget();
    case MOZ_GTK_TREEVIEW:
      return CreateTreeViewWidget();
    case MOZ_GTK_TREE_HEADER_CELL:
      return CreateTreeHeaderCellWidget();
    default:
      /* Not implemented */
      return nullptr;
  }
}

GtkWidget* GetWidget(WidgetNodeType aAppearance) {
  GtkWidget* widget = sWidgetStorage[aAppearance];
  if (!widget) {
    widget = CreateWidget(aAppearance);
    // Some widgets may not be available or implemented.
    if (!widget) {
      return nullptr;
    }
    sWidgetStorage[aAppearance] = widget;
  }
  return widget;
}

static void AddStyleClassesFromStyle(GtkStyleContext* aDest,
                                     GtkStyleContext* aSrc) {
  GList* classes = gtk_style_context_list_classes(aSrc);
  for (GList* link = classes; link; link = link->next) {
    gtk_style_context_add_class(aDest, static_cast<gchar*>(link->data));
  }
  g_list_free(classes);
}

GtkStyleContext* CreateStyleForWidget(GtkWidget* aWidget,
                                      GtkStyleContext* aParentStyle) {
  static auto sGtkWidgetClassGetCSSName =
      reinterpret_cast<const char* (*)(GtkWidgetClass*)>(
          dlsym(RTLD_DEFAULT, "gtk_widget_class_get_css_name"));

  GtkWidgetClass* widgetClass = GTK_WIDGET_GET_CLASS(aWidget);
  const gchar* name = sGtkWidgetClassGetCSSName
                          ? sGtkWidgetClassGetCSSName(widgetClass)
                          : nullptr;

  GtkStyleContext* context =
      CreateCSSNode(name, aParentStyle, G_TYPE_FROM_CLASS(widgetClass));

  // Classes are stored on the style context instead of the path so that any
  // future gtk_style_context_save() will inherit classes on the head CSS
  // node, in the same way as happens when called on a style context owned by
  // a widget.
  //
  // Classes can be stored on a GtkCssNodeDeclaration and/or the path.
  // gtk_style_context_save() reuses the GtkCssNodeDeclaration, and appends a
  // new object to the path, without copying the classes from the old path
  // head.  The new head picks up classes from the GtkCssNodeDeclaration, but
  // not the path.  GtkWidgets store their classes on the
  // GtkCssNodeDeclaration, so make sure to add classes there.
  //
  // Picking up classes from the style context also means that
  // https://bugzilla.gnome.org/show_bug.cgi?id=767312, which can stop
  // gtk_widget_path_append_for_widget() from finding classes in GTK 3.20,
  // is not a problem.
  GtkStyleContext* widgetStyle = gtk_widget_get_style_context(aWidget);
  AddStyleClassesFromStyle(context, widgetStyle);

  // Release any floating reference on aWidget.
  g_object_ref_sink(aWidget);
  g_object_unref(aWidget);

  return context;
}

static GtkStyleContext* CreateStyleForWidget(GtkWidget* aWidget,
                                             WidgetNodeType aParentType) {
  return CreateStyleForWidget(aWidget, GetWidgetRootStyle(aParentType));
}

GtkStyleContext* CreateCSSNode(const char* aName, GtkStyleContext* aParentStyle,
                               GType aType) {
  static auto sGtkWidgetPathIterSetObjectName =
      reinterpret_cast<void (*)(GtkWidgetPath*, gint, const char*)>(
          dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_set_object_name"));

  GtkWidgetPath* path;
  if (aParentStyle) {
    path = gtk_widget_path_copy(gtk_style_context_get_path(aParentStyle));
    // Copy classes from the parent style context to its corresponding node in
    // the path, because GTK will only match against ancestor classes if they
    // are on the path.
    GList* classes = gtk_style_context_list_classes(aParentStyle);
    for (GList* link = classes; link; link = link->next) {
      gtk_widget_path_iter_add_class(path, -1, static_cast<gchar*>(link->data));
    }
    g_list_free(classes);
  } else {
    path = gtk_widget_path_new();
  }

  gtk_widget_path_append_type(path, aType);

  if (sGtkWidgetPathIterSetObjectName) {
    (*sGtkWidgetPathIterSetObjectName)(path, -1, aName);
  }

  GtkStyleContext* context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_style_context_set_parent(context, aParentStyle);
  gtk_widget_path_unref(path);

  return context;
}

// Return a style context matching that of the root CSS node of a widget.
// This is used by all GTK versions.
static GtkStyleContext* GetWidgetRootStyle(WidgetNodeType aNodeType) {
  GtkStyleContext* style = sStyleStorage[aNodeType];
  if (style) return style;

  switch (aNodeType) {
    case MOZ_GTK_MENUITEM:
      style = CreateStyleForWidget(gtk_menu_item_new(), MOZ_GTK_MENUPOPUP);
      break;
    case MOZ_GTK_MENUBARITEM:
      style = CreateStyleForWidget(gtk_menu_item_new(), MOZ_GTK_MENUBAR);
      break;
    case MOZ_GTK_TEXT_VIEW:
      style =
          CreateStyleForWidget(gtk_text_view_new(), MOZ_GTK_SCROLLED_WINDOW);
      break;
    case MOZ_GTK_TOOLTIP:
      if (gtk_check_version(3, 20, 0) != nullptr) {
        GtkWidget* tooltipWindow = gtk_window_new(GTK_WINDOW_POPUP);
        GtkStyleContext* style = gtk_widget_get_style_context(tooltipWindow);
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_TOOLTIP);
        style = CreateStyleForWidget(tooltipWindow, nullptr);
        gtk_widget_destroy(tooltipWindow);  // Release GtkWindow self-reference.
      } else {
        // We create this from the path because GtkTooltipWindow is not public.
        style = CreateCSSNode("tooltip", nullptr, GTK_TYPE_TOOLTIP);
        gtk_style_context_add_class(style, GTK_STYLE_CLASS_BACKGROUND);
      }
      break;
    case MOZ_GTK_TOOLTIP_BOX:
      style = CreateStyleForWidget(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0),
                                   MOZ_GTK_TOOLTIP);
      break;
    case MOZ_GTK_TOOLTIP_BOX_LABEL:
      style = CreateStyleForWidget(gtk_label_new(nullptr), MOZ_GTK_TOOLTIP_BOX);
      break;
    default:
      GtkWidget* widget = GetWidget(aNodeType);
      MOZ_ASSERT(widget);
      return gtk_widget_get_style_context(widget);
  }

  MOZ_ASSERT(style);
  sStyleStorage[aNodeType] = style;
  return style;
}

static GtkStyleContext* CreateChildCSSNode(const char* aName,
                                           WidgetNodeType aParentNodeType) {
  return CreateCSSNode(aName, GetCssNodeStyleInternal(aParentNodeType));
}

// Create a style context equivalent to a saved root style context of
// |aAppearance| with |aStyleClass| as an additional class.  This is used to
// produce a context equivalent to what GTK versions < 3.20 use for many
// internal parts of widgets.
static GtkStyleContext* CreateSubStyleWithClass(WidgetNodeType aAppearance,
                                                const gchar* aStyleClass) {
  static auto sGtkWidgetPathIterGetObjectName =
      reinterpret_cast<const char* (*)(const GtkWidgetPath*, gint)>(
          dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_get_object_name"));

  GtkStyleContext* parentStyle = GetWidgetRootStyle(aAppearance);

  // Create a new context that behaves like |parentStyle| would after
  // gtk_style_context_save(parentStyle).
  //
  // Avoiding gtk_style_context_save() avoids the need to manage the
  // restore, and a new context permits caching style resolution.
  //
  // gtk_style_context_save(context) changes the node hierarchy of |context|
  // to add a new GtkCssNodeDeclaration that is a copy of its original node.
  // The new node is a child of the original node, and so the new heirarchy is
  // one level deeper.  The new node receives the same classes as the
  // original, but any changes to the classes on |context| will change only
  // the new node.  The new node inherits properties from the original node
  // (which retains the original heirarchy and classes) and matches CSS rules
  // with the new heirarchy and any changes to the classes.
  //
  // The change in hierarchy can produce some surprises in matching theme CSS
  // rules (e.g. https://bugzilla.gnome.org/show_bug.cgi?id=761870#c2), but it
  // is important here to produce the same behavior so that rules match the
  // same widget parts in Gecko as they do in GTK.
  //
  // When using public GTK API to construct style contexts, a widget path is
  // required.  CSS rules are not matched against the style context heirarchy
  // but according to the heirarchy in the widget path.  The path that matches
  // the same CSS rules as a saved context is like the path of |parentStyle|
  // but with an extra copy of the head (last) object appended.  Setting
  // |parentStyle| as the parent context provides the same inheritance of
  // properties from the widget root node.
  const GtkWidgetPath* parentPath = gtk_style_context_get_path(parentStyle);
  const gchar* name = sGtkWidgetPathIterGetObjectName
                          ? sGtkWidgetPathIterGetObjectName(parentPath, -1)
                          : nullptr;
  GType objectType = gtk_widget_path_get_object_type(parentPath);

  GtkStyleContext* style = CreateCSSNode(name, parentStyle, objectType);

  // Start with the same classes on the new node as were on |parentStyle|.
  // GTK puts no regions or junction_sides on widget root nodes, and so there
  // is no need to copy these.
  AddStyleClassesFromStyle(style, parentStyle);

  gtk_style_context_add_class(style, aStyleClass);
  return style;
}

/* GetCssNodeStyleInternal is used by Gtk >= 3.20 */
static GtkStyleContext* GetCssNodeStyleInternal(WidgetNodeType aNodeType) {
  GtkStyleContext* style = sStyleStorage[aNodeType];
  if (style) return style;

  switch (aNodeType) {
    case MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL:
      style = CreateChildCSSNode("contents", MOZ_GTK_SCROLLBAR_VERTICAL);
      break;
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH,
                                 MOZ_GTK_SCROLLBAR_CONTENTS_VERTICAL);
      break;
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_SLIDER,
                                 MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL);
      break;
    case MOZ_GTK_SCROLLED_WINDOW:
      // TODO - create from CSS node
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLED_WINDOW,
                                      GTK_STYLE_CLASS_FRAME);
      break;
    case MOZ_GTK_TEXT_VIEW_TEXT_SELECTION:
      style = CreateChildCSSNode("selection", MOZ_GTK_TEXT_VIEW_TEXT);
      break;
    case MOZ_GTK_TEXT_VIEW_TEXT:
      style = CreateChildCSSNode("text", MOZ_GTK_TEXT_VIEW);
      break;
    case MOZ_GTK_FRAME_BORDER:
      style = CreateChildCSSNode("border", MOZ_GTK_FRAME);
      break;
    case MOZ_GTK_WINDOW_DECORATION: {
      GtkStyleContext* parentStyle =
          CreateSubStyleWithClass(MOZ_GTK_WINDOW, "csd");
      style = CreateCSSNode("decoration", parentStyle);
      g_object_unref(parentStyle);
      break;
    }
    case MOZ_GTK_WINDOW_DECORATION_SOLID: {
      GtkStyleContext* parentStyle =
          CreateSubStyleWithClass(MOZ_GTK_WINDOW, "solid-csd");
      style = CreateCSSNode("decoration", parentStyle);
      g_object_unref(parentStyle);
      break;
    }
    default:
      return GetWidgetRootStyle(aNodeType);
  }

  MOZ_ASSERT(style, "missing style context for node type");
  sStyleStorage[aNodeType] = style;
  return style;
}

/* GetWidgetStyleInternal is used by Gtk < 3.20 */
static GtkStyleContext* GetWidgetStyleInternal(WidgetNodeType aNodeType) {
  GtkStyleContext* style = sStyleStorage[aNodeType];
  if (style) return style;

  switch (aNodeType) {
    case MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLBAR_VERTICAL,
                                      GTK_STYLE_CLASS_TROUGH);
      break;
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLBAR_VERTICAL,
                                      GTK_STYLE_CLASS_SLIDER);
      break;
    case MOZ_GTK_SCROLLED_WINDOW:
      style = CreateSubStyleWithClass(MOZ_GTK_SCROLLED_WINDOW,
                                      GTK_STYLE_CLASS_FRAME);
      break;
    case MOZ_GTK_TEXT_VIEW_TEXT:
      // GTK versions prior to 3.20 do not have the view class on the root
      // node, but add this to determine the background for the text window.
      style = CreateSubStyleWithClass(MOZ_GTK_TEXT_VIEW, GTK_STYLE_CLASS_VIEW);
      break;
    case MOZ_GTK_FRAME_BORDER:
      return GetWidgetRootStyle(MOZ_GTK_FRAME);
    default:
      return GetWidgetRootStyle(aNodeType);
  }

  MOZ_ASSERT(style);
  sStyleStorage[aNodeType] = style;
  return style;
}

void ResetWidgetCache() {
  for (auto* style : sStyleStorage) {
    if (style) {
      g_object_unref(style);
    }
  }
  mozilla::PodArrayZero(sStyleStorage);

  gCSDStyle = CSDStyle::Unknown;

  /* This will destroy all of our widgets */
  if (sWidgetStorage[MOZ_GTK_WINDOW]) {
    gtk_widget_destroy(sWidgetStorage[MOZ_GTK_WINDOW]);
  }

  /* Clear already freed arrays */
  mozilla::PodArrayZero(sWidgetStorage);
}

static void StyleContextSetScale(GtkStyleContext* style, gint aScaleFactor) {
  // Support HiDPI styles on Gtk 3.20+
  static auto sGtkStyleContextSetScalePtr =
      (void (*)(GtkStyleContext*, gint))dlsym(RTLD_DEFAULT,
                                              "gtk_style_context_set_scale");
  if (sGtkStyleContextSetScalePtr && style) {
    sGtkStyleContextSetScalePtr(style, aScaleFactor);
  }
}

GtkStyleContext* GetStyleContext(WidgetNodeType aNodeType, int aScale,
                                 GtkStateFlags aState) {
  GtkStyleContext* style;
  if (gtk_check_version(3, 20, 0) != nullptr) {
    style = GetWidgetStyleInternal(aNodeType);
  } else {
    style = GetCssNodeStyleInternal(aNodeType);
    StyleContextSetScale(style, aScale);
  }
  if (gtk_style_context_get_state(style) != aState) {
    gtk_style_context_set_state(style, aState);
  }
  return style;
}

bool HeaderBarShouldDrawContainer() {
  mozilla::Unused << GetWidget(MOZ_GTK_HEADER_BAR);
  return gHeaderBarShouldDrawContainer;
}

gint GetBorderRadius(GtkStyleContext* aStyle) {
  GValue value = G_VALUE_INIT;
  // NOTE(emilio): In an ideal world, we'd query the two longhands
  // (border-top-left-radius and border-top-right-radius) separately. However,
  // that doesn't work (GTK rejects the query with:
  //
  //   Style property "border-top-left-radius" is not gettable
  //
  // However! Getting border-radius does work, and it does return the
  // border-top-left-radius as a gint:
  //
  //   https://docs.gtk.org/gtk3/const.STYLE_PROPERTY_BORDER_RADIUS.html
  //   https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-20/gtk/gtkcssshorthandpropertyimpl.c#L961-977
  //
  // So we abuse this fact, and make the assumption here that the
  // border-top-{left,right}-radius are the same, and roll with it.
  gtk_style_context_get_property(aStyle, "border-radius", GTK_STATE_FLAG_NORMAL,
                                 &value);
  gint result = 0;
  auto type = G_VALUE_TYPE(&value);
  if (type == G_TYPE_INT) {
    result = g_value_get_int(&value);
  } else {
    NS_WARNING(nsPrintfCString("Unknown value type %lu for border-radius", type)
                   .get());
  }
  g_value_unset(&value);
  return result;
}
