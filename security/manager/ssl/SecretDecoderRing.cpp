/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SecretDecoderRing.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/Services.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsITokenPasswordDialogs.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsPK11TokenDB.h"
#include "pk11func.h"

// temporary includes for key3.db cleanup
#include "nsAppDirectoryServiceDefs.h"
#include "pk11pub.h"

#include "pk11sdr.h"  // For PK11SDR_Encrypt, PK11SDR_Decrypt
#include "ssl.h"      // For SSL_ClearSessionCache

#ifdef XP_WIN
#  include "nsILocalFileWin.h"
#endif

using namespace mozilla;
using dom::Promise;

NS_IMPL_ISUPPORTS(SecretDecoderRing, nsISecretDecoderRing)

void BackgroundSdrEncryptStrings(const nsTArray<nsCString>& plaintexts,
                                 RefPtr<Promise>& aPromise) {
  nsCOMPtr<nsISecretDecoderRing> sdrService =
      do_GetService(NS_SECRETDECODERRING_CONTRACTID);
  InfallibleTArray<nsString> cipherTexts(plaintexts.Length());

  nsresult rv = NS_ERROR_FAILURE;
  for (uint32_t i = 0; i < plaintexts.Length(); ++i) {
    const nsCString& plaintext = plaintexts[i];
    nsCString cipherText;
    rv = sdrService->EncryptString(plaintext, cipherText);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    cipherTexts.AppendElement(NS_ConvertASCIItoUTF16(cipherText));
  }

  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("BackgroundSdrEncryptStringsResolve",
                             [rv, aPromise = std::move(aPromise),
                              cipherTexts = std::move(cipherTexts)]() {
                               if (NS_FAILED(rv)) {
                                 aPromise->MaybeReject(rv);
                               } else {
                                 aPromise->MaybeResolve(cipherTexts);
                               }
                             }));
  NS_DispatchToMainThread(runnable.forget());
}

nsresult SecretDecoderRing::Encrypt(const nsACString& data,
                                    /*out*/ nsACString& result) {
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Make sure token is initialized. */
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsresult rv = setPassword(slot.get(), ctx);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /* Force authentication */
  if (PK11_Authenticate(slot.get(), true, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  /* Use default key id */
  SECItem keyid;
  keyid.data = nullptr;
  keyid.len = 0;
  SECItem request;
  request.data = BitwiseCast<unsigned char*, const char*>(data.BeginReading());
  request.len = data.Length();
  ScopedAutoSECItem reply;
  if (PK11SDR_Encrypt(&keyid, &request, &reply, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  result.Assign(BitwiseCast<char*, unsigned char*>(reply.data), reply.len);
  return NS_OK;
}

nsresult SecretDecoderRing::Decrypt(const nsACString& data,
                                    /*out*/ nsACString& result) {
  /* Find token with SDR key */
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* Force authentication */
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  if (PK11_Authenticate(slot.get(), true, ctx) != SECSuccess) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SECItem request;
  request.data = BitwiseCast<unsigned char*, const char*>(data.BeginReading());
  request.len = data.Length();
  ScopedAutoSECItem reply;
  if (PK11SDR_Decrypt(&request, &reply, ctx) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  result.Assign(BitwiseCast<char*, unsigned char*>(reply.data), reply.len);
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::EncryptString(const nsACString& text,
                                 /*out*/ nsACString& encryptedBase64Text) {
  nsAutoCString encryptedText;
  nsresult rv = Encrypt(text, encryptedText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Base64Encode(encryptedText, encryptedBase64Text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::AsyncEncryptStrings(uint32_t plaintextsCount,
                                       const char16_t** plaintexts,
                                       JSContext* aCx, Promise** aPromise) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(plaintextsCount);
  NS_ENSURE_ARG_POINTER(plaintexts);
  NS_ENSURE_ARG_POINTER(aCx);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  InfallibleTArray<nsCString> plaintextsUtf8(plaintextsCount);
  for (uint32_t i = 0; i < plaintextsCount; ++i) {
    plaintextsUtf8.AppendElement(NS_ConvertUTF16toUTF8(plaintexts[i]));
  }
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundSdrEncryptStrings",
      [promise, plaintextsUtf8 = std::move(plaintextsUtf8)]() mutable {
        BackgroundSdrEncryptStrings(plaintextsUtf8, promise);
      }));

  nsCOMPtr<nsIThread> encryptionThread;
  nsresult rv = NS_NewNamedThread("AsyncSDRThread",
                                  getter_AddRefs(encryptionThread), runnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

//// Function TEMPORARILY copied from nsNSSComponent.cpp
// Helper function to take a path and a file name and create a handle for the
// file in that location, if it exists. |path| is encoded in UTF-8.
static nsresult GetFileIfExists(const nsACString& path,
                                const nsACString& filename,
                                /* out */ nsIFile** result) {
  MOZ_ASSERT(result);
  if (!result) {
    return NS_ERROR_INVALID_ARG;
  }
  *result = nullptr;
  nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
  if (!file) {
    return NS_ERROR_FAILURE;
  }
#  ifdef XP_WIN
  // |path| is encoded in UTF-8 because SQLite always takes UTF-8 file paths
  // regardless of the current system code page.
  nsresult rv = file->InitWithPath(NS_ConvertUTF8toUTF16(path));
#  else
  nsresult rv = file->InitWithNativePath(path);
#  endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = file->AppendNative(filename);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (exists) {
    file.forget(result);
  }
  return NS_OK;
}

//// Function TEMPORARILY copied from nsNSSComponent.cpp
static nsresult GetNSSProfilePath(nsAutoCString& aProfilePath) {
  aProfilePath.Truncate();
  nsCOMPtr<nsIFile> profileFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileFile));
  if (NS_FAILED(rv)) {
    return rv;
  }

#if defined(XP_WIN)
  // SQLite always takes UTF-8 file paths regardless of the current system
  // code page.
  nsCOMPtr<nsILocalFileWin> profileFileWin(do_QueryInterface(profileFile));
  if (!profileFileWin) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString u16ProfilePath;
  rv = profileFileWin->GetCanonicalPath(u16ProfilePath);
  CopyUTF16toUTF8(u16ProfilePath, aProfilePath);
#else
  rv = profileFile->GetNativePath(aProfilePath);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

static void CleanupKey3DB()
{
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());

  if (PK11_IsReadOnly(slot.get()) || !PK11_IsLoggedIn(slot.get(), nullptr))
    return;

  nsAutoCString profileStr;
  nsresult rv = GetNSSProfilePath(profileStr);
  if (NS_FAILED(rv))
    return;

  NS_NAMED_LITERAL_CSTRING(newKeyDBFilename, "key4.db");
  nsCOMPtr<nsIFile> newDBFile;
  rv = GetFileIfExists(profileStr, newKeyDBFilename,
                       getter_AddRefs(newDBFile));
  if (NS_FAILED(rv) || !newDBFile) {
    // If we don't have key4, then we shouldn't delete key3.
    // Potentially we're a patched application that doesn't use sql:
    return;
  }

  NS_NAMED_LITERAL_CSTRING(oldKeyDBFilename, "key3.db");
  nsCOMPtr<nsIFile> oldDBFile;
  rv = GetFileIfExists(profileStr, oldKeyDBFilename,
                       getter_AddRefs(oldDBFile));
  if (!oldDBFile) {
    return;
  }
  // Since this isn't a directory, the `recursive` argument to `Remove` is
  // irrelevant.
  Unused << oldDBFile->Remove(false);
}

NS_IMETHODIMP
SecretDecoderRing::DecryptString(const nsACString& encryptedBase64Text,
                                 /*out*/ nsACString& decryptedText) {
  nsAutoCString encryptedText;
  nsresult rv = Base64Decode(encryptedBase64Text, encryptedText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Decrypt(encryptedText, decryptedText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // This is a good time to perform a necessary key3.db cleanup,
  // see bug 1606619. Only do it once per session.
  static bool alreadyCheckedKey3Cleanup = false;
  if (!alreadyCheckedKey3Cleanup) {
    alreadyCheckedKey3Cleanup = true;
    CleanupKey3DB();
  }

  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::ChangePassword() {
  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // nsPK11Token::nsPK11Token takes its own reference to slot, so we pass a
  // non-owning pointer here.
  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(slot.get());

  nsCOMPtr<nsITokenPasswordDialogs> dialogs;
  nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                              NS_GET_IID(nsITokenPasswordDialogs),
                              NS_TOKENPASSWORDSDIALOG_CONTRACTID);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  bool canceled;  // Ignored
  return dialogs->SetPassword(ctx, token, &canceled);
}

NS_IMETHODIMP
SecretDecoderRing::Logout() {
  PK11_LogoutAll();
  SSL_ClearSessionCache();
  return NS_OK;
}

NS_IMETHODIMP
SecretDecoderRing::LogoutAndTeardown() {
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  PK11_LogoutAll();
  SSL_ClearSessionCache();

  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nssComponent->LogoutAuthenticatedPK11();

  // After we just logged out, we need to prune dead connections to make
  // sure that all connections that should be stopped, are stopped. See
  // bug 517584.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "net:prune-dead-connections", nullptr);
  }

  return rv;
}
