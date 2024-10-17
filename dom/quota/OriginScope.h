/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_originorpatternstring_h__
#define mozilla_dom_quota_originorpatternstring_h__

#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsStringFlags.h"
#include "nsStringFwd.h"

namespace mozilla::dom::quota {

class OriginScope {
  class Origin {
    const PrincipalMetadata mPrincipalMetadata;
    nsCString mOriginNoSuffix;
    UniquePtr<OriginAttributes> mAttributes;

   public:
    explicit Origin(const PrincipalMetadata& aPrincipalMetadata)
        : mPrincipalMetadata(aPrincipalMetadata) {
      InitMembers();
    }

    Origin(const Origin& aOther)
        : mPrincipalMetadata(aOther.mPrincipalMetadata),
          mOriginNoSuffix(aOther.mOriginNoSuffix),
          mAttributes(MakeUnique<OriginAttributes>(*aOther.mAttributes)) {}

    Origin(Origin&& aOther) = default;

    const PrincipalMetadata& GetPrincipalMetadata() const {
      return mPrincipalMetadata;
    }

    const nsACString& GetGroup() const { return mPrincipalMetadata.mGroup; }

    const nsACString& GetOrigin() const { return mPrincipalMetadata.mOrigin; }

    const nsACString& GetOriginNoSuffix() const { return mOriginNoSuffix; }

    const OriginAttributes& GetAttributes() const {
      MOZ_ASSERT(mAttributes);

      return *mAttributes;
    }

   private:
    void InitMembers() {
      mAttributes = MakeUnique<OriginAttributes>();

      MOZ_ALWAYS_TRUE(mAttributes->PopulateFromOrigin(
          mPrincipalMetadata.mOrigin, mOriginNoSuffix));
    }
  };

  class Prefix {
    const PrincipalMetadata mPrincipalMetadata;
    nsCString mGroupNoSuffix;
    nsCString mOriginNoSuffix;

   public:
    explicit Prefix(const PrincipalMetadata& aPrincipalMetadata)
        : mGroupNoSuffix(aPrincipalMetadata.mGroup),
          mOriginNoSuffix(aPrincipalMetadata.mOrigin) {}

    const nsACString& GetGroupNoSuffix() const { return mGroupNoSuffix; }

    const nsCString& GetOriginNoSuffix() const { return mOriginNoSuffix; }
  };

  class Group {
    nsCString mGroup;
    nsCString mGroupNoSuffix;
    UniquePtr<OriginAttributes> mAttributes;

   public:
    explicit Group(const nsACString& aGroup) : mGroup(aGroup) { InitMembers(); }

    Group(const Group& aOther)
        : mGroup(aOther.mGroup),
          mGroupNoSuffix(aOther.mGroupNoSuffix),
          mAttributes(MakeUnique<OriginAttributes>(*aOther.mAttributes)) {}

    Group(Group&& aOther) = default;

    const nsACString& GetGroup() const { return mGroup; }

    const nsACString& GetGroupNoSuffix() const { return mGroupNoSuffix; }

    const OriginAttributes& GetAttributes() const {
      MOZ_ASSERT(mAttributes);

      return *mAttributes;
    }

   private:
    void InitMembers() {
      mAttributes = MakeUnique<OriginAttributes>();

      MOZ_ALWAYS_TRUE(mAttributes->PopulateFromOrigin(mGroup, mGroupNoSuffix));
    }
  };

  class Pattern {
    UniquePtr<OriginAttributesPattern> mPattern;

   public:
    explicit Pattern(const OriginAttributesPattern& aPattern)
        : mPattern(MakeUnique<OriginAttributesPattern>(aPattern)) {}

    explicit Pattern(const nsAString& aJSONPattern)
        : mPattern(MakeUnique<OriginAttributesPattern>()) {
      MOZ_ALWAYS_TRUE(mPattern->Init(aJSONPattern));
    }

    Pattern(const Pattern& aOther)
        : mPattern(MakeUnique<OriginAttributesPattern>(*aOther.mPattern)) {}

    Pattern(Pattern&& aOther) = default;

    const OriginAttributesPattern& GetPattern() const {
      MOZ_ASSERT(mPattern);

      return *mPattern;
    }

    nsString GetJSONPattern() const {
      MOZ_ASSERT(mPattern);

      nsString result;
      MOZ_ALWAYS_TRUE(mPattern->ToJSON(result));

      return result;
    }
  };

  struct Null {};

  using DataType = Variant<Origin, Prefix, Group, Pattern, Null>;

  DataType mData;

 public:
  OriginScope() : mData(Null()) {}

  // XXX Consider renaming these static methods to Create
  static OriginScope FromOrigin(const PrincipalMetadata& aPrincipalMetadata) {
    return OriginScope(std::move(Origin(aPrincipalMetadata)));
  }

  static OriginScope FromPrefix(const PrincipalMetadata& aPrincipalMetadata) {
    return OriginScope(std::move(Prefix(aPrincipalMetadata)));
  }

  static OriginScope FromGroup(const nsACString& aGroup) {
    return OriginScope(std::move(Group(aGroup)));
  }

  static OriginScope FromPattern(const OriginAttributesPattern& aPattern) {
    return OriginScope(std::move(Pattern(aPattern)));
  }

  static OriginScope FromJSONPattern(const nsAString& aJSONPattern) {
    return OriginScope(std::move(Pattern(aJSONPattern)));
  }

  static OriginScope FromNull() { return OriginScope(std::move(Null())); }

  bool IsOrigin() const { return mData.is<Origin>(); }

  bool IsPrefix() const { return mData.is<Prefix>(); }

  bool IsPattern() const { return mData.is<Pattern>(); }

  bool IsNull() const { return mData.is<Null>(); }

  void SetFromOrigin(const PrincipalMetadata& aPrincipalMetadata) {
    mData = AsVariant(Origin(aPrincipalMetadata));
  }

  void SetFromPrefix(const PrincipalMetadata& aPrincipalMetadata) {
    mData = AsVariant(Prefix(aPrincipalMetadata));
  }

  void SetFromPattern(const OriginAttributesPattern& aPattern) {
    mData = AsVariant(Pattern(aPattern));
  }

  void SetFromJSONPattern(const nsAString& aJSONPattern) {
    mData = AsVariant(Pattern(aJSONPattern));
  }

  void SetFromNull() { mData = AsVariant(Null()); }

  const PrincipalMetadata& GetPrincipalMetadata() const {
    MOZ_ASSERT(IsOrigin());

    return mData.as<Origin>().GetPrincipalMetadata();
  }

  const nsACString& GetOrigin() const {
    MOZ_ASSERT(IsOrigin());

    return mData.as<Origin>().GetOrigin();
  }

  const nsACString& GetOriginNoSuffix() const {
    MOZ_ASSERT(IsOrigin() || IsPrefix());

    if (IsOrigin()) {
      return mData.as<Origin>().GetOriginNoSuffix();
    }
    return mData.as<Prefix>().GetOriginNoSuffix();
  }

  const OriginAttributesPattern& GetPattern() const {
    MOZ_ASSERT(IsPattern());

    return mData.as<Pattern>().GetPattern();
  }

  nsString GetJSONPattern() const {
    MOZ_ASSERT(IsPattern());

    return mData.as<Pattern>().GetJSONPattern();
  }

  bool Matches(const OriginScope& aOther) const {
    struct Matcher {
      const OriginScope& mThis;

      explicit Matcher(const OriginScope& aThis) : mThis(aThis) {}

      bool operator()(const Origin& aOther) {
        return mThis.MatchesOrigin(aOther);
      }

      bool operator()(const Prefix& aOther) {
        return mThis.MatchesPrefix(aOther);
      }

      bool operator()(const Group& aOther) {
        return mThis.MatchesGroup(aOther);
      }

      bool operator()(const Pattern& aOther) {
        return mThis.MatchesPattern(aOther);
      }

      bool operator()(const Null& aOther) { return true; }
    };

    return aOther.mData.match(Matcher(*this));
  }

  OriginScope Clone() { return OriginScope(mData); }

 private:
  // Move constructors
  explicit OriginScope(const Origin&& aOrigin) : mData(aOrigin) {}

  explicit OriginScope(const Prefix&& aPrefix) : mData(aPrefix) {}

  explicit OriginScope(const Group&& aGroup) : mData(aGroup) {}

  explicit OriginScope(const Pattern&& aPattern) : mData(aPattern) {}

  explicit OriginScope(const Null&& aNull) : mData(aNull) {}

  // Copy constructor
  explicit OriginScope(const DataType& aOther) : mData(aOther) {}

  bool MatchesOrigin(const Origin& aOther) const {
    struct OriginMatcher {
      const Origin& mOther;

      explicit OriginMatcher(const Origin& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return aThis.GetOrigin().Equals(mOther.GetOrigin());
      }

      bool operator()(const Prefix& aThis) {
        return aThis.GetOriginNoSuffix().Equals(mOther.GetOriginNoSuffix());
      }

      bool operator()(const Group& aThis) {
        return aThis.GetGroup().Equals(mOther.GetGroup());
      }

      bool operator()(const Pattern& aThis) {
        return aThis.GetPattern().Matches(mOther.GetAttributes());
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(OriginMatcher(aOther));
  }

  bool MatchesPrefix(const Prefix& aOther) const {
    struct PrefixMatcher {
      const Prefix& mOther;

      explicit PrefixMatcher(const Prefix& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return aThis.GetOriginNoSuffix().Equals(mOther.GetOriginNoSuffix());
      }

      bool operator()(const Prefix& aThis) {
        return aThis.GetOriginNoSuffix().Equals(mOther.GetOriginNoSuffix());
      }

      bool operator()(const Group& aThis) {
        return aThis.GetGroupNoSuffix().Equals(mOther.GetGroupNoSuffix());
      }

      bool operator()(const Pattern& aThis) {
        // The match will be always true here because any origin attributes
        // pattern overlaps any origin prefix (an origin prefix targets all
        // origin attributes).
        return true;
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(PrefixMatcher(aOther));
  }

  bool MatchesGroup(const Group& aOther) const {
    struct GroupMatcher {
      const Group& mOther;

      explicit GroupMatcher(const Group& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return aThis.GetGroup().Equals(mOther.GetGroup());
      }

      bool operator()(const Prefix& aThis) {
        return aThis.GetGroupNoSuffix().Equals(mOther.GetGroupNoSuffix());
      }

      bool operator()(const Group& aThis) {
        return aThis.GetGroup().Equals(mOther.GetGroup());
      }

      bool operator()(const Pattern& aThis) {
        return aThis.GetPattern().Matches(mOther.GetAttributes());
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(GroupMatcher(aOther));
  }

  bool MatchesPattern(const Pattern& aOther) const {
    struct PatternMatcher {
      const Pattern& mOther;

      explicit PatternMatcher(const Pattern& aOther) : mOther(aOther) {}

      bool operator()(const Origin& aThis) {
        return mOther.GetPattern().Matches(aThis.GetAttributes());
      }

      bool operator()(const Prefix& aThis) {
        // The match will be always true here because any origin attributes
        // pattern overlaps any origin prefix (an origin prefix targets all
        // origin attributes).
        return true;
      }

      bool operator()(const Group& aThis) {
        return mOther.GetPattern().Matches(aThis.GetAttributes());
      }

      bool operator()(const Pattern& aThis) {
        return aThis.GetPattern().Overlaps(mOther.GetPattern());
      }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    PatternMatcher patternMatcher(aOther);
    return mData.match(PatternMatcher(aOther));
  }

  bool operator==(const OriginScope& aOther) = delete;
};

}  // namespace mozilla::dom::quota

#endif  // mozilla_dom_quota_originorpatternstring_h__
