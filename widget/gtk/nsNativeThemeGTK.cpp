/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeGTK.h"
#include "nsDeviceContext.h"
#include "gtk/gtk.h"
#include "nsPresContext.h"
#include "gtkdrawing.h"
#include "nsIFrame.h"

#include "gfxContext.h"
#include "mozilla/gfx/BorrowedContext.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/WidgetUtilsGtk.h"

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

// Return widget scale factor of the monitor where the window is located by the
// most part. We intentionally honor the text scale factor here in order to
// have consistent scaling with other UI elements, except for the window
// decorations, which should use unscaled pixels.
static inline CSSToLayoutDeviceScale GetWidgetScaleFactor(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::MozWindowDecorations) {
    // Window decorations can't honor the text scale.
    return CSSToLayoutDeviceScale{
        float(AppUnitsPerCSSPixel()) /
        float(aFrame->PresContext()
                  ->DeviceContext()
                  ->AppUnitsPerDevPixelAtUnitFullZoom())};
  }
  return aFrame->PresContext()->CSSToDevPixelScale();
}

nsNativeThemeGTK::nsNativeThemeGTK() : Theme(ScrollbarStyle()) {
  moz_gtk_init();
}

nsNativeThemeGTK::~nsNativeThemeGTK() { moz_gtk_shutdown(); }

static Maybe<WidgetNodeType> GeckoToGtkWidgetType(StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::MozWindowDecorations:
      return Some(MOZ_GTK_WINDOW_DECORATION);
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown widget");
      break;
  }
  return {};
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
                               const GtkDrawingParams& aParams,
                               double aScaleFactor, bool aSnapped,
                               const Point& aDrawOrigin,
                               const nsIntSize& aDrawSize,
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

          moz_gtk_widget_paint(cr, &aParams);

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

        moz_gtk_widget_paint(cr, &aParams);

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

          moz_gtk_widget_paint(cr, &aParams);
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

void nsNativeThemeGTK::DrawWidgetBackground(
    gfxContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    const nsRect& aRect, const nsRect& aDirtyRect, DrawOverflow aDrawOverflow) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::DrawWidgetBackground(aContext, aFrame, aAppearance, aRect,
                                       aDirtyRect, aDrawOverflow);
  }

  auto gtkType = GeckoToGtkWidgetType(aAppearance);
  if (!gtkType) {
    return;
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
    return;
  }

  Transparency transparency = GetWidgetTransparency(aFrame, aAppearance);

  // gdk rectangles are wrt the drawing rect.
  auto scaleFactor = GetWidgetScaleFactor(aFrame, aAppearance);
  LayoutDeviceIntRect gdkDevRect(-drawingRect.TopLeft(), widgetRect.Size());

  auto gdkCssRect = CSSIntRect::RoundIn(gdkDevRect / scaleFactor);
  GdkRectangle gdk_rect = {gdkCssRect.x, gdkCssRect.y, gdkCssRect.width,
                           gdkCssRect.height};

  // Save actual widget scale to GtkWidgetState as we don't provide
  // the frame to gtk3drawing routines.
  GtkDrawingParams params{
      .widget = *gtkType,
      .rect = gdk_rect,
      .state = GTK_STATE_FLAG_NORMAL,
      .image_scale = gint(std::ceil(scaleFactor.scale)),
  };
  if (aFrame->PresContext()->Document()->State().HasState(
          dom::DocumentState::WINDOW_INACTIVE)) {
    params.state = GtkStateFlags(gint(params.state) | GTK_STATE_FLAG_BACKDROP);
  }
  // translate everything so (0,0) is the top left of the drawingRect
  gfxPoint origin = rect.TopLeft() + drawingRect.TopLeft().ToUnknownPoint();

  DrawThemeWithCairo(ctx, aContext->GetDrawTarget(), params, scaleFactor.scale,
                     snapped, ToPoint(origin),
                     drawingRect.Size().ToUnknownSize(), transparency);
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
  if (aAppearance == StyleAppearance::MozWindowDecorations &&
      GdkIsWaylandDisplay()) {
    // On wayland we don't need to draw window decorations.
    return true;
  }
  return false;
}

LayoutDeviceIntMargin nsNativeThemeGTK::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetBorder(aContext, aFrame, aAppearance);
  }
  return {};
}

bool nsNativeThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                        nsIFrame* aFrame,
                                        StyleAppearance aAppearance,
                                        LayoutDeviceIntMargin* aResult) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetPadding(aContext, aFrame, aAppearance, aResult);
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
  if (aAppearance == StyleAppearance::MozWindowDecorations) {
    return false;
  }
  return Theme::WidgetAttributeChangeRequiresRepaint(aAppearance, aAttribute);
}

bool nsNativeThemeGTK::ThemeSupportsWidget(nsPresContext* aPresContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::ThemeSupportsWidget(aPresContext, aFrame, aAppearance);
  }

  switch (aAppearance) {
    case StyleAppearance::MozWindowDecorations:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    default:
      break;
  }

  return false;
}

bool nsNativeThemeGTK::WidgetIsContainer(StyleAppearance aAppearance) {
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
