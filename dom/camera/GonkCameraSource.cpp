/*
 * Copyright (C) 2009 The Android Open Source Project
 * Copyright (C) 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <base/basictypes.h>
#include "nsDebug.h"
#define DOM_CAMERA_LOG_LEVEL        3
#include "CameraCommon.h"
/*
#define CS_LOGD(...) DOM_CAMERA_LOGA(__VA_ARGS__)
#define CS_LOGV(...) DOM_CAMERA_LOGI(__VA_ARGS__)
#define CS_LOGI(...) DOM_CAMERA_LOGI(__VA_ARGS__)
#define CS_LOGW(...) DOM_CAMERA_LOGW(__VA_ARGS__)
#define CS_LOGE(...) DOM_CAMERA_LOGE(__VA_ARGS__)
*/

#define CS_LOGD(fmt, ...) DOM_CAMERA_LOGA("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define CS_LOGV(fmt, ...) DOM_CAMERA_LOGI("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define CS_LOGI(fmt, ...) DOM_CAMERA_LOGI("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define CS_LOGW(fmt, ...) DOM_CAMERA_LOGW("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define CS_LOGE(fmt, ...) DOM_CAMERA_LOGE("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)

#include <OMX_Component.h>
#include <binder/IPCThreadState.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <camera/CameraParameters.h>
#include <utils/String8.h>
#include <cutils/properties.h>

#include "GonkCameraSource.h"
#include "GonkCameraListener.h"
#include "GonkCameraHwMgr.h"
#include "ICameraControl.h"

using namespace mozilla;

namespace android {

static const int64_t CAMERA_SOURCE_TIMEOUT_NS = 3000000000LL;

struct GonkCameraSourceListener : public GonkCameraListener {
    GonkCameraSourceListener(const sp<GonkCameraSource> &source);

    virtual void notify(int32_t msgType, int32_t ext1, int32_t ext2);
    virtual bool postData(int32_t msgType, const sp<IMemory> &dataPtr,
                          camera_frame_metadata_t *metadata);

    virtual bool postDataTimestamp(
            nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr);

protected:
    virtual ~GonkCameraSourceListener();

private:
    wp<GonkCameraSource> mSource;

    GonkCameraSourceListener(const GonkCameraSourceListener &);
    GonkCameraSourceListener &operator=(const GonkCameraSourceListener &);
};

GonkCameraSourceListener::GonkCameraSourceListener(const sp<GonkCameraSource> &source)
    : mSource(source) {
}

GonkCameraSourceListener::~GonkCameraSourceListener() {
}

void GonkCameraSourceListener::notify(int32_t msgType, int32_t ext1, int32_t ext2) {
    CS_LOGV("notify(%d, %d, %d)", msgType, ext1, ext2);
}

bool GonkCameraSourceListener::postData(int32_t msgType, const sp<IMemory> &dataPtr,
                                    camera_frame_metadata_t *metadata) {
    CS_LOGV("postData(%d, ptr:%p, size:%d)",
         msgType, dataPtr->pointer(), dataPtr->size());

    sp<GonkCameraSource> source = mSource.promote();
    if (source.get() != NULL) {
        source->dataCallback(msgType, dataPtr);
        return true;
    }
    return false;
}

bool GonkCameraSourceListener::postDataTimestamp(
        nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr) {

    sp<GonkCameraSource> source = mSource.promote();
    if (source.get() != NULL) {
        source->dataCallbackTimestamp(timestamp/1000, msgType, dataPtr);
        return true;
    }
    return false;
}

static int32_t getColorFormat(const char* colorFormat) {
    return OMX_COLOR_FormatYUV420SemiPlanar; //XXX nsGonkCameraControl uses only YUV420SemiPlanar

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV420P)) {
       return OMX_COLOR_FormatYUV420Planar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV422SP)) {
       return OMX_COLOR_FormatYUV422SemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV420SP)) {
        return OMX_COLOR_FormatYUV420SemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV422I)) {
        return OMX_COLOR_FormatYCbYCr;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_RGB565)) {
       return OMX_COLOR_Format16bitRGB565;
    }

    if (!strcmp(colorFormat, "OMX_TI_COLOR_FormatYUV420PackedSemiPlanar")) {
       return OMX_TI_COLOR_FormatYUV420PackedSemiPlanar;
    }
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_ANDROID_OPAQUE)) {
        return OMX_COLOR_FormatAndroidOpaque;
    }
#endif
    CS_LOGE("Uknown color format (%s), please add it to "
         "GonkCameraSource::getColorFormat", colorFormat);

    CHECK(!"Unknown color format");
}

GonkCameraSource *GonkCameraSource::Create(
    const sp<GonkCameraHardware>& aCameraHw,
    Size videoSize,
    int32_t frameRate,
    bool storeMetaDataInVideoBuffers) {

    GonkCameraSource *source = new GonkCameraSource(aCameraHw,
                    videoSize, frameRate,
                    storeMetaDataInVideoBuffers);
    return source;
}

GonkCameraSource *GonkCameraSource::Create(
    ICameraControl* aControl,
    Size videoSize,
    int32_t frameRate)
{
    mozilla::nsGonkCameraControl* control =
        static_cast<mozilla::nsGonkCameraControl*>(aControl);
    return Create(control->GetCameraHw(), videoSize, frameRate, false);
}

GonkCameraSource::GonkCameraSource(
    const sp<GonkCameraHardware>& aCameraHw,
    Size videoSize,
    int32_t frameRate,
    bool storeMetaDataInVideoBuffers)
    : mCameraFlags(0),
      mNumInputBuffers(0),
      mVideoFrameRate(-1),
      mNumFramesReceived(0),
      mLastFrameTimestampUs(0),
      mStarted(false),
      mNumFramesEncoded(0),
      mTimeBetweenFrameCaptureUs(0),
      mRateLimit(false),
      mFirstFrameTimeUs(0),
      mNumFramesDropped(0),
      mNumGlitches(0),
      mGlitchDurationThresholdUs(200000),
      mCollectStats(false),
      mCameraHw(aCameraHw) {
    mVideoSize.width  = -1;
    mVideoSize.height = -1;

    mInitCheck = init(
                    videoSize, frameRate,
                    storeMetaDataInVideoBuffers);
    if (mInitCheck != OK) releaseCamera();
}

status_t GonkCameraSource::initCheck() const {
    return mInitCheck;
}

//TODO: Do we need to reimplement isCameraAvailable?

/*
 * Check to see whether the requested video width and height is one
 * of the supported sizes.
 * @param width the video frame width in pixels
 * @param height the video frame height in pixels
 * @param suppportedSizes the vector of sizes that we check against
 * @return true if the dimension (width and height) is supported.
 */
static bool isVideoSizeSupported(
    int32_t width, int32_t height,
    const Vector<Size>& supportedSizes) {

    CS_LOGV("isVideoSizeSupported");
    for (size_t i = 0; i < supportedSizes.size(); ++i) {
        if (width  == supportedSizes[i].width &&
            height == supportedSizes[i].height) {
            return true;
        }
    }
    return false;
}

/*
 * If the preview and video output is separate, we only set the
 * the video size, and applications should set the preview size
 * to some proper value, and the recording framework will not
 * change the preview size; otherwise, if the video and preview
 * output is the same, we need to set the preview to be the same
 * as the requested video size.
 *
 */
/*
 * Query the camera to retrieve the supported video frame sizes
 * and also to see whether CameraParameters::setVideoSize()
 * is supported or not.
 * @param params CameraParameters to retrieve the information
 * @@param isSetVideoSizeSupported retunrs whether method
 *      CameraParameters::setVideoSize() is supported or not.
 * @param sizes returns the vector of Size objects for the
 *      supported video frame sizes advertised by the camera.
 */
static void getSupportedVideoSizes(
    const CameraParameters& params,
    bool *isSetVideoSizeSupported,
    Vector<Size>& sizes) {

    *isSetVideoSizeSupported = true;
    params.getSupportedVideoSizes(sizes);
    if (sizes.size() == 0) {
        CS_LOGD("Camera does not support setVideoSize()");
        params.getSupportedPreviewSizes(sizes);
        *isSetVideoSizeSupported = false;
    }
}

/*
 * Check whether the camera has the supported color format
 * @param params CameraParameters to retrieve the information
 * @return OK if no error.
 */
status_t GonkCameraSource::isCameraColorFormatSupported(
        const CameraParameters& params) {
    mColorFormat = getColorFormat(params.get(
            CameraParameters::KEY_VIDEO_FRAME_FORMAT));
    if (mColorFormat == -1) {
        return BAD_VALUE;
    }
    return OK;
}

/*
 * Configure the camera to use the requested video size
 * (width and height) and/or frame rate. If both width and
 * height are -1, configuration on the video size is skipped.
 * if frameRate is -1, configuration on the frame rate
 * is skipped. Skipping the configuration allows one to
 * use the current camera setting without the need to
 * actually know the specific values (see Create() method).
 *
 * @param params the CameraParameters to be configured
 * @param width the target video frame width in pixels
 * @param height the target video frame height in pixels
 * @param frameRate the target frame rate in frames per second.
 * @return OK if no error.
 */
status_t GonkCameraSource::configureCamera(
        CameraParameters* params,
        int32_t width, int32_t height,
        int32_t frameRate) {
    CS_LOGV("configureCamera");
    Vector<Size> sizes;
    bool isSetVideoSizeSupportedByCamera = true;
    getSupportedVideoSizes(*params, &isSetVideoSizeSupportedByCamera, sizes);
    bool isCameraParamChanged = false;
    if (width != -1 && height != -1) {
        if (!isVideoSizeSupported(width, height, sizes)) {
            CS_LOGE("Video dimension (%dx%d) is unsupported", width, height);
            return BAD_VALUE;
        }
        if (isSetVideoSizeSupportedByCamera) {
            params->setVideoSize(width, height);
        } else {
            params->setPreviewSize(width, height);
        }
        isCameraParamChanged = true;
    } else if ((width == -1 && height != -1) ||
               (width != -1 && height == -1)) {
        // If one and only one of the width and height is -1
        // we reject such a request.
        CS_LOGE("Requested video size (%dx%d) is not supported", width, height);
        return BAD_VALUE;
    } else {  // width == -1 && height == -1
        // Do not configure the camera.
        // Use the current width and height value setting from the camera.
    }

    if (frameRate != -1) {
        CHECK(frameRate > 0 && frameRate <= 120);
        const char* supportedFrameRates =
                params->get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES);
        CHECK(supportedFrameRates != NULL);
        CS_LOGV("Supported frame rates: %s", supportedFrameRates);
        char buf[4];
        snprintf(buf, 4, "%d", frameRate);
        if (strstr(supportedFrameRates, buf) == NULL) {
            CS_LOGE("Requested frame rate (%d) is not supported: %s",
                frameRate, supportedFrameRates);
            return BAD_VALUE;
        }

        // The frame rate is supported, set the camera to the requested value.
        params->setPreviewFrameRate(frameRate);
        isCameraParamChanged = true;
    } else {  // frameRate == -1
        // Do not configure the camera.
        // Use the current frame rate value setting from the camera
    }

    if (isCameraParamChanged) {
        // Either frame rate or frame size needs to be changed.
        if (OK != mCameraHw->PushParameters(*params)) {
            CS_LOGE("Could not change settings."
                 " Someone else is using camera?");
            return -EBUSY;
        }
    }
    return OK;
}

/*
 * Check whether the requested video frame size
 * has been successfully configured or not. If both width and height
 * are -1, check on the current width and height value setting
 * is performed.
 *
 * @param params CameraParameters to retrieve the information
 * @param the target video frame width in pixels to check against
 * @param the target video frame height in pixels to check against
 * @return OK if no error
 */
status_t GonkCameraSource::checkVideoSize(
        const CameraParameters& params,
        int32_t width, int32_t height) {

    CS_LOGV("checkVideoSize");
    // The actual video size is the same as the preview size
    // if the camera hal does not support separate video and
    // preview output. In this case, we retrieve the video
    // size from preview.
    int32_t frameWidthActual = -1;
    int32_t frameHeightActual = -1;
    Vector<Size> sizes;
    params.getSupportedVideoSizes(sizes);
    if (sizes.size() == 0) {
        // video size is the same as preview size
        params.getPreviewSize(&frameWidthActual, &frameHeightActual);
    } else {
        // video size may not be the same as preview
        params.getVideoSize(&frameWidthActual, &frameHeightActual);
    }
    if (frameWidthActual < 0 || frameHeightActual < 0) {
        CS_LOGE("Failed to retrieve video frame size (%dx%d)",
                frameWidthActual, frameHeightActual);
        return UNKNOWN_ERROR;
    }

    // Check the actual video frame size against the target/requested
    // video frame size.
    if (width != -1 && height != -1) {
        if (frameWidthActual != width || frameHeightActual != height) {
            CS_LOGE("Failed to set video frame size to %dx%d. "
                    "The actual video size is %dx%d ", width, height,
                    frameWidthActual, frameHeightActual);
            return UNKNOWN_ERROR;
        }
    }

    // Good now.
    mVideoSize.width = frameWidthActual;
    mVideoSize.height = frameHeightActual;
    return OK;
}

/*
 * Check the requested frame rate has been successfully configured or not.
 * If the target frameRate is -1, check on the current frame rate value
 * setting is performed.
 *
 * @param params CameraParameters to retrieve the information
 * @param the target video frame rate to check against
 * @return OK if no error.
 */
status_t GonkCameraSource::checkFrameRate(
        const CameraParameters& params,
        int32_t frameRate) {

    CS_LOGV("checkFrameRate");
    int32_t frameRateActual = params.getPreviewFrameRate();
    if (frameRateActual < 0) {
        CS_LOGE("Failed to retrieve preview frame rate (%d)", frameRateActual);
        return UNKNOWN_ERROR;
    }

    // Check the actual video frame rate against the target/requested
    // video frame rate.
    if (frameRate != -1 && (frameRateActual - frameRate) != 0) {
        CS_LOGE("Failed to set preview frame rate to %d fps. The actual "
                "frame rate is %d", frameRate, frameRateActual);
        return UNKNOWN_ERROR;
    }

    // Good now.
    mVideoFrameRate = frameRateActual;
    return OK;
}

/*
 * Initialize the GonkCameraSource so that it becomes
 * ready for providing the video input streams as requested.
 * @param camera the camera object used for the video source
 * @param cameraId if camera == 0, use camera with this id
 *      as the video source
 * @param videoSize the target video frame size. If both
 *      width and height in videoSize is -1, use the current
 *      width and heigth settings by the camera
 * @param frameRate the target frame rate in frames per second.
 *      if it is -1, use the current camera frame rate setting.
 * @param storeMetaDataInVideoBuffers request to store meta
 *      data or real YUV data in video buffers. Request to
 *      store meta data in video buffers may not be honored
 *      if the source does not support this feature.
 *
 * @return OK if no error.
 */
status_t GonkCameraSource::init(
        Size videoSize,
        int32_t frameRate,
        bool storeMetaDataInVideoBuffers) {

    CS_LOGV("init");
    status_t err = OK;
    //TODO: need to do something here to check the sanity of camera

    CameraParameters params;
    mCameraHw->PullParameters(params);
    if ((err = isCameraColorFormatSupported(params)) != OK) {
        return err;
    }

    // Set the camera to use the requested video frame size
    // and/or frame rate.
    if ((err = configureCamera(&params,
                    videoSize.width, videoSize.height,
                    frameRate))) {
        return err;
    }

    // Check on video frame size and frame rate.
    CameraParameters newCameraParams;
    mCameraHw->PullParameters(newCameraParams);
    if ((err = checkVideoSize(newCameraParams,
                videoSize.width, videoSize.height)) != OK) {
        return err;
    }
    if ((err = checkFrameRate(newCameraParams, frameRate)) != OK) {
        return err;
    }

    // By default, do not store metadata in video buffers
    mIsMetaDataStoredInVideoBuffers = false;
    mCameraHw->StoreMetaDataInBuffers(false);
    if (storeMetaDataInVideoBuffers) {
        if (OK == mCameraHw->StoreMetaDataInBuffers(true)) {
            mIsMetaDataStoredInVideoBuffers = true;
        }
    }

    int64_t glitchDurationUs = (1000000LL / mVideoFrameRate);
    if (glitchDurationUs > mGlitchDurationThresholdUs) {
        mGlitchDurationThresholdUs = glitchDurationUs;
    }

    // XXX: query camera for the stride and slice height
    // when the capability becomes available.
    mMeta = new MetaData;
    mMeta->setCString(kKeyMIMEType,  MEDIA_MIMETYPE_VIDEO_RAW);
    mMeta->setInt32(kKeyColorFormat, mColorFormat);
    mMeta->setInt32(kKeyWidth,       mVideoSize.width);
    mMeta->setInt32(kKeyHeight,      mVideoSize.height);
    mMeta->setInt32(kKeyStride,      mVideoSize.width);
    mMeta->setInt32(kKeySliceHeight, mVideoSize.height);
    mMeta->setInt32(kKeyFrameRate,   mVideoFrameRate);
    return OK;
}

GonkCameraSource::~GonkCameraSource() {
    if (mStarted) {
        reset();
    } else if (mInitCheck == OK) {
        // Camera is initialized but because start() is never called,
        // the lock on Camera is never released(). This makes sure
        // Camera's lock is released in this case.
        // TODO: Don't think I need to do this
        releaseCamera();
    }
}

int GonkCameraSource::startCameraRecording() {
    CS_LOGV("startCameraRecording");
    return mCameraHw->StartRecording();
}

status_t GonkCameraSource::start(MetaData *meta) {
    int rv;

    CS_LOGV("start");
    CHECK(!mStarted);
    if (mInitCheck != OK) {
        CS_LOGE("GonkCameraSource is not initialized yet");
        return mInitCheck;
    }

    char value[PROPERTY_VALUE_MAX];
    if (property_get("media.stagefright.record-stats", value, NULL)
        && (!strcmp(value, "1") || !strcasecmp(value, "true"))) {
        mCollectStats = true;
    }

    mStartTimeUs = 0;
    mNumInputBuffers = 0;
    if (meta) {
        int64_t startTimeUs;
        if (meta->findInt64(kKeyTime, &startTimeUs)) {
            mStartTimeUs = startTimeUs;
        }
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
        int32_t nBuffers;
        if (meta->findInt32(kKeyNumBuffers, &nBuffers)) {
            CHECK_GT(nBuffers, 0);
            mNumInputBuffers = nBuffers;
        }
#endif
    }

    // Register a listener with GonkCameraHardware so that we can get callbacks
    mCameraHw->SetListener(new GonkCameraSourceListener(this));

    rv = startCameraRecording();

    mStarted = (rv == OK);
    return rv;
}

void GonkCameraSource::stopCameraRecording() {
    CS_LOGV("stopCameraRecording");
    mCameraHw->StopRecording();
}

void GonkCameraSource::releaseCamera() {
    CS_LOGV("releaseCamera");
}

status_t GonkCameraSource::reset() {
    CS_LOGD("reset: E");
    Mutex::Autolock autoLock(mLock);
    mStarted = false;
    mFrameAvailableCondition.signal();

    releaseQueuedFrames();
    while (!mFramesBeingEncoded.empty()) {
        if (NO_ERROR !=
            mFrameCompleteCondition.waitRelative(mLock,
                    mTimeBetweenFrameCaptureUs * 1000LL + CAMERA_SOURCE_TIMEOUT_NS)) {
            CS_LOGW("Timed out waiting for outstanding frames being encoded: %d",
                mFramesBeingEncoded.size());
        }
    }
    stopCameraRecording();
    if (mRateLimit) {
      mRateLimit = false;
      mCameraHw->OnRateLimitPreview(false);
    }
    releaseCamera();

    if (mDirectBufferListener.get()) {
      mDirectBufferListener = nullptr;
    }

    if (mCollectStats) {
        CS_LOGI("Frames received/encoded/dropped: %d/%d/%d in %lld us",
                mNumFramesReceived, mNumFramesEncoded, mNumFramesDropped,
                mLastFrameTimestampUs - mFirstFrameTimeUs);
    }

    if (mNumGlitches > 0) {
        CS_LOGW("%d long delays between neighboring video frames", mNumGlitches);
    }

    CHECK_EQ(mNumFramesReceived, mNumFramesEncoded + mNumFramesDropped);
    CS_LOGD("reset: X");
    return OK;
}

void GonkCameraSource::releaseRecordingFrame(const sp<IMemory>& frame) {
    CS_LOGV("releaseRecordingFrame");
    mCameraHw->ReleaseRecordingFrame(frame);
}

void GonkCameraSource::releaseQueuedFrames() {
    List<sp<IMemory> >::iterator it;
    while (!mFramesReceived.empty()) {
        it = mFramesReceived.begin();
        releaseRecordingFrame(*it);
        mFramesReceived.erase(it);
        ++mNumFramesDropped;
    }
}

sp<MetaData> GonkCameraSource::getFormat() {
    return mMeta;
}

void GonkCameraSource::releaseOneRecordingFrame(const sp<IMemory>& frame) {
    releaseRecordingFrame(frame);
}

void GonkCameraSource::signalBufferReturned(MediaBuffer *buffer) {
    CS_LOGV("signalBufferReturned: %p", buffer->data());
    Mutex::Autolock autoLock(mLock);
    for (List<sp<IMemory> >::iterator it = mFramesBeingEncoded.begin();
         it != mFramesBeingEncoded.end(); ++it) {
        if ((*it)->pointer() ==  buffer->data()) {
            releaseOneRecordingFrame((*it));
            mFramesBeingEncoded.erase(it);
            ++mNumFramesEncoded;
            buffer->setObserver(0);
            buffer->release();
            mFrameCompleteCondition.signal();
            return;
        }
    }
    CHECK(!"signalBufferReturned: bogus buffer");
}

status_t GonkCameraSource::AddDirectBufferListener(DirectBufferListener* aListener) {
    if (mDirectBufferListener.get()) {
        return UNKNOWN_ERROR;
    }
    mDirectBufferListener = aListener;
    return OK;
}

status_t GonkCameraSource::read(
        MediaBuffer **buffer, const ReadOptions *options) {
    CS_LOGV("read");

    *buffer = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        return ERROR_UNSUPPORTED;
    }

    sp<IMemory> frame;
    int64_t frameTime;

    {
        Mutex::Autolock autoLock(mLock);
        while (mStarted && mFramesReceived.empty()) {
            if (NO_ERROR !=
                mFrameAvailableCondition.waitRelative(mLock,
                    mTimeBetweenFrameCaptureUs * 1000LL + CAMERA_SOURCE_TIMEOUT_NS)) {
                //TODO: check sanity of camera?
                CS_LOGW("Timed out waiting for incoming camera video frames: %lld us",
                    mLastFrameTimestampUs);
            }
        }
        if (!mStarted) {
            return OK;
        }
        frame = *mFramesReceived.begin();
        mFramesReceived.erase(mFramesReceived.begin());

        frameTime = *mFrameTimes.begin();
        mFrameTimes.erase(mFrameTimes.begin());
        mFramesBeingEncoded.push_back(frame);
        *buffer = new MediaBuffer(frame->pointer(), frame->size());
        (*buffer)->setObserver(this);
        (*buffer)->add_ref();
        (*buffer)->meta_data()->setInt64(kKeyTime, frameTime);
    }
    return OK;
}

void GonkCameraSource::dataCallbackTimestamp(int64_t timestampUs,
        int32_t msgType, const sp<IMemory> &data) {
    bool rateLimit;
    bool prevRateLimit;
    CS_LOGV("dataCallbackTimestamp: timestamp %lld us", timestampUs);
    {
        Mutex::Autolock autoLock(mLock);
        if (!mStarted || (mNumFramesReceived == 0 && timestampUs < mStartTimeUs)) {
            CS_LOGV("Drop frame at %lld/%lld us", timestampUs, mStartTimeUs);
            releaseOneRecordingFrame(data);
            return;
        }

        if (mNumFramesReceived > 0) {
            if (timestampUs <= mLastFrameTimestampUs) {
                CS_LOGE("Drop frame at %lld us, before last at %lld us",
                    timestampUs, mLastFrameTimestampUs);
                releaseOneRecordingFrame(data);
                return;
            }
            if (timestampUs - mLastFrameTimestampUs > mGlitchDurationThresholdUs) {
                ++mNumGlitches;
            }
        }

        // May need to skip frame or modify timestamp. Currently implemented
        // by the subclass CameraSourceTimeLapse.
        if (skipCurrentFrame(timestampUs)) {
            releaseOneRecordingFrame(data);
            return;
        }

        mLastFrameTimestampUs = timestampUs;
        if (mNumFramesReceived == 0) {
            mFirstFrameTimeUs = timestampUs;
            // Initial delay
            if (mStartTimeUs > 0) {
                if (timestampUs < mStartTimeUs) {
                    // Frame was captured before recording was started
                    // Drop it without updating the statistical data.
                    releaseOneRecordingFrame(data);
                    return;
                }
                mStartTimeUs = timestampUs - mStartTimeUs;
            }
        }
        ++mNumFramesReceived;

        // If a backlog is building up in the receive queue, we are likely
        // resource constrained and we need to throttle
        prevRateLimit = mRateLimit;
        rateLimit = mFramesReceived.empty();
        mRateLimit = rateLimit;

        CHECK(data != NULL && data->size() > 0);
        mFramesReceived.push_back(data);
        int64_t timeUs = mStartTimeUs + (timestampUs - mFirstFrameTimeUs);
        mFrameTimes.push_back(timeUs);
        CS_LOGV("initial delay: %lld, current time stamp: %lld",
            mStartTimeUs, timeUs);
        mFrameAvailableCondition.signal();
    }

    if(prevRateLimit != rateLimit) {
        mCameraHw->OnRateLimitPreview(rateLimit);
    }

    if (mDirectBufferListener.get()) {
        MediaBuffer* mediaBuffer;
        if (read(&mediaBuffer) == OK) {
            mDirectBufferListener->BufferAvailable(mediaBuffer);
            // read() calls MediaBuffer->add_ref() so it needs to be released here.
            mediaBuffer->release();
        }
    }
}

bool GonkCameraSource::isMetaDataStoredInVideoBuffers() const {
    CS_LOGV("isMetaDataStoredInVideoBuffers");
    return mIsMetaDataStoredInVideoBuffers;
}

}  // namespace android
