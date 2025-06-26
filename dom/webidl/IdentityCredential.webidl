/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c-fedid.github.io/FedCM.
 */

 // https://w3c-fedid.github.io/FedCM/#browser-api-identity-credential-interface
[Exposed=Window, SecureContext,
 Pref="dom.security.credentialmanagement.identity.enabled"]
interface IdentityCredential : Credential {
 [Throws]
 static Promise<undefined> disconnect(optional IdentityCredentialDisconnectOptions options = {});
 readonly attribute UTF8String? token;
};

enum IdentityCredentialRequestOptionsMode {
  "active",
  "passive"
};

dictionary IdentityCredentialRequestOptions {
 required sequence<IdentityProviderRequestOptions> providers;
 IdentityCredentialRequestOptionsMode mode = "passive";
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityproviderconfig
[GenerateConversionToJS]
dictionary IdentityProviderConfig {
 required UTF8String configURL;
 required UTF8String clientId;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityproviderrequestoptions
[GenerateConversionToJS]
dictionary IdentityProviderRequestOptions : IdentityProviderConfig {
  UTF8String nonce;
  UTF8String loginHint;
  UTF8String domainHint;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identitycredentialdisconnectoptions
dictionary IdentityCredentialDisconnectOptions : IdentityProviderConfig  {
  required UTF8String accountHint;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityproviderwellknown
[GenerateInit]
dictionary IdentityProviderWellKnown {
  required sequence<UTF8String> provider_urls;
  UTF8String accounts_endpoint;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityprovidericon
dictionary IdentityProviderIcon {
  required UTF8String url;
  unsigned long size;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityproviderbranding
dictionary IdentityProviderBranding {
  USVString background_color;
  USVString color;
  sequence<IdentityProviderIcon> icons;
  USVString name;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityproviderapiconfig
[GenerateInit, GenerateConversionToJS]
dictionary IdentityProviderAPIConfig {
  required UTF8String accounts_endpoint;
  // We do not want to gather consent for identity providers, so we
  // omit this requirement and its use: https://github.com/w3c-fedid/FedCM/issues/703
  // required UTF8String client_metadata_endpoint;
  required UTF8String id_assertion_endpoint;
  UTF8String disconnect_endpoint;
  IdentityProviderBranding branding;
  UTF8String account_label;
};


// https://w3c-fedid.github.io/FedCM/#dictdef-identityprovideraccount
dictionary IdentityProviderAccount {
  required USVString id;
  required USVString name;
  required USVString email;
  USVString given_name;
  USVString picture;
  sequence<USVString> approved_clients;
  sequence<UTF8String> login_hints;
  sequence<UTF8String> domain_hints;
  sequence<UTF8String> label_hints;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityprovideraccountlist
[GenerateInit, GenerateConversionToJS]
dictionary IdentityProviderAccountList {
  sequence<IdentityProviderAccount> accounts;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityproviderclientmetadata
[GenerateInit, GenerateConversionToJS]
dictionary IdentityProviderClientMetadata {
  USVString privacy_policy_url;
  USVString terms_of_service_url;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityassertionresponse
[GenerateInit]
dictionary IdentityAssertionResponse {
  UTF8String token;
  UTF8String continue_on;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-disconnectedaccount
[GenerateInit]
dictionary DisconnectedAccount {
  required UTF8String account_id;
};

// https://w3c-fedid.github.io/FedCM/#dictdef-identityresolveoptions
dictionary IdentityResolveOptions {
  UTF8String accountId;
};

 // https://w3c-fedid.github.io/FedCM/#identityprovider
[Exposed=Window, SecureContext,
 Pref="dom.security.credentialmanagement.identity.enabled"]
interface IdentityProvider {
    static undefined close();
    [Throws]
    static Promise<undefined> resolve(UTF8String token, optional IdentityResolveOptions options = {});
    // static Promise<sequence<IdentityUserInfo>> getUserInfo(IdentityProviderConfig config); Bug 1945589: Not yet implemented
};
