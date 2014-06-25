/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMetadataElement_h
#define mozilla_dom_SVGMetadataElement_h

#include "mozilla/Attributes.h"
#include "nsSVGElement.h"

nsresult NS_NewSVGMetadataElement(nsIContent **aResult,
                                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGElement SVGMetadataElementBase;

namespace mozilla {
namespace dom {

class SVGMetadataElement MOZ_FINAL : public SVGMetadataElementBase
{
protected:
  friend nsresult (::NS_NewSVGMetadataElement(nsIContent **aResult,
                                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  SVGMetadataElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
  nsresult Init();

public:
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMetadataElement_h
