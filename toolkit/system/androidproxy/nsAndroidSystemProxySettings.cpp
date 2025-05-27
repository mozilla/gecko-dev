/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "mozilla/Maybe.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "ProxyUtils.h"

#include "AndroidBridge.h"

class SystemProxyConfig final {
 public:
  explicit SystemProxyConfig(const nsACString& aHost, int32_t aPort,
                             const nsACString& aPacFileUrl,
                             const nsTArray<nsCString>& aExclusionList)
      : mHost(aHost),
        mPort(aPort),
        mPacUrl(aPacFileUrl),
        mExclusionList(aExclusionList) {}

  nsresult GetProxyForURI(const nsACString& aHost, nsACString& aResult);
  nsresult GetPACURI(nsACString& aResult);

 private:
  nsCString mHost;
  int32_t mPort = 0;
  nsCString mPacUrl;
  CopyableTArray<nsCString> mExclusionList;

  bool IsInExceptionList(const nsACString& aHost);
};

nsresult SystemProxyConfig::GetProxyForURI(const nsACString& aHost,
                                           nsACString& aResult) {
  if (mHost.IsEmpty() || mPort <= 0) {
    aResult.AssignLiteral("DIRECT");
  } else if (IsInExceptionList(aHost)) {
    aResult.AssignLiteral("DIRECT");
  } else {
    aResult.Assign("PROXY "_ns + mHost + nsPrintfCString(":%d", mPort));
  }

  return NS_OK;
}

nsresult SystemProxyConfig::GetPACURI(nsACString& aResult) {
  aResult.Assign(mPacUrl);
  return NS_OK;
}

bool SystemProxyConfig::IsInExceptionList(const nsACString& aHost) {
  for (const auto& item : mExclusionList) {
    if (mozilla::toolkit::system::IsHostProxyEntry(aHost, item)) {
      return true;
    }
  }

  return false;
}

class nsAndroidSystemProxySettings : public nsISystemProxySettings {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsAndroidSystemProxySettings() {};

 private:
  virtual ~nsAndroidSystemProxySettings() {}

  mozilla::Maybe<SystemProxyConfig> mSystemProxyConfig;
};

NS_IMPL_ISUPPORTS(nsAndroidSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsAndroidSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  *aMainThreadOnly = true;
  return NS_OK;
}

nsresult nsAndroidSystemProxySettings::GetPACURI(nsACString& aResult) {
  if (mSystemProxyConfig) {
    return mSystemProxyConfig->GetPACURI(aResult);
  }

  return NS_OK;
}

nsresult nsAndroidSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                      const nsACString& aScheme,
                                                      const nsACString& aHost,
                                                      const int32_t aPort,
                                                      nsACString& aResult) {
  if (mSystemProxyConfig && (aScheme.LowerCaseEqualsASCII("http") ||
                             aScheme.LowerCaseEqualsASCII("https"))) {
    return mSystemProxyConfig->GetProxyForURI(aHost, aResult);
  }

  return mozilla::AndroidBridge::Bridge()->GetProxyForURI(aSpec, aScheme, aHost,
                                                          aPort, aResult);
}

NS_IMETHODIMP
nsAndroidSystemProxySettings::GetSystemWPADSetting(bool* aSystemWPADSetting) {
  *aSystemWPADSetting = false;
  return NS_OK;
}

NS_IMETHODIMP nsAndroidSystemProxySettings::SetSystemProxyInfo(
    const nsACString& aHost, int32_t aPort, const nsACString& aPacFileUrl,
    const nsTArray<nsCString>& aExclusionList) {
  mSystemProxyConfig = mozilla::Some(
      SystemProxyConfig(aHost, aPort, aPacFileUrl, aExclusionList));
  return NS_OK;
}

NS_IMPL_COMPONENT_FACTORY(nsAndroidSystemProxySettings) {
  return mozilla::MakeAndAddRef<nsAndroidSystemProxySettings>()
      .downcast<nsISupports>();
}
