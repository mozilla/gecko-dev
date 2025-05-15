/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeGTK.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "gtkdrawing.h"
#include "ScreenHelperGTK.h"
#include "WidgetUtilsGtk.h"

#include "gfx2DGlue.h"
#include "nsIObserverService.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsViewManager.h"
#include "nsNameSpaceManager.h"
#include "nsGfxCIID.h"
#include "nsTransform2D.h"
#include "nsXULPopupManager.h"
#include "tree/nsTreeBodyFrame.h"
#include "prlink.h"
#include "nsGkAtoms.h"
#include "nsAttrValueInlines.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"

#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>

#include "gfxContext.h"
#include "mozilla/dom/XULButtonElement.h"
#include "mozilla/gfx/BorrowedContext.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsWindow.h"
#include "nsLayoutUtils.h"
#include "Theme.h"

#ifdef MOZ_X11
#  ifdef CAIRO_HAS_XLIB_SURFACE
#    include "cairo-xlib.h"
#  endif
#endif

#include <algorithm>
#include <dlfcn.h>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

static int gLastGdkError;

// Return widget scale factor of the monitor where the window is located by the
// most part. We intentionally honor the text scale factor here in order to
// have consistent scaling with other UI elements.
static inline CSSToLayoutDeviceScale GetWidgetScaleFactor(nsIFrame* aFrame) {
  return aFrame->PresContext()->CSSToDevPixelScale();
}

nsNativeThemeGTK::nsNativeThemeGTK() : Theme(ScrollbarStyle()) {
  if (moz_gtk_init() != MOZ_GTK_SUCCESS) {
    memset(mDisabledWidgetTypes, 0xff, sizeof(mDisabledWidgetTypes));
    return;
  }

  ThemeChanged();
}

nsNativeThemeGTK::~nsNativeThemeGTK() { moz_gtk_shutdown(); }

void nsNativeThemeGTK::RefreshWidgetWindow(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aFrame->PresShell());

  nsViewManager* vm = aFrame->PresShell()->GetViewManager();
  if (!vm) {
    return;
  }
  vm->InvalidateAllViews();
}

static bool IsFrameContentNodeInNamespace(nsIFrame* aFrame,
                                          uint32_t aNamespace) {
  nsIContent* content = aFrame ? aFrame->GetContent() : nullptr;
  if (!content) return false;
  return content->IsInNamespace(aNamespace);
}

static bool IsWidgetTypeDisabled(const uint8_t* aDisabledVector,
                                 StyleAppearance aAppearance) {
  auto type = static_cast<size_t>(aAppearance);
  MOZ_ASSERT(type < static_cast<size_t>(StyleAppearance::Count));
  return (aDisabledVector[type >> 3] & (1 << (type & 7))) != 0;
}

static void SetWidgetTypeDisabled(uint8_t* aDisabledVector,
                                  StyleAppearance aAppearance) {
  auto type = static_cast<size_t>(aAppearance);
  MOZ_ASSERT(type < static_cast<size_t>(mozilla::StyleAppearance::Count));
  aDisabledVector[type >> 3] |= (1 << (type & 7));
}

static inline uint16_t GetWidgetStateKey(StyleAppearance aAppearance,
                                         GtkWidgetState* aWidgetState) {
  return (aWidgetState->active | aWidgetState->focused << 1 |
          aWidgetState->inHover << 2 | aWidgetState->disabled << 3 |
          aWidgetState->isDefault << 4 |
          static_cast<uint16_t>(aAppearance) << 5);
}

static bool IsWidgetStateSafe(uint8_t* aSafeVector, StyleAppearance aAppearance,
                              GtkWidgetState* aWidgetState) {
  MOZ_ASSERT(static_cast<size_t>(aAppearance) <
             static_cast<size_t>(mozilla::StyleAppearance::Count));
  uint16_t key = GetWidgetStateKey(aAppearance, aWidgetState);
  return (aSafeVector[key >> 3] & (1 << (key & 7))) != 0;
}

static void SetWidgetStateSafe(uint8_t* aSafeVector,
                               StyleAppearance aAppearance,
                               GtkWidgetState* aWidgetState) {
  MOZ_ASSERT(static_cast<size_t>(aAppearance) <
             static_cast<size_t>(mozilla::StyleAppearance::Count));
  uint16_t key = GetWidgetStateKey(aAppearance, aWidgetState);
  aSafeVector[key >> 3] |= (1 << (key & 7));
}

/* static */
GtkTextDirection nsNativeThemeGTK::GetTextDirection(nsIFrame* aFrame) {
  // IsFrameRTL() treats vertical-rl modes as right-to-left (in addition to
  // horizontal text with direction=RTL), rather than just considering the
  // text direction.  GtkTextDirection does not have distinct values for
  // vertical writing modes, but considering the block flow direction is
  // important for resizers and scrollbar elements, at least.
  return IsFrameRTL(aFrame) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
}

// Returns positive for negative margins (otherwise 0).
gint nsNativeThemeGTK::GetTabMarginPixels(nsIFrame* aFrame) {
  nscoord margin = IsBottomTab(aFrame) ? aFrame->GetUsedMargin().top
                                       : aFrame->GetUsedMargin().bottom;

  return std::min<gint>(
      MOZ_GTK_TAB_MARGIN_MASK,
      std::max(0, aFrame->PresContext()->AppUnitsToDevPixels(-margin)));
}

bool nsNativeThemeGTK::GetGtkWidgetAndState(StyleAppearance aAppearance,
                                            nsIFrame* aFrame,
                                            WidgetNodeType& aGtkWidgetType,
                                            GtkWidgetState* aState,
                                            gint* aWidgetFlags) {
  if (aWidgetFlags) {
    *aWidgetFlags = 0;
  }

  ElementState elementState = GetContentState(aFrame, aAppearance);
  if (aState) {
    memset(aState, 0, sizeof(GtkWidgetState));
    if (aWidgetFlags) {
      if (elementState.HasState(ElementState::CHECKED)) {
        *aWidgetFlags |= MOZ_GTK_WIDGET_CHECKED;
      }
      if (elementState.HasState(ElementState::INDETERMINATE)) {
        *aWidgetFlags |= MOZ_GTK_WIDGET_INCONSISTENT;
      }
    }

    aState->disabled =
        elementState.HasState(ElementState::DISABLED) || IsReadOnly(aFrame);
    aState->active = elementState.HasState(ElementState::ACTIVE);
    aState->focused = elementState.HasState(ElementState::FOCUS);
    aState->inHover = elementState.HasState(ElementState::HOVER);
    aState->isDefault = IsDefaultButton(aFrame);
    aState->canDefault = FALSE;  // XXX fix me

    if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
      // For these widget types, some element (either a child or parent)
      // actually has element focus, so we check the focused attribute
      // to see whether to draw in the focused state.
      aState->focused = elementState.HasState(ElementState::FOCUSRING);
    }

    if (aAppearance == StyleAppearance::MozWindowTitlebar ||
        aAppearance == StyleAppearance::MozWindowTitlebarMaximized) {
      aState->backdrop = aFrame->PresContext()->Document()->State().HasState(
          dom::DocumentState::WINDOW_INACTIVE);
    }
  }

  switch (aAppearance) {
    case StyleAppearance::Tabpanels:
      aGtkWidgetType = MOZ_GTK_TABPANELS;
      break;
    case StyleAppearance::Tab: {
      if (IsBottomTab(aFrame)) {
        aGtkWidgetType = MOZ_GTK_TAB_BOTTOM;
      } else {
        aGtkWidgetType = MOZ_GTK_TAB_TOP;
      }

      if (aWidgetFlags) {
        /* First bits will be used to store max(0,-bmargin) where bmargin
         * is the bottom margin of the tab in pixels  (resp. top margin,
         * for bottom tabs). */
        *aWidgetFlags = GetTabMarginPixels(aFrame);

        if (IsSelectedTab(aFrame)) *aWidgetFlags |= MOZ_GTK_TAB_SELECTED;

        if (IsFirstTab(aFrame)) *aWidgetFlags |= MOZ_GTK_TAB_FIRST;
      }
    } break;
    case StyleAppearance::MozWindowDecorations:
      aGtkWidgetType = MOZ_GTK_WINDOW_DECORATION;
      break;
    default:
      return false;
  }

  return true;
}

class SystemCairoClipper : public ClipExporter {
 public:
  explicit SystemCairoClipper(cairo_t* aContext, gint aScaleFactor = 1)
      : mContext(aContext), mScaleFactor(aScaleFactor) {}

  void BeginClip(const Matrix& aTransform) override {
    cairo_matrix_t mat;
    GfxMatrixToCairoMatrix(aTransform, mat);
    // We also need to remove the scale factor effect from the matrix
    mat.y0 = mat.y0 / mScaleFactor;
    mat.x0 = mat.x0 / mScaleFactor;
    cairo_set_matrix(mContext, &mat);

    cairo_new_path(mContext);
  }

  void MoveTo(const Point& aPoint) override {
    cairo_move_to(mContext, aPoint.x / mScaleFactor, aPoint.y / mScaleFactor);
    mBeginPoint = aPoint;
    mCurrentPoint = aPoint;
  }

  void LineTo(const Point& aPoint) override {
    cairo_line_to(mContext, aPoint.x / mScaleFactor, aPoint.y / mScaleFactor);
    mCurrentPoint = aPoint;
  }

  void BezierTo(const Point& aCP1, const Point& aCP2,
                const Point& aCP3) override {
    cairo_curve_to(mContext, aCP1.x / mScaleFactor, aCP1.y / mScaleFactor,
                   aCP2.x / mScaleFactor, aCP2.y / mScaleFactor,
                   aCP3.x / mScaleFactor, aCP3.y / mScaleFactor);
    mCurrentPoint = aCP3;
  }

  void QuadraticBezierTo(const Point& aCP1, const Point& aCP2) override {
    Point CP0 = CurrentPoint();
    Point CP1 = (CP0 + aCP1 * 2.0) / 3.0;
    Point CP2 = (aCP2 + aCP1 * 2.0) / 3.0;
    Point CP3 = aCP2;
    cairo_curve_to(mContext, CP1.x / mScaleFactor, CP1.y / mScaleFactor,
                   CP2.x / mScaleFactor, CP2.y / mScaleFactor,
                   CP3.x / mScaleFactor, CP3.y / mScaleFactor);
    mCurrentPoint = aCP2;
  }

  void Arc(const Point& aOrigin, float aRadius, float aStartAngle,
           float aEndAngle, bool aAntiClockwise) override {
    ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
                aAntiClockwise);
  }

  void Close() override {
    cairo_close_path(mContext);
    mCurrentPoint = mBeginPoint;
  }

  void EndClip() override { cairo_clip(mContext); }

 private:
  cairo_t* mContext;
  gint mScaleFactor;
};

static void DrawThemeWithCairo(gfxContext* aContext, DrawTarget* aDrawTarget,
                               GtkWidgetState aState,
                               WidgetNodeType aGTKWidgetType, gint aFlags,
                               GtkTextDirection aDirection, double aScaleFactor,
                               bool aSnapped, const Point& aDrawOrigin,
                               const nsIntSize& aDrawSize,
                               GdkRectangle& aGDKRect,
                               nsITheme::Transparency aTransparency) {
  static auto sCairoSurfaceSetDeviceScalePtr =
      (void (*)(cairo_surface_t*, double, double))dlsym(
          RTLD_DEFAULT, "cairo_surface_set_device_scale");
  const bool useHiDPIWidgets =
      aScaleFactor != 1.0 && sCairoSurfaceSetDeviceScalePtr;

  Point drawOffsetScaled;
  Point drawOffsetOriginal;
  Matrix transform;
  if (!aSnapped) {
    // If we are not snapped, we depend on the DT for translation.
    drawOffsetOriginal = aDrawOrigin;
    drawOffsetScaled = useHiDPIWidgets ? drawOffsetOriginal / aScaleFactor
                                       : drawOffsetOriginal;
    transform = aDrawTarget->GetTransform().PreTranslate(drawOffsetScaled);
  } else {
    // Otherwise, we only need to take the device offset into account.
    drawOffsetOriginal = aDrawOrigin - aContext->GetDeviceOffset();
    drawOffsetScaled = useHiDPIWidgets ? drawOffsetOriginal / aScaleFactor
                                       : drawOffsetOriginal;
    transform = Matrix::Translation(drawOffsetScaled);
  }

  if (!useHiDPIWidgets && aScaleFactor != 1) {
    transform.PreScale(aScaleFactor, aScaleFactor);
  }

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(transform, mat);

  Size clipSize((aDrawSize.width + aScaleFactor - 1) / aScaleFactor,
                (aDrawSize.height + aScaleFactor - 1) / aScaleFactor);

  // A direct Cairo draw target is not available, so we need to create a
  // temporary one.
#if defined(MOZ_X11) && defined(CAIRO_HAS_XLIB_SURFACE)
  if (GdkIsX11Display()) {
    // If using a Cairo xlib surface, then try to reuse it.
    BorrowedXlibDrawable borrow(aDrawTarget);
    if (Drawable drawable = borrow.GetDrawable()) {
      nsIntSize size = borrow.GetSize();
      cairo_surface_t* surf = cairo_xlib_surface_create(
          borrow.GetDisplay(), drawable, borrow.GetVisual(), size.width,
          size.height);
      if (!NS_WARN_IF(!surf)) {
        Point offset = borrow.GetOffset();
        if (offset != Point()) {
          cairo_surface_set_device_offset(surf, offset.x, offset.y);
        }
        cairo_t* cr = cairo_create(surf);
        if (!NS_WARN_IF(!cr)) {
          RefPtr<SystemCairoClipper> clipper = new SystemCairoClipper(cr);
          aContext->ExportClip(*clipper);

          cairo_set_matrix(cr, &mat);

          cairo_new_path(cr);
          cairo_rectangle(cr, 0, 0, clipSize.width, clipSize.height);
          cairo_clip(cr);

          moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags,
                               aDirection);

          cairo_destroy(cr);
        }
        cairo_surface_destroy(surf);
      }
      borrow.Finish();
      return;
    }
  }
#endif

  // Check if the widget requires complex masking that must be composited.
  // Try to directly write to the draw target's pixels if possible.
  uint8_t* data;
  nsIntSize size;
  int32_t stride;
  SurfaceFormat format;
  IntPoint origin;
  if (aDrawTarget->LockBits(&data, &size, &stride, &format, &origin)) {
    // Create a Cairo image surface context the device rectangle.
    cairo_surface_t* surf = cairo_image_surface_create_for_data(
        data, GfxFormatToCairoFormat(format), size.width, size.height, stride);
    if (!NS_WARN_IF(!surf)) {
      if (useHiDPIWidgets) {
        sCairoSurfaceSetDeviceScalePtr(surf, aScaleFactor, aScaleFactor);
      }
      if (origin != IntPoint()) {
        cairo_surface_set_device_offset(surf, -origin.x, -origin.y);
      }
      cairo_t* cr = cairo_create(surf);
      if (!NS_WARN_IF(!cr)) {
        RefPtr<SystemCairoClipper> clipper =
            new SystemCairoClipper(cr, useHiDPIWidgets ? aScaleFactor : 1);
        aContext->ExportClip(*clipper);

        cairo_set_matrix(cr, &mat);

        cairo_new_path(cr);
        cairo_rectangle(cr, 0, 0, clipSize.width, clipSize.height);
        cairo_clip(cr);

        moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags,
                             aDirection);

        cairo_destroy(cr);
      }
      cairo_surface_destroy(surf);
    }
    aDrawTarget->ReleaseBits(data);
  } else {
    // If the widget has any transparency, make sure to choose an alpha format.
    format = aTransparency != nsITheme::eOpaque ? SurfaceFormat::B8G8R8A8
                                                : aDrawTarget->GetFormat();
    // Create a temporary data surface to render the widget into.
    RefPtr<DataSourceSurface> dataSurface = Factory::CreateDataSourceSurface(
        aDrawSize, format, aTransparency != nsITheme::eOpaque);
    DataSourceSurface::MappedSurface map;
    if (!NS_WARN_IF(
            !(dataSurface &&
              dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)))) {
      // Create a Cairo image surface wrapping the data surface.
      cairo_surface_t* surf = cairo_image_surface_create_for_data(
          map.mData, GfxFormatToCairoFormat(format), aDrawSize.width,
          aDrawSize.height, map.mStride);
      cairo_t* cr = nullptr;
      if (!NS_WARN_IF(!surf)) {
        cr = cairo_create(surf);
        if (!NS_WARN_IF(!cr)) {
          if (aScaleFactor != 1) {
            if (useHiDPIWidgets) {
              sCairoSurfaceSetDeviceScalePtr(surf, aScaleFactor, aScaleFactor);
            } else {
              cairo_scale(cr, aScaleFactor, aScaleFactor);
            }
          }

          moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags,
                               aDirection);
        }
      }

      // Unmap the surface before using it as a source
      dataSurface->Unmap();

      if (cr) {
        // The widget either needs to be masked or has transparency, so use the
        // slower drawing path.
        aDrawTarget->DrawSurface(
            dataSurface,
            Rect(aSnapped ? drawOffsetOriginal -
                                aDrawTarget->GetTransform().GetTranslation()
                          : drawOffsetOriginal,
                 Size(aDrawSize)),
            Rect(0, 0, aDrawSize.width, aDrawSize.height));
        cairo_destroy(cr);
      }

      if (surf) {
        cairo_surface_destroy(surf);
      }
    }
  }
}

NS_IMETHODIMP
nsNativeThemeGTK::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect,
                                       DrawOverflow aDrawOverflow) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::DrawWidgetBackground(aContext, aFrame, aAppearance, aRect,
                                       aDirtyRect, aDrawOverflow);
  }

  GtkWidgetState state;
  WidgetNodeType gtkWidgetType;
  GtkTextDirection direction = GetTextDirection(aFrame);
  gint flags;

  if (!GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, &state,
                            &flags)) {
    return NS_OK;
  }

  gfxContext* ctx = aContext;
  nsPresContext* presContext = aFrame->PresContext();

  gfxRect rect = presContext->AppUnitsToGfxUnits(aRect);
  gfxRect dirtyRect = presContext->AppUnitsToGfxUnits(aDirtyRect);

  // Align to device pixels where sensible
  // to provide crisper and faster drawing.
  // Don't snap if it's a non-unit scale factor. We're going to have to take
  // slow paths then in any case.
  // We prioritize the size when snapping in order to avoid distorting widgets
  // that should be square, which can occur if edges are snapped independently.
  bool snapped = ctx->UserToDevicePixelSnapped(
      rect, gfxContext::SnapOption::PrioritizeSize);
  if (snapped) {
    // Leave rect in device coords but make dirtyRect consistent.
    dirtyRect = ctx->UserToDevice(dirtyRect);
  }

  // Translate the dirty rect so that it is wrt the widget top-left.
  dirtyRect.MoveBy(-rect.TopLeft());
  // Round out the dirty rect to gdk pixels to ensure that gtk draws
  // enough pixels for interpolation to device pixels.
  dirtyRect.RoundOut();

  // GTK themes can only draw an integer number of pixels
  // (even when not snapped).
  LayoutDeviceIntRect widgetRect(0, 0, NS_lround(rect.Width()),
                                 NS_lround(rect.Height()));

  // This is the rectangle that will actually be drawn, in gdk pixels
  LayoutDeviceIntRect drawingRect(
      int32_t(dirtyRect.X()), int32_t(dirtyRect.Y()),
      int32_t(dirtyRect.Width()), int32_t(dirtyRect.Height()));
  if (widgetRect.IsEmpty() ||
      !drawingRect.IntersectRect(widgetRect, drawingRect)) {
    return NS_OK;
  }

  NS_ASSERTION(!IsWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance),
               "Trying to render an unsafe widget!");

  bool safeState = IsWidgetStateSafe(mSafeWidgetStates, aAppearance, &state);
  if (!safeState) {
    gLastGdkError = 0;
    gdk_error_trap_push();
  }

  Transparency transparency = GetWidgetTransparency(aFrame, aAppearance);

  // gdk rectangles are wrt the drawing rect.
  auto scaleFactor = GetWidgetScaleFactor(aFrame);
  LayoutDeviceIntRect gdkDevRect(-drawingRect.TopLeft(), widgetRect.Size());

  auto gdkCssRect = CSSIntRect::RoundIn(gdkDevRect / scaleFactor);
  GdkRectangle gdk_rect = {gdkCssRect.x, gdkCssRect.y, gdkCssRect.width,
                           gdkCssRect.height};

  // Save actual widget scale to GtkWidgetState as we don't provide
  // the frame to gtk3drawing routines.
  state.image_scale = std::ceil(scaleFactor.scale);

  // translate everything so (0,0) is the top left of the drawingRect
  gfxPoint origin = rect.TopLeft() + drawingRect.TopLeft().ToUnknownPoint();

  DrawThemeWithCairo(ctx, aContext->GetDrawTarget(), state, gtkWidgetType,
                     flags, direction, scaleFactor.scale, snapped,
                     ToPoint(origin), drawingRect.Size().ToUnknownSize(),
                     gdk_rect, transparency);

  if (!safeState) {
    // gdk_flush() call from expose event crashes Gtk+ on Wayland
    // (Gnome BZ #773307)
    if (GdkIsX11Display()) {
      gdk_flush();
    }
    gLastGdkError = gdk_error_trap_pop();

    if (gLastGdkError) {
#ifdef DEBUG
      printf(
          "GTK theme failed for widget type %d, error was %d, state was "
          "[active=%d,focused=%d,inHover=%d,disabled=%d]\n",
          static_cast<int>(aAppearance), gLastGdkError, state.active,
          state.focused, state.inHover, state.disabled);
#endif
      NS_WARNING("GTK theme failed; disabling unsafe widget");
      SetWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance);
      // force refresh of the window, because the widget was not
      // successfully drawn it must be redrawn using the default look
      RefreshWidgetWindow(aFrame);
    } else {
      SetWidgetStateSafe(mSafeWidgetStates, aAppearance, &state);
    }
  }

  return NS_OK;
}

bool nsNativeThemeGTK::CreateWebRenderCommandsForWidget(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const mozilla::layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager, nsIFrame* aFrame,
    StyleAppearance aAppearance, const nsRect& aRect) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::CreateWebRenderCommandsForWidget(
        aBuilder, aResources, aSc, aManager, aFrame, aAppearance, aRect);
  }
  if (aAppearance == StyleAppearance::MozWindowDecorations && GdkIsWaylandDisplay()) {
    // On wayland we don't need to draw window decorations.
    return true;
  }
  return false;
}

WidgetNodeType nsNativeThemeGTK::NativeThemeToGtkTheme(
    StyleAppearance aAppearance, nsIFrame* aFrame) {
  WidgetNodeType gtkWidgetType;
  gint unusedFlags;

  if (!GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                            &unusedFlags)) {
    MOZ_ASSERT_UNREACHABLE("Unknown native widget to gtk widget mapping");
    return MOZ_GTK_WINDOW;
  }
  return gtkWidgetType;
}

static void FixupForVerticalWritingMode(WritingMode aWritingMode,
                                        CSSIntMargin* aResult) {
  if (aWritingMode.IsVertical()) {
    bool rtl = aWritingMode.IsBidiRTL();
    LogicalMargin logical(aWritingMode, aResult->top,
                          rtl ? aResult->left : aResult->right, aResult->bottom,
                          rtl ? aResult->right : aResult->left);
    nsMargin physical = logical.GetPhysicalMargin(aWritingMode);
    aResult->top = physical.top;
    aResult->right = physical.right;
    aResult->bottom = physical.bottom;
    aResult->left = physical.left;
  }
}

CSSIntMargin nsNativeThemeGTK::GetCachedWidgetBorder(
    nsIFrame* aFrame, StyleAppearance aAppearance,
    GtkTextDirection aDirection) {
  CSSIntMargin result;

  WidgetNodeType gtkWidgetType;
  gint unusedFlags;
  if (GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                           &unusedFlags)) {
    MOZ_ASSERT(0 <= gtkWidgetType && gtkWidgetType < MOZ_GTK_WIDGET_NODE_COUNT);
    uint8_t cacheIndex = gtkWidgetType / 8;
    uint8_t cacheBit = 1u << (gtkWidgetType % 8);

    if (mBorderCacheValid[cacheIndex] & cacheBit) {
      result = mBorderCache[gtkWidgetType];
    } else {
      moz_gtk_get_widget_border(gtkWidgetType, &result.left.value,
                                &result.top.value, &result.right.value,
                                &result.bottom.value, aDirection);
      mBorderCacheValid[cacheIndex] |= cacheBit;
      mBorderCache[gtkWidgetType] = result;
    }
  }
  FixupForVerticalWritingMode(aFrame->GetWritingMode(), &result);
  return result;
}

LayoutDeviceIntMargin nsNativeThemeGTK::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetBorder(aContext, aFrame, aAppearance);
  }

  CSSIntMargin result;
  GtkTextDirection direction = GetTextDirection(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Tab: {
      WidgetNodeType gtkWidgetType;
      gint flags;

      if (!GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                                &flags)) {
        return {};
      }
      moz_gtk_get_tab_border(&result.left.value, &result.top.value,
                             &result.right.value, &result.bottom.value,
                             direction, (GtkTabFlags)flags, gtkWidgetType);
    } break;
    default: {
      result = GetCachedWidgetBorder(aFrame, aAppearance, direction);
    }
  }

  return (CSSMargin(result) * GetWidgetScaleFactor(aFrame)).Rounded();
}

bool nsNativeThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                        nsIFrame* aFrame,
                                        StyleAppearance aAppearance,
                                        LayoutDeviceIntMargin* aResult) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetPadding(aContext, aFrame, aAppearance, aResult);
  }
  switch (aAppearance) {
    case StyleAppearance::Toolbarbutton:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    default:
      break;
  }

  return false;
}

bool nsNativeThemeGTK::GetWidgetOverflow(nsDeviceContext* aContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         nsRect* aOverflowRect) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::GetWidgetOverflow(aContext, aFrame, aAppearance,
                                    aOverflowRect);
  }
  return false;
}

auto nsNativeThemeGTK::IsWidgetNonNative(nsIFrame* aFrame,
                                         StyleAppearance aAppearance)
    -> NonNative {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return NonNative::Always;
  }

  // If the current GTK theme color scheme matches our color-scheme, then we
  // can draw a native widget.
  if (LookAndFeel::ColorSchemeForFrame(aFrame) ==
      PreferenceSheet::ColorSchemeForChrome()) {
    return NonNative::No;
  }

  // If the non-native theme doesn't support the widget then oh well...
  if (!Theme::ThemeSupportsWidget(aFrame->PresContext(), aFrame, aAppearance)) {
    return NonNative::No;
  }

  return NonNative::BecauseColorMismatch;
}

bool nsNativeThemeGTK::IsWidgetAlwaysNonNative(nsIFrame* aFrame,
                                               StyleAppearance aAppearance) {
  return Theme::IsWidgetAlwaysNonNative(aFrame, aAppearance) ||
         aAppearance == StyleAppearance::MozMenulistArrowButton ||
         aAppearance == StyleAppearance::Textfield ||
         aAppearance == StyleAppearance::NumberInput ||
         aAppearance == StyleAppearance::PasswordInput ||
         aAppearance == StyleAppearance::Textarea ||
         aAppearance == StyleAppearance::Checkbox ||
         aAppearance == StyleAppearance::Radio ||
         aAppearance == StyleAppearance::Button ||
         aAppearance == StyleAppearance::Toolbarbutton ||
         aAppearance == StyleAppearance::Listbox ||
         aAppearance == StyleAppearance::Menulist ||
         aAppearance == StyleAppearance::ProgressBar ||
         aAppearance == StyleAppearance::Progresschunk ||
         aAppearance == StyleAppearance::Range ||
         aAppearance == StyleAppearance::RangeThumb;
}

LayoutDeviceIntSize nsNativeThemeGTK::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame,
    StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetMinimumWidgetSize(aPresContext, aFrame, aAppearance);
  }
  return {};
}

bool nsNativeThemeGTK::WidgetAttributeChangeRequiresRepaint(
    StyleAppearance aAppearance, nsAtom* aAttribute) {
  // Some widget types just never change state.
  if (aAppearance == StyleAppearance::Progresschunk ||
      aAppearance == StyleAppearance::ProgressBar ||
      aAppearance == StyleAppearance::MozWindowDecorations) {
    return false;
  }
  return Theme::WidgetAttributeChangeRequiresRepaint(aAppearance, aAttribute);
}

NS_IMETHODIMP
nsNativeThemeGTK::ThemeChanged() {
  memset(mDisabledWidgetTypes, 0, sizeof(mDisabledWidgetTypes));
  memset(mSafeWidgetStates, 0, sizeof(mSafeWidgetStates));
  memset(mBorderCacheValid, 0, sizeof(mBorderCacheValid));
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      StyleAppearance aAppearance) {
  if (IsWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance)) {
    return false;
  }

  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::ThemeSupportsWidget(aPresContext, aFrame, aAppearance);
  }

  switch (aAppearance) {
    case StyleAppearance::Tab:
    // case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::MozWindowTitlebarMaximized:
    case StyleAppearance::MozWindowDecorations:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    default:
      break;
  }

  return false;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::WidgetIsContainer(StyleAppearance aAppearance) {
  // XXXdwh At some point flesh all of this out.
  return true;
}

bool nsNativeThemeGTK::ThemeDrawsFocusForWidget(nsIFrame* aFrame,
                                                StyleAppearance aAppearance) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::ThemeDrawsFocusForWidget(aFrame, aAppearance);
  }
  return false;
}

nsITheme::Transparency nsNativeThemeGTK::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::GetWidgetTransparency(aFrame, aAppearance);
  }

  return eUnknownTransparency;
}

already_AddRefed<Theme> do_CreateNativeThemeDoNotUseDirectly() {
  if (gfxPlatform::IsHeadless()) {
    return do_AddRef(new Theme(Theme::ScrollbarStyle()));
  }
  return do_AddRef(new nsNativeThemeGTK());
}
