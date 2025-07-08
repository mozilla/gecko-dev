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

GdkWindow* ScreenHelperGTK::sRootWindow = nullptr;
StaticRefPtr<ScreenGetterGtk> ScreenHelperGTK::gLastScreenGetter;
int ScreenHelperGTK::gLastSerial = 0;

class ScreenGetterGtk final {
 public:
  NS_INLINE_DECL_REFCOUNTING(ScreenGetterGtk)

  explicit ScreenGetterGtk(int aSerial);
  void AddScreen(RefPtr<Screen> aScreen);
  bool AddScreenHDRAsync(unsigned int aMonitor);

 protected:
  ~ScreenGetterGtk() = default;

 private:
  AutoTArray<RefPtr<Screen>, 4> mScreenList;
  int mSerial = 0;
  unsigned int mMonitorNum = 0;
};

static GdkMonitor* GdkDisplayGetMonitor(GdkDisplay* aDisplay,
                                        unsigned int aMonitor) {
  static auto s_gdk_display_get_monitor = (GdkMonitor * (*)(GdkDisplay*, int))
      dlsym(RTLD_DEFAULT, "gdk_display_get_monitor");
  if (!s_gdk_display_get_monitor) {
    return nullptr;
  }
  return s_gdk_display_get_monitor(aDisplay, aMonitor);
}

static uint32_t GetGTKPixelDepth() {
  GdkVisual* visual = gdk_screen_get_system_visual(gdk_screen_get_default());
  return gdk_visual_get_depth(visual);
}

static already_AddRefed<Screen> MakeScreenGtk(unsigned int aMonitor,
                                              bool aIsHDR) {
  GdkScreen* defaultScreen = gdk_screen_get_default();
  gint gdkScaleFactor = ScreenHelperGTK::GetGTKMonitorScaleFactor(aMonitor);

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
        GdkDisplayGetMonitor(gdk_display_get_default(), aMonitor);
    if (!monitor) {
      return 0;
    }
    // Convert to Hz.
    return NSToIntRound(s_gdk_monitor_get_refresh_rate(monitor) / 1000.0f);
  }();

  GdkRectangle workarea;
  gdk_screen_get_monitor_workarea(defaultScreen, aMonitor, &workarea);
  LayoutDeviceIntRect availRect(workarea.x * geometryScaleFactor,
                                workarea.y * geometryScaleFactor,
                                workarea.width * geometryScaleFactor,
                                workarea.height * geometryScaleFactor);
  LayoutDeviceIntRect rect;
  DesktopToLayoutDeviceScale contentsScale(1.0);
  if (GdkIsX11Display()) {
    GdkRectangle monitor;
    gdk_screen_get_monitor_geometry(defaultScreen, aMonitor, &monitor);
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
  gint heightMM = gdk_screen_get_monitor_height_mm(defaultScreen, aMonitor);
  if (heightMM > 0) {
    dpi = rect.height / (heightMM / MM_PER_INCH_FLOAT);
  }

  LOG_SCREEN(
      "New monitor %d size [%d,%d -> %d x %d] depth %d scale %f CssScale %f  "
      "DPI %f refresh %d HDR %d]",
      aMonitor, rect.x, rect.y, rect.width, rect.height, pixelDepth,
      contentsScale.scale, defaultCssScale.scale, dpi, refreshRate, aIsHDR);
  return MakeAndAddRef<Screen>(
      rect, availRect, pixelDepth, pixelDepth, refreshRate, contentsScale,
      defaultCssScale, dpi, Screen::IsPseudoDisplay::No, Screen::IsHDR(aIsHDR));
}

#ifdef MOZ_WAYLAND
class WaylandMonitor {
 public:
  WaylandMonitor(ScreenGetterGtk* aScreenGetter, unsigned int aMonitor,
                 wp_color_management_output_v1* aOutput,
                 wp_image_description_v1* aDescription)
      : mScreenGetter(aScreenGetter),
        mMonitor(aMonitor),
        mOutput(aOutput),
        mDescription(aDescription) {
    LOG_SCREEN("WaylandMonitor()[%p] monitor %d", this, mMonitor);
  }
  ~WaylandMonitor() {
    LOG_SCREEN("~WaylandMonitor()[%p]", this);
    MozClearPointer(mDescription, wp_image_description_v1_destroy);
    MozClearPointer(mOutput, wp_color_management_output_v1_destroy);
    mScreenGetter = nullptr;
  }

  unsigned int GetMonitor() const { return mMonitor; }

  void SetHDR(bool aIsHDR) {
    LOG_SCREEN("WaylandMonitor()[%p]: monitor num [%d] HDR %d", this, mMonitor,
               aIsHDR);
    mIsHDR = aIsHDR;
  }

  void Done() {
    LOG_SCREEN("WaylandMonitor()[%p] Done", this);
    mScreenGetter->AddScreen(MakeScreenGtk(mMonitor, mIsHDR));
  }

 private:
  RefPtr<ScreenGetterGtk> mScreenGetter;
  unsigned int mMonitor = 0;
  wp_color_management_output_v1* mOutput = nullptr;
  wp_image_description_v1* mDescription = nullptr;
  bool mIsHDR = false;
};

void image_description_info_done(
    void* data,
    struct wp_image_description_info_v1* wp_image_description_info_v1) {
  auto* info = static_cast<WaylandMonitor*>(data);
  info->Done();
  delete info;
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
  auto* info = static_cast<WaylandMonitor*>(data);
  LOG_SCREEN("Monitor num [%d] Luminance min %d max %d reference %d",
             info->GetMonitor(), min_lum, max_lum, reference_lum);
  info->SetHDR(max_lum > reference_lum);
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

bool ScreenGetterGtk::AddScreenHDRAsync(unsigned int aMonitor) {
  MOZ_DIAGNOSTIC_ASSERT(WaylandDisplayGet()->GetColorManager());
  GdkMonitor* monitor =
      GdkDisplayGetMonitor(gdk_display_get_default(), aMonitor);
  if (!monitor) {
    LOG_SCREEN("ScreenGetterGtk::AddScreenHDRAsync() failed to get monitor %d",
               aMonitor);
    return false;
  }
  static auto s_gdk_wayland_monitor_get_wl_output =
      (struct wl_output * (*)(GdkMonitor*))
          dlsym(RTLD_DEFAULT, "gdk_wayland_monitor_get_wl_output");
  if (!s_gdk_wayland_monitor_get_wl_output) {
    LOG_SCREEN(
        "ScreenGetterGtk::AddScreenHDRAsync() missing "
        "gdk_wayland_monitor_get_wl_output");
    return false;
  }
  auto wlOutput = s_gdk_wayland_monitor_get_wl_output(monitor);
  if (!wlOutput) {
    LOG_SCREEN("ScreenGetterGtk::AddScreenHDRAsync() missing wl_output");
    return false;
  }

  LOG_SCREEN("ScreenGetterGtk::AddScreenHDR() monitor %d", aMonitor);
  auto* output = wp_color_manager_v1_get_output(
      WaylandDisplayGet()->GetColorManager(), wlOutput);
  auto* description =
      wp_color_management_output_v1_get_image_description(output);
  auto descriptionInfo = wp_image_description_v1_get_information(description);
  wp_image_description_info_v1_add_listener(
      descriptionInfo, &image_description_info_listener,
      new WaylandMonitor(this, aMonitor, output, description));
  return true;
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

void ScreenGetterGtk::AddScreen(RefPtr<Screen> aScreen) {
  mScreenList.AppendElement(std::move(aScreen));
  MOZ_DIAGNOSTIC_ASSERT(mScreenList.Length() <= mMonitorNum);

  if (mScreenList.Length() == mMonitorNum) {
    if (mSerial != ScreenHelperGTK::GetLastSerial()) {
      MOZ_DIAGNOSTIC_ASSERT(mSerial <= ScreenHelperGTK::GetLastSerial());
      LOG_SCREEN(
          "ScreenGetterGtk::AddScreen(): rejected, old wrong serial %d latest "
          "%d",
          mSerial, ScreenHelperGTK::GetLastSerial());
      return;
    }

    LOG_SCREEN("ScreenGetterGtk::AddScreen(): Set screens, serial %d ",
               mSerial);
    ScreenManager::Refresh(std::move(mScreenList));
  }
}

ScreenGetterGtk::ScreenGetterGtk(int aSerial)
    : mSerial(aSerial),
      mMonitorNum(gdk_screen_get_n_monitors(gdk_screen_get_default())) {
  LOG_SCREEN("ScreenGetterGtk(): monitor num %d", mMonitorNum);
#ifdef MOZ_WAYLAND
  LOG_SCREEN("HDR Protocol %s",
             GdkIsWaylandDisplay() && WaylandDisplayGet()->IsHDREnabled()
                 ? "present"
                 : "missing");
#endif

  for (unsigned int i = 0; i < mMonitorNum; i++) {
#ifdef MOZ_WAYLAND
    if (GdkIsWaylandDisplay() && WaylandDisplayGet()->IsHDREnabled()) {
      if (AddScreenHDRAsync(i)) {
        continue;
      }
    }
#endif
    AddScreen(MakeScreenGtk(i, /* aIsHDR */ false));
  }
}

void ScreenHelperGTK::RequestRefreshScreens() {
  LOG_SCREEN("ScreenHelperGTK::RequestRefreshScreens");

  gLastSerial++;
  gLastScreenGetter = new ScreenGetterGtk(gLastSerial);
}

gint ScreenHelperGTK::GetGTKMonitorScaleFactor(gint aMonitor) {
  MOZ_ASSERT(NS_IsMainThread());
  GdkScreen* screen = gdk_screen_get_default();
  return aMonitor < gdk_screen_get_n_monitors(screen)
             ? gdk_screen_get_monitor_scale_factor(screen, aMonitor)
             : 1;
}

static void monitors_changed(GdkScreen* aScreen, gpointer unused) {
  LOG_SCREEN("Received monitors-changed event");
  ScreenHelperGTK::RequestRefreshScreens();
}

static void screen_resolution_changed(GdkScreen* aScreen, GParamSpec* aPspec,
                                      gpointer unused) {
  LOG_SCREEN("Received resolution-changed event");
  ScreenHelperGTK::RequestRefreshScreens();
}

static GdkFilterReturn root_window_event_filter(GdkXEvent* aGdkXEvent,
                                                GdkEvent* aGdkEvent,
                                                gpointer aClosure) {
#ifdef MOZ_X11
  static Atom netWorkareaAtom =
      XInternAtom(GDK_WINDOW_XDISPLAY(gdk_get_default_root_window()),
                  "_NET_WORKAREA", X11False);
  XEvent* xevent = static_cast<XEvent*>(aGdkXEvent);

  switch (xevent->type) {
    case PropertyNotify: {
      XPropertyEvent* propertyEvent = &xevent->xproperty;
      if (propertyEvent->atom == netWorkareaAtom) {
        LOG_SCREEN("X11 Work area size changed");
        ScreenHelperGTK::RequestRefreshScreens();
      }
    } break;
    default:
      break;
  }
#endif

  return GDK_FILTER_CONTINUE;
}

ScreenHelperGTK::ScreenHelperGTK() {
  LOG_SCREEN("ScreenHelperGTK created");
  GdkScreen* defaultScreen = gdk_screen_get_default();
  if (!defaultScreen) {
    // Sometimes we don't initial X (e.g., xpcshell)
    MOZ_LOG(sScreenLog, LogLevel::Debug,
            ("defaultScreen is nullptr, running headless"));
    return;
  }
  sRootWindow = gdk_get_default_root_window();
  MOZ_ASSERT(sRootWindow);
  g_object_ref(sRootWindow);

  // GDK_PROPERTY_CHANGE_MASK ==> PropertyChangeMask, for PropertyNotify
  gdk_window_set_events(sRootWindow,
                        GdkEventMask(gdk_window_get_events(sRootWindow) |
                                     GDK_PROPERTY_CHANGE_MASK));

  g_signal_connect(defaultScreen, "monitors-changed",
                   G_CALLBACK(monitors_changed), this);
  // Use _after to ensure this callback is run after gfxPlatformGtk.cpp's
  // handler.
  g_signal_connect_after(defaultScreen, "notify::resolution",
                         G_CALLBACK(screen_resolution_changed), this);
#ifdef MOZ_X11
  gdk_window_add_filter(sRootWindow, root_window_event_filter, this);
#endif
  RequestRefreshScreens();
}

int ScreenHelperGTK::GetMonitorCount() {
  return gdk_screen_get_n_monitors(gdk_screen_get_default());
}

ScreenHelperGTK::~ScreenHelperGTK() {
  if (sRootWindow) {
    g_signal_handlers_disconnect_by_data(gdk_screen_get_default(), this);
    gdk_window_remove_filter(sRootWindow, root_window_event_filter, this);
    g_object_unref(sRootWindow);
    sRootWindow = nullptr;
  }
  gLastScreenGetter = nullptr;
}

}  // namespace mozilla::widget
