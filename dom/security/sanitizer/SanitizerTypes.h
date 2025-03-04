/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SanitizerTypes_h
#define mozilla_dom_SanitizerTypes_h

#include "mozilla/dom/SanitizerBinding.h"
#include "mozilla/Maybe.h"

namespace mozilla::dom::sanitizer {

// The name of an element/attribute combined with its namespace.
class CanonicalName {
 public:
  CanonicalName(CanonicalName&&) = default;
  CanonicalName(RefPtr<nsAtom> aLocalName, RefPtr<nsAtom> aNamespace)
      : mLocalName(std::move(aLocalName)), mNamespace(std::move(aNamespace)) {}
  ~CanonicalName() = default;

  bool operator==(const CanonicalName& aOther) const {
    return mLocalName == aOther.mLocalName && mNamespace == aOther.mNamespace;
  }

  SanitizerElementNamespace ToSanitizerElementNamespace() const;
  SanitizerAttributeNamespace ToSanitizerAttributeNamespace() const;

 protected:
  RefPtr<nsAtom> mLocalName;
  // A "null" namespace is represented by the nullptr.
  RefPtr<nsAtom> mNamespace;
};

// TODO: Replace this with some kind of optimized ordered set.
template <typename ValueType>
class ListSet {
 public:
  void Insert(ValueType&& aValue) {
    if (Contains(aValue)) {
      return;
    }

    mValues.AppendElement(std::move(aValue));
  }
  void Remove(const CanonicalName& aValue) { mValues.RemoveElement(aValue); }
  bool Contains(const CanonicalName& aValue) const {
    return mValues.Contains(aValue);
  }
  bool IsEmpty() const { return mValues.IsEmpty(); }

  const nsTArray<ValueType>& Values() const { return mValues; }

 private:
  nsTArray<ValueType> mValues;
};

class CanonicalElementWithAttributes : public CanonicalName {
 public:
  explicit CanonicalElementWithAttributes(CanonicalName&& aName)
      : CanonicalName(std::move(aName)) {}

  SanitizerElementNamespaceWithAttributes
  ToSanitizerElementNamespaceWithAttributes() const;

  Maybe<ListSet<CanonicalName>> mAttributes;
  Maybe<ListSet<CanonicalName>> mRemoveAttributes;
};

nsTArray<OwningStringOrSanitizerAttributeNamespace> ToSanitizerAttributes(
    const ListSet<CanonicalName>& aList);

}  // namespace mozilla::dom::sanitizer

#endif
