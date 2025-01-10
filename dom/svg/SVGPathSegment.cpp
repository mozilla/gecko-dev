/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGPathSegment.h"

#include "mozilla/dom/SVGPathElementBinding.h"
#include "SVGPathSegUtils.h"

namespace mozilla::dom {

JSObject* SVGPathSegment::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGPathSegment_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SVGPathSegment, mSVGPathElement)

//----------------------------------------------------------------------
// Implementation

SVGPathSegment::SVGPathSegment(SVGPathElement* aSVGPathElement,
                               const StylePathCommand& aCommand)
    : mSVGPathElement(aSVGPathElement) {
  switch (aCommand.tag) {
    case StylePathCommand::Tag::Close:
      mCommand.AssignLiteral("Z");
      break;
    case StylePathCommand::Tag::Move:
      mCommand.AssignLiteral(aCommand.move.by_to == StyleByTo::To ? "M" : "m");
      mValues.AppendElement(aCommand.move.point.x);
      mValues.AppendElement(aCommand.move.point.y);
      break;
    case StylePathCommand::Tag::Line:
      mCommand.AssignLiteral(aCommand.line.by_to == StyleByTo::To ? "L" : "l");
      mValues.AppendElement(aCommand.line.point.x);
      mValues.AppendElement(aCommand.line.point.y);
      break;
    case StylePathCommand::Tag::CubicCurve:
      mCommand.AssignLiteral(aCommand.cubic_curve.by_to == StyleByTo::To ? "C"
                                                                         : "c");
      mValues.AppendElement(aCommand.cubic_curve.control1.x);
      mValues.AppendElement(aCommand.cubic_curve.control1.y);
      mValues.AppendElement(aCommand.cubic_curve.control2.x);
      mValues.AppendElement(aCommand.cubic_curve.control2.y);
      mValues.AppendElement(aCommand.cubic_curve.point.x);
      mValues.AppendElement(aCommand.cubic_curve.point.y);
      break;
    case StylePathCommand::Tag::QuadCurve:
      mCommand.AssignLiteral(aCommand.quad_curve.by_to == StyleByTo::To ? "Q"
                                                                        : "q");
      mValues.AppendElement(aCommand.quad_curve.control1.x);
      mValues.AppendElement(aCommand.quad_curve.control1.y);
      mValues.AppendElement(aCommand.quad_curve.point.x);
      mValues.AppendElement(aCommand.quad_curve.point.y);
      break;
    case StylePathCommand::Tag::Arc:
      mCommand.AssignLiteral(aCommand.arc.by_to == StyleByTo::To ? "A" : "a");
      mValues.AppendElement(aCommand.arc.radii.x);
      mValues.AppendElement(aCommand.arc.radii.y);
      mValues.AppendElement(aCommand.arc.rotate);
      mValues.AppendElement(aCommand.arc.arc_size == StyleArcSize::Large);
      mValues.AppendElement(aCommand.arc.arc_sweep == StyleArcSweep::Cw);
      mValues.AppendElement(aCommand.arc.point.x);
      mValues.AppendElement(aCommand.arc.point.y);
      break;
    case StylePathCommand::Tag::HLine:
      mCommand.AssignLiteral(aCommand.h_line.by_to == StyleByTo::To ? "H"
                                                                    : "h");
      mValues.AppendElement(aCommand.h_line.x);
      break;
    case StylePathCommand::Tag::VLine:
      mCommand.AssignLiteral(aCommand.v_line.by_to == StyleByTo::To ? "V"
                                                                    : "v");
      mValues.AppendElement(aCommand.v_line.y);
      break;
    case StylePathCommand::Tag::SmoothCubic:
      mCommand.AssignLiteral(
          aCommand.smooth_cubic.by_to == StyleByTo::To ? "S" : "s");
      mValues.AppendElement(aCommand.smooth_cubic.control2.x);
      mValues.AppendElement(aCommand.smooth_cubic.control2.y);
      mValues.AppendElement(aCommand.smooth_cubic.point.x);
      mValues.AppendElement(aCommand.smooth_cubic.point.y);
      break;
    case StylePathCommand::Tag::SmoothQuad:
      mCommand.AssignLiteral(aCommand.smooth_quad.by_to == StyleByTo::To ? "T"
                                                                         : "t");
      mValues.AppendElement(aCommand.smooth_quad.point.x);
      mValues.AppendElement(aCommand.smooth_quad.point.y);
      break;
  }
}

static int32_t ArgCountForType(char aType) {
  switch (ToLowerCase(aType)) {
    case 'z':
      return 0;
    case 'm':
    case 'l':
      return 2;
    case 'c':
      return 6;
    case 'q':
      return 4;
    case 'a':
      return 7;
    case 'h':
    case 'v':
      return 1;
    case 's':
      return 4;
    case 't':
      return 2;
  }
  return -1;
}

bool SVGPathSegment::IsMove() const {
  return mCommand.EqualsLiteral("M") || mCommand.EqualsLiteral("m");
}

bool SVGPathSegment::IsArc() const {
  return mCommand.EqualsLiteral("A") || mCommand.EqualsLiteral("a");
}

bool IsValidFlag(float aFlag) { return aFlag == 0.0f || aFlag == 1.0f; }

bool SVGPathSegment::IsValid() const {
  if (mCommand.Length() != 1) {
    return false;
  }
  auto expectedArgCount = ArgCountForType(mCommand.First());
  if (expectedArgCount < 0 || mValues.Length() != uint32_t(expectedArgCount)) {
    return false;
  }
  if (IsArc() && !(IsValidFlag(mValues[3]) && IsValidFlag(mValues[4]))) {
    return false;
  }
  return true;
}

StylePathCommand SVGPathSegment::ToStylePathCommand() const {
  MOZ_ASSERT(IsValid(), "Trying to convert invalid SVGPathSegment");
  switch (mCommand.First()) {
    case 'M':
      return StylePathCommand::Move(StyleByTo::To, {mValues[0], mValues[1]});
    case 'm':
      return StylePathCommand::Move(StyleByTo::By, {mValues[0], mValues[1]});
    case 'L':
      return StylePathCommand::Line(StyleByTo::To, {mValues[0], mValues[1]});
    case 'l':
      return StylePathCommand::Line(StyleByTo::By, {mValues[0], mValues[1]});
    case 'C':
      return StylePathCommand::CubicCurve(
          StyleByTo::To, {mValues[4], mValues[5]}, {mValues[0], mValues[1]},
          {mValues[2], mValues[3]});
    case 'c':
      return StylePathCommand::CubicCurve(
          StyleByTo::By, {mValues[4], mValues[5]}, {mValues[0], mValues[1]},
          {mValues[2], mValues[3]});
    case 'Q':
      return StylePathCommand::QuadCurve(
          StyleByTo::To, {mValues[2], mValues[3]}, {mValues[0], mValues[1]});
    case 'q':
      return StylePathCommand::QuadCurve(
          StyleByTo::By, {mValues[2], mValues[3]}, {mValues[0], mValues[1]});
    case 'A':
      return StylePathCommand::Arc(
          StyleByTo::To, {mValues[5], mValues[6]}, {mValues[0], mValues[1]},
          mValues[3] ? StyleArcSweep::Cw : StyleArcSweep::Ccw,
          mValues[4] ? StyleArcSize::Large : StyleArcSize::Small, mValues[2]);
    case 'a':
      return StylePathCommand::Arc(
          StyleByTo::By, {mValues[5], mValues[6]}, {mValues[0], mValues[1]},
          mValues[3] ? StyleArcSweep::Cw : StyleArcSweep::Ccw,
          mValues[4] ? StyleArcSize::Large : StyleArcSize::Small, mValues[2]);
    case 'H':
      return StylePathCommand::HLine(StyleByTo::To, mValues[0]);
    case 'h':
      return StylePathCommand::HLine(StyleByTo::By, mValues[0]);
    case 'V':
      return StylePathCommand::VLine(StyleByTo::To, mValues[0]);
    case 'v':
      return StylePathCommand::VLine(StyleByTo::By, mValues[0]);
    case 'S':
      return StylePathCommand::SmoothCubic(
          StyleByTo::To, {mValues[2], mValues[3]}, {mValues[0], mValues[1]});
    case 's':
      return StylePathCommand::SmoothCubic(
          StyleByTo::By, {mValues[2], mValues[3]}, {mValues[0], mValues[1]});
    case 'T':
      return StylePathCommand::SmoothQuad(StyleByTo::To,
                                          {mValues[0], mValues[1]});
    case 't':
      return StylePathCommand::SmoothQuad(StyleByTo::By,
                                          {mValues[0], mValues[1]});
  }
  return StylePathCommand::Close();
}

void SVGPathSegment::GetType(DOMString& aType) {
  aType.SetKnownLiveString(mCommand);
}

void SVGPathSegment::SetType(const nsAString& aType) { mCommand = aType; }

void SVGPathSegment::GetValues(nsTArray<float>& aValues) {
  aValues = mValues.Clone();
}

void SVGPathSegment::SetValues(const nsTArray<float>& aValues) {
  mValues = aValues.Clone();
}

}  // namespace mozilla::dom
