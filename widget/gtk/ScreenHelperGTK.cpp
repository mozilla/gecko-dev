/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperGTK.h"

#ifdef MOZ_X11
#  include <gdk/gdkx.h>
#  include <X11/Xlib.h>
#  include "X11UndefineNone.h"
#endif /* MOZ_X11 */
#ifdef MOZ_WAYLAND
#  include <gdk/gdkwayland.h>
#endif /* MOZ_WAYLAND */
#include <dlfcn.h>
#include <gtk/gtk.h>

#include "gfxPlatformGtk.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "nsGtkUtils.h"
#include "nsTArray.h"
#include "nsWindow.h"

struct wl_registry;

#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#endif

namespace mozilla::widget {

#ifdef MOZ_LOGGING
static LazyLogModule sScreenLog("WidgetScreen");
#  define LOG_SCREEN(...) MOZ_LOG(sScreenLog, LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOG_SCREEN(...)
#endif /* MOZ_LOGGING */

using GdkMonitor = struct _GdkMonitor;

class ScreenGetterGtk final {
 public:
  ScreenGetterGtk() = default;
  ~ScreenGetterGtk();

  void Init();

#ifdef MOZ_X11
  Atom NetWorkareaAtom() { return mNetWorkareaAtom; }
#endif

  // For internal use from signal callback functions
  void RefreshScreens();

 private:
  GdkWindow* mRootWindow = nullptr;
#ifdef MOZ_X11
  Atom mNetWorkareaAtom = 0;
#endif
};

static GdkMonitor* GdkDisplayGetMonitor(GdkDisplay* aDisplay, int aMonitorNum) {
  static auto s_gdk_display_get_monitor = (GdkMonitor * (*)(GdkDisplay*, int))
      dlsym(RTLD_DEFAULT, "gdk_display_get_monitor");
  if (!s_gdk_display_get_monitor) {
    return nullptr;
  }
  return s_gdk_display_get_monitor(aDisplay, aMonitorNum);
}

#ifdef MOZ_WAYLAND
struct HDRMonitorInfo {
  int mMonitorNum = 0;
  bool mIsHDR = false;
  bool mIsDone = false;
};

void image_description_info_done(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1) {
  auto* info = static_cast<HDRMonitorInfo*>(data);
  info->mIsDone = true;
  LOG_SCREEN("Monitor num [%d] Done", info->mMonitorNum);
}

/**
 * ICC profile matching the image description
 *
 * The icc argument provides a file descriptor to the client
 * which may be memory-mapped to provide the ICC profile matching
 * the image description. The fd is read-only, and if mapped then
 * it must be mapped with MAP_PRIVATE by the client.
 *
 * The ICC profile version and other details are determined by the
 * compositor. There is no provision for a client to ask for a
 * specific kind of a profile.
 * @param icc ICC profile file descriptor
 * @param icc_size ICC profile size, in bytes
 */
void image_description_info_icc_file(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    int32_t icc, uint32_t icc_size) {}
/**
 * primaries as chromaticity coordinates
 *
 * Delivers the primary color volume primaries and white point
 * using CIE 1931 xy chromaticity coordinates.
 *
 * Each coordinate value is multiplied by 1 million to get the
 * argument value to carry precision of 6 decimals.
 * @param r_x Red x * 1M
 * @param r_y Red y * 1M
 * @param g_x Green x * 1M
 * @param g_y Green y * 1M
 * @param b_x Blue x * 1M
 * @param b_y Blue y * 1M
 * @param w_x White x * 1M
 * @param w_y White y * 1M
 */
void image_description_info_primaries(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x,
    int32_t b_y, int32_t w_x, int32_t w_y) {}
/**
 * named primaries
 *
 * Delivers the primary color volume primaries and white point
 * using an explicitly enumerated named set.
 * @param primaries named primaries
 */
void image_description_info_primaries_named(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t primaries) {}

/**
 * transfer characteristic as a power curve
 *
 * The color component transfer characteristic of this image
 * description is a pure power curve. This event provides the
 * exponent of the power function. This curve represents the
 * conversion from electrical to optical pixel or color values.
 *
 * The curve exponent has been multiplied by 10000 to get the
 * argument eexp value to carry the precision of 4 decimals.
 * @param eexp the exponent * 10000
 */
void image_description_info_tf_power(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t eexp) {}
/**
 * named transfer characteristic
 *
 * Delivers the transfer characteristic using an explicitly
 * enumerated named function.
 * @param tf named transfer function
 */
void image_description_info_tf_named(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t tf) {}
/**
 * primary color volume luminance range and reference white
 *
 * Delivers the primary color volume luminance range and the
 * reference white luminance level. These values include the
 * minimum display emission and ambient flare luminances, assumed
 * to be optically additive and have the chromaticity of the
 * primary color volume white point.
 *
 * The minimum luminance is multiplied by 10000 to get the argument
 * 'min_lum' value and carries precision of 4 decimals. The maximum
 * luminance and reference white luminance values are unscaled.
 * @param min_lum minimum luminance (cd/m²) * 10000
 * @param max_lum maximum luminance (cd/m²)
 * @param reference_lum reference white luminance (cd/m²)
 */
void image_description_info_luminances(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
  auto* info = static_cast<HDRMonitorInfo*>(data);
  LOG_SCREEN("Monitor num [%d] Luminance min %d max %d reference %d",
             info->mMonitorNum, min_lum, max_lum, reference_lum);
  info->mIsHDR = max_lum > reference_lum;
}
/**
 * target primaries as chromaticity coordinates
 *
 * Provides the color primaries and white point of the target
 * color volume using CIE 1931 xy chromaticity coordinates. This is
 * compatible with the SMPTE ST 2086 definition of HDR static
 * metadata for mastering displays.
 *
 * While primary color volume is about how color is encoded, the
 * target color volume is the actually displayable color volume. If
 * target color volume is equal to the primary color volume, then
 * this event is not sent.
 *
 * Each coordinate value is multiplied by 1 million to get the
 * argument value to carry precision of 6 decimals.
 * @param r_x Red x * 1M
 * @param r_y Red y * 1M
 * @param g_x Green x * 1M
 * @param g_y Green y * 1M
 * @param b_x Blue x * 1M
 * @param b_y Blue y * 1M
 * @param w_x White x * 1M
 * @param w_y White y * 1M
 */
void image_description_info_target_primaries(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x,
    int32_t b_y, int32_t w_x, int32_t w_y) {}
/**
 * target luminance range
 *
 * Provides the luminance range that the image description is
 * targeting as the minimum and maximum absolute luminance L. These
 * values include the minimum display emission and ambient flare
 * luminances, assumed to be optically additive and have the
 * chromaticity of the primary color volume white point. This
 * should be compatible with the SMPTE ST 2086 definition of HDR
 * static metadata.
 *
 * This luminance range is only theoretical and may not correspond
 * to the luminance of light emitted on an actual display.
 *
 * Min L value is multiplied by 10000 to get the argument min_lum
 * value and carry precision of 4 decimals. Max L value is unscaled
 * for max_lum.
 * @param min_lum min L (cd/m²) * 10000
 * @param max_lum max L (cd/m²)
 */
void image_description_info_target_luminance(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t min_lum, uint32_t max_lum) {}
/**
 * target maximum content light level
 *
 * Provides the targeted max_cll of the image description.
 * max_cll is defined by CTA-861-H.
 *
 * This luminance is only theoretical and may not correspond to the
 * luminance of light emitted on an actual display.
 * @param max_cll Maximum content light-level (cd/m²)
 */
void image_description_info_target_max_cll(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t max_cll) {}
/**
 * target maximum frame-average light level
 *
 * Provides the targeted max_fall of the image description.
 * max_fall is defined by CTA-861-H.
 *
 * This luminance is only theoretical and may not correspond to the
 * luminance of light emitted on an actual display.
 * @param max_fall Maximum frame-average light level (cd/m²)
 */
void image_description_info_target_max_fall(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1,
    uint32_t max_fall) {}

static const struct wp_image_description_info_v1_listener
    image_description_info_listener{image_description_info_done,
                                    image_description_info_icc_file,
                                    image_description_info_primaries,
                                    image_description_info_primaries_named,
                                    image_description_info_tf_power,
                                    image_description_info_tf_named,
                                    image_description_info_luminances,
                                    image_description_info_target_primaries,
                                    image_description_info_target_luminance,
                                    image_description_info_target_max_cll,
                                    image_description_info_target_max_fall};

static bool IsMonitorHDR(gint aMonitorNum) {
  if (!WaylandDisplayGet() || !WaylandDisplayGet()->GetColorManager()) {
    return false;
  }
  GdkMonitor* monitor =
      GdkDisplayGetMonitor(gdk_display_get_default(), aMonitorNum);
  if (!monitor) {
    return monitor;
  }
  static auto s_gdk_wayland_monitor_get_wl_output =
      (struct wl_output * (*)(GdkMonitor*))
          dlsym(RTLD_DEFAULT, "gdk_wayland_monitor_get_wl_output");
  if (!s_gdk_wayland_monitor_get_wl_output) {
    return false;
  }
  auto wlOutput = s_gdk_wayland_monitor_get_wl_output(monitor);
  if (!wlOutput) {
    return false;
  }
  auto output = wp_color_manager_v1_get_output(
      WaylandDisplayGet()->GetColorManager(), wlOutput);
  auto description =
      wp_color_management_output_v1_get_image_description(output);
  auto descriptionInfo = wp_image_description_v1_get_information(description);

  HDRMonitorInfo monitorInfo;
  monitorInfo.mMonitorNum = aMonitorNum;
  wp_image_description_info_v1_add_listener(
      descriptionInfo, &image_description_info_listener, &monitorInfo);

  WaylandDisplayGet()->RequestAsyncRoundtrip();
  WaylandDisplayGet()->WaitForAsyncRoundtrips();

  wp_image_description_v1_destroy(description);
  wp_color_management_output_v1_destroy(output);

  MOZ_DIAGNOSTIC_ASSERT(monitorInfo.mIsDone);
  return monitorInfo.mIsHDR;
}
#endif

RefPtr<Screen> ScreenHelperGTK::GetScreenForWindow(nsWindow* aWindow) {
  LOG_SCREEN("GetScreenForWindow() [%p]", aWindow);

  static auto s_gdk_display_get_monitor_at_window =
      (GdkMonitor * (*)(GdkDisplay*, GdkWindow*))
          dlsym(RTLD_DEFAULT, "gdk_display_get_monitor_at_window");

  if (!s_gdk_display_get_monitor_at_window) {
    LOG_SCREEN("  failed, missing Gtk helpers");
    return nullptr;
  }

  GdkWindow* gdkWindow = aWindow->GetToplevelGdkWindow();
  if (!gdkWindow) {
    LOG_SCREEN("  failed, can't get GdkWindow");
    return nullptr;
  }

  GdkDisplay* display = gdk_display_get_default();
  GdkMonitor* monitor = s_gdk_display_get_monitor_at_window(display, gdkWindow);
  if (!monitor) {
    LOG_SCREEN("  failed, can't get monitor for GdkWindow");
    return nullptr;
  }

  int index = -1;
  while (GdkMonitor* m = GdkDisplayGetMonitor(display, ++index)) {
    if (m == monitor) {
      return ScreenManager::GetSingleton().CurrentScreenList().SafeElementAt(
          index);
    }
  }

  LOG_SCREEN("  Couldn't find monitor %p", monitor);
  return nullptr;
}

static StaticAutoPtr<ScreenGetterGtk> gScreenGetter;

static void monitors_changed(GdkScreen* aScreen, gpointer aClosure) {
  LOG_SCREEN("Received monitors-changed event");
  auto* self = static_cast<ScreenGetterGtk*>(aClosure);
  self->RefreshScreens();
}

static void screen_resolution_changed(GdkScreen* aScreen, GParamSpec* aPspec,
                                      ScreenGetterGtk* self) {
  self->RefreshScreens();
}

static GdkFilterReturn root_window_event_filter(GdkXEvent* aGdkXEvent,
                                                GdkEvent* aGdkEvent,
                                                gpointer aClosure) {
#ifdef MOZ_X11
  ScreenGetterGtk* self = static_cast<ScreenGetterGtk*>(aClosure);
  XEvent* xevent = static_cast<XEvent*>(aGdkXEvent);

  switch (xevent->type) {
    case PropertyNotify: {
      XPropertyEvent* propertyEvent = &xevent->xproperty;
      if (propertyEvent->atom == self->NetWorkareaAtom()) {
        LOG_SCREEN("Work area size changed");
        self->RefreshScreens();
      }
    } break;
    default:
      break;
  }
#endif

  return GDK_FILTER_CONTINUE;
}

void ScreenGetterGtk::Init() {
  LOG_SCREEN("ScreenGetterGtk created");
  GdkScreen* defaultScreen = gdk_screen_get_default();
  if (!defaultScreen) {
    // Sometimes we don't initial X (e.g., xpcshell)
    MOZ_LOG(sScreenLog, LogLevel::Debug,
            ("defaultScreen is nullptr, running headless"));
    return;
  }
  mRootWindow = gdk_get_default_root_window();
  MOZ_ASSERT(mRootWindow);

  g_object_ref(mRootWindow);

  // GDK_PROPERTY_CHANGE_MASK ==> PropertyChangeMask, for PropertyNotify
  gdk_window_set_events(mRootWindow,
                        GdkEventMask(gdk_window_get_events(mRootWindow) |
                                     GDK_PROPERTY_CHANGE_MASK));

  g_signal_connect(defaultScreen, "monitors-changed",
                   G_CALLBACK(monitors_changed), this);
  // Use _after to ensure this callback is run after gfxPlatformGtk.cpp's
  // handler.
  g_signal_connect_after(defaultScreen, "notify::resolution",
                         G_CALLBACK(screen_resolution_changed), this);
#ifdef MOZ_X11
  gdk_window_add_filter(mRootWindow, root_window_event_filter, this);
  if (GdkIsX11Display()) {
    mNetWorkareaAtom = XInternAtom(GDK_WINDOW_XDISPLAY(mRootWindow),
                                   "_NET_WORKAREA", X11False);
  }
#endif
  RefreshScreens();
}

ScreenGetterGtk::~ScreenGetterGtk() {
  if (mRootWindow) {
    g_signal_handlers_disconnect_by_data(gdk_screen_get_default(), this);

    gdk_window_remove_filter(mRootWindow, root_window_event_filter, this);
    g_object_unref(mRootWindow);
    mRootWindow = nullptr;
  }
}

static uint32_t GetGTKPixelDepth() {
  GdkVisual* visual = gdk_screen_get_system_visual(gdk_screen_get_default());
  return gdk_visual_get_depth(visual);
}

static already_AddRefed<Screen> MakeScreenGtk(GdkScreen* aScreen,
                                              gint aMonitorNum) {
  gint gdkScaleFactor = ScreenHelperGTK::GetGTKMonitorScaleFactor(aMonitorNum);

  // gdk_screen_get_monitor_geometry / workarea returns application pixels
  // (desktop pixels), so we need to convert it to device pixels with
  // gdkScaleFactor.
  gint geometryScaleFactor = gdkScaleFactor;

  gint refreshRate = [&] {
    // Since gtk 3.22
    static auto s_gdk_monitor_get_refresh_rate = (int (*)(GdkMonitor*))dlsym(
        RTLD_DEFAULT, "gdk_monitor_get_refresh_rate");
    if (!s_gdk_monitor_get_refresh_rate) {
      return 0;
    }
    GdkMonitor* monitor =
        GdkDisplayGetMonitor(gdk_display_get_default(), aMonitorNum);
    if (!monitor) {
      return 0;
    }
    // Convert to Hz.
    return NSToIntRound(s_gdk_monitor_get_refresh_rate(monitor) / 1000.0f);
  }();

  GdkRectangle workarea;
  gdk_screen_get_monitor_workarea(aScreen, aMonitorNum, &workarea);
  LayoutDeviceIntRect availRect(workarea.x * geometryScaleFactor,
                                workarea.y * geometryScaleFactor,
                                workarea.width * geometryScaleFactor,
                                workarea.height * geometryScaleFactor);
  LayoutDeviceIntRect rect;
  DesktopToLayoutDeviceScale contentsScale(1.0);
  if (GdkIsX11Display()) {
    GdkRectangle monitor;
    gdk_screen_get_monitor_geometry(aScreen, aMonitorNum, &monitor);
    rect = LayoutDeviceIntRect(monitor.x * geometryScaleFactor,
                               monitor.y * geometryScaleFactor,
                               monitor.width * geometryScaleFactor,
                               monitor.height * geometryScaleFactor);
  } else {
    // Don't report screen shift in Wayland, see bug 1795066.
    availRect.MoveTo(0, 0);
    // We use Gtk workarea on Wayland as it matches our needs (Bug 1732682).
    rect = availRect;
    // Use per-monitor scaling factor in Wayland.
    contentsScale.scale = gdkScaleFactor;
  }

  uint32_t pixelDepth = GetGTKPixelDepth();
  if (pixelDepth == 32) {
    // If a device uses 32 bits per pixel, it's still only using 8 bits
    // per color component, which is what our callers want to know.
    // (Some devices report 32 and some devices report 24.)
    pixelDepth = 24;
  }

  CSSToLayoutDeviceScale defaultCssScale(gdkScaleFactor);

  float dpi = 96.0f;
  gint heightMM = gdk_screen_get_monitor_height_mm(aScreen, aMonitorNum);
  if (heightMM > 0) {
    dpi = rect.height / (heightMM / MM_PER_INCH_FLOAT);
  }

  bool isHDR = false;
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    isHDR = IsMonitorHDR(aMonitorNum);
  }
#endif

  LOG_SCREEN(
      "New monitor %d size [%d,%d -> %d x %d] depth %d scale %f CssScale %f  "
      "DPI %f refresh %d HDR %d]",
      aMonitorNum, rect.x, rect.y, rect.width, rect.height, pixelDepth,
      contentsScale.scale, defaultCssScale.scale, dpi, refreshRate, isHDR);
  return MakeAndAddRef<Screen>(
      rect, availRect, pixelDepth, pixelDepth, refreshRate, contentsScale,
      defaultCssScale, dpi, Screen::IsPseudoDisplay::No, Screen::IsHDR(isHDR));
}

void ScreenGetterGtk::RefreshScreens() {
  LOG_SCREEN("ScreenGetterGtk::RefreshScreens()");
  AutoTArray<RefPtr<Screen>, 4> screenList;

  GdkScreen* defaultScreen = gdk_screen_get_default();
  gint numScreens = gdk_screen_get_n_monitors(defaultScreen);
  LOG_SCREEN("GDK reports %d screens", numScreens);

  for (gint i = 0; i < numScreens; i++) {
    screenList.AppendElement(MakeScreenGtk(defaultScreen, i));
  }

  ScreenManager::Refresh(std::move(screenList));
}

gint ScreenHelperGTK::GetGTKMonitorScaleFactor(gint aMonitorNum) {
  MOZ_ASSERT(NS_IsMainThread());
  GdkScreen* screen = gdk_screen_get_default();
  return aMonitorNum < gdk_screen_get_n_monitors(screen)
             ? gdk_screen_get_monitor_scale_factor(screen, aMonitorNum)
             : 1;
}

ScreenHelperGTK::ScreenHelperGTK() {
  gScreenGetter = new ScreenGetterGtk();
  gScreenGetter->Init();
}

int ScreenHelperGTK::GetMonitorCount() {
  return gdk_screen_get_n_monitors(gdk_screen_get_default());
}

ScreenHelperGTK::~ScreenHelperGTK() { gScreenGetter = nullptr; }

}  // namespace mozilla::widget
