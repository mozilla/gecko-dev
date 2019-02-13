/* Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GonkDisplayICS.h"
#include <ui/FramebufferNativeWindow.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <hardware/hwcomposer.h>
#include <hardware_legacy/power.h>
#include <cutils/log.h>
#include <fcntl.h>

#include "mozilla/Assertions.h"
#include "mozilla/FileUtils.h"
#include "mozilla/FileUtils.h"

using namespace android;


namespace {
static const char* kSleepFile = "/sys/power/wait_for_fb_sleep";
static const char* kWakeFile = "/sys/power/wait_for_fb_wake";
static mozilla::GonkDisplay::OnEnabledCallbackType sEnabledCallback;
static pthread_t sFramebufferWatchThread;

static void *
frameBufferWatcher(void *)
{
    int len = 0;
    char buf;

    while (true) {
        // Cannot use epoll here because kSleepFile and kWakeFile are
        // always ready to read and blocking.
        {
            mozilla::ScopedClose fd(open(kSleepFile, O_RDONLY, 0));
            do {
                len = read(fd.get(), &buf, 1);
            } while (len < 0 && errno == EINTR);
            NS_WARN_IF_FALSE(len >= 0, "WAIT_FOR_FB_SLEEP failed");
        }
        sEnabledCallback(false);

        {
            mozilla::ScopedClose fd(open(kWakeFile, O_RDONLY, 0));
            do {
                len = read(fd.get(), &buf, 1);
            } while (len < 0 && errno == EINTR);
            NS_WARN_IF_FALSE(len >= 0, "WAIT_FOR_FB_WAKE failed");
        }
        sEnabledCallback(true);
    }

    return nullptr;
}

} // anonymous namespace


namespace mozilla {

static GonkDisplayICS* sGonkDisplay = nullptr;

static int
FramebufferNativeWindowCancelBufferNoop(ANativeWindow* aWindow,
    android_native_buffer_t* aBuffer)
{
    return 0;
}

GonkDisplayICS::GonkDisplayICS()
    : mModule(nullptr)
    , mHwc(nullptr)
{
    // Some gralloc HALs need this in order to open the
    // framebuffer device after we restart with the screen off.
    //
    // this *must* run BEFORE allocating the
    // FramebufferNativeWindow.
    set_screen_state(1);

    // For some devices, it takes a while for the framebuffer to become
    // usable. So we wait until the framebuffer has woken up before we
    // try to open it.
    {
        char buf;
        int len = 0;
        ScopedClose fd(open("/sys/power/wait_for_fb_wake", O_RDONLY, 0));
        do {
            len = read(fd.get(), &buf, 1);
        } while (len < 0 && errno == EINTR);
        if (len < 0) {
            LOGE("wait_for_fb_sleep failed errno: %d", errno);
        }
    }

    mFBSurface = new FramebufferNativeWindow();

    // ICS FramebufferNativeWindow doesn't set the |cancelBuffer|
    // function pointer.
    // It will crash when deleting the EGL window surface.
    if (!mFBSurface->cancelBuffer) {
        mFBSurface->cancelBuffer = FramebufferNativeWindowCancelBufferNoop;
    }

    int err = hw_get_module(HWC_HARDWARE_MODULE_ID, &mModule);
    LOGW_IF(err, "%s module not found", HWC_HARDWARE_MODULE_ID);
    if (!err) {
        err = hwc_open(mModule, &mHwc);
        LOGE_IF(err, "%s device failed to initialize (%s)",
                 HWC_HARDWARE_COMPOSER, strerror(-err));
    }

    xdpi = mFBSurface->xdpi;

    const framebuffer_device_t *fbdev = mFBSurface->getDevice();
    surfaceformat = fbdev->format;
}

GonkDisplayICS::~GonkDisplayICS()
{
    if (mHwc)
        hwc_close(mHwc);
}

void
GonkDisplayICS::SetEnabled(bool enabled)
{
    set_screen_state(enabled);
}

void
GonkDisplayICS::OnEnabled(OnEnabledCallbackType callback)
{
    if (sEnabledCallback)
        return;
    sEnabledCallback = callback;

    // Watching screen on/off state by using a pthread
    // which implicitly calls exit() when the main thread ends
    if (pthread_create(&sFramebufferWatchThread, NULL, frameBufferWatcher, NULL)) {
        NS_RUNTIMEABORT("Failed to create framebufferWatcherThread, aborting...");
    }
}


void*
GonkDisplayICS::GetHWCDevice()
{
    return mHwc;
}

bool
GonkDisplayICS::SwapBuffers(EGLDisplay dpy, EGLSurface sur)
{
    // Should be called when composition rendering is complete for a frame.
    // Only HWC v1.0 needs this call. ICS gonk always needs the call.
    return !mFBSurface->compositionComplete();
}

ANativeWindowBuffer*
GonkDisplayICS::DequeueBuffer()
{
    ANativeWindow *window = static_cast<ANativeWindow *>(mFBSurface.get());
    ANativeWindowBuffer *buf = nullptr;
    window->dequeueBuffer(window, &buf);
    return buf;
}

bool
GonkDisplayICS::QueueBuffer(ANativeWindowBuffer *buf)
{
    ANativeWindow *window = static_cast<ANativeWindow *>(mFBSurface.get());
    return !window->queueBuffer(window, buf);
}

void
GonkDisplayICS::UpdateDispSurface(EGLDisplay dpy, EGLSurface sur)
{
}

void
GonkDisplayICS::SetDispReleaseFd(int fd)
{
}

GonkDisplay::NativeData
GonkDisplayICS::GetNativeData(GonkDisplay::DisplayType aDisplayType,
                              android::IGraphicBufferProducer* aProducer)
{
    MOZ_ASSERT(aDisplayType == DISPLAY_PRIMARY, "ICS gonk supports primary display only.");

    NativeData data;
    data.mNativeWindow = static_cast<ANativeWindow *>(mFBSurface.get());
    data.mXdpi = xdpi;

    return data;
}

__attribute__ ((visibility ("default")))
GonkDisplay*
GetGonkDisplay()
{
    if (!sGonkDisplay)
        sGonkDisplay = new GonkDisplayICS();
    return sGonkDisplay;
}

} // namespace mozilla
