/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTextPathElement_h
#define mozilla_dom_SVGTextPathElement_h

#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"
#include "mozilla/dom/SVGTextContentElement.h"

class nsIAtom;
class nsIContent;

nsresult NS_NewSVGTextPathElement(nsIContent **aResult,
                                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

// textPath Method Types
static const unsigned short TEXTPATH_METHODTYPE_UNKNOWN  = 0;
static const unsigned short TEXTPATH_METHODTYPE_ALIGN    = 1;
static const unsigned short TEXTPATH_METHODTYPE_STRETCH  = 2;
// textPath Spacing Types
static const unsigned short TEXTPATH_SPACINGTYPE_UNKNOWN = 0;
static const unsigned short TEXTPATH_SPACINGTYPE_AUTO    = 1;
static const unsigned short TEXTPATH_SPACINGTYPE_EXACT   = 2;

typedef SVGTextContentElement SVGTextPathElementBase;

class SVGTextPathElement MOZ_FINAL : public SVGTextPathElementBase
{
friend class ::SVGTextFrame;

protected:
  friend nsresult (::NS_NewSVGTextPathElement(nsIContent **aResult,
                                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  SVGTextPathElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> StartOffset();
  already_AddRefed<SVGAnimatedEnumeration> Method();
  already_AddRefed<SVGAnimatedEnumeration> Spacing();
  already_AddRefed<SVGAnimatedString> Href();

 protected:

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { /* TEXTLENGTH, */ STARTOFFSET = 1 };
  nsSVGLength2 mLengthAttributes[2];
  virtual nsSVGLength2* LengthAttributes() MOZ_OVERRIDE
    { return mLengthAttributes; }
  static LengthInfo sLengthInfo[2];

  enum { /* LENGTHADJUST, */ METHOD = 1, SPACING };
  nsSVGEnum mEnumAttributes[3];
  virtual nsSVGEnum* EnumAttributes() MOZ_OVERRIDE
    { return mEnumAttributes; }
  static nsSVGEnumMapping sMethodMap[];
  static nsSVGEnumMapping sSpacingMap[];
  static EnumInfo sEnumInfo[3];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTextPathElement_h
