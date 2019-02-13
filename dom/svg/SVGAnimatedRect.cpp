/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedRect.h"
#include "mozilla/dom/SVGAnimatedRectBinding.h"
#include "nsSVGElement.h"
#include "nsSVGViewBox.h"
#include "SVGIRect.h"

namespace mozilla {
namespace dom {

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedRect, mSVGElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SVGAnimatedRect, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SVGAnimatedRect, Release)

SVGAnimatedRect::SVGAnimatedRect(nsSVGViewBox* aVal, nsSVGElement* aSVGElement)
  : mVal(aVal)
  , mSVGElement(aSVGElement)
{
}

SVGAnimatedRect::~SVGAnimatedRect()
{
  nsSVGViewBox::sSVGAnimatedRectTearoffTable.RemoveTearoff(mVal);
}

already_AddRefed<SVGIRect>
SVGAnimatedRect::GetBaseVal()
{
  return mVal->ToDOMBaseVal(mSVGElement);
}

already_AddRefed<SVGIRect>
SVGAnimatedRect::GetAnimVal()
{
  return mVal->ToDOMAnimVal(mSVGElement);
}

JSObject*
SVGAnimatedRect::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGAnimatedRectBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
