/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_PERSISTENCESCOPE_H_
#define DOM_QUOTA_PERSISTENCESCOPE_H_

#include "mozilla/Assertions.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/quota/PersistenceType.h"

namespace mozilla::dom::quota {

class PersistenceScope {
  class Value {
    PersistenceType mValue;

   public:
    explicit Value(PersistenceType aValue) : mValue(aValue) {}

    PersistenceType GetValue() const { return mValue; }
  };

  class Set {
    EnumSet<PersistenceType> mSet;

   public:
    explicit Set(const EnumSet<PersistenceType>& aSet) : mSet(aSet) {}

    const EnumSet<PersistenceType>& GetSet() const { return mSet; }
  };

  struct Null {};

  using DataType = Variant<Value, Set, Null>;

  DataType mData;

 public:
  PersistenceScope() : mData(Null()) {}

  // XXX Consider renaming these static methods to Create
  static PersistenceScope CreateFromValue(PersistenceType aValue) {
    return PersistenceScope(std::move(Value(aValue)));
  }

  template <typename... Args>
  static PersistenceScope CreateFromSet(Args... aArgs) {
    return PersistenceScope(std::move(Set(EnumSet<PersistenceType>(aArgs...))));
  }

  static PersistenceScope CreateFromNull() {
    return PersistenceScope(std::move(Null()));
  }

  bool IsValue() const { return mData.is<Value>(); }

  bool IsSet() const { return mData.is<Set>(); }

  bool IsNull() const { return mData.is<Null>(); }

  void SetFromValue(PersistenceType aValue) {
    mData = AsVariant(Value(aValue));
  }

  void SetFromNull() { mData = AsVariant(Null()); }

  PersistenceType GetValue() const {
    MOZ_ASSERT(IsValue());

    return mData.as<Value>().GetValue();
  }

  const EnumSet<PersistenceType>& GetSet() const {
    MOZ_ASSERT(IsSet());

    return mData.as<Set>().GetSet();
  }

  bool Matches(const PersistenceScope& aOther) const {
    struct Matcher {
      const PersistenceScope& mThis;

      explicit Matcher(const PersistenceScope& aThis) : mThis(aThis) {}

      bool operator()(const Value& aOther) {
        return mThis.MatchesValue(aOther);
      }

      bool operator()(const Set& aOther) { return mThis.MatchesSet(aOther); }

      bool operator()(const Null& aOther) { return true; }
    };

    return aOther.mData.match(Matcher(*this));
  }

 private:
  // Move constructors
  explicit PersistenceScope(const Value&& aValue) : mData(aValue) {}

  explicit PersistenceScope(const Set&& aSet) : mData(aSet) {}

  explicit PersistenceScope(const Null&& aNull) : mData(aNull) {}

  // Copy constructor
  explicit PersistenceScope(const DataType& aOther) : mData(aOther) {}

  bool MatchesValue(const Value& aOther) const {
    struct ValueMatcher {
      const Value& mOther;

      explicit ValueMatcher(const Value& aOther) : mOther(aOther) {}

      bool operator()(const Value& aThis) {
        return aThis.GetValue() == mOther.GetValue();
      }

      bool operator()(const Set& aThis) {
        return aThis.GetSet().contains(mOther.GetValue());
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(ValueMatcher(aOther));
  }

  bool MatchesSet(const Set& aOther) const {
    struct SetMatcher {
      const Set& mOther;

      explicit SetMatcher(const Set& aOther) : mOther(aOther) {}

      bool operator()(const Value& aThis) {
        return mOther.GetSet().contains(aThis.GetValue());
      }

      bool operator()(const Set& aThis) {
        for (auto persistenceType : aThis.GetSet()) {
          if (mOther.GetSet().contains(persistenceType)) {
            return true;
          }
        }
        return false;
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(SetMatcher(aOther));
  }

  bool operator==(const PersistenceScope& aOther) = delete;
};

bool MatchesPersistentPersistenceScope(
    const PersistenceScope& aPersistenceScope);

bool MatchesBestEffortPersistenceScope(
    const PersistenceScope& aPersistenceScope);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_PERSISTENCESCOPE_H_
