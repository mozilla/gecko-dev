/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_identitycredentialserializationhelpers_h__
#define mozilla_dom_identitycredentialserializationhelpers_h__

#include "mozilla/dom/BindingIPCUtils.h"
#include "mozilla/dom/IdentityCredential.h"
#include "mozilla/dom/IdentityCredentialBinding.h"
#include "mozilla/dom/CredentialManagementBinding.h"
#include "mozilla/dom/LoginStatusBinding.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::IdentityProviderConfig> {
  typedef mozilla::dom::IdentityProviderConfig paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mConfigURL);
    WriteParam(aWriter, aParam.mClientId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mConfigURL) &&
           ReadParam(aReader, &aResult->mClientId);
  }
};

template <>
struct ParamTraits<mozilla::dom::IdentityProviderRequestOptions> {
  typedef mozilla::dom::IdentityProviderRequestOptions paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mConfigURL);
    WriteParam(aWriter, aParam.mClientId);
    WriteParam(aWriter, aParam.mNonce);
    WriteParam(aWriter, aParam.mLoginHint);
    WriteParam(aWriter, aParam.mDomainHint);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mConfigURL) &&
           ReadParam(aReader, &aResult->mClientId) &&
           ReadParam(aReader, &aResult->mNonce) &&
           ReadParam(aReader, &aResult->mLoginHint) &&
           ReadParam(aReader, &aResult->mDomainHint);
  }
};

template <>
struct ParamTraits<mozilla::dom::IdentityCredentialDisconnectOptions> {
  typedef mozilla::dom::IdentityCredentialDisconnectOptions paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mConfigURL);
    WriteParam(aWriter, aParam.mClientId);
    WriteParam(aWriter, aParam.mAccountHint);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mConfigURL) &&
           ReadParam(aReader, &aResult->mClientId) &&
           ReadParam(aReader, &aResult->mAccountHint);
  }
};

template <>
struct ParamTraits<mozilla::dom::CredentialMediationRequirement>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::CredentialMediationRequirement> {};

template <>
struct ParamTraits<mozilla::dom::IdentityCredentialRequestOptions> {
  typedef mozilla::dom::IdentityCredentialRequestOptions paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mProviders);
    WriteParam(aWriter, aParam.mMode);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mProviders) &&
           ReadParam(aReader, &aResult->mMode);
  }
};

template <>
struct ParamTraits<mozilla::dom::LoginStatus>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::LoginStatus> {};

template <>
struct ParamTraits<mozilla::dom::IdentityCredentialRequestOptionsMode>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::IdentityCredentialRequestOptionsMode> {};

}  // namespace IPC

#endif  // mozilla_dom_identitycredentialserializationhelpers_h__
