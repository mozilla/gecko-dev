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

#ifndef GONKDISPLAYICS_H
#define GONKDISPLAYICS_H

#include <string.h>

#include "GonkDisplay.h"
#include "ui/FramebufferNativeWindow.h"
#include "hardware/hwcomposer.h"
#include "utils/RefBase.h"

namespace android {
class IGraphicBufferProducer;
}

namespace mozilla {

class MOZ_EXPORT GonkDisplayICS : public GonkDisplay {
public:
    GonkDisplayICS();
    ~GonkDisplayICS();

    virtual void SetEnabled(bool enabled);

    virtual void OnEnabled(OnEnabledCallbackType callback);

    virtual void* GetHWCDevice();

    virtual bool SwapBuffers(EGLDisplay dpy, EGLSurface sur);

    virtual ANativeWindowBuffer* DequeueBuffer();

    virtual bool QueueBuffer(ANativeWindowBuffer* handle);

    virtual void UpdateDispSurface(EGLDisplay dpy, EGLSurface sur);

    virtual void SetDispReleaseFd(int fd);

    virtual int GetPrevDispAcquireFd()
    {
        return -1;
    }

    virtual NativeData GetNativeData(
        GonkDisplay::DisplayType aDisplayType,
        android::IGraphicBufferProducer* aProducer = nullptr);

    virtual void NotifyBootAnimationStopped() {}

private:
    hw_module_t const*        mModule;
    hwc_composer_device_t*    mHwc;
    android::sp<android::FramebufferNativeWindow> mFBSurface;
};

}

#endif /* GONKDISPLAYICS_H */
