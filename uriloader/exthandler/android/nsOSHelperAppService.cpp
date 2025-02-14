/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSHelperAppService.h"

#include "mozilla/java/GeckoAppShellWrappers.h"
#include "nsMIMEInfoAndroid.h"

using namespace mozilla;

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService() {}

nsOSHelperAppService::~nsOSHelperAppService() {}

nsresult nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                                 const nsACString& aFileExt,
                                                 bool* aFound,
                                                 nsIMIMEInfo** aMIMEInfo) {
  RefPtr<nsMIMEInfoAndroid> mimeInfo;
  *aFound = false;

  if (jni::IsAvailable()) {
    if (!aMIMEType.IsEmpty()) {
      nsCString fileExt;
      auto fileExtJavaStr =
          java::GeckoAppShell::GetExtensionFromMimeType(aMIMEType);
      if (fileExtJavaStr) {
        fileExt = fileExtJavaStr->ToCString();
      }

      if (!fileExt.IsEmpty()) {
        mimeInfo = new nsMIMEInfoAndroid(aMIMEType);
        mimeInfo->SetPrimaryExtension(fileExt);
        *aFound = true;
      }
    }

    if (!*aFound) {
      nsCString mimeType;
      auto javaString =
          java::GeckoAppShell::GetMimeTypeFromExtensions(aFileExt);
      if (javaString) {
        mimeType = javaString->ToCString();
      }

      // "*/*" means that the MimeTypeMap didn't know.
      if (!mimeType.IsEmpty() &&
          !mimeType.Equals(nsDependentCString("*/*"),
                           nsCaseInsensitiveCStringComparator)) {
        mimeInfo = new nsMIMEInfoAndroid(mimeType);
        mimeInfo->SetPrimaryExtension(aFileExt);
        *aFound = true;
      }
    }
  }

  // Code that calls this requires an object regardless if the OS has
  // something for us, so we return the empty object.
  if (!*aFound) {
    mimeInfo = new nsMIMEInfoAndroid(aMIMEType);
  }

  mimeInfo.forget(aMIMEInfo);
  return NS_OK;
}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char* aScheme,
                                                       bool* aExists) {
  // Support any URI barring a couple schemes we use in testing; let the
  // app decide what to do with them.
  nsAutoCString scheme(aScheme);
  *aExists =
      !scheme.Equals("unsupported"_ns) && !scheme.Equals("unknownextproto"_ns);
  return NS_OK;
}

NS_IMETHODIMP
nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme,
                                                nsAString& _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsOSHelperAppService::IsCurrentAppOSDefaultForProtocol(
    const nsACString& aScheme, bool* _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsOSHelperAppService::GetProtocolHandlerInfoFromOS(
    const nsACString& aScheme, bool* found, nsIHandlerInfo** info) {
  // We don't want to get protocol handlers from the OS in GV; the app
  // should take care of that in NavigationDelegate.onLoadRequest().
  return NS_ERROR_NOT_IMPLEMENTED;
}
