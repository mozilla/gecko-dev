/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSimpleURI_h__
#define nsSimpleURI_h__

#include "mozilla/MemoryReporting.h"
#include "nsIURI.h"
#include "nsISerializable.h"
#include "nsString.h"
#include "nsIClassInfo.h"
#include "nsISizeOf.h"
#include "nsIURIMutator.h"
#include "nsISimpleURIMutator.h"

namespace mozilla {
namespace net {

#define NS_THIS_SIMPLEURI_IMPLEMENTATION_CID  \
  {/* 0b9bb0c2-fee6-470b-b9b9-9fd9462b5e19 */ \
   0x0b9bb0c2,                                \
   0xfee6,                                    \
   0x470b,                                    \
   {0xb9, 0xb9, 0x9f, 0xd9, 0x46, 0x2b, 0x5e, 0x19}}

class nsSimpleURI : public nsIURI, public nsISerializable, public nsISizeOf {
 protected:
  nsSimpleURI() = default;
  virtual ~nsSimpleURI() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSISERIALIZABLE

  static already_AddRefed<nsSimpleURI> From(nsIURI* aURI);

  // nsSimpleURI methods:

  bool Equals(nsSimpleURI* aOther) { return EqualsInternal(aOther, eHonorRef); }

  // nsISizeOf
  // Among the sub-classes that inherit (directly or indirectly) from
  // nsSimpleURI, measurement of the following members may be added later if
  // DMD finds it is worthwhile:
  // - nsJSURI: mBaseURI
  // - nsSimpleNestedURI: mInnerURI
  // - nsBlobURI: mPrincipal
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 protected:
  // enum used in a few places to specify how .ref attribute should be handled
  enum RefHandlingEnum { eIgnoreRef, eHonorRef, eReplaceRef };

  virtual nsresult Clone(nsIURI** result);
  virtual nsresult SetSpecInternal(const nsACString& aSpec,
                                   bool aStripWhitespace = false);
  virtual nsresult SetScheme(const nsACString& input);
  virtual nsresult SetUserPass(const nsACString& input);
  nsresult SetUsername(const nsACString& input);
  virtual nsresult SetPassword(const nsACString& input);
  virtual nsresult SetHostPort(const nsACString& aValue);
  virtual nsresult SetHost(const nsACString& input);
  virtual nsresult SetPort(int32_t port);
  virtual nsresult SetPathQueryRef(const nsACString& aPath);
  virtual nsresult SetRef(const nsACString& aRef);
  virtual nsresult SetFilePath(const nsACString& aFilePath);
  virtual nsresult SetQuery(const nsACString& aQuery);
  virtual nsresult SetQueryWithEncoding(const nsACString& aQuery,
                                        const Encoding* encoding);
  nsresult ReadPrivate(nsIObjectInputStream* stream);

  // Helper to share code between Equals methods.
  virtual nsresult EqualsInternal(nsIURI* other,
                                  RefHandlingEnum refHandlingMode,
                                  bool* result);

  // Helper to be used by inherited classes who want to test
  // equality given an assumed nsSimpleURI.  This must NOT check
  // the passed-in other for QI to our CID.
  bool EqualsInternal(nsSimpleURI* otherUri, RefHandlingEnum refHandlingMode);

  virtual already_AddRefed<nsSimpleURI> StartClone();

  void TrimTrailingCharactersFromPath();

  // Initialize `mQueryPos` and `mRefPos` from `mSpec`, and perform
  // component-specific escapes. `mPathPos` should already be initialized.
  nsresult SetPathQueryRefInternal();

  bool Deserialize(const mozilla::ipc::URIParams&);

  // computed index helpers
  size_t SchemeStart() const { return 0; }
  size_t SchemeEnd() const { return mPathSep; }
  size_t SchemeLen() const { return SchemeEnd() - SchemeStart(); }

  size_t PathStart() const { return mPathSep + 1; }
  inline size_t PathEnd() const;
  size_t PathLen() const { return PathEnd() - PathStart(); }

  bool IsQueryValid() const { return mQuerySep != kNotFound; }
  inline size_t QueryStart() const;
  inline size_t QueryEnd() const;
  size_t QueryLen() const { return QueryEnd() - QueryStart(); }

  bool IsRefValid() const { return mRefSep != kNotFound; }
  inline size_t RefStart() const;
  inline size_t RefEnd() const;
  size_t RefLen() const { return RefEnd() - RefStart(); }

  // dependent substring getters
  nsDependentCSubstring Scheme() {
    return Substring(mSpec, SchemeStart(), SchemeLen());
  }
  nsDependentCSubstring Path() {
    return Substring(mSpec, PathStart(), PathLen());
  }
  nsDependentCSubstring Query() {
    return Substring(mSpec, QueryStart(), QueryLen());
  }
  nsDependentCSubstring Ref() { return Substring(mSpec, RefStart(), RefLen()); }
  nsDependentCSubstring SpecIgnoringRef() {
    return Substring(mSpec, 0, IsRefValid() ? mRefSep : -1);
  }

  // mSpec contains the normalized version of the URL spec (UTF-8 encoded).
  nsCString mSpec;

  // Index of the `:` character which indicates the start of the path.
  int32_t mPathSep = kNotFound;
  // Index of the `?` character which indicates the start of the query.
  // Will be `kNotFound` if there is no query.
  int32_t mQuerySep = kNotFound;
  // Index of the `#` character which indicates the start of the ref.
  // Will be `kNotFound` if there is no ref.
  int32_t mRefSep = kNotFound;

 public:
  class Mutator final : public nsIURIMutator,
                        public BaseURIMutator<nsSimpleURI>,
                        public nsISimpleURIMutator,
                        public nsISerializable {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
    NS_DEFINE_NSIMUTATOR_COMMON

    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    [[nodiscard]] NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return InitFromInputStream(aStream);
    }

    [[nodiscard]] NS_IMETHOD SetSpecAndFilterWhitespace(
        const nsACString& aSpec, nsIURIMutator** aMutator) override {
      if (aMutator) {
        *aMutator = do_AddRef(this).take();
      }

      nsresult rv = NS_OK;
      RefPtr<nsSimpleURI> uri = new nsSimpleURI();
      rv = uri->SetSpecInternal(aSpec, /* filterWhitespace */ true);
      if (NS_FAILED(rv)) {
        return rv;
      }
      mURI = std::move(uri);
      return NS_OK;
    }

    explicit Mutator() = default;

   private:
    virtual ~Mutator() = default;

    friend class nsSimpleURI;
  };

  friend BaseURIMutator<nsSimpleURI>;
};

//-----------------------------------------------------------------------------
// Computed index helpers
//-----------------------------------------------------------------------------

inline size_t nsSimpleURI::PathEnd() const {
  if (IsQueryValid()) {
    return mQuerySep;
  }
  if (IsRefValid()) {
    return mRefSep;
  }
  return mSpec.Length();
}

inline size_t nsSimpleURI::QueryStart() const {
  MOZ_DIAGNOSTIC_ASSERT(IsQueryValid());
  return mQuerySep + 1;
}

inline size_t nsSimpleURI::QueryEnd() const {
  MOZ_DIAGNOSTIC_ASSERT(IsQueryValid());
  if (IsRefValid()) {
    return mRefSep;
  }
  return mSpec.Length();
}

inline size_t nsSimpleURI::RefStart() const {
  MOZ_DIAGNOSTIC_ASSERT(IsRefValid());
  return mRefSep + 1;
}

inline size_t nsSimpleURI::RefEnd() const {
  MOZ_DIAGNOSTIC_ASSERT(IsRefValid());
  return mSpec.Length();
}

}  // namespace net
}  // namespace mozilla

#endif  // nsSimpleURI_h__
