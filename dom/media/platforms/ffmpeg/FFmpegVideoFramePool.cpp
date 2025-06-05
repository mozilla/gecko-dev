/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoFramePool.h"
#include "PlatformDecoderModule.h"
#include "FFmpegLog.h"
#include "mozilla/widget/DMABufDevice.h"
#include "libavutil/pixfmt.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/widget/va_drmcommon.h"

// DMABufLibWrapper defines its own version of this which collides with the
// official version in drm_fourcc.h
#ifdef DRM_FORMAT_MOD_INVALID
#  undef DRM_FORMAT_MOD_INVALID
#endif
#include <libdrm/drm_fourcc.h>

#ifdef MOZ_LOGGING
#  undef DMABUF_LOG
extern mozilla::LazyLogModule gDmabufLog;
#  define DMABUF_LOG(str, ...) \
    MOZ_LOG(gDmabufLog, mozilla::LogLevel::Debug, (str, ##__VA_ARGS__))
#else
#  define DMABUF_LOG(args)
#endif /* MOZ_LOGGING */

// Start copying surfaces when free ffmpeg surface count is below 1/4 of all
// available surfaces.
#define SURFACE_COPY_THRESHOLD (1.0f / 4.0f)

constexpr static VASurfaceID sInvalidFFMPEGSurfaceID = -1;

namespace mozilla {

RefPtr<layers::Image> VideoFrameSurface<LIBAV_VER>::GetAsImage() {
  return new layers::DMABUFSurfaceImage(mSurface);
}

VideoFrameSurface<LIBAV_VER>::VideoFrameSurface(DMABufSurface* aSurface,
                                                VASurfaceID aFFMPEGSurfaceID)
    : mSurface(aSurface),
      mLib(nullptr),
      mAVHWFrameContext(nullptr),
      mHWAVBuffer(nullptr),
      mFFMPEGSurfaceID(aFFMPEGSurfaceID),
      mHoldByFFmpeg(false) {
  // Create global refcount object to track mSurface usage over
  // gects rendering engine. We can't release it until it's used
  // by GL compositor / WebRender.
  MOZ_ASSERT(mSurface);
  MOZ_RELEASE_ASSERT(mSurface->GetAsDMABufSurfaceYUV());
  mSurface->GlobalRefCountCreate();
  DMABUF_LOG("VideoFrameSurface: creating surface UID %d FFmpeg ID %x",
             mSurface->GetUID(), aFFMPEGSurfaceID);
}

VideoFrameSurface<LIBAV_VER>::~VideoFrameSurface() {
  DMABUF_LOG("~VideoFrameSurface: deleting dmabuf surface UID %d",
             mSurface->GetUID());
  mSurface->GlobalRefCountDelete();
  // We're about to quit, no need to recycle the frames.
  if (mHoldByFFmpeg) {
    ReleaseVAAPIData(/* aForFrameRecycle */ false);
  }
}

void VideoFrameSurface<LIBAV_VER>::DisableRecycle() {
  MOZ_DIAGNOSTIC_ASSERT(mFFMPEGSurfaceID == sInvalidFFMPEGSurfaceID,
                        "VideoFrameSurface::DisableRecycle(): can't disable "
                        "recycle for FFmpeg surfaces!");
  mSurface->DisableRecycle();
}

void VideoFrameSurface<LIBAV_VER>::LockVAAPIData(
    AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
    FFmpegLibWrapper* aLib) {
  mLib = aLib;
  mHoldByFFmpeg = true;

  // V4L2 frames don't have hw_frames_ctx because the v4l2-wrapper codecs
  // don't actually use hwaccel.  In this case we don't need to add a
  // HW frame context reference
  if (aAVCodecContext->hw_frames_ctx) {
    mAVHWFrameContext = aLib->av_buffer_ref(aAVCodecContext->hw_frames_ctx);
    mHWAVBuffer = aLib->av_buffer_ref(aAVFrame->buf[0]);
    DMABUF_LOG(
        "VideoFrameSurface: VAAPI locking dmabuf surface UID %d FFMPEG ID 0x%x "
        "mAVHWFrameContext %p mHWAVBuffer %p",
        mSurface->GetUID(), mFFMPEGSurfaceID, mAVHWFrameContext, mHWAVBuffer);
  } else {
    mAVHWFrameContext = nullptr;
    mHWAVBuffer = aLib->av_buffer_ref(aAVFrame->buf[0]);
    DMABUF_LOG(
        "VideoFrameSurface: V4L2 locking dmabuf surface UID %d FFMPEG ID 0x%x "
        "mHWAVBuffer %p",
        mSurface->GetUID(), mFFMPEGSurfaceID, mHWAVBuffer);
  }
}

void VideoFrameSurface<LIBAV_VER>::ReleaseVAAPIData(bool aForFrameRecycle) {
  DMABUF_LOG(
      "VideoFrameSurface: Releasing dmabuf surface UID %d FFMPEG ID 0x%x "
      "aForFrameRecycle %d mLib %p mAVHWFrameContext %p mHWAVBuffer %p",
      mSurface->GetUID(), mFFMPEGSurfaceID, aForFrameRecycle, mLib,
      mAVHWFrameContext, mHWAVBuffer);
  // It's possible to unref GPU data while IsUsedByRenderer() is still set.
  // It can happen when VideoFramePool is deleted while decoder shutdown
  // but related dmabuf surfaces are still used in another process.
  // In such case we don't care as the dmabuf surface will not be
  // recycled for another frame and stays here untill last fd of it
  // is closed.
  if (mLib) {
    mLib->av_buffer_unref(&mHWAVBuffer);
    if (mAVHWFrameContext) {
      mLib->av_buffer_unref(&mAVHWFrameContext);
    }
    mLib = nullptr;
  }

  mHoldByFFmpeg = false;
  mSurface->ReleaseSurface();

  if (aForFrameRecycle && IsUsedByRenderer()) {
    NS_WARNING("Reusing live dmabuf surface, visual glitches ahead");
  }
}

VideoFramePool<LIBAV_VER>::VideoFramePool(int aFFMPEGPoolSize)
    : mSurfaceLock("VideoFramePoolSurfaceLock"),
      mMaxFFMPEGPoolSize(aFFMPEGPoolSize) {
  DMABUF_LOG("VideoFramePool::VideoFramePool() pool size %d",
             mMaxFFMPEGPoolSize);
}

VideoFramePool<LIBAV_VER>::~VideoFramePool() {
  DMABUF_LOG("VideoFramePool::~VideoFramePool()");
  MutexAutoLock lock(mSurfaceLock);
  mDMABufSurfaces.Clear();
}

void VideoFramePool<LIBAV_VER>::ReleaseUnusedVAAPIFrames() {
  MutexAutoLock lock(mSurfaceLock);
  for (const auto& surface : mDMABufSurfaces) {
    if (!surface->mHoldByFFmpeg && surface->IsUsedByRenderer()) {
      DMABUF_LOG("Copied and used surface UID %d",
                 surface->GetDMABufSurface()->GetUID());
    }
    if (surface->mHoldByFFmpeg && !surface->IsUsedByRenderer()) {
      surface->ReleaseVAAPIData();
    }
  }
}

// Unlink all FFmpeg frames from ID. That ensures we'll allocate new
// DMABuf surfaces with fresh UID and we won't recycle old ones.
// It's used when FFmpeg invalides frames after avcodec_flush_buffers() call,
// before seek for instance.
void VideoFramePool<LIBAV_VER>::FlushFFmpegFrames() {
  MutexAutoLock lock(mSurfaceLock);
  for (const auto& surface : mDMABufSurfaces) {
    surface->mFFMPEGSurfaceID = sInvalidFFMPEGSurfaceID;
  }
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetFFmpegVideoFrameSurfaceLocked(
    const MutexAutoLock& aProofOfLock, VASurfaceID aFFMPEGSurfaceID) {
  MOZ_DIAGNOSTIC_ASSERT(
      aFFMPEGSurfaceID != sInvalidFFMPEGSurfaceID,
      "GetFFmpegVideoFrameSurfaceLocked(): expects valid aFFMPEGSurfaceID");

  // Try to find existing surface by ffmpeg ID. We want to re-use it
  // to keep matched surface UID / FFmpeg ID.
  for (auto& surface : mDMABufSurfaces) {
    if (surface->mFFMPEGSurfaceID == aFFMPEGSurfaceID) {
      // This should not happen as we reference FFmpeg surfaces from
      // renderer process.
      if (surface->IsUsedByRenderer()) {
        NS_WARNING("Using live surfaces, visual glitches ahead!");
      }
      return surface;
    }
  }
  return nullptr;
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetFreeVideoFrameSurfaceLocked(
    const MutexAutoLock& aProofOfLock) {
  for (auto& surface : mDMABufSurfaces) {
    if (surface->mFFMPEGSurfaceID != sInvalidFFMPEGSurfaceID) {
      continue;
    }
    if (surface->mHoldByFFmpeg) {
      continue;
    }
    if (surface->IsUsedByRenderer()) {
      continue;
    }
    surface->ReleaseVAAPIData();
    return surface;
  }
  return nullptr;
}

bool VideoFramePool<LIBAV_VER>::ShouldCopySurface() {
  // Number of used HW surfaces.
  int surfacesUsed = 0;
  int surfacesUsedFFmpeg = 0;
  for (const auto& surface : mDMABufSurfaces) {
    if (surface->IsUsedByRenderer()) {
      surfacesUsed++;
      if (surface->IsFFMPEGSurface()) {
        DMABUF_LOG("Used HW surface UID %d FFMPEG ID 0x%x\n",
                   surface->mSurface->GetUID(), surface->mFFMPEGSurfaceID);
        surfacesUsedFFmpeg++;
      }
    } else {
      if (surface->IsFFMPEGSurface()) {
        DMABUF_LOG("Free HW surface UID %d FFMPEG ID 0x%x\n",
                   surface->mSurface->GetUID(), surface->mFFMPEGSurfaceID);
      }
    }
  }

  // mMaxFFMPEGPoolSize can be zero for dynamic pools,
  // we don't do copy in that case unless it's requested by HW setup.
  float freeRatio =
      mMaxFFMPEGPoolSize
          ? 1.0f - (surfacesUsedFFmpeg / (float)mMaxFFMPEGPoolSize)
          : 1.0;
  DMABUF_LOG(
      "Surface pool size %d used copied %d used ffmpeg %d (max %d) free ratio "
      "%f",
      (int)mDMABufSurfaces.Length(), surfacesUsed - surfacesUsedFFmpeg,
      surfacesUsedFFmpeg, mMaxFFMPEGPoolSize, freeRatio);
  if (!gfx::gfxVars::HwDecodedVideoZeroCopy()) {
    return true;
  }
  return freeRatio < SURFACE_COPY_THRESHOLD;
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetTargetVideoFrameSurfaceLocked(
    const MutexAutoLock& aProofOfLock, VASurfaceID aFFmpegSurfaceID,
    bool aRecycleSurface) {
  RefPtr<DMABufSurfaceYUV> surface;
  RefPtr<VideoFrameSurface<LIBAV_VER>> videoSurface;

  // Look for surface pool to select existing or unused surface
  if (!aRecycleSurface) {
    // Copied surfaces are not recycled.
    videoSurface = GetFreeVideoFrameSurfaceLocked(aProofOfLock);
  } else {
    // Use FFmpeg ID to find appropriate dmabuf surface. We want to use
    // the same DMABuf surface for FFmpeg decoded frame (FFmpeg ID).
    // It allows us to recycle buffers in rendering process.
    MOZ_DIAGNOSTIC_ASSERT(aFFmpegSurfaceID != sInvalidFFMPEGSurfaceID,
                          "Wrong FFMPEGSurfaceID to recycle!");
    videoSurface =
        GetFFmpegVideoFrameSurfaceLocked(aProofOfLock, aFFmpegSurfaceID);
  }

  // Okay, create a new one
  if (!videoSurface) {
    surface = new DMABufSurfaceYUV();
    videoSurface = new VideoFrameSurface<LIBAV_VER>(
        surface, aRecycleSurface ? aFFmpegSurfaceID : sInvalidFFMPEGSurfaceID);
    mDMABufSurfaces.AppendElement(videoSurface);
    DMABUF_LOG("Added new DMABufSurface UID %d", surface->GetUID());
  } else {
    surface = videoSurface->GetDMABufSurface();
    DMABUF_LOG("Matched DMABufSurface UID %d", surface->GetUID());
  }

  return videoSurface;
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetVideoFrameSurface(
    VADRMPRIMESurfaceDescriptor& aVaDesc, int aWidth, int aHeight,
    AVCodecContext* aAVCodecContext, AVFrame* aAVFrame,
    FFmpegLibWrapper* aLib) {
  if (aVaDesc.fourcc != VA_FOURCC_NV12 && aVaDesc.fourcc != VA_FOURCC_YV12 &&
      aVaDesc.fourcc != VA_FOURCC_P010 && aVaDesc.fourcc != VA_FOURCC_P016) {
    DMABUF_LOG("Unsupported VA-API surface format %d", aVaDesc.fourcc);
    return nullptr;
  }

  MutexAutoLock lock(mSurfaceLock);

  bool copySurface = mTextureCopyWorks && ShouldCopySurface();

  VASurfaceID ffmpegSurfaceID = (uintptr_t)aAVFrame->data[3];
  MOZ_DIAGNOSTIC_ASSERT(ffmpegSurfaceID != sInvalidFFMPEGSurfaceID,
                        "Exported invalid FFmpeg surface ID");
  DMABUF_LOG("Got VA-API DMABufSurface FFMPEG ID 0x%x", ffmpegSurfaceID);

  RefPtr<VideoFrameSurface<LIBAV_VER>> videoSurface =
      GetTargetVideoFrameSurfaceLocked(lock, ffmpegSurfaceID,
                                       /* aRecycleSurface */ !copySurface);
  RefPtr<DMABufSurfaceYUV> surface = videoSurface->GetDMABufSurface();

  if (!surface->UpdateYUVData(aVaDesc, aWidth, aHeight, copySurface)) {
    if (!copySurface) {
      // We failed to move data to DMABuf, so quit now.
      return nullptr;
    }

    // We failed to copy data, try again as move.
    DMABUF_LOG("  DMABuf texture copy is broken");
    copySurface = mTextureCopyWorks = false;

    videoSurface = GetTargetVideoFrameSurfaceLocked(lock, ffmpegSurfaceID,
                                                    /* aRecycleSurface */ true);
    surface = videoSurface->GetDMABufSurface();
    if (!surface->UpdateYUVData(aVaDesc, aWidth, aHeight,
                                /* copySurface */ false)) {
      return nullptr;
    }
  }

  if (MOZ_UNLIKELY(!mTextureCreationWorks)) {
    mTextureCreationWorks = Some(surface->VerifyTextureCreation());
    if (!*mTextureCreationWorks) {
      DMABUF_LOG("  failed to create texture over DMABuf memory!");
      return nullptr;
    }
  }

  if (copySurface) {
    // Disable recycling for copied DMABuf surfaces as we can't ensure
    // match between FFmpeg frame with DMABufSurface.
    // It doesn't matter much as surface copy uses extra GPU resources
    // anyway.
    videoSurface->DisableRecycle();
  } else {
    videoSurface->LockVAAPIData(aAVCodecContext, aAVFrame, aLib);
  }

  return videoSurface;
}

static gfx::SurfaceFormat GetSurfaceFormat(enum AVPixelFormat aPixFmt) {
  switch (aPixFmt) {
    case AV_PIX_FMT_YUV420P10LE:
      return gfx::SurfaceFormat::YUV420P10;
    case AV_PIX_FMT_YUV420P:
      return gfx::SurfaceFormat::YUV420;
    default:
      return gfx::SurfaceFormat::UNKNOWN;
  }
}

// TODO: Add support for AV_PIX_FMT_YUV444P / AV_PIX_FMT_GBRP
RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetVideoFrameSurface(
    const layers::PlanarYCbCrData& aData, AVCodecContext* aAVCodecContext) {
  static gfx::SurfaceFormat format = GetSurfaceFormat(aAVCodecContext->pix_fmt);
  if (format == gfx::SurfaceFormat::UNKNOWN) {
    DMABUF_LOG("Unsupported FFmpeg DMABuf format %x", aAVCodecContext->pix_fmt);
    return nullptr;
  }

  MutexAutoLock lock(mSurfaceLock);

  RefPtr<VideoFrameSurface<LIBAV_VER>> videoSurface =
      GetTargetVideoFrameSurfaceLocked(lock, sInvalidFFMPEGSurfaceID,
                                       /* aRecycleSurface */ false);
  RefPtr<DMABufSurfaceYUV> surface = videoSurface->GetDMABufSurface();

  DMABUF_LOG("Using SW DMABufSurface UID %d", surface->GetUID());

  if (!surface->UpdateYUVData(aData, format)) {
    DMABUF_LOG("  failed to convert YUV data to DMABuf memory!");
    return nullptr;
  }

  if (MOZ_UNLIKELY(!mTextureCreationWorks)) {
    mTextureCreationWorks = Some(surface->VerifyTextureCreation());
    if (!*mTextureCreationWorks) {
      DMABUF_LOG("  failed to create texture over DMABuf memory!");
      return nullptr;
    }
  }

  // Disable recycling for copied DMABuf surfaces as we can't ensure
  // match between FFmpeg frame with DMABufSurface.
  // It doesn't matter much as surface copy/texture upload uses extra
  // GPU resources anyway.
  videoSurface->DisableRecycle();
  return videoSurface;
}

// Convert an FFmpeg-specific DRM descriptor into a
// VADRMPRIMESurfaceDescriptor.  There is no fundamental difference between
// the descriptor structs and using the latter means this can use all the
// existing machinery in DMABufSurfaceYUV.
static Maybe<VADRMPRIMESurfaceDescriptor> FFmpegDescToVA(
    AVDRMFrameDescriptor& aDesc, AVFrame* aAVFrame) {
  VADRMPRIMESurfaceDescriptor vaDesc{};

  if (aAVFrame->format != AV_PIX_FMT_DRM_PRIME) {
    DMABUF_LOG("Got non-DRM-PRIME frame from FFmpeg V4L2");
    return Nothing();
  }

  if (aAVFrame->crop_top != 0 || aAVFrame->crop_left != 0) {
    DMABUF_LOG("Top and left-side cropping are not supported");
    return Nothing();
  }

  // Width and height after crop
  vaDesc.width = aAVFrame->width;
  vaDesc.height = aAVFrame->height - aAVFrame->crop_bottom;

  // Native width and height before crop is applied
  unsigned int uncrop_width = aDesc.layers[0].planes[0].pitch;
  unsigned int uncrop_height = aAVFrame->height;

  unsigned int offset = aDesc.layers[0].planes[0].offset;

  if (aDesc.layers[0].format == DRM_FORMAT_YUV420) {
    vaDesc.fourcc = VA_FOURCC_I420;

    // V4L2 expresses YUV420 as a single contiguous buffer containing
    // all three planes.  DMABufSurfaceYUV expects the three planes
    // separately, so we have to split them out
    MOZ_ASSERT(aDesc.nb_objects == 1);
    MOZ_ASSERT(aDesc.nb_layers == 1);

    vaDesc.num_objects = 1;
    vaDesc.objects[0].drm_format_modifier = aDesc.objects[0].format_modifier;
    vaDesc.objects[0].size = aDesc.objects[0].size;
    vaDesc.objects[0].fd = aDesc.objects[0].fd;

    vaDesc.num_layers = 3;
    for (int i = 0; i < 3; i++) {
      vaDesc.layers[i].drm_format = DRM_FORMAT_R8;
      vaDesc.layers[i].num_planes = 1;
      vaDesc.layers[i].object_index[0] = 0;
    }
    vaDesc.layers[0].offset[0] = offset;
    vaDesc.layers[0].pitch[0] = uncrop_width;
    vaDesc.layers[1].offset[0] = offset + uncrop_width * uncrop_height;
    vaDesc.layers[1].pitch[0] = uncrop_width / 2;
    vaDesc.layers[2].offset[0] = offset + uncrop_width * uncrop_height * 5 / 4;
    vaDesc.layers[2].pitch[0] = uncrop_width / 2;
  } else if (aDesc.layers[0].format == DRM_FORMAT_NV12) {
    vaDesc.fourcc = VA_FOURCC_NV12;

    // V4L2 expresses NV12 as a single contiguous buffer containing both
    // planes.  DMABufSurfaceYUV expects the two planes separately, so we have
    // to split them out
    MOZ_ASSERT(aDesc.nb_objects == 1);
    MOZ_ASSERT(aDesc.nb_layers == 1);

    vaDesc.num_objects = 1;
    vaDesc.objects[0].drm_format_modifier = aDesc.objects[0].format_modifier;
    vaDesc.objects[0].size = aDesc.objects[0].size;
    vaDesc.objects[0].fd = aDesc.objects[0].fd;

    vaDesc.num_layers = 2;
    for (int i = 0; i < 2; i++) {
      vaDesc.layers[i].num_planes = 1;
      vaDesc.layers[i].object_index[0] = 0;
      vaDesc.layers[i].pitch[0] = uncrop_width;
    }
    vaDesc.layers[0].drm_format = DRM_FORMAT_R8;  // Y plane
    vaDesc.layers[0].offset[0] = offset;
    vaDesc.layers[1].drm_format = DRM_FORMAT_GR88;  // UV plane
    vaDesc.layers[1].offset[0] = offset + uncrop_width * uncrop_height;
  } else {
    DMABUF_LOG("Don't know how to deal with FOURCC 0x%x",
               aDesc.layers[0].format);
    return Nothing();
  }

  return Some(vaDesc);
}

RefPtr<VideoFrameSurface<LIBAV_VER>>
VideoFramePool<LIBAV_VER>::GetVideoFrameSurface(AVDRMFrameDescriptor& aDesc,
                                                int aWidth, int aHeight,
                                                AVCodecContext* aAVCodecContext,
                                                AVFrame* aAVFrame,
                                                FFmpegLibWrapper* aLib) {
  MOZ_ASSERT(aDesc.nb_layers > 0);

  auto layerDesc = FFmpegDescToVA(aDesc, aAVFrame);
  if (layerDesc.isNothing()) {
    return nullptr;
  }

  // Width and height, after cropping
  int crop_width = (int)layerDesc->width;
  int crop_height = (int)layerDesc->height;

  MutexAutoLock lock(mSurfaceLock);

  RefPtr<VideoFrameSurface<LIBAV_VER>> videoSurface =
      GetTargetVideoFrameSurfaceLocked(lock, sInvalidFFMPEGSurfaceID,
                                       /* aRecycleSurface */ false);
  RefPtr<DMABufSurfaceYUV> surface = videoSurface->GetDMABufSurface();

  DMABUF_LOG("Using V4L2 DMABufSurface UID %d", surface->GetUID());

  bool copySurface = mTextureCopyWorks && ShouldCopySurface();
  if (!surface->UpdateYUVData(layerDesc.value(), crop_width, crop_height,
                              copySurface)) {
    if (!copySurface) {
      // Failed without texture copy. We can't do more here.
      return nullptr;
    }
    // Try again without texture copy
    DMABUF_LOG("  DMABuf texture copy is broken");
    copySurface = mTextureCopyWorks = false;
    if (!surface->UpdateYUVData(layerDesc.value(), crop_width, crop_height,
                                copySurface)) {
      return nullptr;
    }
  }

  if (MOZ_UNLIKELY(!mTextureCreationWorks)) {
    mTextureCreationWorks = Some(surface->VerifyTextureCreation());
    if (!*mTextureCreationWorks) {
      DMABUF_LOG("  failed to create texture over DMABuf memory!");
      return nullptr;
    }
  }

  // Don't recycle v4l surfaces, we don't have FFmpegID and we can't ensure
  // match between FFmpeg frame with DMABufSurface.
  videoSurface->DisableRecycle();

  if (!copySurface) {
    videoSurface->LockVAAPIData(aAVCodecContext, aAVFrame, aLib);
  }

  return videoSurface;
}

}  // namespace mozilla
