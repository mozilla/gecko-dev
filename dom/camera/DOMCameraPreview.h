/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERAPREVIEW_H
#define DOM_CAMERA_DOMCAMERAPREVIEW_H

#include "nsCycleCollectionParticipant.h"
#include "MediaStreamGraph.h"
#include "StreamBuffer.h"
#include "ICameraControl.h"
#include "DOMMediaStream.h"
#include "CameraPreviewMediaStream.h"
#include "CameraCommon.h"

class nsGlobalWindow;

namespace mozilla {

/**
 * DOMCameraPreview is only exposed to the DOM as an nsDOMMediaStream,
 * which is a cycle-collection participant already, and we don't
 * add any traceable fields here, so we don't need to declare any
 * more cycle-collection goop.
 */
class DOMCameraPreview : public DOMMediaStream
{
protected:
  enum { TRACK_VIDEO = 1 };

public:
  DOMCameraPreview(nsGlobalWindow* aWindow, ICameraControl* aCameraControl);

  void Start();   // called by the MediaStreamListener to start preview
  void Started(); // called by the CameraControl when preview is started
  void StopPreview(); // called by the MediaStreamListener to stop preview
  void Stopped(bool aForced = false);
                  // called by the CameraControl when preview is stopped
  void Error();   // something went wrong, NS_RELEASE needed

  void SetStateStarted();
  void SetStateStopped();

protected:
  virtual ~DOMCameraPreview();

  enum {
    STOPPED,
    STARTING,
    STARTED,
    STOPPING
  };
  uint32_t mState;

  // Helper function, used in conjunction with the macro below, to make
  //  it easy to track state changes, which must happen only on the main
  //  thread.
  void
  SetState(uint32_t aNewState, const char* aFileOrFunc, int aLine)
  {
#ifdef PR_LOGGING
    const char* states[] = { "stopped", "starting", "started", "stopping" };
    MOZ_ASSERT(mState < sizeof(states) / sizeof(states[0]));
    MOZ_ASSERT(aNewState < sizeof(states) / sizeof(states[0]));
    DOM_CAMERA_LOGI("SetState: (this=%p) '%s' --> '%s' : %s:%d\n", this, states[mState], states[aNewState], aFileOrFunc, aLine);
#endif

    NS_ASSERTION(NS_IsMainThread(), "Preview state set OFF OF main thread!");
    mState = aNewState;
  }

  nsRefPtr<CameraPreviewMediaStream> mInput;
  nsRefPtr<ICameraControl> mCameraControl;

  // Raw pointer; AddListener() keeps the reference for us
  MediaStreamListener* mListener;

private:
  DOMCameraPreview(const DOMCameraPreview&) MOZ_DELETE;
  DOMCameraPreview& operator=(const DOMCameraPreview&) MOZ_DELETE;
};

} // namespace mozilla

#define DOM_CAMERA_SETSTATE(newState)   SetState((newState), __func__, __LINE__)

#endif // DOM_CAMERA_DOMCAMERAPREVIEW_H
