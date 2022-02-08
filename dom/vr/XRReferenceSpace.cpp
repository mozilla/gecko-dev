/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRReferenceSpace.h"
#include "mozilla/dom/XRRigidTransform.h"
#include "VRDisplayClient.h"

namespace mozilla {
namespace dom {

XRReferenceSpace::XRReferenceSpace(nsIGlobalObject* aParent,
                                   XRSession* aSession,
                                   XRNativeOrigin* aNativeOrigin,
                                   XRReferenceSpaceType aType)
    : XRSpace(aParent, aSession, aNativeOrigin), mType(aType) {}

already_AddRefed<XRReferenceSpace> XRReferenceSpace::GetOffsetReferenceSpace(
    const XRRigidTransform& aOffsetTransform) {
  RefPtr<XRReferenceSpace> offsetReferenceSpace =
      new XRReferenceSpace(GetParentObject(), mSession, mNativeOrigin, mType);

  // https://immersive-web.github.io/webxr/#dom-xrreferencespace-getoffsetreferencespace
  // Set offsetSpace’s origin offset to the result of multiplying base’s origin offset by
  // originOffset in the relevant realm of base.
  offsetReferenceSpace->mOriginOffset = aOffsetTransform.RawTransform() * mOriginOffset;

  return offsetReferenceSpace.forget();
}

JSObject* XRReferenceSpace::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return XRReferenceSpace_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
