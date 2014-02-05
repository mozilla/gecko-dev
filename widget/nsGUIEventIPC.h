/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEventIPC_h__
#define nsGUIEventIPC_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

namespace IPC
{

template<>
struct ParamTraits<mozilla::BaseEventFlags>
{
  typedef mozilla::BaseEventFlags paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult))) {
      return false;
    }
    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

template<>
struct ParamTraits<mozilla::WidgetEvent>
{
  typedef mozilla::WidgetEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, (uint8_t) aParam.eventStructType);
    WriteParam(aMsg, aParam.message);
    WriteParam(aMsg, aParam.refPoint);
    WriteParam(aMsg, aParam.time);
    WriteParam(aMsg, aParam.mFlags);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t eventStructType = 0;
    bool ret = ReadParam(aMsg, aIter, &eventStructType) &&
               ReadParam(aMsg, aIter, &aResult->message) &&
               ReadParam(aMsg, aIter, &aResult->refPoint) &&
               ReadParam(aMsg, aIter, &aResult->time) &&
               ReadParam(aMsg, aIter, &aResult->mFlags);
    aResult->eventStructType = static_cast<nsEventStructType>(eventStructType);
    return ret;
  }
};

template<>
struct ParamTraits<mozilla::WidgetGUIEvent>
{
  typedef mozilla::WidgetGUIEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetEvent>(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, static_cast<mozilla::WidgetEvent*>(aResult));
  }
};

template<>
struct ParamTraits<mozilla::WidgetInputEvent>
{
  typedef mozilla::WidgetInputEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.modifiers);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->modifiers);
  }
};

template<>
struct ParamTraits<mozilla::WidgetMouseEventBase>
{
  typedef mozilla::WidgetMouseEventBase paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetInputEvent>(aParam));
    WriteParam(aMsg, aParam.button);
    WriteParam(aMsg, aParam.buttons);
    WriteParam(aMsg, aParam.pressure);
    WriteParam(aMsg, aParam.inputSource);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->button) &&
           ReadParam(aMsg, aIter, &aResult->buttons) &&
           ReadParam(aMsg, aIter, &aResult->pressure) &&
           ReadParam(aMsg, aIter, &aResult->inputSource);
  }
};

template<>
struct ParamTraits<mozilla::WidgetWheelEvent>
{
  typedef mozilla::WidgetWheelEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEventBase>(aParam));
    WriteParam(aMsg, aParam.deltaX);
    WriteParam(aMsg, aParam.deltaY);
    WriteParam(aMsg, aParam.deltaZ);
    WriteParam(aMsg, aParam.deltaMode);
    WriteParam(aMsg, aParam.customizedByUserPrefs);
    WriteParam(aMsg, aParam.isMomentum);
    WriteParam(aMsg, aParam.isPixelOnlyDevice);
    WriteParam(aMsg, aParam.lineOrPageDeltaX);
    WriteParam(aMsg, aParam.lineOrPageDeltaY);
    WriteParam(aMsg, static_cast<int32_t>(aParam.scrollType));
    WriteParam(aMsg, aParam.overflowDeltaX);
    WriteParam(aMsg, aParam.overflowDeltaY);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int32_t scrollType = 0;
    bool rv =
      ReadParam(aMsg, aIter,
                static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
      ReadParam(aMsg, aIter, &aResult->deltaX) &&
      ReadParam(aMsg, aIter, &aResult->deltaY) &&
      ReadParam(aMsg, aIter, &aResult->deltaZ) &&
      ReadParam(aMsg, aIter, &aResult->deltaMode) &&
      ReadParam(aMsg, aIter, &aResult->customizedByUserPrefs) &&
      ReadParam(aMsg, aIter, &aResult->isMomentum) &&
      ReadParam(aMsg, aIter, &aResult->isPixelOnlyDevice) &&
      ReadParam(aMsg, aIter, &aResult->lineOrPageDeltaX) &&
      ReadParam(aMsg, aIter, &aResult->lineOrPageDeltaY) &&
      ReadParam(aMsg, aIter, &scrollType) &&
      ReadParam(aMsg, aIter, &aResult->overflowDeltaX) &&
      ReadParam(aMsg, aIter, &aResult->overflowDeltaY);
    aResult->scrollType =
      static_cast<mozilla::WidgetWheelEvent::ScrollType>(scrollType);
    return rv;
  }
};

template<>
struct ParamTraits<mozilla::WidgetMouseEvent>
{
  typedef mozilla::WidgetMouseEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEventBase>(aParam));
    WriteParam(aMsg, aParam.ignoreRootScrollFrame);
    WriteParam(aMsg, (uint8_t) aParam.reason);
    WriteParam(aMsg, (uint8_t) aParam.context);
    WriteParam(aMsg, (uint8_t) aParam.exit);
    WriteParam(aMsg, aParam.clickCount);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool rv;
    uint8_t reason = 0, context = 0, exit = 0;
    rv = ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetMouseEventBase*>(aResult)) &&
         ReadParam(aMsg, aIter, &aResult->ignoreRootScrollFrame) &&
         ReadParam(aMsg, aIter, &reason) &&
         ReadParam(aMsg, aIter, &context) &&
         ReadParam(aMsg, aIter, &exit) &&
         ReadParam(aMsg, aIter, &aResult->clickCount);
    aResult->reason =
      static_cast<mozilla::WidgetMouseEvent::reasonType>(reason);
    aResult->context =
      static_cast<mozilla::WidgetMouseEvent::contextType>(context);
    aResult->exit = static_cast<mozilla::WidgetMouseEvent::exitType>(exit);
    return rv;
  }
};

template<>
struct ParamTraits<mozilla::WidgetPointerEvent>
{
  typedef mozilla::WidgetPointerEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetMouseEvent>(aParam));
    WriteParam(aMsg, aParam.pointerId);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
    WriteParam(aMsg, aParam.tiltX);
    WriteParam(aMsg, aParam.tiltY);
    WriteParam(aMsg, aParam.isPrimary);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool rv =
      ReadParam(aMsg, aIter, static_cast<mozilla::WidgetMouseEvent*>(aResult)) &&
      ReadParam(aMsg, aIter, &aResult->pointerId) &&
      ReadParam(aMsg, aIter, &aResult->width) &&
      ReadParam(aMsg, aIter, &aResult->height) &&
      ReadParam(aMsg, aIter, &aResult->tiltX) &&
      ReadParam(aMsg, aIter, &aResult->tiltY) &&
      ReadParam(aMsg, aIter, &aResult->isPrimary);
    return rv;
  }
};

template<>
struct ParamTraits<mozilla::WidgetTouchEvent>
{
  typedef mozilla::WidgetTouchEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const mozilla::WidgetInputEvent&>(aParam));
    // Sigh, Touch bites us again!  We want to be able to do
    //   WriteParam(aMsg, aParam.touches);
    const nsTArray< nsRefPtr<mozilla::dom::Touch> >& touches = aParam.touches;
    WriteParam(aMsg, touches.Length());
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      mozilla::dom::Touch* touch = touches[i];
      WriteParam(aMsg, touch->mIdentifier);
      WriteParam(aMsg, touch->mRefPoint);
      WriteParam(aMsg, touch->mRadius);
      WriteParam(aMsg, touch->mRotationAngle);
      WriteParam(aMsg, touch->mForce);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint32_t numTouches;
    if (!ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetInputEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &numTouches)) {
      return false;
    }
    for (uint32_t i = 0; i < numTouches; ++i) {
        int32_t identifier;
        mozilla::LayoutDeviceIntPoint refPoint;
        nsIntPoint radius;
        float rotationAngle;
        float force;
        if (!ReadParam(aMsg, aIter, &identifier) ||
            !ReadParam(aMsg, aIter, &refPoint) ||
            !ReadParam(aMsg, aIter, &radius) ||
            !ReadParam(aMsg, aIter, &rotationAngle) ||
            !ReadParam(aMsg, aIter, &force)) {
          return false;
        }
        aResult->touches.AppendElement(
          new mozilla::dom::Touch(
            identifier, mozilla::LayoutDeviceIntPoint::ToUntyped(refPoint),
            radius, rotationAngle, force));
    }
    return true;
  }
};

template<>
struct ParamTraits<mozilla::WidgetKeyboardEvent>
{
  typedef mozilla::WidgetKeyboardEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetInputEvent>(aParam));
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mKeyNameIndex));
    WriteParam(aMsg, aParam.mKeyValue);
    WriteParam(aMsg, aParam.keyCode);
    WriteParam(aMsg, aParam.charCode);
    WriteParam(aMsg, aParam.isChar);
    WriteParam(aMsg, aParam.mIsRepeat);
    WriteParam(aMsg, aParam.location);
    WriteParam(aMsg, aParam.mUniqueId);
    // An OS-specific native event might be attached in |mNativeKeyEvent|,  but
    // that cannot be copied across process boundaries.
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint32_t keyNameIndex = 0;
    if (ReadParam(aMsg, aIter,
                  static_cast<mozilla::WidgetInputEvent*>(aResult)) &&
        ReadParam(aMsg, aIter, &keyNameIndex) &&
        ReadParam(aMsg, aIter, &aResult->mKeyValue) &&
        ReadParam(aMsg, aIter, &aResult->keyCode) &&
        ReadParam(aMsg, aIter, &aResult->charCode) &&
        ReadParam(aMsg, aIter, &aResult->isChar) &&
        ReadParam(aMsg, aIter, &aResult->mIsRepeat) &&
        ReadParam(aMsg, aIter, &aResult->location) &&
        ReadParam(aMsg, aIter, &aResult->mUniqueId))
    {
      aResult->mKeyNameIndex = static_cast<mozilla::KeyNameIndex>(keyNameIndex);
      aResult->mNativeKeyEvent = nullptr;
      return true;
    }
    return false;
  }
};

template<>
struct ParamTraits<mozilla::TextRangeStyle>
{
  typedef mozilla::TextRangeStyle paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mDefinedStyles);
    WriteParam(aMsg, aParam.mLineStyle);
    WriteParam(aMsg, aParam.mIsBoldLine);
    WriteParam(aMsg, aParam.mForegroundColor);
    WriteParam(aMsg, aParam.mBackgroundColor);
    WriteParam(aMsg, aParam.mUnderlineColor);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mDefinedStyles) &&
           ReadParam(aMsg, aIter, &aResult->mLineStyle) &&
           ReadParam(aMsg, aIter, &aResult->mIsBoldLine) &&
           ReadParam(aMsg, aIter, &aResult->mForegroundColor) &&
           ReadParam(aMsg, aIter, &aResult->mBackgroundColor) &&
           ReadParam(aMsg, aIter, &aResult->mUnderlineColor);
  }
};

template<>
struct ParamTraits<mozilla::TextRange>
{
  typedef mozilla::TextRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mStartOffset);
    WriteParam(aMsg, aParam.mEndOffset);
    WriteParam(aMsg, aParam.mRangeType);
    WriteParam(aMsg, aParam.mRangeStyle);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mStartOffset) &&
           ReadParam(aMsg, aIter, &aResult->mEndOffset) &&
           ReadParam(aMsg, aIter, &aResult->mRangeType) &&
           ReadParam(aMsg, aIter, &aResult->mRangeStyle);
  }
};

template<>
struct ParamTraits<mozilla::WidgetTextEvent>
{
  typedef mozilla::WidgetTextEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mSeqno);
    WriteParam(aMsg, aParam.theText);
    WriteParam(aMsg, aParam.isChar);
    WriteParam(aMsg, aParam.rangeCount);
    for (uint32_t index = 0; index < aParam.rangeCount; index++)
      WriteParam(aMsg, aParam.rangeArray[index]);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter,
                   static_cast<mozilla::WidgetGUIEvent*>(aResult)) ||
        !ReadParam(aMsg, aIter, &aResult->mSeqno) ||
        !ReadParam(aMsg, aIter, &aResult->theText) ||
        !ReadParam(aMsg, aIter, &aResult->isChar) ||
        !ReadParam(aMsg, aIter, &aResult->rangeCount))
      return false;

    if (!aResult->rangeCount) {
      aResult->rangeArray = nullptr;
      return true;
    }

    aResult->rangeArray = new mozilla::TextRange[aResult->rangeCount];
    if (!aResult->rangeArray)
      return false;

    for (uint32_t index = 0; index < aResult->rangeCount; index++)
      if (!ReadParam(aMsg, aIter, &aResult->rangeArray[index])) {
        Free(*aResult);
        return false;
      }
    return true;
  }

  static void Free(const paramType& aResult)
  {
    if (aResult.rangeArray)
      delete [] aResult.rangeArray;
  }
};

template<>
struct ParamTraits<mozilla::WidgetCompositionEvent>
{
  typedef mozilla::WidgetCompositionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mSeqno);
    WriteParam(aMsg, aParam.data);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mSeqno) &&
           ReadParam(aMsg, aIter, &aResult->data);
  }
};

template<>
struct ParamTraits<mozilla::WidgetQueryContentEvent>
{
  typedef mozilla::WidgetQueryContentEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mSucceeded);
    WriteParam(aMsg, aParam.mInput.mOffset);
    WriteParam(aMsg, aParam.mInput.mLength);
    WriteParam(aMsg, aParam.mReply.mOffset);
    WriteParam(aMsg, aParam.mReply.mString);
    WriteParam(aMsg, aParam.mReply.mRect);
    WriteParam(aMsg, aParam.mReply.mReversed);
    WriteParam(aMsg, aParam.mReply.mHasSelection);
    WriteParam(aMsg, aParam.mReply.mWidgetIsHit);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    aResult->mWasAsync = true;
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mSucceeded) &&
           ReadParam(aMsg, aIter, &aResult->mInput.mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mInput.mLength) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mString) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mRect) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mHasSelection) &&
           ReadParam(aMsg, aIter, &aResult->mReply.mWidgetIsHit);
  }
};

template<>
struct ParamTraits<mozilla::WidgetSelectionEvent>
{
  typedef mozilla::WidgetSelectionEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.mSeqno);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mLength);
    WriteParam(aMsg, aParam.mReversed);
    WriteParam(aMsg, aParam.mExpandToClusterBoundary);
    WriteParam(aMsg, aParam.mSucceeded);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->mSeqno) &&
           ReadParam(aMsg, aIter, &aResult->mOffset) &&
           ReadParam(aMsg, aIter, &aResult->mLength) &&
           ReadParam(aMsg, aIter, &aResult->mReversed) &&
           ReadParam(aMsg, aIter, &aResult->mExpandToClusterBoundary) &&
           ReadParam(aMsg, aIter, &aResult->mSucceeded);
  }
};

template<>
struct ParamTraits<nsIMEUpdatePreference>
{
  typedef nsIMEUpdatePreference paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mWantUpdates);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mWantUpdates);
  }
};

template<>
struct ParamTraits<mozilla::WidgetPluginEvent>
{
  typedef mozilla::WidgetPluginEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<mozilla::WidgetGUIEvent>(aParam));
    WriteParam(aMsg, aParam.retargetToFocusedDocument);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter,
                     static_cast<mozilla::WidgetGUIEvent*>(aResult)) &&
           ReadParam(aMsg, aIter, &aResult->retargetToFocusedDocument);
  }
};

} // namespace IPC

#endif // nsGUIEventIPC_h__

