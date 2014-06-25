/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTSpanElement_h
#define mozilla_dom_SVGTSpanElement_h

#include "mozilla/dom/SVGTextPositioningElement.h"

nsresult NS_NewSVGTSpanElement(nsIContent **aResult,
                               already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTextPositioningElement SVGTSpanElementBase;

class SVGTSpanElement MOZ_FINAL : public SVGTSpanElementBase
{
protected:
  friend nsresult (::NS_NewSVGTSpanElement(nsIContent **aResult,
                                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  SVGTSpanElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;

public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

protected:
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;

  nsSVGEnum mEnumAttributes[1];
  virtual nsSVGEnum* EnumAttributes() MOZ_OVERRIDE
    { return mEnumAttributes; }

  nsSVGLength2 mLengthAttributes[1];
  virtual nsSVGLength2* LengthAttributes() MOZ_OVERRIDE
    { return mLengthAttributes; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTSpanElement_h
