/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "gfxPlatformGtk.h"
#include "prenv.h"

#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "gfx2DGlue.h"
#include "gfxFcPlatformFontList.h"
#include "gfxConfig.h"
#include "gfxContext.h"
#include "gfxUserFontSet.h"
#include "gfxUtils.h"
#include "gfxFT2FontBase.h"
#include "gfxPrefs.h"
#include "gfxTextRun.h"
#include "VsyncSource.h"
#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/message_loop.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/gfx/Logging.h"

#include "mozilla/gfx/2D.h"

#include "cairo.h"
#include <gtk/gtk.h>

#include "gfxImageSurface.h"
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
#include "cairo-xlib.h"
#include "mozilla/Preferences.h"
#include "mozilla/X11Util.h"

#include "GLContextProvider.h"
#include "GLContextGLX.h"
#include "GLXLibrary.h"

/* Undefine the Status from Xlib since it will conflict with system headers on
 * OSX */
#if defined(__APPLE__) && defined(Status)
#undef Status
#endif

#ifdef MOZ_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#endif /* MOZ_X11 */

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"

#define GDK_PIXMAP_SIZE_MAX 32767

#define GFX_PREF_MAX_GENERIC_SUBSTITUTIONS \
  "gfx.font_rendering.fontconfig.max_generic_substitutions"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::dom::SystemFontListEntry;

gfxPlatformGtk::gfxPlatformGtk() {
  if (!gfxPlatform::IsHeadless()) {
    gtk_init(nullptr, nullptr);
  }

  mMaxGenericSubstitutions = UNINITIALIZED_VALUE;

#ifdef MOZ_X11
  if (!gfxPlatform::IsHeadless() && XRE_IsParentProcess()) {
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default()) &&
        mozilla::Preferences::GetBool("gfx.xrender.enabled")) {
      gfxVars::SetUseXRender(true);
    }
  }
#endif

  InitBackendPrefs(GetBackendPrefs());

#ifdef MOZ_X11
  if (gfxPlatform::IsHeadless() &&
      GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    mCompositorDisplay = XOpenDisplay(nullptr);
    MOZ_ASSERT(mCompositorDisplay, "Failed to create compositor display!");
  } else {
    mCompositorDisplay = nullptr;
  }
#endif  // MOZ_X11
#ifdef MOZ_WAYLAND
  // Wayland compositors use g_get_monotonic_time() to get timestamps.
  mWaylandLastVsyncTimestamp = (g_get_monotonic_time() / 1000);
  // Set default display fps to 60
  mWaylandFrameDelay = 1000 / 60;
#endif
}

gfxPlatformGtk::~gfxPlatformGtk() {
#ifdef MOZ_X11
  if (mCompositorDisplay) {
    XCloseDisplay(mCompositorDisplay);
  }
#endif  // MOZ_X11
}

void gfxPlatformGtk::FlushContentDrawing() {
  if (gfxVars::UseXRender()) {
    XFlush(DefaultXDisplay());
  }
}

already_AddRefed<gfxASurface> gfxPlatformGtk::CreateOffscreenSurface(
    const IntSize& aSize, gfxImageFormat aFormat) {
  if (!Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<gfxASurface> newSurface;
  bool needsClear = true;
#ifdef MOZ_X11
  // XXX we really need a different interface here, something that passes
  // in more context, including the display and/or target surface type that
  // we should try to match
  GdkScreen* gdkScreen = gdk_screen_get_default();
  if (gdkScreen) {
    // When forcing PaintedLayers to use image surfaces for content,
    // force creation of gfxImageSurface surfaces.
    if (gfxVars::UseXRender() && !UseImageOffscreenSurfaces()) {
      Screen* screen = gdk_x11_screen_get_xscreen(gdkScreen);
      XRenderPictFormat* xrenderFormat =
          gfxXlibSurface::FindRenderFormat(DisplayOfScreen(screen), aFormat);

      if (xrenderFormat) {
        newSurface = gfxXlibSurface::Create(screen, xrenderFormat, aSize);
      }
    } else {
      // We're not going to use XRender, so we don't need to
      // search for a render format
      newSurface = new gfxImageSurface(aSize, aFormat);
      // The gfxImageSurface ctor zeroes this for us, no need to
      // waste time clearing again
      needsClear = false;
    }
  }
#endif

  if (!newSurface) {
    // We couldn't create a native surface for whatever reason;
    // e.g., no display, no RENDER, bad size, etc.
    // Fall back to image surface for the data.
    newSurface = new gfxImageSurface(aSize, aFormat);
  }

  if (newSurface->CairoStatus()) {
    newSurface = nullptr;  // surface isn't valid for some reason
  }

  if (newSurface && needsClear) {
    gfxUtils::ClearThebesSurface(newSurface);
  }

  return newSurface.forget();
}

nsresult gfxPlatformGtk::GetFontList(nsAtom* aLangGroup,
                                     const nsACString& aGenericFamily,
                                     nsTArray<nsString>& aListOfFonts) {
  gfxPlatformFontList::PlatformFontList()->GetFontList(
      aLangGroup, aGenericFamily, aListOfFonts);
  return NS_OK;
}

nsresult gfxPlatformGtk::UpdateFontList() {
  gfxPlatformFontList::PlatformFontList()->UpdateFontList();
  return NS_OK;
}

// xxx - this is ubuntu centric, need to go through other distros and flesh
// out a more general list
static const char kFontDejaVuSans[] = "DejaVu Sans";
static const char kFontDejaVuSerif[] = "DejaVu Serif";
static const char kFontFreeSans[] = "FreeSans";
static const char kFontFreeSerif[] = "FreeSerif";
static const char kFontTakaoPGothic[] = "TakaoPGothic";
static const char kFontTwemojiMozilla[] = "Twemoji Mozilla";
static const char kFontDroidSansFallback[] = "Droid Sans Fallback";
static const char kFontWenQuanYiMicroHei[] = "WenQuanYi Micro Hei";
static const char kFontNanumGothic[] = "NanumGothic";
static const char kFontSymbola[] = "Symbola";

void gfxPlatformGtk::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                            Script aRunScript,
                                            nsTArray<const char*>& aFontList) {
  EmojiPresentation emoji = GetEmojiPresentation(aCh);
  if (emoji != EmojiPresentation::TextOnly) {
    if (aNextCh == kVariationSelector16 ||
        (aNextCh != kVariationSelector15 &&
         emoji == EmojiPresentation::EmojiDefault)) {
      // if char is followed by VS16, try for a color emoji glyph
      aFontList.AppendElement(kFontTwemojiMozilla);
    }
  }

  aFontList.AppendElement(kFontDejaVuSerif);
  aFontList.AppendElement(kFontFreeSerif);
  aFontList.AppendElement(kFontDejaVuSans);
  aFontList.AppendElement(kFontFreeSans);
  aFontList.AppendElement(kFontSymbola);

  // add fonts for CJK ranges
  // xxx - this isn't really correct, should use the same CJK font ordering
  // as the pref font code
  if (aCh >= 0x3000 && ((aCh < 0xe000) || (aCh >= 0xf900 && aCh < 0xfff0) ||
                        ((aCh >> 16) == 2))) {
    aFontList.AppendElement(kFontTakaoPGothic);
    aFontList.AppendElement(kFontDroidSansFallback);
    aFontList.AppendElement(kFontWenQuanYiMicroHei);
    aFontList.AppendElement(kFontNanumGothic);
  }
}

void gfxPlatformGtk::ReadSystemFontList(
    InfallibleTArray<SystemFontListEntry>* retValue) {
  gfxFcPlatformFontList::PlatformFontList()->ReadSystemFontList(retValue);
}

gfxPlatformFontList* gfxPlatformGtk::CreatePlatformFontList() {
  gfxPlatformFontList* list = new gfxFcPlatformFontList();
  if (NS_SUCCEEDED(list->InitFontList())) {
    return list;
  }
  gfxPlatformFontList::Shutdown();
  return nullptr;
}

gfxFontGroup* gfxPlatformGtk::CreateFontGroup(
    const FontFamilyList& aFontFamilyList, const gfxFontStyle* aStyle,
    gfxTextPerfMetrics* aTextPerf, gfxUserFontSet* aUserFontSet,
    gfxFloat aDevToCssSize) {
  return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf, aUserFontSet,
                          aDevToCssSize);
}

FT_Library gfxPlatformGtk::GetFTLibrary() {
  return gfxFcPlatformFontList::GetFTLibrary();
}

static int32_t sDPI = 0;

int32_t gfxPlatformGtk::GetFontScaleDPI() {
  if (!sDPI) {
    // Make sure init is run so we have a resolution
    GdkScreen* screen = gdk_screen_get_default();
    gtk_settings_get_for_screen(screen);
    sDPI = int32_t(round(gdk_screen_get_resolution(screen)));
    if (sDPI <= 0) {
      // Fall back to something sane
      sDPI = 96;
    }
  }
  return sDPI;
}

double gfxPlatformGtk::GetFontScaleFactor() {
  // Integer scale factors work well with GTK window scaling, image scaling,
  // and pixel alignment, but there is a range where 1 is too small and 2 is
  // too big.  An additional step of 1.5 is added because this is common
  // scale on WINNT and at this ratio the advantages of larger rendering
  // outweigh the disadvantages from scaling and pixel mis-alignment.
  int32_t dpi = GetFontScaleDPI();
  if (dpi < 132) {
    return 1.0;
  }
  if (dpi < 168) {
    return 1.5;
  }
  return round(dpi / 96.0);
}

bool gfxPlatformGtk::UseImageOffscreenSurfaces() {
  return GetDefaultContentBackend() != mozilla::gfx::BackendType::CAIRO ||
         gfxPrefs::UseImageOffscreenSurfaces();
}

gfxImageFormat gfxPlatformGtk::GetOffscreenFormat() {
  // Make sure there is a screen
  GdkScreen* screen = gdk_screen_get_default();
  if (screen && gdk_visual_get_depth(gdk_visual_get_system()) == 16) {
    return SurfaceFormat::R5G6B5_UINT16;
  }

  return SurfaceFormat::X8R8G8B8_UINT32;
}

void gfxPlatformGtk::FontsPrefsChanged(const char* aPref) {
  // only checking for generic substitions, pass other changes up
  if (strcmp(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, aPref)) {
    gfxPlatform::FontsPrefsChanged(aPref);
    return;
  }

  mMaxGenericSubstitutions = UNINITIALIZED_VALUE;
  gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
  pfl->ClearGenericMappings();
  FlushFontAndWordCaches();
}

uint32_t gfxPlatformGtk::MaxGenericSubstitions() {
  if (mMaxGenericSubstitutions == UNINITIALIZED_VALUE) {
    mMaxGenericSubstitutions =
        Preferences::GetInt(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, 3);
    if (mMaxGenericSubstitutions < 0) {
      mMaxGenericSubstitutions = 3;
    }
  }

  return uint32_t(mMaxGenericSubstitutions);
}

bool gfxPlatformGtk::AccelerateLayersByDefault() {
  return gfxPrefs::WebRenderAll();
}

void gfxPlatformGtk::GetPlatformCMSOutputProfile(void*& mem, size_t& size) {
  mem = nullptr;
  size = 0;

#ifdef MOZ_X11
  GdkDisplay* display = gdk_display_get_default();
  if (!GDK_IS_X11_DISPLAY(display)) return;

  const char EDID1_ATOM_NAME[] = "XFree86_DDC_EDID1_RAWDATA";
  const char ICC_PROFILE_ATOM_NAME[] = "_ICC_PROFILE";

  Atom edidAtom, iccAtom;
  Display* dpy = GDK_DISPLAY_XDISPLAY(display);
  // In xpcshell tests, we never initialize X and hence don't have a Display.
  // In this case, there's no output colour management to be done, so we just
  // return with nullptr.
  if (!dpy) return;

  Window root = gdk_x11_get_default_root_xwindow();

  Atom retAtom;
  int retFormat;
  unsigned long retLength, retAfter;
  unsigned char* retProperty;

  iccAtom = XInternAtom(dpy, ICC_PROFILE_ATOM_NAME, TRUE);
  if (iccAtom) {
    // read once to get size, once for the data
    if (Success == XGetWindowProperty(dpy, root, iccAtom, 0,
                                      INT_MAX /* length */, False,
                                      AnyPropertyType, &retAtom, &retFormat,
                                      &retLength, &retAfter, &retProperty)) {
      if (retLength > 0) {
        void* buffer = malloc(retLength);
        if (buffer) {
          memcpy(buffer, retProperty, retLength);
          mem = buffer;
          size = retLength;
        }
      }

      XFree(retProperty);
      if (size > 0) {
#ifdef DEBUG_tor
        fprintf(stderr, "ICM profile read from %s successfully\n",
                ICC_PROFILE_ATOM_NAME);
#endif
        return;
      }
    }
  }

  edidAtom = XInternAtom(dpy, EDID1_ATOM_NAME, TRUE);
  if (edidAtom) {
    if (Success == XGetWindowProperty(dpy, root, edidAtom, 0, 32, False,
                                      AnyPropertyType, &retAtom, &retFormat,
                                      &retLength, &retAfter, &retProperty)) {
      double gamma;
      qcms_CIE_xyY whitePoint;
      qcms_CIE_xyYTRIPLE primaries;

      if (retLength != 128) {
#ifdef DEBUG_tor
        fprintf(stderr, "Short EDID data\n");
#endif
        return;
      }

      // Format documented in "VESA E-EDID Implementation Guide"

      gamma = (100 + retProperty[0x17]) / 100.0;
      whitePoint.x =
          ((retProperty[0x21] << 2) | (retProperty[0x1a] >> 2 & 3)) / 1024.0;
      whitePoint.y =
          ((retProperty[0x22] << 2) | (retProperty[0x1a] >> 0 & 3)) / 1024.0;
      whitePoint.Y = 1.0;

      primaries.red.x =
          ((retProperty[0x1b] << 2) | (retProperty[0x19] >> 6 & 3)) / 1024.0;
      primaries.red.y =
          ((retProperty[0x1c] << 2) | (retProperty[0x19] >> 4 & 3)) / 1024.0;
      primaries.red.Y = 1.0;

      primaries.green.x =
          ((retProperty[0x1d] << 2) | (retProperty[0x19] >> 2 & 3)) / 1024.0;
      primaries.green.y =
          ((retProperty[0x1e] << 2) | (retProperty[0x19] >> 0 & 3)) / 1024.0;
      primaries.green.Y = 1.0;

      primaries.blue.x =
          ((retProperty[0x1f] << 2) | (retProperty[0x1a] >> 6 & 3)) / 1024.0;
      primaries.blue.y =
          ((retProperty[0x20] << 2) | (retProperty[0x1a] >> 4 & 3)) / 1024.0;
      primaries.blue.Y = 1.0;

      XFree(retProperty);

#ifdef DEBUG_tor
      fprintf(stderr, "EDID gamma: %f\n", gamma);
      fprintf(stderr, "EDID whitepoint: %f %f %f\n", whitePoint.x, whitePoint.y,
              whitePoint.Y);
      fprintf(stderr, "EDID primaries: [%f %f %f] [%f %f %f] [%f %f %f]\n",
              primaries.Red.x, primaries.Red.y, primaries.Red.Y,
              primaries.Green.x, primaries.Green.y, primaries.Green.Y,
              primaries.Blue.x, primaries.Blue.y, primaries.Blue.Y);
#endif

      qcms_data_create_rgb_with_gamma(whitePoint, primaries, gamma, &mem,
                                      &size);

#ifdef DEBUG_tor
      if (size > 0) {
        fprintf(stderr, "ICM profile read from %s successfully\n",
                EDID1_ATOM_NAME);
      }
#endif
    }
  }
#endif
}

bool gfxPlatformGtk::CheckVariationFontSupport() {
  // Although there was some variation/multiple-master support in FreeType
  // in older versions, it seems too incomplete/unstable for us to use
  // until at least 2.7.1.
  FT_Int major, minor, patch;
  FT_Library_Version(GetFTLibrary(), &major, &minor, &patch);
  return major * 1000000 + minor * 1000 + patch >= 2007001;
}

#ifdef MOZ_X11

class GtkVsyncSource final : public VsyncSource {
 public:
  GtkVsyncSource() {
    MOZ_ASSERT(NS_IsMainThread());
    mGlobalDisplay = new GLXDisplay();
  }

  virtual ~GtkVsyncSource() { MOZ_ASSERT(NS_IsMainThread()); }

  virtual Display& GetGlobalDisplay() override { return *mGlobalDisplay; }

  class GLXDisplay final : public VsyncSource::Display {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GLXDisplay)

   public:
    GLXDisplay()
        : mGLContext(nullptr),
          mXDisplay(nullptr),
          mSetupLock("GLXVsyncSetupLock"),
          mVsyncThread("GLXVsyncThread"),
          mVsyncTask(nullptr),
          mVsyncEnabledLock("GLXVsyncEnabledLock"),
          mVsyncEnabled(false)
#ifdef MOZ_WAYLAND
          ,
          mIsWaylandDisplay(false)
#endif
    {
    }

    // Sets up the display's GL context on a worker thread.
    // Required as GLContexts may only be used by the creating thread.
    // Returns true if setup was a success.
    bool Setup() {
      MonitorAutoLock lock(mSetupLock);
      MOZ_ASSERT(NS_IsMainThread());
      if (!mVsyncThread.Start()) return false;

      RefPtr<Runnable> vsyncSetup =
          NewRunnableMethod("GtkVsyncSource::GLXDisplay::SetupGLContext", this,
                            &GLXDisplay::SetupGLContext);
      mVsyncThread.message_loop()->PostTask(vsyncSetup.forget());
      // Wait until the setup has completed.
      lock.Wait();
      return mGLContext != nullptr;
    }

#ifdef MOZ_WAYLAND
    bool SetupWayland() {
      MonitorAutoLock lock(mSetupLock);
      MOZ_ASSERT(NS_IsMainThread());
      mIsWaylandDisplay = true;
      return mVsyncThread.Start();
    }
#endif

    // Called on the Vsync thread to setup the GL context.
    void SetupGLContext() {
      MonitorAutoLock lock(mSetupLock);
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(!mGLContext, "GLContext already setup!");

      // Create video sync timer on a separate Display to prevent locking the
      // main thread X display.
      mXDisplay = XOpenDisplay(nullptr);
      if (!mXDisplay) {
        lock.NotifyAll();
        return;
      }

      // Most compositors wait for vsync events on the root window.
      Window root = DefaultRootWindow(mXDisplay);
      int screen = DefaultScreen(mXDisplay);

      ScopedXFree<GLXFBConfig> cfgs;
      GLXFBConfig config;
      int visid;
      bool forWebRender = false;
      if (!gl::GLContextGLX::FindFBConfigForWindow(
              mXDisplay, screen, root, &cfgs, &config, &visid, forWebRender)) {
        lock.NotifyAll();
        return;
      }

      mGLContext = gl::GLContextGLX::CreateGLContext(
          gl::CreateContextFlags::NONE, gl::SurfaceCaps::Any(), false,
          mXDisplay, root, config, false, nullptr);

      if (!mGLContext) {
        lock.NotifyAll();
        return;
      }

      mGLContext->MakeCurrent();

      // Test that SGI_video_sync lets us get the counter.
      unsigned int syncCounter = 0;
      if (gl::sGLXLibrary.fGetVideoSync(&syncCounter) != 0) {
        mGLContext = nullptr;
      }

      lock.NotifyAll();
    }

    virtual void EnableVsync() override {
      MOZ_ASSERT(NS_IsMainThread());
#if !defined(MOZ_WAYLAND)
      MOZ_ASSERT(mGLContext, "GLContext not setup!");
#endif

      MonitorAutoLock lock(mVsyncEnabledLock);
      if (mVsyncEnabled) {
        return;
      }
      mVsyncEnabled = true;

      // If the task has not nulled itself out, it hasn't yet realized
      // that vsync was disabled earlier, so continue its execution.
      if (!mVsyncTask) {
        mVsyncTask =
            NewRunnableMethod("GtkVsyncSource::GLXDisplay::RunVsync", this,
#if defined(MOZ_WAYLAND)
                              mIsWaylandDisplay ? &GLXDisplay::RunVsyncWayland :
#endif
                                                &GLXDisplay::RunVsync);
        RefPtr<Runnable> addrefedTask = mVsyncTask;
        mVsyncThread.message_loop()->PostTask(addrefedTask.forget());
      }
    }

    virtual void DisableVsync() override {
      MonitorAutoLock lock(mVsyncEnabledLock);
      mVsyncEnabled = false;
    }

    virtual bool IsVsyncEnabled() override {
      MonitorAutoLock lock(mVsyncEnabledLock);
      return mVsyncEnabled;
    }

    virtual void Shutdown() override {
      MOZ_ASSERT(NS_IsMainThread());
      DisableVsync();

      // Cleanup thread-specific resources before shutting down.
      RefPtr<Runnable> shutdownTask = NewRunnableMethod(
          "GtkVsyncSource::GLXDisplay::Cleanup", this, &GLXDisplay::Cleanup);
      mVsyncThread.message_loop()->PostTask(shutdownTask.forget());

      // Stop, waiting for the cleanup task to finish execution.
      mVsyncThread.Stop();
    }

   private:
    virtual ~GLXDisplay() {}

    void RunVsync() {
      MOZ_ASSERT(!NS_IsMainThread());

      mGLContext->MakeCurrent();

      unsigned int syncCounter = 0;
      gl::sGLXLibrary.fGetVideoSync(&syncCounter);
      for (;;) {
        {
          MonitorAutoLock lock(mVsyncEnabledLock);
          if (!mVsyncEnabled) {
            mVsyncTask = nullptr;
            return;
          }
        }

        TimeStamp lastVsync = TimeStamp::Now();
        bool useSoftware = false;

        // Wait until the video sync counter reaches the next value by waiting
        // until the parity of the counter value changes.
        unsigned int nextSync = syncCounter + 1;
        int status;
        if ((status = gl::sGLXLibrary.fWaitVideoSync(2, nextSync % 2,
                                                     &syncCounter)) != 0) {
          gfxWarningOnce() << "glXWaitVideoSync returned " << status;
          useSoftware = true;
        }

        if (syncCounter == (nextSync - 1)) {
          gfxWarningOnce()
              << "glXWaitVideoSync failed to increment the sync counter.";
          useSoftware = true;
        }

        if (useSoftware) {
          double remaining =
              (1000.f / 60.f) - (TimeStamp::Now() - lastVsync).ToMilliseconds();
          if (remaining > 0) {
            PlatformThread::Sleep(remaining);
          }
        }

        lastVsync = TimeStamp::Now();
        NotifyVsync(lastVsync);
      }
    }

#ifdef MOZ_WAYLAND
    /* VSync on Wayland is tricky as we can get only "last VSync" event signal.
     * That means we should draw next frame at "last Vsync + frame delay" time.
     */
    void RunVsyncWayland() {
      MOZ_ASSERT(!NS_IsMainThread());

      for (;;) {
        {
          MonitorAutoLock lock(mVsyncEnabledLock);
          if (!mVsyncEnabled) {
            mVsyncTask = nullptr;
            return;
          }
        }

        gint64 lastVsync = gfxPlatformGtk::GetPlatform()->GetWaylandLastVsync();
        gint64 currTime = (g_get_monotonic_time() / 1000);

        gint64 remaining =
            gfxPlatformGtk::GetPlatform()->GetWaylandFrameDelay() -
            (currTime - lastVsync);
        if (remaining > 0) {
          PlatformThread::Sleep(remaining);
        } else {
          // Time from last HW Vsync is longer than our frame delay,
          // use our approximation then.
          gfxPlatformGtk::GetPlatform()->SetWaylandLastVsync(currTime);
        }

        NotifyVsync(TimeStamp::Now());
      }
    }
#endif

    void Cleanup() {
      MOZ_ASSERT(!NS_IsMainThread());

      mGLContext = nullptr;
      if (mXDisplay) XCloseDisplay(mXDisplay);
    }

    // Owned by the vsync thread.
    RefPtr<gl::GLContextGLX> mGLContext;
    _XDisplay* mXDisplay;
    Monitor mSetupLock;
    base::Thread mVsyncThread;
    RefPtr<Runnable> mVsyncTask;
    Monitor mVsyncEnabledLock;
    bool mVsyncEnabled;
#ifdef MOZ_WAYLAND
    bool mIsWaylandDisplay;
#endif
  };

 private:
  // We need a refcounted VsyncSource::Display to use chromium IPC runnables.
  RefPtr<GLXDisplay> mGlobalDisplay;
};

already_AddRefed<gfx::VsyncSource> gfxPlatformGtk::CreateHardwareVsyncSource() {
#ifdef MOZ_WAYLAND
  if (!GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    RefPtr<VsyncSource> vsyncSource = new GtkVsyncSource();
    VsyncSource::Display& display = vsyncSource->GetGlobalDisplay();
    static_cast<GtkVsyncSource::GLXDisplay&>(display).SetupWayland();
    return vsyncSource.forget();
  }
#endif

  // Only use GLX vsync when the OpenGL compositor is being used.
  // The extra cost of initializing a GLX context while blocking the main
  // thread is not worth it when using basic composition.
  if (gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    if (gl::sGLXLibrary.SupportsVideoSync()) {
      RefPtr<VsyncSource> vsyncSource = new GtkVsyncSource();
      VsyncSource::Display& display = vsyncSource->GetGlobalDisplay();
      if (!static_cast<GtkVsyncSource::GLXDisplay&>(display).Setup()) {
        NS_WARNING(
            "Failed to setup GLContext, falling back to software vsync.");
        return gfxPlatform::CreateHardwareVsyncSource();
      }
      return vsyncSource.forget();
    }
    NS_WARNING("SGI_video_sync unsupported. Falling back to software vsync.");
  }
  return gfxPlatform::CreateHardwareVsyncSource();
}

#endif
