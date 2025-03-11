/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SanitizerTypes.h"

namespace mozilla::dom::sanitizer {

SanitizerAttributeNamespace CanonicalName::ToSanitizerAttributeNamespace()
    const {
  SanitizerAttributeNamespace result;
  mLocalName->ToString(result.mName);
  if (mNamespace) {
    mNamespace->ToString(result.mNamespace);
  } else {
    result.mNamespace.SetIsVoid(true);
  }
  return result;
}

SanitizerElementNamespaceWithAttributes
CanonicalElementWithAttributes::ToSanitizerElementNamespaceWithAttributes()
    const {
  SanitizerElementNamespaceWithAttributes result;
  mLocalName->ToString(result.mName);
  if (mNamespace) {
    mNamespace->ToString(result.mNamespace);
  } else {
    MOZ_ASSERT(false, "An element namespace should never be null");
  }
  if (mAttributes) {
    result.mAttributes.Construct(ToSanitizerAttributes(*mAttributes));
  }
  if (mRemoveAttributes) {
    result.mRemoveAttributes.Construct(
        ToSanitizerAttributes(*mRemoveAttributes));
  }
  return result;
}

SanitizerElementNamespace CanonicalName::ToSanitizerElementNamespace() const {
  SanitizerElementNamespace result;
  mLocalName->ToString(result.mName);
  if (mNamespace) {
    mNamespace->ToString(result.mNamespace);
  } else {
    MOZ_ASSERT(false, "An element namespace should never be null");
  }
  return result;
}

CanonicalElementWithAttributes CanonicalElementWithAttributes::Clone() const {
  CanonicalElementWithAttributes elem(CanonicalName::Clone());

  if (mAttributes) {
    ListSet<CanonicalName> attributes;
    for (const auto& attr : mAttributes->Values()) {
      attributes.InsertNew(attr.Clone());
    }
    elem.mAttributes = Some(std::move(attributes));
  }

  if (mRemoveAttributes) {
    ListSet<CanonicalName> attributes;
    for (const auto& attr : mRemoveAttributes->Values()) {
      attributes.InsertNew(attr.Clone());
    }
    elem.mRemoveAttributes = Some(std::move(attributes));
  }

  return elem;
}

nsTArray<OwningStringOrSanitizerAttributeNamespace> ToSanitizerAttributes(
    const ListSet<CanonicalName>& aList) {
  nsTArray<OwningStringOrSanitizerAttributeNamespace> attributes;
  for (const CanonicalName& canonical : aList.Values()) {
    attributes.AppendElement()->SetAsSanitizerAttributeNamespace() =
        canonical.ToSanitizerAttributeNamespace();
  }
  return attributes;
}

}  // namespace mozilla::dom::sanitizer
