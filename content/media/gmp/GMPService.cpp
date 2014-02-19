/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPService.h"
#include "GMPVideoDecoderParent.h"
#include "nsIObserverService.h"
#if defined(XP_WIN)
#include "nsIWindowsRegKey.h"
#endif

namespace mozilla {
namespace gmp {

NS_IMPL_ISUPPORTS2(GeckoMediaPluginService, mozIGeckoMediaPluginService, nsIObserver)

GeckoMediaPluginService::GeckoMediaPluginService()
{
  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  if (obsService) {
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }
}

GeckoMediaPluginService::~GeckoMediaPluginService()
{
}

NS_IMETHODIMP
GeckoMediaPluginService::Observe(nsISupports *aSubject,
                                 const char *aTopic,
                                 const char16_t *someData)
{
  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    UnloadPlugins();
  }
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoDecoderVP8(GMPVideoHost** outVideoHost, GMPVideoDecoder** gmpVD)
{
  *gmpVD = nullptr;

  nsCOMPtr<GMPParent> gmp = SelectPluginForAPI(NS_LITERAL_CSTRING("decode-video"),
                                               NS_LITERAL_CSTRING("vp8"));
  if (!gmp) {
    return NS_ERROR_FAILURE;
  }

  GMPVideoDecoderParent* gmpVDP;
  nsresult rv = gmp->GetGMPVideoDecoder(&gmpVDP);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  *gmpVD = gmpVDP;
  *outVideoHost = &gmpVDP->Host();

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoEncoderVP8(GMPVideoHost** outVideoHost, GMPVideoEncoder** gmpVE)
{
  *gmpVE = nullptr;

  nsCOMPtr<GMPParent> gmp = SelectPluginForAPI(NS_LITERAL_CSTRING("encode-video"),
                                               NS_LITERAL_CSTRING("vp8"));
  if (!gmp) {
    return NS_ERROR_FAILURE;
  }

  GMPVideoEncoderParent* gmpVEP;
  nsresult rv = gmp->GetGMPVideoEncoder(&gmpVEP);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  *gmpVE = gmpVEP;
  *outVideoHost = &gmpVEP->Host();

  return NS_OK;
}

void
GeckoMediaPluginService::UnloadPlugins()
{
  for (uint32_t i = 0; i < mPlugins.Length(); i++) {
    mPlugins[i]->UnloadProcess();
  }
}

GMPParent* GeckoMediaPluginService::SelectPluginForAPI(const nsCString& api,
                                                       const nsCString& tag)
{
  GMPParent *gmp = SelectPluginFromListForAPI(api, tag);
  if (gmp) {
    return gmp;
  }

  RefreshPluginList();

  return SelectPluginFromListForAPI(api, tag);
}

GMPParent*
GeckoMediaPluginService::SelectPluginFromListForAPI(const nsCString& api,
                                                    const nsCString& tag)
{
  for (uint32_t i = 0; i < mPlugins.Length(); i++) {
    GMPParent *gmp = mPlugins[i];
    if (gmp->SupportsAPI(api, tag)) {
      return gmp;
    }
  }
  return nullptr;
}

nsresult
GeckoMediaPluginService::GetDirectoriesToSearch(nsTArray<nsCOMPtr<nsIFile>> &aDirs)
{
#if defined(XP_MACOSX)
  nsCOMPtr<nsIFile> searchDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  nsresult rv = searchDir->InitWithPath(NS_LITERAL_STRING("~/Library/Internet Plug-Ins/"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  aDirs.AppendElement(searchDir);
  searchDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  rv = searchDir->InitWithPath(NS_LITERAL_STRING("/Library/Internet Plug-Ins/"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  aDirs.AppendElement(searchDir);
#elif defined(OS_POSIX)
  nsCOMPtr<nsIFile> searchDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  nsresult rv = searchDir->InitWithPath(NS_LITERAL_STRING("/usr/lib/mozilla/plugins/"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  aDirs.AppendElement(searchDir);
#endif
  return NS_OK;
}

#if defined(XP_WIN)
static nsresult
GetPossiblePluginsForRegRoot(uint32_t aKey, nsTArray<nsCOMPtr<nsIFile>> &aDirs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey = do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = regKey->Open(aKey,
                             NS_LITERAL_STRING("Software\\MozillaPlugins"),
                             nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t childCount = 0;
  regKey->GetChildCount(&childCount);
  for (uint32_t index = 0; index < childCount; index++) {
    nsAutoString childName;
    rv = regKey->GetChildName(index, childName);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIWindowsRegKey> childKey;
      rv = regKey->OpenChild(childName, nsIWindowsRegKey::ACCESS_QUERY_VALUE,
                             getter_AddRefs(childKey));
      if (NS_SUCCEEDED(rv) && childKey) {
        nsAutoString path;
        rv = childKey->ReadStringValue(NS_LITERAL_STRING("Path"), path);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIFile> localFile;
          if (NS_SUCCEEDED(NS_NewLocalFile(path, true, getter_AddRefs(localFile))) &&
              localFile) {
            bool isFileThere = false;
            if (NS_SUCCEEDED(localFile->Exists(&isFileThere)) && isFileThere) {
              aDirs.AppendElement(localFile);
            }
          }
        }
      }
    }
  }

  regKey->Close();

  return NS_OK;
}
#endif

nsresult
GeckoMediaPluginService::GetPossiblePlugins(nsTArray<nsCOMPtr<nsIFile>> &aDirs)
{
#if defined(XP_WIN)
  // The ROOT_KEY_CURRENT_USER entry typically fails to open, causing this call to
  // fail. Don't check any return values because if we find nothing we don't care.
  GetPossiblePluginsForRegRoot(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, aDirs);
  GetPossiblePluginsForRegRoot(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, aDirs);
#endif
  return NS_OK;
}

nsresult
GeckoMediaPluginService::SearchDirectory(nsIFile* aSearchDir)
{
  if (!aSearchDir) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISimpleEnumerator> iter;
  nsresult rv = aSearchDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool hasMore;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) {
      continue;
    }
    nsCOMPtr<nsIFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv)) {
      continue;
    }
    ProcessPossiblePlugin(dirEntry);
  }

  return NS_OK;
}

void
GeckoMediaPluginService::ProcessPossiblePlugin(nsIFile* aDir)
{
  if (!aDir) {
    return;
  }

  bool isDirectory = false;
  nsresult rv = aDir->IsDirectory(&isDirectory);
  if (NS_FAILED(rv) || !isDirectory) {
    return;
  }

  nsAutoString leafName;
  rv = aDir->GetLeafName(leafName);
  if (NS_FAILED(rv)) {
    return;
  }

  nsString prefix = NS_LITERAL_STRING("gmp-");
  if (leafName.Length() <= prefix.Length() ||
      !Substring(leafName, 0, prefix.Length()).Equals(prefix)) {
    return;
  }

  nsRefPtr<GMPParent> gmp = new GMPParent();
  if (NS_FAILED(gmp->Init(aDir))) {
    return;
  }

  mPlugins.AppendElement(gmp);
}

void
GeckoMediaPluginService::RefreshPluginList()
{
  nsTArray<nsCOMPtr<nsIFile>> searchDirs;
  nsresult rv = GetDirectoriesToSearch(searchDirs);
  if (NS_FAILED(rv)) {
    return;
  }

  for (uint32_t i = 0; i < searchDirs.Length(); i++) {
    SearchDirectory(searchDirs[i]);
  }

  nsTArray<nsCOMPtr<nsIFile>> possiblePlugins;
  rv = GetPossiblePlugins(possiblePlugins);
  if (NS_FAILED(rv)) {
    return;
  }

  for (uint32_t i = 0; i < possiblePlugins.Length(); i++) {
    ProcessPossiblePlugin(possiblePlugins[i]);
  }
}

} // namespace gmp
} // namespace mozilla
