/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGLineElement.h"
#include "mozilla/dom/SVGLineElementBinding.h"
#include "mozilla/gfx/2D.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Line)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGLineElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGLineElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::LengthInfo SVGLineElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::x2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// Implementation

SVGLineElement::SVGLineElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGLineElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGLineElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGLineElement::X1()
{
  return mLengthAttributes[ATTR_X1].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLineElement::Y1()
{
  return mLengthAttributes[ATTR_Y1].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLineElement::X2()
{
  return mLengthAttributes[ATTR_X2].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLineElement::Y2()
{
  return mLengthAttributes[ATTR_Y2].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGLineElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };

  return FindAttributeDependence(name, map) ||
    SVGLineElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
SVGLineElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
SVGLineElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks) {
  float x1, y1, x2, y2;

  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  float angle = atan2(y2 - y1, x2 - x1);

  aMarks->AppendElement(nsSVGMark(x1, y1, angle, nsSVGMark::eStart));
  aMarks->AppendElement(nsSVGMark(x2, y2, angle, nsSVGMark::eEnd));
}

void
SVGLineElement::GetAsSimplePath(SimplePath* aSimplePath)
{
  float x1, y1, x2, y2;
  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);
  aSimplePath->SetLine(x1, y1, x2, y2);
}

TemporaryRef<Path>
SVGLineElement::BuildPath(PathBuilder* aBuilder)
{
  float x1, y1, x2, y2;
  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  aBuilder->MoveTo(Point(x1, y1));
  aBuilder->LineTo(Point(x2, y2));

  return aBuilder->Finish();
}

bool
SVGLineElement::GetGeometryBounds(
  Rect* aBounds, const StrokeOptions& aStrokeOptions, const Matrix& aTransform)
{
  float x1, y1, x2, y2;
  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  if (aStrokeOptions.mLineWidth <= 0) {
    *aBounds = Rect(aTransform * Point(x1, y1), Size());
    aBounds->ExpandToEnclose(aTransform * Point(x2, y2));
    return true;
  }

  if (aStrokeOptions.mLineCap == CapStyle::ROUND) {
    if (!aTransform.IsRectilinear()) {
      // TODO: handle this case.
      return false;
    }
    Rect bounds(Point(x1, y1), Size());
    bounds.ExpandToEnclose(Point(x2, y2));
    bounds.Inflate(aStrokeOptions.mLineWidth / 2.f);
    *aBounds = aTransform.TransformBounds(bounds);
    return true;
  }

  Float length = Float(NS_hypot(x2 - x1, y2 - y1));
  Float xDelta;
  Float yDelta;

  if (aStrokeOptions.mLineCap == CapStyle::BUTT) {
    if (length == 0.f) {
      xDelta = yDelta = 0.f;
    } else {
      Float ratio = aStrokeOptions.mLineWidth / 2.f / length;
      xDelta = ratio * (y2 - y1);
      yDelta = ratio * (x2 - x1);
    }
  } else {
    MOZ_ASSERT(aStrokeOptions.mLineCap == CapStyle::SQUARE);
    if (length == 0.f) {
      xDelta = yDelta = aStrokeOptions.mLineWidth / 2.f;
    } else {
      Float ratio = aStrokeOptions.mLineWidth / 2.f / length;
      xDelta = yDelta = ratio * (fabs(y2 - y1) + fabs(x2 - x1));
    }
  }

  Point points[4];

  points[0] = Point(x1 - xDelta, y1 - yDelta);
  points[1] = Point(x1 + xDelta, y1 + yDelta);
  points[2] = Point(x2 + xDelta, y2 + yDelta);
  points[3] = Point(x2 - xDelta, y2 - yDelta);

  *aBounds = Rect(aTransform * points[0], Size());
  for (uint32_t i = 1; i < 4; ++i) {
    aBounds->ExpandToEnclose(aTransform * points[i]);
  }
  return true;
}

} // namespace dom
} // namespace mozilla
