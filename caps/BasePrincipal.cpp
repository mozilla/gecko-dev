/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"

#include "nsDocShell.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIStandardURL.h"

#include "ExpandedPrincipal.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsIURIWithSpecialOrigin.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsAboutProtocolUtils.h"
#include "ThirdPartyUtil.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ToJSValue.h"

#include "json/json.h"
#include "nsSerializationHelper.h"

namespace mozilla {

BasePrincipal::BasePrincipal(PrincipalKind aKind)
    : mKind(aKind), mHasExplicitDomain(false), mInitialized(false) {}

BasePrincipal::~BasePrincipal() {}

NS_IMETHODIMP
BasePrincipal::GetOrigin(nsACString& aOrigin) {
  MOZ_ASSERT(mInitialized);

  nsresult rv = GetOriginNoSuffix(aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  rv = GetOriginSuffix(suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  aOrigin.Append(suffix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginNoSuffix(nsACString& aOrigin) {
  MOZ_ASSERT(mInitialized);
  mOriginNoSuffix->ToUTF8String(aOrigin);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetSiteOrigin(nsACString& aSiteOrigin) {
  MOZ_ASSERT(mInitialized);
  return GetOrigin(aSiteOrigin);
}

// Returns the inner Json::value of the serialized principal
// Example input and return values:
// Null principal:
// {"0":{"0":"moz-nullprincipal:{56cac540-864d-47e7-8e25-1614eab5155e}"}} ->
// {"0":"moz-nullprincipal:{56cac540-864d-47e7-8e25-1614eab5155e}"}
//
// Content principal:
// {"1":{"0":"https://mozilla.com"}} -> {"0":"https://mozilla.com"}
//
// Expanded principal:
// {"2":{"0":"<base64principal1>,<base64principal2>"}} ->
// {"0":"<base64principal1>,<base64principal2>"}
//
// System principal:
// {"3":{}} -> {}
// The aKey passed in also returns the corresponding PrincipalKind enum
//
// Warning: The Json::Value* pointer is into the aRoot object
static const Json::Value* GetPrincipalObject(const Json::Value& aRoot,
                                             int& aOutPrincipalKind) {
  const Json::Value::Members members = aRoot.getMemberNames();
  // We only support one top level key in the object
  if (members.size() != 1) {
    return nullptr;
  }
  // members[0] here is the "0", "1", "2", "3" principalKind
  // that is the top level of the serialized JSON principal
  const std::string stringPrincipalKind = members[0];

  // Next we take the string value from the JSON
  // and convert it into the int for the BasePrincipal::PrincipalKind enum

  // Verify that the key is within the valid range
  int principalKind = std::stoi(stringPrincipalKind);
  MOZ_ASSERT(BasePrincipal::eNullPrincipal == 0,
             "We need to rely on 0 being a bounds check for the first "
             "principal kind.");
  if (principalKind < 0 || principalKind > BasePrincipal::eKindMax) {
    return nullptr;
  }
  MOZ_ASSERT(principalKind == BasePrincipal::eNullPrincipal ||
             principalKind == BasePrincipal::eContentPrincipal ||
             principalKind == BasePrincipal::eExpandedPrincipal ||
             principalKind == BasePrincipal::eSystemPrincipal);
  aOutPrincipalKind = principalKind;

  if (!aRoot[stringPrincipalKind].isObject()) {
    return nullptr;
  }

  // Return the inner value of the principal object
  return &aRoot[stringPrincipalKind];
}

// Accepts the JSON inner object without the wrapping principalKind
// (See GetPrincipalObject for the inner object response examples)
// Creates an array of KeyVal objects that are all defined on the principal
// Each principal type (null, content, expanded) has a KeyVal that stores the
// fields of the JSON
//
// This simplifies deserializing elsewhere as we do the checking for presence
// and string values here for the complete set of serializable keys that the
// corresponding principal supports.
//
// The KeyVal object has the following fields:
// - valueWasSerialized: is true if the deserialized JSON contained a string
// value
// - value: The string that was serialized for this key
// - key: an SerializableKeys enum value specific to the principal.
//        For example content principal is an enum of: eURI, eDomain,
//        eSuffix, eCSP
//
//
//  Given an inner content principal:
//  {"0": "https://mozilla.com", "2": "^privateBrowsingId=1"}
//    |                |          |         |
//    -----------------------------         |
//         |           |                    |
//        Key          ----------------------
//                                |
//                              Value
//
// They Key "0" corresponds to ContentPrincipal::eURI
// They Key "1" corresponds to ContentPrincipal::eSuffix
template <typename T>
static nsTArray<typename T::KeyVal> GetJSONKeys(const Json::Value* aInput) {
  int size = T::eMax + 1;
  nsTArray<typename T::KeyVal> fields;
  for (int i = 0; i != size; i++) {
    typename T::KeyVal field;
    // field.valueWasSerialized returns if the field was found in the
    // deserialized code. This simplifies the consumers from having to check
    // length.
    field.valueWasSerialized = false;
    field.key = static_cast<typename T::SerializableKeys>(i);
    const std::string key = std::to_string(field.key);
    if (aInput->isMember(key) && (*aInput)[key].isString()) {
      field.value.Append(nsDependentCString((*aInput)[key].asCString()));
      field.valueWasSerialized = true;
    }
    fields.AppendElement(field);
  }
  return fields;
}

// Takes a JSON string and parses it turning it into a principal of the
// corresponding type
//
// Given a content principal:
//
//                               inner JSON object
//                                      |
//       ---------------------------------------------------------
//       |                                                       |
// {"1": {"0": "https://mozilla.com", "2": "^privateBrowsingId=1"}}
//   |     |             |             |            |
//   |     -----------------------------            |
//   |              |    |                          |
// PrincipalKind    |    |                          |
//                  |    ----------------------------
//           SerializableKeys           |
//                                    Value
//
// The string is first deserialized with jsoncpp to get the Json::Value of the
// object. The inner JSON object is parsed with GetPrincipalObject which returns
// a KeyVal array of the inner object's fields. PrincipalKind is returned by
// GetPrincipalObject which is then used to decide which principal
// implementation of FromProperties to call. The corresponding FromProperties
// call takes the KeyVal fields and turns it into a principal.
already_AddRefed<BasePrincipal> BasePrincipal::FromJSON(
    const nsACString& aJSON) {
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> const reader(builder.newCharReader());
  bool parseSuccess =
      reader->parse(aJSON.BeginReading(), aJSON.EndReading(), &root, nullptr);
  if (!parseSuccess) {
    MOZ_ASSERT(false,
               "Unable to parse string as JSON to deserialize as a principal");
    return nullptr;
  }

  int principalKind = -1;
  const Json::Value* value = GetPrincipalObject(root, principalKind);
  if (!value) {
#ifdef DEBUG
    fprintf(stderr, "Unexpected JSON principal %s\n",
            root.toStyledString().c_str());
#endif
    MOZ_ASSERT(false, "Unexpected JSON to deserialize as a principal");

    return nullptr;
  }
  MOZ_ASSERT(principalKind != -1,
             "PrincipalKind should always be >=0 by this point");

  if (principalKind == eSystemPrincipal) {
    RefPtr<BasePrincipal> principal =
        BasePrincipal::Cast(nsContentUtils::GetSystemPrincipal());
    return principal.forget();
  }

  if (principalKind == eNullPrincipal) {
    nsTArray<NullPrincipal::KeyVal> res = GetJSONKeys<NullPrincipal>(value);
    return NullPrincipal::FromProperties(res);
  }

  if (principalKind == eContentPrincipal) {
    nsTArray<ContentPrincipal::KeyVal> res =
        GetJSONKeys<ContentPrincipal>(value);
    return ContentPrincipal::FromProperties(res);
  }

  if (principalKind == eExpandedPrincipal) {
    nsTArray<ExpandedPrincipal::KeyVal> res =
        GetJSONKeys<ExpandedPrincipal>(value);
    return ExpandedPrincipal::FromProperties(res);
  }

  MOZ_RELEASE_ASSERT(false, "Unexpected enum to deserialize as a principal");
}

nsresult BasePrincipal::PopulateJSONObject(Json::Value& aObject) {
  return NS_OK;
}

// Returns a JSON representation of the principal.
// Calling BasePrincipal::FromJSON will deserialize the JSON into
// the corresponding principal type.
nsresult BasePrincipal::ToJSON(nsACString& aResult) {
  MOZ_ASSERT(aResult.IsEmpty(), "ToJSON only supports an empty result input");
  aResult.Truncate();

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  Json::Value innerJSONObject = Json::objectValue;

  nsresult rv = PopulateJSONObject(innerJSONObject);
  NS_ENSURE_SUCCESS(rv, rv);

  Json::Value root = Json::objectValue;
  std::string key = std::to_string(Kind());
  root[key] = innerJSONObject;
  std::string result = Json::writeString(builder, root);
  aResult.Append(result);
  if (aResult.Length() == 0) {
    MOZ_ASSERT(false, "JSON writer failed to output a principal serialization");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

bool BasePrincipal::Subsumes(nsIPrincipal* aOther,
                             DocumentDomainConsideration aConsideration) {
  MOZ_ASSERT(aOther);
  MOZ_ASSERT_IF(Kind() == eContentPrincipal, mOriginSuffix);

  // Expanded principals handle origin attributes for each of their
  // sub-principals individually, null principals do only simple checks for
  // pointer equality, and system principals are immune to origin attributes
  // checks, so only do this check for content principals.
  if (Kind() == eContentPrincipal &&
      mOriginSuffix != Cast(aOther)->mOriginSuffix) {
    return false;
  }

  return SubsumesInternal(aOther, aConsideration);
}

NS_IMETHODIMP
BasePrincipal::Equals(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastEquals(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsConsideringDomain(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastEqualsConsideringDomain(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::Subsumes(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastSubsumes(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomain(nsIPrincipal* aOther, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastSubsumesConsideringDomain(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomainIgnoringFPD(nsIPrincipal* aOther,
                                                    bool* aResult) {
  NS_ENSURE_ARG_POINTER(aOther);

  *aResult = FastSubsumesConsideringDomainIgnoringFPD(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::CheckMayLoad(nsIURI* aURI, bool aReport,
                            bool aAllowIfInheritsPrincipal) {
  NS_ENSURE_ARG_POINTER(aURI);

  // Check the internal method first, which allows us to quickly approve loads
  // for the System Principal.
  if (MayLoadInternal(aURI)) {
    return NS_OK;
  }

  nsresult rv;
  if (aAllowIfInheritsPrincipal) {
    // If the caller specified to allow loads of URIs that inherit
    // our principal, allow the load if this URI inherits its principal.
    bool doesInheritSecurityContext;
    rv = NS_URIChainHasFlags(aURI,
                             nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                             &doesInheritSecurityContext);
    if (NS_SUCCEEDED(rv) && doesInheritSecurityContext) {
      return NS_OK;
    }
  }

  bool fetchableByAnyone;
  rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_FETCHABLE_BY_ANYONE,
                           &fetchableByAnyone);
  if (NS_SUCCEEDED(rv) && fetchableByAnyone) {
    return NS_OK;
  }

  if (aReport) {
    nsCOMPtr<nsIURI> prinURI;
    rv = GetURI(getter_AddRefs(prinURI));
    if (NS_SUCCEEDED(rv) && prinURI) {
      nsScriptSecurityManager::ReportError(
          "CheckSameOriginError", prinURI, aURI,
          mOriginAttributes.mPrivateBrowsingId > 0);
    }
  }

  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
BasePrincipal::IsThirdPartyURI(nsIURI* aURI, bool* aRes) {
  *aRes = true;
  // If we do not have a URI its always 3rd party.
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
  return thirdPartyUtil->IsThirdPartyURI(prinURI, aURI, aRes);
}

NS_IMETHODIMP
BasePrincipal::IsThirdPartyPrincipal(nsIPrincipal* aPrin, bool* aRes) {
  *aRes = true;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return aPrin->IsThirdPartyURI(prinURI, aRes);
}

NS_IMETHODIMP
BasePrincipal::GetIsNullPrincipal(bool* aResult) {
  *aResult = Kind() == eNullPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsContentPrincipal(bool* aResult) {
  *aResult = Kind() == eContentPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsExpandedPrincipal(bool* aResult) {
  *aResult = Kind() == eExpandedPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAsciiSpec(nsACString& aSpec) {
  aSpec.Truncate();
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  return prinURI->GetAsciiSpec(aSpec);
}

NS_IMETHODIMP
BasePrincipal::GetIsSystemPrincipal(bool* aResult) {
  *aResult = IsSystemPrincipal();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsAddonOrExpandedAddonPrincipal(bool* aResult) {
  *aResult = AddonPolicy() || ContentScriptAddonPolicy();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SchemeIs(const char* aScheme, bool* aResult) {
  *aResult = false;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_OK;
  }
  *aResult = prinURI->SchemeIs(aScheme);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAboutModuleFlags(uint32_t* flags) {
  *flags = 0;
  nsCOMPtr<nsIURI> prinURI;
  nsresult rv = GetURI(getter_AddRefs(prinURI));
  if (NS_FAILED(rv) || !prinURI) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (!prinURI->SchemeIs("about")) {
    return NS_OK;
  }

  nsCOMPtr<nsIAboutModule> aboutModule;
  rv = NS_GetAboutModule(prinURI, getter_AddRefs(aboutModule));
  if (NS_FAILED(rv) || !aboutModule) {
    return rv;
  }
  return aboutModule->GetURIFlags(prinURI, flags);
}

NS_IMETHODIMP
BasePrincipal::GetOriginAttributes(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal) {
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginSuffix(nsACString& aOriginAttributes) {
  MOZ_ASSERT(mOriginSuffix);
  mOriginSuffix->ToUTF8String(aOriginAttributes);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetUserContextId(uint32_t* aUserContextId) {
  *aUserContextId = UserContextId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId) {
  *aPrivateBrowsingId = PrivateBrowsingId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsInIsolatedMozBrowserElement(
    bool* aIsInIsolatedMozBrowserElement) {
  *aIsInIsolatedMozBrowserElement = IsInIsolatedMozBrowserElement();
  return NS_OK;
}

nsresult BasePrincipal::GetAddonPolicy(nsISupports** aResult) {
  RefPtr<extensions::WebExtensionPolicy> policy(AddonPolicy());
  policy.forget(aResult);
  return NS_OK;
}

extensions::WebExtensionPolicy* BasePrincipal::AddonPolicy() {
  if (Is<ContentPrincipal>()) {
    return As<ContentPrincipal>()->AddonPolicy();
  }
  return nullptr;
}

bool BasePrincipal::AddonHasPermission(const nsAtom* aPerm) {
  if (auto policy = AddonPolicy()) {
    return policy->HasPermission(aPerm);
  }
  return false;
}

nsIPrincipal* BasePrincipal::PrincipalToInherit(nsIURI* aRequestedURI) {
  if (Is<ExpandedPrincipal>()) {
    return As<ExpandedPrincipal>()->PrincipalToInherit(aRequestedURI);
  }
  return this;
}

already_AddRefed<BasePrincipal> BasePrincipal::CreateContentPrincipal(
    nsIURI* aURI, const OriginAttributes& aAttrs) {
  MOZ_ASSERT(aURI);

  nsAutoCString originNoSuffix;
  nsresult rv =
      ContentPrincipal::GenerateOriginNoSuffixFromURI(aURI, originNoSuffix);
  if (NS_FAILED(rv)) {
    // If the generation of the origin fails, we still want to have a valid
    // principal. Better to return a null principal here.
    return NullPrincipal::Create(aAttrs);
  }

  return CreateContentPrincipal(aURI, aAttrs, originNoSuffix);
}

already_AddRefed<BasePrincipal> BasePrincipal::CreateContentPrincipal(
    nsIURI* aURI, const OriginAttributes& aAttrs,
    const nsACString& aOriginNoSuffix) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(!aOriginNoSuffix.IsEmpty());

  // If the URI is supposed to inherit the security context of whoever loads it,
  // we shouldn't make a content principal for it.
  bool inheritsPrincipal;
  nsresult rv = NS_URIChainHasFlags(
      aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
      &inheritsPrincipal);
  if (NS_FAILED(rv) || inheritsPrincipal) {
    return NullPrincipal::Create(aAttrs);
  }

  // Check whether the URI knows what its principal is supposed to be.
#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  nsCOMPtr<nsIURIWithSpecialOrigin> uriWithSpecialOrigin =
      do_QueryInterface(aURI);
  if (uriWithSpecialOrigin) {
    nsCOMPtr<nsIURI> origin;
    rv = uriWithSpecialOrigin->GetOrigin(getter_AddRefs(origin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    MOZ_ASSERT(origin);
    OriginAttributes attrs;
    RefPtr<BasePrincipal> principal = CreateContentPrincipal(origin, attrs);
    return principal.forget();
  }
#endif

  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          aURI, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    RefPtr<BasePrincipal> principal = Cast(blobPrincipal);
    return principal.forget();
  }

  // Mint a content principal.
  RefPtr<ContentPrincipal> principal = new ContentPrincipal();
  rv = principal->Init(aURI, aAttrs, aOriginNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return principal.forget();
}

already_AddRefed<BasePrincipal> BasePrincipal::CreateContentPrincipal(
    const nsACString& aOrigin) {
  MOZ_ASSERT(!StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("[")),
             "CreateContentPrincipal does not support System and Expanded "
             "principals");

  MOZ_ASSERT(!StringBeginsWith(aOrigin,
                               NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME ":")),
             "CreateContentPrincipal does not support NullPrincipal");

  nsAutoCString originNoSuffix;
  OriginAttributes attrs;
  if (!attrs.PopulateFromOrigin(aOrigin, originNoSuffix)) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return BasePrincipal::CreateContentPrincipal(uri, attrs);
}

already_AddRefed<BasePrincipal> BasePrincipal::CloneForcingOriginAttributes(
    const OriginAttributes& aOriginAttributes) {
  if (NS_WARN_IF(!IsContentPrincipal())) {
    return nullptr;
  }

  nsAutoCString originNoSuffix;
  nsresult rv = GetOriginNoSuffix(originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsIURI* uri = static_cast<ContentPrincipal*>(this)->mURI;
  RefPtr<ContentPrincipal> copy = new ContentPrincipal();
  rv = copy->Init(uri, aOriginAttributes, originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return copy.forget();
}

extensions::WebExtensionPolicy* BasePrincipal::ContentScriptAddonPolicy() {
  if (!Is<ExpandedPrincipal>()) {
    return nullptr;
  }

  auto expanded = As<ExpandedPrincipal>();
  for (auto& prin : expanded->AllowList()) {
    if (auto policy = BasePrincipal::Cast(prin)->AddonPolicy()) {
      return policy;
    }
  }

  return nullptr;
}

bool BasePrincipal::AddonAllowsLoad(nsIURI* aURI,
                                    bool aExplicit /* = false */) {
  if (Is<ExpandedPrincipal>()) {
    return As<ExpandedPrincipal>()->AddonAllowsLoad(aURI, aExplicit);
  }
  if (auto policy = AddonPolicy()) {
    return policy->CanAccessURI(aURI, aExplicit);
  }
  return false;
}

void BasePrincipal::FinishInit(const nsACString& aOriginNoSuffix,
                               const OriginAttributes& aOriginAttributes) {
  mInitialized = true;
  mOriginAttributes = aOriginAttributes;

  // First compute the origin suffix since it's infallible.
  nsAutoCString originSuffix;
  mOriginAttributes.CreateSuffix(originSuffix);
  mOriginSuffix = NS_Atomize(originSuffix);

  MOZ_ASSERT(!aOriginNoSuffix.IsEmpty());
  mOriginNoSuffix = NS_Atomize(aOriginNoSuffix);
}

void BasePrincipal::FinishInit(BasePrincipal* aOther,
                               const OriginAttributes& aOriginAttributes) {
  mInitialized = true;
  mOriginAttributes = aOriginAttributes;

  // First compute the origin suffix since it's infallible.
  nsAutoCString originSuffix;
  mOriginAttributes.CreateSuffix(originSuffix);
  mOriginSuffix = NS_Atomize(originSuffix);

  mOriginNoSuffix = aOther->mOriginNoSuffix;
  mHasExplicitDomain = aOther->mHasExplicitDomain;
}

bool SiteIdentifier::Equals(const SiteIdentifier& aOther) const {
  MOZ_ASSERT(IsInitialized());
  MOZ_ASSERT(aOther.IsInitialized());
  return mPrincipal->FastEquals(aOther.mPrincipal);
}

}  // namespace mozilla
