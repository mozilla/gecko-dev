/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoNetworkManager_h
#define GeckoNetworkManager_h

#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsINetworkLinkService.h"
#include "nsISystemProxySettings.h"

#include "mozilla/java/GeckoNetworkManagerNatives.h"
#include "mozilla/Services.h"

namespace mozilla {

class GeckoNetworkManager final
    : public java::GeckoNetworkManager::Natives<GeckoNetworkManager> {
  GeckoNetworkManager() = delete;

 public:
  static void OnConnectionChanged(int32_t aType, jni::String::Param aSubType,
                                  bool aIsWifi, int32_t aGateway) {
    hal::NotifyNetworkChange(hal::NetworkInformation(aType, aIsWifi, aGateway));

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, NS_NETWORK_LINK_TYPE_TOPIC,
                          aSubType->ToString().get());
    }
  }

  static void OnStatusChanged(jni::String::Param aStatus) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, NS_NETWORK_LINK_TOPIC,
                          aStatus->ToString().get());
    }
  }

  static void OnProxyChanged(jni::String::Param aHost, int32_t aPort,
                             jni::String::Param aPacFileUrl,
                             jni::ObjectArray::Param aExclusionList) {
    nsCOMPtr<nsISystemProxySettings> sp =
        do_GetService(NS_SYSTEMPROXYSETTINGS_CONTRACTID);
    if (!sp) {
      return;
    }
    nsAutoCString host = NS_ConvertUTF16toUTF8(aHost->ToString().get());
    nsAutoCString pacFileUrl =
        NS_ConvertUTF16toUTF8(aPacFileUrl->ToString().get());
    int size = aExclusionList->Length();
    JNIEnv* env = jni::GetEnvForThread();
    nsTArray<nsCString> exclusionList;
    for (int32_t i = 0; i < size; i++) {
      jstring javaString =
          (jstring)(env->GetObjectArrayElement(aExclusionList.Get(), i));
      const char* rawString = env->GetStringUTFChars(javaString, 0);
      exclusionList.AppendElement(nsCString(rawString));
      env->ReleaseStringUTFChars(javaString, rawString);
    }

    sp->SetSystemProxyInfo(host, aPort, pacFileUrl, exclusionList);
  }
};

}  // namespace mozilla

#endif  // GeckoNetworkManager_h
