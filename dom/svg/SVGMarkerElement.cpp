/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsError.h"
#include "mozilla/dom/SVGAngle.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGMarkerElement.h"
#include "mozilla/dom/SVGMarkerElementBinding.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/FloatingPoint.h"
#include "SVGContentUtils.h"

using namespace mozilla::gfx;
using namespace mozilla::dom::SVGMarkerElement_Binding;

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Marker)

namespace mozilla {
namespace dom {

using namespace SVGAngle_Binding;

JSObject* SVGMarkerElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGMarkerElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::LengthInfo SVGMarkerElement::sLengthInfo[4] = {
    {nsGkAtoms::refX, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::refY, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::markerWidth, 3, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::markerHeight, 3, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
};

nsSVGEnumMapping SVGMarkerElement::sUnitsMap[] = {
    {nsGkAtoms::strokeWidth, SVG_MARKERUNITS_STROKEWIDTH},
    {nsGkAtoms::userSpaceOnUse, SVG_MARKERUNITS_USERSPACEONUSE},
    {nullptr, 0}};

nsSVGElement::EnumInfo SVGMarkerElement::sEnumInfo[1] = {
    {nsGkAtoms::markerUnits, sUnitsMap, SVG_MARKERUNITS_STROKEWIDTH}};

nsSVGElement::AngleInfo SVGMarkerElement::sAngleInfo[1] = {
    {nsGkAtoms::orient, 0, SVG_ANGLETYPE_UNSPECIFIED}};

//----------------------------------------------------------------------
// Implementation

nsresult nsSVGOrientType::SetBaseValue(uint16_t aValue,
                                       nsSVGElement* aSVGElement) {
  if (aValue == SVG_MARKER_ORIENT_AUTO || aValue == SVG_MARKER_ORIENT_ANGLE ||
      aValue == SVG_MARKER_ORIENT_AUTO_START_REVERSE) {
    SetBaseValue(aValue);
    aSVGElement->SetAttr(kNameSpaceID_None, nsGkAtoms::orient, nullptr,
                         (aValue == SVG_MARKER_ORIENT_AUTO
                              ? NS_LITERAL_STRING("auto")
                              : aValue == SVG_MARKER_ORIENT_ANGLE
                                    ? NS_LITERAL_STRING("0")
                                    : NS_LITERAL_STRING("auto-start-reverse")),
                         true);
    return NS_OK;
  }
  return NS_ERROR_DOM_TYPE_ERR;
}

already_AddRefed<SVGAnimatedEnumeration> nsSVGOrientType::ToDOMAnimatedEnum(
    nsSVGElement* aSVGElement) {
  RefPtr<SVGAnimatedEnumeration> toReturn =
      new DOMAnimatedEnum(this, aSVGElement);
  return toReturn.forget();
}

SVGMarkerElement::SVGMarkerElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGMarkerElementBase(std::move(aNodeInfo)), mCoordCtx(nullptr) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMarkerElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedRect> SVGMarkerElement::ViewBox() {
  return mViewBox.ToSVGAnimatedRect(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGMarkerElement::PreserveAspectRatio() {
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength> SVGMarkerElement::RefX() {
  return mLengthAttributes[REFX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength> SVGMarkerElement::RefY() {
  return mLengthAttributes[REFY].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedEnumeration> SVGMarkerElement::MarkerUnits() {
  return mEnumAttributes[MARKERUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedLength> SVGMarkerElement::MarkerWidth() {
  return mLengthAttributes[MARKERWIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength> SVGMarkerElement::MarkerHeight() {
  return mLengthAttributes[MARKERHEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedEnumeration> SVGMarkerElement::OrientType() {
  return mOrientType.ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedAngle> SVGMarkerElement::OrientAngle() {
  return mAngleAttributes[ORIENT].ToDOMAnimatedAngle(this);
}

void SVGMarkerElement::SetOrientToAuto() {
  SetAttr(kNameSpaceID_None, nsGkAtoms::orient, nullptr,
          NS_LITERAL_STRING("auto"), true);
}

void SVGMarkerElement::SetOrientToAngle(SVGAngle& angle, ErrorResult& rv) {
  float f = angle.Value();
  if (!IsFinite(f)) {
    rv.Throw(NS_ERROR_DOM_SVG_WRONG_TYPE_ERR);
    return;
  }
  mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
  mAngleAttributes[ORIENT].SetBaseValue(f, angle.UnitType(), this, true);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGMarkerElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sFEFloodMap,
                                                    sFiltersMap,
                                                    sFontSpecificationMap,
                                                    sGradientStopMap,
                                                    sLightingEffectsMap,
                                                    sMarkersMap,
                                                    sTextContentElementsMap,
                                                    sViewportsMap,
                                                    sColorMap,
                                                    sFillStrokeMap,
                                                    sGraphicsMap};

  return FindAttributeDependence(name, map) ||
         SVGMarkerElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

bool SVGMarkerElement::ParseAttribute(int32_t aNameSpaceID, nsAtom* aName,
                                      const nsAString& aValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      nsAttrValue& aResult) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::orient) {
    if (aValue.EqualsLiteral("auto")) {
      mOrientType.SetBaseValue(SVG_MARKER_ORIENT_AUTO);
      aResult.SetTo(aValue);
      mAngleAttributes[ORIENT].SetBaseValue(0.f, SVG_ANGLETYPE_UNSPECIFIED,
                                            this, false);
      return true;
    }
    if (aValue.EqualsLiteral("auto-start-reverse")) {
      mOrientType.SetBaseValue(SVG_MARKER_ORIENT_AUTO_START_REVERSE);
      aResult.SetTo(aValue);
      mAngleAttributes[ORIENT].SetBaseValue(0.f, SVG_ANGLETYPE_UNSPECIFIED,
                                            this, false);
      return true;
    }
    mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
  }
  return SVGMarkerElementBase::ParseAttribute(aNameSpaceID, aName, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

nsresult SVGMarkerElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                        const nsAttrValue* aValue,
                                        const nsAttrValue* aOldValue,
                                        nsIPrincipal* aMaybeScriptedPrincipal,
                                        bool aNotify) {
  if (!aValue && aNamespaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::orient) {
    mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
  }

  return SVGMarkerElementBase::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void SVGMarkerElement::SetParentCoordCtxProvider(SVGViewportElement* aContext) {
  mCoordCtx = aContext;
  mViewBoxToViewportTransform = nullptr;
}

/* virtual */ bool SVGMarkerElement::HasValidDimensions() const {
  return (!mLengthAttributes[MARKERWIDTH].IsExplicitlySet() ||
          mLengthAttributes[MARKERWIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[MARKERHEIGHT].IsExplicitlySet() ||
          mLengthAttributes[MARKERHEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

nsSVGElement::LengthAttributesInfo SVGMarkerElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::AngleAttributesInfo SVGMarkerElement::GetAngleInfo() {
  return AngleAttributesInfo(mAngleAttributes, sAngleInfo,
                             ArrayLength(sAngleInfo));
}

nsSVGElement::EnumAttributesInfo SVGMarkerElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

nsSVGViewBox* SVGMarkerElement::GetViewBox() { return &mViewBox; }

SVGAnimatedPreserveAspectRatio* SVGMarkerElement::GetPreserveAspectRatio() {
  return &mPreserveAspectRatio;
}

//----------------------------------------------------------------------
// public helpers

gfx::Matrix SVGMarkerElement::GetMarkerTransform(float aStrokeWidth,
                                                 const nsSVGMark& aMark) {
  float scale =
      mEnumAttributes[MARKERUNITS].GetAnimValue() == SVG_MARKERUNITS_STROKEWIDTH
          ? aStrokeWidth
          : 1.0f;

  float angle;
  switch (mOrientType.GetAnimValueInternal()) {
    case SVG_MARKER_ORIENT_AUTO:
      angle = aMark.angle;
      break;
    case SVG_MARKER_ORIENT_AUTO_START_REVERSE:
      angle = aMark.angle + (aMark.type == nsSVGMark::eStart ? M_PI : 0.0f);
      break;
    default:  // SVG_MARKER_ORIENT_ANGLE
      angle = mAngleAttributes[ORIENT].GetAnimValue() * M_PI / 180.0f;
      break;
  }

  return gfx::Matrix(cos(angle) * scale, sin(angle) * scale,
                     -sin(angle) * scale, cos(angle) * scale, aMark.x, aMark.y);
}

nsSVGViewBoxRect SVGMarkerElement::GetViewBoxRect() {
  if (mViewBox.HasRect()) {
    return mViewBox.GetAnimValue();
  }
  return nsSVGViewBoxRect(
      0, 0, mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx),
      mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx));
}

gfx::Matrix SVGMarkerElement::GetViewBoxTransform() {
  if (!mViewBoxToViewportTransform) {
    float viewportWidth =
        mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx);
    float viewportHeight =
        mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx);

    nsSVGViewBoxRect viewbox = GetViewBoxRect();

    MOZ_ASSERT(viewbox.width > 0.0f && viewbox.height > 0.0f,
               "Rendering should be disabled");

    gfx::Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
        viewportWidth, viewportHeight, viewbox.x, viewbox.y, viewbox.width,
        viewbox.height, mPreserveAspectRatio);

    float refX = mLengthAttributes[REFX].GetAnimValue(mCoordCtx);
    float refY = mLengthAttributes[REFY].GetAnimValue(mCoordCtx);

    gfx::Point ref = viewBoxTM.TransformPoint(gfx::Point(refX, refY));

    Matrix TM = viewBoxTM;
    TM.PostTranslate(-ref.x, -ref.y);

    mViewBoxToViewportTransform = new gfx::Matrix(TM);
  }

  return *mViewBoxToViewportTransform;
}

}  // namespace dom
}  // namespace mozilla
