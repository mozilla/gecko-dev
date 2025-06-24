/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "URLPattern.h"
#include "mozilla/net/MozURL.h"
#include "mozilla/ErrorResult.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URLPattern, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(URLPattern)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URLPattern)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URLPattern)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* URLPattern::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return URLPattern_Binding::Wrap(aCx, this, aGivenProto);
}

void GlueToBindingInit(const UrlpInit& aGInit, URLPatternInit& aBInit) {
  if (aGInit.protocol.valid) {
    aBInit.mProtocol.Construct(aGInit.protocol.string);
  }
  if (aGInit.username.valid) {
    aBInit.mUsername.Construct(aGInit.username.string);
  }
  if (aGInit.password.valid) {
    aBInit.mPassword.Construct(aGInit.password.string);
  }
  if (aGInit.hostname.valid) {
    aBInit.mHostname.Construct(aGInit.hostname.string);
  }
  if (aGInit.port.valid) {
    aBInit.mPort.Construct(aGInit.port.string);
  }
  if (aGInit.pathname.valid) {
    aBInit.mPathname.Construct(aGInit.pathname.string);
  }
  if (aGInit.search.valid) {
    aBInit.mSearch.Construct(aGInit.search.string);
  }
  if (aGInit.hash.valid) {
    aBInit.mHash.Construct(aGInit.hash.string);
  }
  if (aGInit.base_url.valid) {
    aBInit.mBaseURL.Construct(aGInit.base_url.string);
  }
}

void BindingToGlueInit(const URLPatternInit& aBInit, UrlpInit& aGInit) {
  if (aBInit.mProtocol.WasPassed()) {
    aGInit.protocol = net::CreateMaybeString(aBInit.mProtocol.Value(), true);
  }
  if (aBInit.mUsername.WasPassed()) {
    aGInit.username = net::CreateMaybeString(aBInit.mUsername.Value(), true);
  }
  if (aBInit.mPassword.WasPassed()) {
    aGInit.password = net::CreateMaybeString(aBInit.mPassword.Value(), true);
  }
  if (aBInit.mHostname.WasPassed()) {
    aGInit.hostname = net::CreateMaybeString(aBInit.mHostname.Value(), true);
  }
  if (aBInit.mPort.WasPassed()) {
    aGInit.port = net::CreateMaybeString(aBInit.mPort.Value(), true);
  }
  if (aBInit.mPathname.WasPassed()) {
    aGInit.pathname = net::CreateMaybeString(aBInit.mPathname.Value(), true);
  }
  if (aBInit.mSearch.WasPassed()) {
    aGInit.search = net::CreateMaybeString(aBInit.mSearch.Value(), true);
  }
  if (aBInit.mHash.WasPassed()) {
    aGInit.hash = net::CreateMaybeString(aBInit.mHash.Value(), true);
  }
  if (aBInit.mBaseURL.WasPassed()) {
    aGInit.base_url = net::CreateMaybeString(aBInit.mBaseURL.Value(), true);
  }
}

// static
already_AddRefed<URLPattern> URLPattern::Constructor(
    const GlobalObject& aGlobal, const UTF8StringOrURLPatternInit& aInput,
    const URLPatternOptions& aOptions, ErrorResult& rv) {
  MOZ_LOG(gUrlPatternLog, LogLevel::Debug,
          ("UrlPattern::Constructor() (without base)"));
  UrlpPattern pattern{};
  UrlpOptions options{};
  options.ignore_case = aOptions.mIgnoreCase;
  if (!aInput.IsURLPatternInit()) {
    bool res = urlp_parse_pattern_from_string(&aInput.GetAsUTF8String(),
                                              nullptr, options, &pattern);
    if (!res) {
      rv.ThrowTypeError("Failed to create URLPattern (from string)");
      return nullptr;
    }
  } else {
    UrlpInit init{};
    URLPatternInit b_init;
    b_init = aInput.GetAsURLPatternInit();
    BindingToGlueInit(b_init, init);
    if (init.base_url.valid && init.base_url.string.Equals("")) {
      rv.ThrowTypeError("Should not provide empty base url with init");
      return nullptr;
    }
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    if (!res) {
      rv.ThrowTypeError("Failed to create URLPattern (from init)");
      return nullptr;
    }
  }

  return MakeAndAddRef<URLPattern>(aGlobal.GetAsSupports(), pattern,
                                   aOptions.mIgnoreCase);
}

// static
already_AddRefed<URLPattern> URLPattern::Constructor(
    const GlobalObject& aGlobal, const UTF8StringOrURLPatternInit& aInput,
    const nsACString& aBase, const URLPatternOptions& aOptions,
    ErrorResult& rv) {
  MOZ_LOG(gUrlPatternLog, LogLevel::Debug,
          ("UrlPattern::Constructor() (w base)"));
  UrlpPattern pattern{};
  UrlpOptions options{};
  options.ignore_case = aOptions.mIgnoreCase;
  if (!aInput.IsURLPatternInit()) {
    bool res = urlp_parse_pattern_from_string(&aInput.GetAsUTF8String(), &aBase,
                                              options, &pattern);
    if (!res) {
      rv.ThrowTypeError(
          "Failed to create URLPattern with base url (from string)");
      return nullptr;
    }
  } else {
    if (!aBase.IsEmpty()) {
      rv.ThrowTypeError("Should not provide base url with init");
      return nullptr;
    }
    UrlpInit init{};
    URLPatternInit b_init;
    b_init = aInput.GetAsURLPatternInit();
    BindingToGlueInit(b_init, init);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    if (!res) {
      rv.ThrowTypeError(
          "Failed to create URLPattern with base url (from init)");
      return nullptr;
    }
  }
  return MakeAndAddRef<URLPattern>(aGlobal.GetAsSupports(), pattern,
                                   aOptions.mIgnoreCase);
}

URLPattern::~URLPattern() { urlp_pattern_free(mPattern); }

void ConvertGroupsToRecord(
    const nsTHashMap<nsCStringHashKey, MaybeString>& aGroups,
    Optional<Record<nsCString, OwningUTF8StringOrUndefined>>& aRes) {
  Record<nsCString, OwningUTF8StringOrUndefined> record;
  for (auto iter = aGroups.ConstIter(); !iter.Done(); iter.Next()) {
    OwningUTF8StringOrUndefined value;
    value.SetUndefined();
    MaybeString s = iter.Data();
    if (s.valid) {
      value.SetAsUTF8String().Assign(s.string);
    }
    auto* entry = record.Entries().AppendElement().get();
    entry->mKey.Assign(iter.Key());
    entry->mValue = std::move(value);
  }
  aRes.Construct(std::move(record));
}

void GlueToBindingComponent(const net::UrlpComponentResult& aGlueCompRes,
                            URLPatternComponentResult& aBindingCompRes) {
  aBindingCompRes.mInput.Construct(aGlueCompRes.mInput);
  ConvertGroupsToRecord(aGlueCompRes.mGroups, aBindingCompRes.mGroups);
}

void ConvertInputsToSequence(
    const CopyableTArray<UrlpInput>& aInputs,
    Optional<Sequence<OwningUTF8StringOrURLPatternInit>>& aRes,
    ErrorResult& rv) {
  Sequence<OwningUTF8StringOrURLPatternInit> sequence;
  for (const auto& input : aInputs) {
    OwningUTF8StringOrURLPatternInit variant;
    if (input.string_or_init_type == UrlpStringOrInitType::String) {
      variant.SetAsUTF8String().Assign(input.str);
    } else {
      GlueToBindingInit(input.init, variant.SetAsURLPatternInit());
    }

    if (!sequence.AppendElement(std::move(variant), fallible)) {
      aRes.Reset();
      rv.ThrowOperationError("Failed to append inputs list to sequence");
      return;
    }
  }
  aRes.Construct(std::move(sequence));
}

void GlueToBindingResult(const net::UrlpResult& aGlueRes,
                         URLPatternResult& aBindingRes, ErrorResult& rv) {
  if (aGlueRes.mProtocol.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mProtocol.value(), tmp);
    aBindingRes.mProtocol.Construct(std::move(tmp));
  }
  if (aGlueRes.mUsername.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mUsername.value(), tmp);
    aBindingRes.mUsername.Construct(std::move(tmp));
  }
  if (aGlueRes.mPassword.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mPassword.value(), tmp);
    aBindingRes.mPassword.Construct(std::move(tmp));
  }
  if (aGlueRes.mHostname.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mHostname.value(), tmp);
    aBindingRes.mHostname.Construct(std::move(tmp));
  }
  if (aGlueRes.mPort.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mPort.value(), tmp);
    aBindingRes.mPort.Construct(std::move(tmp));
  }
  if (aGlueRes.mPathname.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mPathname.value(), tmp);
    aBindingRes.mPathname.Construct(std::move(tmp));
  }
  if (aGlueRes.mSearch.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mSearch.value(), tmp);
    aBindingRes.mSearch.Construct(std::move(tmp));
  }
  if (aGlueRes.mHash.isSome()) {
    URLPatternComponentResult tmp;
    GlueToBindingComponent(aGlueRes.mHash.value(), tmp);
    aBindingRes.mHash.Construct(std::move(tmp));
  }
  ConvertInputsToSequence(aGlueRes.mInputs, aBindingRes.mInputs, rv);
}

bool URLPattern::Test(const UTF8StringOrURLPatternInit& aInput,
                      const Optional<nsACString>& aBaseUrl, ErrorResult& rv) {
  MOZ_LOG(gUrlPatternLog, LogLevel::Debug, ("UrlPattern::Test()"));
  UrlpInput input;
  Maybe<nsAutoCString> execBaseUrl;
  if (aInput.IsURLPatternInit()) {
    UrlpInit initGlue{};
    BindingToGlueInit(aInput.GetAsURLPatternInit(), initGlue);
    input = net::CreateUrlpInput(initGlue);
    if (aBaseUrl.WasPassed()) {
      rv.ThrowTypeError(
          "Do not pass baseUrl separately with init, use init's baseURL "
          "property");
      return false;
    }
  } else {
    input = net::CreateUrlpInput(aInput.GetAsUTF8String());
    if (aBaseUrl.WasPassed()) {
      execBaseUrl.emplace(aBaseUrl.Value());
    }
  }
  return net::UrlpPatternTest(mPattern, input, execBaseUrl, mIgnoreCase);
}

void URLPattern::Exec(const UTF8StringOrURLPatternInit& aInput,
                      const Optional<nsACString>& aBaseUrl,
                      Nullable<URLPatternResult>& aResult, ErrorResult& rv) {
  MOZ_LOG(gUrlPatternLog, LogLevel::Debug, ("UrlPattern::Exec()"));
  UrlpInput input;
  Maybe<nsAutoCString> execBaseUrl;
  if (aInput.IsURLPatternInit()) {
    UrlpInit initGlue{};
    BindingToGlueInit(aInput.GetAsURLPatternInit(), initGlue);
    input = net::CreateUrlpInput(initGlue);
    if (aBaseUrl.WasPassed()) {
      rv.ThrowTypeError(
          "Do not pass baseUrl separately with init, use init's baseURL "
          "property");
      return;
    }
  } else {
    input = net::CreateUrlpInput(aInput.GetAsUTF8String());
    if (aBaseUrl.WasPassed()) {
      execBaseUrl.emplace(aBaseUrl.Value());
    }
  }

  Maybe<net::UrlpResult> patternResult =
      net::UrlpPatternExec(mPattern, input, execBaseUrl, mIgnoreCase);
  if (patternResult.isSome()) {
    URLPatternResult res;
    GlueToBindingResult(patternResult.value(), res, rv);
    if (rv.Failed()) {
      aResult.SetNull();
      return;
    }
    aResult.SetValue(std::move(res));
    return;
  }
  aResult.SetNull();
}

void URLPattern::GetProtocol(nsACString& aProtocol) const {
  aProtocol.Assign(net::UrlpGetProtocol(mPattern));
}

void URLPattern::GetUsername(nsACString& aUsername) const {
  aUsername.Assign(net::UrlpGetUsername(mPattern));
}

void URLPattern::GetPassword(nsACString& aPassword) const {
  aPassword.Assign(net::UrlpGetPassword(mPattern));
}

void URLPattern::GetHostname(nsACString& aHostname) const {
  aHostname.Assign(net::UrlpGetHostname(mPattern));
}

void URLPattern::GetPort(nsACString& aPort) const {
  aPort.Assign(net::UrlpGetPort(mPattern));
}

void URLPattern::GetPathname(nsACString& aPathname) const {
  aPathname.Assign(net::UrlpGetPathname(mPattern));
}

void URLPattern::GetSearch(nsACString& aSearch) const {
  aSearch.Assign(net::UrlpGetSearch(mPattern));
}

void URLPattern::GetHash(nsACString& aHash) const {
  aHash.Assign(net::UrlpGetHash(mPattern));
}

bool URLPattern::HasRegExpGroups() const {
  return urlp_get_has_regexp_groups(mPattern);
}

}  // namespace mozilla::dom
