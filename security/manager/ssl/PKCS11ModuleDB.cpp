/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PKCS11ModuleDB.h"

#include "CertVerifier.h"
#include "ScopedNSSTypes.h"
#include "mozilla/glean/SecurityManagerSslMetrics.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsNSSCertHelper.h"
#include "nsNSSComponent.h"
#include "nsNativeCharsetUtils.h"
#include "nsPKCS11Slot.h"
#include "nsServiceManagerUtils.h"

#if defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#  include "nsIFile.h"
#endif  // defined(XP_MACOSX)

namespace mozilla {
namespace psm {

NS_IMPL_ISUPPORTS(PKCS11ModuleDB, nsIPKCS11ModuleDB)

// Convert the UTF16 name of the module as it appears to the user to the
// internal representation. For most modules this just involves converting from
// UTF16 to UTF8. For the builtin root module, it also involves mapping from the
// localized name to the internal, non-localized name.
static nsresult NormalizeModuleNameIn(const nsAString& moduleNameIn,
                                      nsCString& moduleNameOut) {
  nsAutoString localizedRootModuleName;
  nsresult rv =
      GetPIPNSSBundleString("RootCertModuleName", localizedRootModuleName);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (moduleNameIn.Equals(localizedRootModuleName)) {
    moduleNameOut.Assign(kRootModuleName.get());
    return NS_OK;
  }
  moduleNameOut.Assign(NS_ConvertUTF16toUTF8(moduleNameIn));
  return NS_OK;
}

// Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
PKCS11ModuleDB::DeleteModule(const nsAString& aModuleName) {
  if (aModuleName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString moduleNameNormalized;
  nsresult rv = NormalizeModuleNameIn(aModuleName, moduleNameNormalized);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // modType is an output variable. We ignore it.
  int32_t modType;
  SECStatus srv = SECMOD_DeleteModule(moduleNameNormalized.get(), &modType);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  if (!certVerifier) {
    return NS_ERROR_FAILURE;
  }
  certVerifier->ClearTrustCache();

  CollectThirdPartyPKCS11ModuleTelemetry();

  return NS_OK;
}

#if defined(XP_MACOSX)
// Given a path to a module, return the filename in `aFilename`.
nsresult ModulePathToFilename(const nsCString& aModulePath,
                              nsCString& aFilename) {
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_NewLocalFile(NS_ConvertUTF8toUTF16(aModulePath), getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString filename;
  rv = file->GetLeafName(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  aFilename = NS_ConvertUTF16toUTF8(filename);
  return NS_OK;
}

// Collect the signature type and filename of a third-party PKCS11 module to
// inform future decisions about module loading restrictions on macOS.
void CollectThirdPartyModuleSignatureType(const nsCString& aModulePath) {
  using mozilla::glean::pkcs11::third_party_module_signature_type;
  using mozilla::glean::pkcs11::ThirdPartyModuleSignatureTypeExtra;
  using nsMacUtilsImpl::CodeSignatureTypeToString;

  nsMacUtilsImpl::CodeSignatureType signatureType =
      nsMacUtilsImpl::GetSignatureType(aModulePath);

  nsCString filename;
  nsresult rv = ModulePathToFilename(aModulePath, filename);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCString signatureTypeStr(CodeSignatureTypeToString(signatureType));
  third_party_module_signature_type.Record(
      Some(ThirdPartyModuleSignatureTypeExtra{
          Some(filename),
          Some(signatureTypeStr),
      }));
}

// Collect the filename of a third-party PKCS11 module to inform future
// decisions about module loading restrictions on macOS.
void CollectThirdPartyModuleFilename(const nsCString& aModulePath) {
  using mozilla::glean::pkcs11::third_party_module_profile_entries;
  nsCString filename;
  nsresult rv = ModulePathToFilename(aModulePath, filename);
  NS_ENSURE_SUCCESS_VOID(rv);
  third_party_module_profile_entries.Add(filename);
}
#endif  // defined(XP_MACOSX)

// Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
PKCS11ModuleDB::AddModule(const nsAString& aModuleName,
                          const nsAString& aLibraryFullPath,
                          int32_t aCryptoMechanismFlags, int32_t aCipherFlags) {
  if (aModuleName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // "Root Certs" is the name some NSS command-line utilities will give the
  // roots module if they decide to load it when there happens to be a
  // `MOZ_DLL_PREFIX "nssckbi" MOZ_DLL_SUFFIX` file in the directory being
  // operated on.  This causes failures, so as a workaround, the PSM
  // initialization code will unconditionally remove any module named "Root
  // Certs". We should prevent the user from adding an unrelated module named
  // "Root Certs" in the first place so PSM doesn't delete it. See bug 1406396.
  if (aModuleName.EqualsLiteral("Root Certs")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // There appears to be a deadlock if we try to load modules concurrently, so
  // just wait until the loadable roots module has been loaded.
  nsresult rv = BlockUntilLoadableCertsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString moduleNameNormalized;
  rv = NormalizeModuleNameIn(aModuleName, moduleNameNormalized);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCString fullPath;
  CopyUTF16toUTF8(aLibraryFullPath, fullPath);
  uint32_t mechFlags = SECMOD_PubMechFlagstoInternal(aCryptoMechanismFlags);
  uint32_t cipherFlags = SECMOD_PubCipherFlagstoInternal(aCipherFlags);
  SECStatus srv = SECMOD_AddNewModule(moduleNameNormalized.get(),
                                      fullPath.get(), mechFlags, cipherFlags);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  if (!certVerifier) {
    return NS_ERROR_FAILURE;
  }
  certVerifier->ClearTrustCache();

#if defined(XP_MACOSX)
  CollectThirdPartyModuleSignatureType(fullPath);
#endif  // defined(XP_MACOSX)

  CollectThirdPartyPKCS11ModuleTelemetry();

  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::ListModules(nsISimpleEnumerator** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = BlockUntilLoadableCertsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  /* lock down the list for reading */
  AutoSECMODListReadLock lock;
  for (SECMODModuleList* list = SECMOD_GetDefaultModuleList(); list;
       list = list->next) {
    nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(list->module);
    nsresult rv = array->AppendElement(module);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  /* Get the modules in the database that didn't load */
  for (SECMODModuleList* list = SECMOD_GetDeadModuleList(); list;
       list = list->next) {
    nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(list->module);
    nsresult rv = array->AppendElement(module);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return array->Enumerate(_retval, NS_GET_IID(nsIPKCS11Module));
}

NS_IMETHODIMP
PKCS11ModuleDB::GetCanToggleFIPS(bool* aCanToggleFIPS) {
  NS_ENSURE_ARG_POINTER(aCanToggleFIPS);

  *aCanToggleFIPS = SECMOD_CanDeleteInternalModule();
  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::ToggleFIPSMode() {
  // The way to toggle FIPS mode in NSS is extremely obscure. Basically, we
  // delete the internal module, and it gets replaced with the opposite module
  // (i.e. if it was FIPS before, then it becomes non-FIPS next).
  // SECMOD_GetInternalModule() returns a pointer to a local copy of the
  // internal module stashed in NSS.  We don't want to delete it since it will
  // cause much pain in NSS.
  SECMODModule* internal = SECMOD_GetInternalModule();
  if (!internal) {
    return NS_ERROR_FAILURE;
  }

  if (SECMOD_DeleteInternalModule(internal->commonName) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::GetIsFIPSEnabled(bool* aIsFIPSEnabled) {
  NS_ENSURE_ARG_POINTER(aIsFIPSEnabled);

  *aIsFIPSEnabled = PK11_IsFIPS();
  return NS_OK;
}

const nsLiteralCString kBuiltInModuleNames[] = {
    kNSSInternalModuleName,
    kRootModuleName,
    kOSClientCertsModuleName,
    kIPCClientCertsModuleName,
};

void CollectThirdPartyPKCS11ModuleTelemetry(bool aIsInitialization) {
  size_t thirdPartyModulesLoaded = 0;
  AutoSECMODListReadLock lock;
  for (SECMODModuleList* list = SECMOD_GetDefaultModuleList(); list;
       list = list->next) {
    bool isThirdParty = true;
    for (const auto& builtInModuleName : kBuiltInModuleNames) {
      if (builtInModuleName.Equals(list->module->commonName)) {
        isThirdParty = false;
        break;
      }
    }
    if (isThirdParty) {
      thirdPartyModulesLoaded++;
#if defined(XP_MACOSX)
      // Collect third party module filenames once per launch.
      // We collect signature type when adding a module. It would be wasteful
      // and duplicative to collect signature information on each launch given
      // that it requires file I/O. Combining the filename of modules collected
      // here with signature type and filename collected when adding a module
      // provides information about existing modules already in use and new
      // modules. No I/O is required to obtain the filename given the path on
      // macOS, but defer it to idle-time to avoid adding more work at startup.
      if (aIsInitialization) {
        nsCString modulePath(list->module->dllName);
        NS_DispatchToMainThreadQueue(
            NS_NewRunnableFunction("CollectThirdPartyModuleFilenameIdle",
                                   [modulePath]() {
                                     CollectThirdPartyModuleFilename(
                                         modulePath);
                                   }),
            EventQueuePriority::Idle);
      }
#endif  // defined(XP_MACOSX)
    }
  }
  mozilla::glean::pkcs11::third_party_modules_loaded.Set(
      thirdPartyModulesLoaded);
}

}  // namespace psm
}  // namespace mozilla
