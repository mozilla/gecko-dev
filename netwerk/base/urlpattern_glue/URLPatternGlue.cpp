/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/URLPatternGlue.h"

mozilla::LazyLogModule gUrlPatternLog("urlpattern");

namespace mozilla::net {

UrlpInput CreateUrlpInput(const nsACString& url) {
  UrlpInput input;
  input.string_or_init_type = UrlpStringOrInitType::String;
  input.str = url;
  return input;
}

UrlpInput CreateUrlpInput(const UrlpInit& init) {
  UrlpInput input;
  input.string_or_init_type = UrlpStringOrInitType::Init;
  input.init = init;
  return input;
}

MaybeString CreateMaybeString(const nsACString& str, bool valid) {
  return MaybeString{.string = nsCString(str), .valid = valid};
}

MaybeString CreateMaybeStringNone() {
  return MaybeString{.string = ""_ns, .valid = false};
}

nsAutoCString UrlpGetProtocol(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_protocol_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetUsername(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_username_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetPassword(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_password_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetHostname(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_hostname_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetPort(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_port_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetPathname(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_pathname_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetSearch(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_search_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

nsAutoCString UrlpGetHash(const UrlpPattern& aPattern) {
  UrlpComponent component;
  urlp_get_hash_component(aPattern, &component);
  return nsAutoCString(component.pattern_string);
}

// https://urlpattern.spec.whatwg.org/#create-a-component-match-result
Maybe<UrlpComponentResult> ComponentMatches(UrlpComponent& aComponent,
                                            nsACString& aInput,
                                            bool aIgnoreCase) {
  UrlpComponentResult res;
  if (aComponent.regexp_string == "^$") {  // empty string
    if (aInput != "") {
      return Nothing();
    }
  } else if (aComponent.regexp_string == "^(.*)$") {  // any string
    res.mGroups.InsertOrUpdate("0"_ns, CreateMaybeString(aInput, true));
  } else {  // check deeper match
    nsTArray<MaybeString> matches;
    if (!urlp_matcher_matches_component(&aComponent.matcher, &aInput,
                                        aIgnoreCase, &matches)) {
      return Nothing();
    }
    for (size_t i = 0; i < matches.Length(); i++) {
      nsAutoCString key;
      key.Assign(aComponent.group_name_list[i]);
      res.mGroups.InsertOrUpdate(key, matches[i]);
    }
  }
  res.mInput = aInput;
  return Some(res);
}

Maybe<UrlpResult> AllComponentMatches(const UrlpPattern& aPattern,
                                      UrlpMatchInput& aMatchInput,
                                      bool aIgnoreCase) {
  UrlpResult res;
  UrlpComponent componentProtocol{};
  urlp_get_protocol_component(aPattern, &componentProtocol);
  res.mProtocol =
      ComponentMatches(componentProtocol, aMatchInput.protocol, aIgnoreCase);
  if (res.mProtocol.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentUsername{};
  urlp_get_username_component(aPattern, &componentUsername);
  res.mUsername =
      ComponentMatches(componentUsername, aMatchInput.username, aIgnoreCase);
  if (res.mUsername.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentPassword{};
  urlp_get_password_component(aPattern, &componentPassword);
  res.mPassword =
      ComponentMatches(componentPassword, aMatchInput.password, aIgnoreCase);
  if (res.mPassword.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentHostname{};
  urlp_get_hostname_component(aPattern, &componentHostname);
  res.mHostname =
      ComponentMatches(componentHostname, aMatchInput.hostname, aIgnoreCase);
  if (res.mHostname.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentPort{};
  urlp_get_port_component(aPattern, &componentPort);
  res.mPort = ComponentMatches(componentPort, aMatchInput.port, aIgnoreCase);
  if (res.mPort.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentPathname{};
  urlp_get_pathname_component(aPattern, &componentPathname);
  res.mPathname =
      ComponentMatches(componentPathname, aMatchInput.pathname, aIgnoreCase);
  if (res.mPathname.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentSearch{};
  urlp_get_search_component(aPattern, &componentSearch);
  res.mSearch =
      ComponentMatches(componentSearch, aMatchInput.search, aIgnoreCase);
  if (res.mSearch.isNothing()) {
    return Nothing();
  }

  UrlpComponent componentHash{};
  urlp_get_hash_component(aPattern, &componentHash);
  res.mHash = ComponentMatches(componentHash, aMatchInput.hash, aIgnoreCase);
  if (res.mHash.isNothing()) {
    return Nothing();
  }

  return Some(res);
}

Maybe<UrlpResult> UrlpPatternExec(UrlpPattern aPattern, const UrlpInput& aInput,
                                  Maybe<nsAutoCString> aMaybeBaseUrl,
                                  bool aIgnoreCase) {
  MOZ_LOG(gUrlPatternLog, LogLevel::Debug, ("UrlpPatternExec()...\n"));
  UrlpMatchInputAndInputs matchInputAndInputs;
  if (aInput.string_or_init_type == UrlpStringOrInitType::Init) {
    MOZ_ASSERT(aMaybeBaseUrl.isNothing());
    if (!urlp_process_match_input_from_init(&aInput.init, nullptr,
                                            &matchInputAndInputs)) {
      return Nothing();
    }
  } else {
    nsAutoCString* baseUrl = nullptr;
    if (aMaybeBaseUrl.isSome()) {
      baseUrl = &aMaybeBaseUrl.ref();
    }
    if (!urlp_process_match_input_from_string(&aInput.str, baseUrl,
                                              &matchInputAndInputs)) {
      return Nothing();
    }
  }

  // shouldn't be any need to convert the urlpatternwrapper to quirks wrapper
  // the all_component_matches signature should be able to receive as a wrapper
  Maybe<UrlpResult> res =
      AllComponentMatches(aPattern, matchInputAndInputs.input, aIgnoreCase);
  if (res.isNothing()) {
    return Nothing();
  }

  if (matchInputAndInputs.inputs.string_or_init_type ==
      UrlpStringOrInitType::Init) {
    res->mInputs.AppendElement(
        CreateUrlpInput(matchInputAndInputs.inputs.init));
  } else {
    res->mInputs.AppendElement(CreateUrlpInput(matchInputAndInputs.inputs.str));
    if (matchInputAndInputs.inputs.base.valid) {
      res->mInputs.AppendElement(
          CreateUrlpInput(matchInputAndInputs.inputs.base.string));
    }
  }

  return res;
}

bool UrlpPatternTest(UrlpPattern aPattern, const UrlpInput& aInput,
                     Maybe<nsAutoCString> aMaybeBaseUrl, bool aIgnoreCase) {
  MOZ_LOG(gUrlPatternLog, LogLevel::Debug, ("UrlpPatternTest()...\n"));
  UrlpMatchInputAndInputs matchInputAndInputs;
  if (aInput.string_or_init_type == UrlpStringOrInitType::Init) {
    MOZ_ASSERT(aMaybeBaseUrl.isNothing());
    if (!urlp_process_match_input_from_init(&aInput.init, nullptr,
                                            &matchInputAndInputs)) {
      return false;
    }
  } else {
    nsAutoCString* baseUrl = nullptr;
    if (aMaybeBaseUrl.isSome()) {
      baseUrl = &aMaybeBaseUrl.ref();
    }
    if (!urlp_process_match_input_from_string(&aInput.str, baseUrl,
                                              &matchInputAndInputs)) {
      return false;
    }
  }

  // shouldn't be any need to convert the urlpatternwrapper to quirks wrapper
  // the all_component_matches signature should be able to receive as a wrapper
  Maybe<UrlpResult> res =
      AllComponentMatches(aPattern, matchInputAndInputs.input, aIgnoreCase);
  return !res.isNothing();
}

}  // namespace mozilla::net
