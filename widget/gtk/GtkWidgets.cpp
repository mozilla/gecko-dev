/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <gtk/gtk.h>
#include "GtkWidgets.h"
#include "mozilla/Assertions.h"
#include "mozilla/PodOperations.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/WidgetUtilsGtk.h"

namespace mozilla::widget::GtkWidgets {

static EnumeratedArray<Type, GtkWidget*, kTypeCount> sWidgetStorage;
static EnumeratedArray<Type, GtkStyleContext*, kTypeCount> sStyleStorage;

static GtkStyleContext* CreateCSSNode(const char* aName,
                                      GtkStyleContext* aParentStyle,
                                      GType aType = G_TYPE_NONE) {
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
    sGtkWidgetPathIterSetObjectName(path, -1, aName);
  }

  GtkStyleContext* context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_style_context_set_parent(context, aParentStyle);
  gtk_widget_path_unref(path);

  return context;
}

static GtkStyleContext* GetWidgetRootStyle(Type aType);
static GtkStyleContext* GetCssNodeStyleInternal(Type aType);

static GtkWidget* CreateWindowContainerWidget() {
  GtkWidget* widget = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(Get(Type::Window)), widget);
  return widget;
}

static void AddToWindowContainer(GtkWidget* widget) {
  gtk_container_add(GTK_CONTAINER(Get(Type::WindowContainer)), widget);
}

static GtkWidget* CreateScrollbarWidget() {
  GtkWidget* widget = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, nullptr);
  AddToWindowContainer(widget);
  return widget;
}

static GtkWidget* CreateMenuPopupWidget() {
  GtkWidget* widget = gtk_menu_new();
  GtkStyleContext* style = gtk_widget_get_style_context(widget);
  gtk_style_context_add_class(style, GTK_STYLE_CLASS_POPUP);
  gtk_menu_attach_to_widget(GTK_MENU(widget), Get(Type::Window), nullptr);
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

  GtkWidget* treeView = Get(Type::TreeView);

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

static void CreateWindowAndHeaderBar() {
  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(window, "MozillaGtkWidget");
  GtkStyleContext* windowStyle = gtk_widget_get_style_context(window);

  // Headerbar has to be placed into a window with csd or solid-csd style
  // to properly draw the decorations.
  gtk_style_context_add_class(windowStyle, "csd");

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

  MOZ_ASSERT(!sWidgetStorage[Type::HeaderBar],
             "Headerbar widget is already created!");
  MOZ_ASSERT(!sWidgetStorage[Type::Window],
             "Window widget is already created!");
  MOZ_ASSERT(!sWidgetStorage[Type::HeaderBarFixed],
             "Fixed widget is already created!");

  sWidgetStorage[Type::HeaderBar] = headerBar;
  sWidgetStorage[Type::Window] = window;
  sWidgetStorage[Type::HeaderBarFixed] = fixed;

  gtk_container_add(GTK_CONTAINER(fixed), headerBar);
  gtk_window_set_titlebar(GTK_WINDOW(window), fixed);

  gtk_widget_show_all(headerBar);
}

static GtkWidget* CreateWidget(Type aType) {
  switch (aType) {
    case Type::Window:
    case Type::HeaderBarFixed:
    case Type::HeaderBar:
      // Create header bar widgets once and fill with child elements as we need
      // the header bar fully configured to get a correct style.
      CreateWindowAndHeaderBar();
      return sWidgetStorage[aType];
    case Type::WindowContainer:
      return CreateWindowContainerWidget();
    case Type::Scrollbar:
      return CreateScrollbarWidget();
    case Type::Menupopup:
      return CreateMenuPopupWidget();
    case Type::Menubar:
      return CreateMenuBarWidget();
    case Type::Frame:
      return CreateFrameWidget();
    case Type::Button:
      return CreateButtonWidget();
    case Type::ScrolledWindow:
      return CreateScrolledWindowWidget();
    case Type::TreeView:
      return CreateTreeViewWidget();
    case Type::TreeHeaderCell:
      return CreateTreeHeaderCellWidget();
    case Type::ScrollbarContents:
    case Type::ScrollbarTrough:
    case Type::ScrollbarThumb:
    case Type::TextView:
    case Type::TextViewText:
    case Type::TextViewTextSelection:
    case Type::Tooltip:
    case Type::TooltipBox:
    case Type::TooltipBoxLabel:
    case Type::FrameBorder:
    case Type::Menuitem:
    case Type::MenubarItem:
    case Type::WindowDecoration:
      break;
  }
  // Not implemented
  return nullptr;
}

GtkWidget* Get(Type aType) {
  GtkWidget* widget = sWidgetStorage[aType];
  if (!widget) {
    widget = CreateWidget(aType);
    sWidgetStorage[aType] = widget;
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
                                             Type aParentType) {
  return CreateStyleForWidget(aWidget, GetWidgetRootStyle(aParentType));
}

// Return a style context matching that of the root CSS node of a widget.
// This is used by all GTK versions.
static GtkStyleContext* GetWidgetRootStyle(Type aType) {
  GtkStyleContext* style = sStyleStorage[aType];
  if (style) {
    return style;
  }

  switch (aType) {
    case Type::Menuitem:
      style = CreateStyleForWidget(gtk_menu_item_new(), Type::Menupopup);
      break;
    case Type::MenubarItem:
      style = CreateStyleForWidget(gtk_menu_item_new(), Type::Menubar);
      break;
    case Type::TextView:
      style = CreateStyleForWidget(gtk_text_view_new(), Type::ScrolledWindow);
      break;
    case Type::Tooltip:
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
    case Type::TooltipBox:
      style = CreateStyleForWidget(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0),
                                   Type::Tooltip);
      break;
    case Type::TooltipBoxLabel:
      style = CreateStyleForWidget(gtk_label_new(nullptr), Type::TooltipBox);
      break;
    default:
      GtkWidget* widget = Get(aType);
      MOZ_ASSERT(widget);
      return gtk_widget_get_style_context(widget);
  }

  MOZ_ASSERT(style);
  sStyleStorage[aType] = style;
  return style;
}

static GtkStyleContext* CreateChildCSSNode(const char* aName,
                                           Type aParentType) {
  return CreateCSSNode(aName, GetCssNodeStyleInternal(aParentType));
}

// Create a style context equivalent to a saved root style context of
// |aType| with |aStyleClass| as an additional class.  This is used to
// produce a context equivalent to what GTK versions < 3.20 use for many
// internal parts of widgets.
static GtkStyleContext* CreateSubStyleWithClass(Type aType,
                                                const gchar* aStyleClass) {
  static auto sGtkWidgetPathIterGetObjectName =
      reinterpret_cast<const char* (*)(const GtkWidgetPath*, gint)>(
          dlsym(RTLD_DEFAULT, "gtk_widget_path_iter_get_object_name"));

  GtkStyleContext* parentStyle = GetWidgetRootStyle(aType);

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

// GetCssNodeStyleInternal is used by Gtk >= 3.20
static GtkStyleContext* GetCssNodeStyleInternal(Type aType) {
  GtkStyleContext* style = sStyleStorage[aType];
  if (style) {
    return style;
  }

  switch (aType) {
    case Type::ScrollbarContents:
      style = CreateChildCSSNode("contents", Type::Scrollbar);
      break;
    case Type::ScrollbarTrough:
      style =
          CreateChildCSSNode(GTK_STYLE_CLASS_TROUGH, Type::ScrollbarContents);
      break;
    case Type::ScrollbarThumb:
      style = CreateChildCSSNode(GTK_STYLE_CLASS_SLIDER, Type::ScrollbarTrough);
      break;
    case Type::ScrolledWindow:
      // TODO - create from CSS node
      style =
          CreateSubStyleWithClass(Type::ScrolledWindow, GTK_STYLE_CLASS_FRAME);
      break;
    case Type::TextViewTextSelection:
      style = CreateChildCSSNode("selection", Type::TextViewText);
      break;
    case Type::TextViewText:
      style = CreateChildCSSNode("text", Type::TextView);
      break;
    case Type::FrameBorder:
      style = CreateChildCSSNode("border", Type::Frame);
      break;
    case Type::WindowDecoration: {
      GtkStyleContext* parentStyle =
          CreateSubStyleWithClass(Type::Window, "csd");
      style = CreateCSSNode("decoration", parentStyle);
      g_object_unref(parentStyle);
      break;
    }
    default:
      return GetWidgetRootStyle(aType);
  }

  MOZ_ASSERT(style, "missing style context for node type");
  sStyleStorage[aType] = style;
  return style;
}

// GetWidgetStyleInternal is used by Gtk < 3.20
static GtkStyleContext* GetWidgetStyleInternal(Type aType) {
  GtkStyleContext* style = sStyleStorage[aType];
  if (style) {
    return style;
  }

  switch (aType) {
    case Type::ScrollbarTrough:
      style = CreateSubStyleWithClass(Type::Scrollbar, GTK_STYLE_CLASS_TROUGH);
      break;
    case Type::ScrollbarThumb:
      style =
          CreateSubStyleWithClass(Type::ScrollbarThumb, GTK_STYLE_CLASS_SLIDER);
      break;
    case Type::ScrolledWindow:
      style =
          CreateSubStyleWithClass(Type::ScrolledWindow, GTK_STYLE_CLASS_FRAME);
      break;
    case Type::TextViewText:
      // GTK versions prior to 3.20 do not have the view class on the root
      // node, but add this to determine the background for the text window.
      style = CreateSubStyleWithClass(Type::TextView, GTK_STYLE_CLASS_VIEW);
      break;
    case Type::FrameBorder:
      return GetWidgetRootStyle(Type::Frame);
    default:
      return GetWidgetRootStyle(aType);
  }

  MOZ_ASSERT(style);
  sStyleStorage[aType] = style;
  return style;
}

static void ResetWidgetCache() {
  for (auto& style : sStyleStorage) {
    if (style) {
      g_object_unref(style);
    }
  }
  mozilla::PodZero(sStyleStorage.begin(), sStyleStorage.size());

  // This will destroy all of our widgets
  if (sWidgetStorage[Type::Window]) {
    gtk_widget_destroy(sWidgetStorage[Type::Window]);
  }

  // Clear already freed arrays
  mozilla::PodZero(sWidgetStorage.begin(), sWidgetStorage.size());
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

GtkStyleContext* GetStyle(Type aType, int aScale, GtkStateFlags aState) {
  GtkStyleContext* style;
  if (gtk_check_version(3, 20, 0) != nullptr) {
    style = GetWidgetStyleInternal(aType);
  } else {
    style = GetCssNodeStyleInternal(aType);
    StyleContextSetScale(style, aScale);
  }
  if (gtk_style_context_get_state(style) != aState) {
    gtk_style_context_set_state(style, aState);
  }
  return style;
}

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

void Refresh() { ResetWidgetCache(); }

static void DrawWindowDecoration(cairo_t* cr, const DrawingParams& aParams) {
  if (GdkIsWaylandDisplay()) {
    // Doesn't seem to be needed.
    return;
  }
  GtkStyleContext* decorationStyle =
      GetStyle(Type::WindowDecoration, aParams.image_scale, aParams.state);

  const auto& rect = aParams.rect;
  gtk_render_background(decorationStyle, cr, rect.x, rect.y, rect.width,
                        rect.height);
  gtk_render_frame(decorationStyle, cr, rect.x, rect.y, rect.width,
                   rect.height);
}

/* cairo_t *cr argument has to be a system-cairo. */
void Draw(cairo_t* cr, const DrawingParams* aParams) {
  /* A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=694086 */
  cairo_new_path(cr);
  switch (aParams->widget) {
    case Type::WindowDecoration:
      return DrawWindowDecoration(cr, *aParams);
    default:
      g_warning("Unknown widget type: %u", uint32_t(aParams->widget));
      return;
  }
}

void Shutdown() {
  /* This will destroy all of our widgets */
  ResetWidgetCache();
}

}  // namespace mozilla::widget::GtkWidgets
