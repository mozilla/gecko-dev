/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRSpace.h"
#include "mozilla/dom/XRRigidTransform.h"
#include "VRDisplayClient.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XRSpace)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XRSpace, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSession)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XRSpace, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSession)
  // Don't need NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XRSpace, DOMEventTargetHelper)

XRSpace::XRSpace(nsIGlobalObject* aParent, XRSession* aSession,
                 XRNativeOrigin* aNativeOrigin)
    : DOMEventTargetHelper(aParent),
      mSession(aSession),
      mNativeOrigin(aNativeOrigin) {}

JSObject* XRSpace::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return XRSpace_Binding::Wrap(aCx, this, aGivenProto);
}

XRSession* XRSpace::GetSession() const { return mSession; }

gfx::Matrix4x4Double XRSpace::GetNativeOriginTransform() const {
  if (!mNativeOrigin) {
    return {};
  }
  gfx::Matrix4x4Double transform;
  transform.SetRotationFromQuaternion(mNativeOrigin->GetOrientation());
  transform.PostTranslate(mNativeOrigin->GetPosition());
  return transform;
}

gfx::Matrix4x4Double XRSpace::GetEffectiveOriginTransform() const {
  return GetNativeOriginTransform() * mOriginOffset;
}

bool XRSpace::IsPositionEmulated() const {
  gfx::VRDisplayClient* display = mSession->GetDisplayClient();
  if (!display) {
    // When there are no sensors, the position is considered emulated.
    return true;
  }
  const gfx::VRDisplayInfo& displayInfo = display->GetDisplayInfo();
  if (displayInfo.GetCapabilities() &
      gfx::VRDisplayCapabilityFlags::Cap_PositionEmulated) {
    // Cap_PositionEmulated indicates that the position is always emulated.
    return true;
  }
  const gfx::VRHMDSensorState& sensorState = display->GetSensorState();
  // When positional tracking is lost, the position is considered emulated.
  return ((sensorState.flags & gfx::VRDisplayCapabilityFlags::Cap_Position) ==
          gfx::VRDisplayCapabilityFlags::Cap_None);
}

}  // namespace dom
}  // namespace mozilla
