/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasePrincipal_h
#define mozilla_BasePrincipal_h

#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsJSPrincipals.h"

#include "mozilla/dom/ChromeUtilsBinding.h"

class nsIContentSecurityPolicy;
class nsIObjectOutputStream;
class nsIObjectInputStream;

namespace mozilla {

class OriginAttributes : public dom::OriginAttributesDictionary
{
public:
  OriginAttributes() {}
  OriginAttributes(uint32_t aAppId, bool aInBrowser)
  {
    mAppId = aAppId;
    mInBrowser = aInBrowser;
  }
  explicit OriginAttributes(const OriginAttributesDictionary& aOther)
    : OriginAttributesDictionary(aOther) {}

  bool operator==(const OriginAttributes& aOther) const
  {
    return mAppId == aOther.mAppId &&
           mInBrowser == aOther.mInBrowser;
  }
  bool operator!=(const OriginAttributes& aOther) const
  {
    return !(*this == aOther);
  }

  // Serializes/Deserializes non-default values into the suffix format, i.e.
  // |!key1=value1&key2=value2|. If there are no non-default attributes, this
  // returns an empty string.
  void CreateSuffix(nsACString& aStr) const;
  bool PopulateFromSuffix(const nsACString& aStr);

  void CookieJar(nsACString& aStr);

  // Populates the attributes from a string like
  // |uri!key1=value1&key2=value2| and returns the uri without the suffix.
  bool PopulateFromOrigin(const nsACString& aOrigin,
                          nsACString& aOriginNoSuffix);
};

/*
 * Base class from which all nsIPrincipal implementations inherit. Use this for
 * default implementations and other commonalities between principal
 * implementations.
 *
 * We should merge nsJSPrincipals into this class at some point.
 */
class BasePrincipal : public nsJSPrincipals
{
public:
  BasePrincipal();

  enum DocumentDomainConsideration { DontConsiderDocumentDomain, ConsiderDocumentDomain};
  bool Subsumes(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration);

  NS_IMETHOD GetOrigin(nsACString& aOrigin) final;
  NS_IMETHOD GetOriginNoSuffix(nsACString& aOrigin) final;
  NS_IMETHOD Equals(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD EqualsConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD Subsumes(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD SubsumesConsideringDomain(nsIPrincipal* other, bool* _retval) final;
  NS_IMETHOD GetCsp(nsIContentSecurityPolicy** aCsp) override;
  NS_IMETHOD SetCsp(nsIContentSecurityPolicy* aCsp) override;
  NS_IMETHOD GetCspJSON(nsAString& outCSPinJSON) override;
  NS_IMETHOD GetIsNullPrincipal(bool* aIsNullPrincipal) override;
  NS_IMETHOD GetJarPrefix(nsACString& aJarPrefix) final;
  NS_IMETHOD GetOriginAttributes(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) final;
  NS_IMETHOD GetOriginSuffix(nsACString& aOriginSuffix) final;
  NS_IMETHOD GetCookieJar(nsACString& aCookieJar) final;
  NS_IMETHOD GetAppStatus(uint16_t* aAppStatus) final;
  NS_IMETHOD GetAppId(uint32_t* aAppStatus) final;
  NS_IMETHOD GetIsInBrowserElement(bool* aIsInBrowserElement) final;
  NS_IMETHOD GetUnknownAppId(bool* aUnknownAppId) final;

  virtual bool IsOnCSSUnprefixingWhitelist() override { return false; }

  virtual bool IsCodebasePrincipal() const { return false; };

  static BasePrincipal* Cast(nsIPrincipal* aPrin) { return static_cast<BasePrincipal*>(aPrin); }
  static already_AddRefed<BasePrincipal> CreateCodebasePrincipal(nsIURI* aURI, OriginAttributes& aAttrs);

  const OriginAttributes& OriginAttributesRef() { return mOriginAttributes; }
  uint32_t AppId() const { return mOriginAttributes.mAppId; }
  bool IsInBrowserElement() const { return mOriginAttributes.mInBrowser; }

protected:
  virtual ~BasePrincipal();

  virtual nsresult GetOriginInternal(nsACString& aOrigin) = 0;
  virtual bool SubsumesInternal(nsIPrincipal* aOther, DocumentDomainConsideration aConsider) = 0;

  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  OriginAttributes mOriginAttributes;
};

} // namespace mozilla

#endif /* mozilla_BasePrincipal_h */
