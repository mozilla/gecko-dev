/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_vr_VRMessageUtils_h
#define mozilla_gfx_vr_VRMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "VRManager.h"

#include "gfxVR.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::gfx::VRDeviceType>
    : public ContiguousEnumSerializer<
          mozilla::gfx::VRDeviceType, mozilla::gfx::VRDeviceType(0),
          mozilla::gfx::VRDeviceType(
              mozilla::gfx::VRDeviceType::NumVRDeviceTypes)> {};

template <>
struct ParamTraits<mozilla::gfx::VRDisplayCapabilityFlags>
    : public BitFlagsEnumSerializer<
          mozilla::gfx::VRDisplayCapabilityFlags,
          mozilla::gfx::VRDisplayCapabilityFlags::Cap_All> {};

template <>
struct ParamTraits<mozilla::gfx::VRDisplayState> {
  typedef mozilla::gfx::VRDisplayState paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    // TODO - VRDisplayState is asserted to be a POD type
    //        A simple memcpy may be sufficient here, or
    //        this code can be refactored out if we use
    //        shmem between the VR and content process.
    nsCString displayName;
    displayName.Assign(aParam.mDisplayName);
    WriteParam(aMsg, displayName);
    WriteParam(aMsg, aParam.mCapabilityFlags);
    WriteParam(aMsg, aParam.mEyeResolution.width);
    WriteParam(aMsg, aParam.mEyeResolution.height);
    WriteParam(aMsg, aParam.mSuppressFrames);
    WriteParam(aMsg, aParam.mIsConnected);
    WriteParam(aMsg, aParam.mIsMounted);
    WriteParam(aMsg, aParam.mStageSize.width);
    WriteParam(aMsg, aParam.mStageSize.height);
    WriteParam(aMsg, aParam.mLastSubmittedFrameId);
    WriteParam(aMsg, aParam.mPresentingGeneration);
    for (int i = 0; i < 16; i++) {
      // TODO - Should probably memcpy the whole array or
      // convert Maxtrix4x4 to a POD type and use it
      // instead
      WriteParam(aMsg, aParam.mSittingToStandingTransform[i]);
    }
    for (int i = 0; i < mozilla::gfx::VRDisplayState::NumEyes; i++) {
      WriteParam(aMsg, aParam.mEyeFOV[i]);
      WriteParam(aMsg, aParam.mEyeTranslation[i].x);
      WriteParam(aMsg, aParam.mEyeTranslation[i].y);
      WriteParam(aMsg, aParam.mEyeTranslation[i].z);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    nsCString displayName;
    if (!ReadParam(aMsg, aIter, &(displayName)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCapabilityFlags)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEyeResolution.width)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEyeResolution.height)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSuppressFrames)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIsConnected)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIsMounted)) ||
        !ReadParam(aMsg, aIter, &(aResult->mStageSize.width)) ||
        !ReadParam(aMsg, aIter, &(aResult->mStageSize.height)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLastSubmittedFrameId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPresentingGeneration))) {
      return false;
    }
    for (int i = 0; i < 16; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mSittingToStandingTransform[i]))) {
        return false;
      }
    }
    strncpy(aResult->mDisplayName, displayName.BeginReading(),
            mozilla::gfx::kVRDisplayNameMaxLen);
    for (int i = 0; i < mozilla::gfx::VRDisplayState::NumEyes; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mEyeFOV[i])) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeTranslation[i].x)) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeTranslation[i].y)) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeTranslation[i].z))) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRDisplayInfo> {
  typedef mozilla::gfx::VRDisplayInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mDisplayID);
    WriteParam(aMsg, aParam.mPresentingGroups);
    WriteParam(aMsg, aParam.mGroupMask);
    WriteParam(aMsg, aParam.mFrameId);
    WriteParam(aMsg, aParam.mDisplayState);
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.mLastSensorState); i++) {
      WriteParam(aMsg, aParam.mLastSensorState[i]);
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.mLastFrameStart); i++) {
      WriteParam(aMsg, aParam.mLastFrameStart[i]);
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.mControllerState); i++) {
      WriteParam(aMsg, aParam.mControllerState[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDisplayID)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPresentingGroups)) ||
        !ReadParam(aMsg, aIter, &(aResult->mGroupMask)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDisplayState))) {
      return false;
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->mLastSensorState);
         i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mLastSensorState[i]))) {
        return false;
      }
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->mLastFrameStart);
         i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mLastFrameStart[i]))) {
        return false;
      }
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->mControllerState);
         i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mControllerState[i]))) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRPose> {
  typedef mozilla::gfx::VRPose paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.orientation[0]);
    WriteParam(aMsg, aParam.orientation[1]);
    WriteParam(aMsg, aParam.orientation[2]);
    WriteParam(aMsg, aParam.orientation[3]);
    WriteParam(aMsg, aParam.position[0]);
    WriteParam(aMsg, aParam.position[1]);
    WriteParam(aMsg, aParam.position[2]);
    WriteParam(aMsg, aParam.angularVelocity[0]);
    WriteParam(aMsg, aParam.angularVelocity[1]);
    WriteParam(aMsg, aParam.angularVelocity[2]);
    WriteParam(aMsg, aParam.angularAcceleration[0]);
    WriteParam(aMsg, aParam.angularAcceleration[1]);
    WriteParam(aMsg, aParam.angularAcceleration[2]);
    WriteParam(aMsg, aParam.linearVelocity[0]);
    WriteParam(aMsg, aParam.linearVelocity[1]);
    WriteParam(aMsg, aParam.linearVelocity[2]);
    WriteParam(aMsg, aParam.linearAcceleration[0]);
    WriteParam(aMsg, aParam.linearAcceleration[1]);
    WriteParam(aMsg, aParam.linearAcceleration[2]);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->orientation[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[3])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularVelocity[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularVelocity[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularVelocity[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularAcceleration[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularAcceleration[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularAcceleration[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearVelocity[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearVelocity[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearVelocity[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[2]))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRHMDSensorState> {
  typedef mozilla::gfx::VRHMDSensorState paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.timestamp);
    WriteParam(aMsg, aParam.inputFrameID);
    WriteParam(aMsg, aParam.flags);
    WriteParam(aMsg, aParam.pose);
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.leftViewMatrix); i++) {
      WriteParam(aMsg, aParam.leftViewMatrix[i]);
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.rightViewMatrix); i++) {
      WriteParam(aMsg, aParam.rightViewMatrix[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->timestamp)) ||
        !ReadParam(aMsg, aIter, &(aResult->inputFrameID)) ||
        !ReadParam(aMsg, aIter, &(aResult->flags)) ||
        !ReadParam(aMsg, aIter, &(aResult->pose))) {
      return false;
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->leftViewMatrix); i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->leftViewMatrix[i]))) {
        return false;
      }
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->rightViewMatrix);
         i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->rightViewMatrix[i]))) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRFieldOfView> {
  typedef mozilla::gfx::VRFieldOfView paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.upDegrees);
    WriteParam(aMsg, aParam.rightDegrees);
    WriteParam(aMsg, aParam.downDegrees);
    WriteParam(aMsg, aParam.leftDegrees);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->upDegrees)) ||
        !ReadParam(aMsg, aIter, &(aResult->rightDegrees)) ||
        !ReadParam(aMsg, aIter, &(aResult->downDegrees)) ||
        !ReadParam(aMsg, aIter, &(aResult->leftDegrees))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRControllerState> {
  typedef mozilla::gfx::VRControllerState paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    nsCString controllerName;
    controllerName.Assign(
        aParam.controllerName);  // FINDME!! HACK! - Bounds checking?
    WriteParam(aMsg, controllerName);
    WriteParam(aMsg, aParam.hand);
    WriteParam(aMsg, aParam.numButtons);
    WriteParam(aMsg, aParam.numAxes);
    WriteParam(aMsg, aParam.numHaptics);
    WriteParam(aMsg, aParam.buttonPressed);
    WriteParam(aMsg, aParam.buttonTouched);
    WriteParam(aMsg, aParam.flags);
    WriteParam(aMsg, aParam.pose);
    WriteParam(aMsg, aParam.isPositionValid);
    WriteParam(aMsg, aParam.isOrientationValid);
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.axisValue); i++) {
      WriteParam(aMsg, aParam.axisValue[i]);
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aParam.triggerValue); i++) {
      WriteParam(aMsg, aParam.triggerValue[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    nsCString controllerName;
    if (!ReadParam(aMsg, aIter, &(controllerName)) ||
        !ReadParam(aMsg, aIter, &(aResult->hand)) ||
        !ReadParam(aMsg, aIter, &(aResult->numButtons)) ||
        !ReadParam(aMsg, aIter, &(aResult->numAxes)) ||
        !ReadParam(aMsg, aIter, &(aResult->numHaptics)) ||
        !ReadParam(aMsg, aIter, &(aResult->buttonPressed)) ||
        !ReadParam(aMsg, aIter, &(aResult->buttonTouched)) ||
        !ReadParam(aMsg, aIter, &(aResult->flags)) ||
        !ReadParam(aMsg, aIter, &(aResult->pose)) ||
        !ReadParam(aMsg, aIter, &(aResult->isPositionValid)) ||
        !ReadParam(aMsg, aIter, &(aResult->isOrientationValid))) {
      return false;
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->axisValue); i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->axisValue[i]))) {
        return false;
      }
    }
    for (size_t i = 0; i < mozilla::ArrayLength(aResult->triggerValue); i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->triggerValue[i]))) {
        return false;
      }
    }
    strncpy(aResult->controllerName, controllerName.BeginReading(),
            mozilla::gfx::kVRControllerNameMaxLen);  // FINDME! TODO! HACK!
                                                     // Safe? Better way?

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRControllerInfo> {
  typedef mozilla::gfx::VRControllerInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mControllerID);
    WriteParam(aMsg, aParam.mMappingType);
    WriteParam(aMsg, aParam.mControllerState);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mControllerID)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMappingType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mControllerState))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRSubmitFrameResultInfo> {
  typedef mozilla::gfx::VRSubmitFrameResultInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mBase64Image);
    WriteParam(aMsg, aParam.mFormat);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mFrameNum);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mBase64Image)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFormat)) ||
        !ReadParam(aMsg, aIter, &(aResult->mWidth)) ||
        !ReadParam(aMsg, aIter, &(aResult->mHeight)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameNum))) {
      return false;
    }

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_gfx_vr_VRMessageUtils_h
