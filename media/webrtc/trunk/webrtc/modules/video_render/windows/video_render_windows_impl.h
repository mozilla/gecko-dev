/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_WINDOWS_IMPL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_WINDOWS_IMPL_H_

#include <Winerror.h>
#include <dxdiag.h>

#include "webrtc/modules/video_render/i_video_render.h"
#include "webrtc/modules/video_render/windows/i_video_render_win.h"

namespace webrtc {
class CriticalSectionWrapper;

#define EXPAND(x)            x, sizeof(x)/sizeof(TCHAR)

enum VideoRenderWinMethod {
  kVideoRenderWinD3D9 = 0,
};

// Class definitions
class VideoRenderWindowsImpl: IVideoRender
{
public:
    /*
     *   Constructor/destructor
     */

    VideoRenderWindowsImpl(const int32_t id,
                           const VideoRenderType videoRenderType,
                           void* window, const bool fullscreen);

    virtual ~VideoRenderWindowsImpl();

    virtual int32_t Init();

    virtual int32_t ChangeWindow(void* window);

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/

    virtual VideoRenderCallback
            * AddIncomingRenderStream(const uint32_t streamId,
                                      const uint32_t zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual int32_t
            DeleteIncomingRenderStream(const uint32_t streamId);

    virtual int32_t
            GetIncomingRenderStreamProperties(const uint32_t streamId,
                                              uint32_t& zOrder,
                                              float& left, float& top,
                                              float& right, float& bottom) const;

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    virtual int32_t StartRender();

    virtual int32_t StopRender();

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/

    virtual VideoRenderType RenderType();

    virtual RawVideoType PerferedVideoType();

    virtual bool FullScreen();

    virtual int32_t
            GetGraphicsMemory(uint64_t& totalGraphicsMemory,
                              uint64_t& availableGraphicsMemory) const;

    virtual int32_t
            GetScreenResolution(uint32_t& screenWidth,
                                uint32_t& screenHeight) const;

    virtual uint32_t RenderFrameRate(const uint32_t streamId);

    virtual int32_t SetStreamCropping(const uint32_t streamId,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual int32_t ConfigureRenderer(const uint32_t streamId,
                                      const unsigned int zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual int32_t SetTransparentBackground(const bool enable);

    virtual int32_t SetText(const uint8_t textId,
                            const uint8_t* text,
                            const int32_t textLength,
                            const uint32_t textColorRef,
                            const uint32_t backgroundColorRef,
                            const float left, const float top,
                            const float right, const float bottom);

    virtual int32_t SetBitmap(const void* bitMap,
                              const uint8_t pictureId,
                              const void* colorKey,
                              const float left, const float top,
                              const float right, const float bottom);

private:
    CriticalSectionWrapper& _renderWindowsCritsect;

    void* _prtWindow;
    bool _fullscreen;

    VideoRenderWinMethod _renderMethod;
    IVideoRenderWin* _ptrRendererWin;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_WINDOWS_IMPL_H_
