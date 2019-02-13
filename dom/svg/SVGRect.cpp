/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGRect.h"
#include "nsSVGElement.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// implementation:

SVGRect::SVGRect(nsIContent* aParent, float x, float y, float w, float h)
  : SVGIRect(), mParent(aParent), mX(x), mY(y), mWidth(w), mHeight(h)
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SVGRect, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGRect)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGRect)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGRect)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

} // namespace dom
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

already_AddRefed<mozilla::dom::SVGRect>
NS_NewSVGRect(nsIContent* aParent, float aX, float aY, float aWidth,
              float aHeight)
{
  nsRefPtr<mozilla::dom::SVGRect> rect =
    new mozilla::dom::SVGRect(aParent, aX, aY, aWidth, aHeight);

  return rect.forget();
}

already_AddRefed<mozilla::dom::SVGRect>
NS_NewSVGRect(nsIContent* aParent, const Rect& aRect)
{
  return NS_NewSVGRect(aParent, aRect.x, aRect.y,
                       aRect.width, aRect.height);
}

