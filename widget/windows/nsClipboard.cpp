/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboard.h"

#include <shlobj.h>
#include <intshcut.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#include <functional>
#include <thread>
#include <chrono>

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_clipboard.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/WindowsVersion.h"
#include "SpecialSystemDirectory.h"

#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDataObj.h"
#include "nsString.h"
#include "nsNativeCharsetUtils.h"
#include "nsIInputStream.h"
#include "nsITransferable.h"
#include "nsXPCOM.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIFileProtocolHandler.h"
#include "nsEscape.h"
#include "nsIObserverService.h"
#include "nsMimeTypes.h"
#include "imgITools.h"
#include "imgIContainer.h"
#include "WinOLELock.h"
#include "WinUtils.h"

/* static */
UINT nsClipboard::GetClipboardFileDescriptorFormatA() {
  static UINT format = ::RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORA);
  MOZ_ASSERT(format);
  return format;
}

/* static */
UINT nsClipboard::GetClipboardFileDescriptorFormatW() {
  static UINT format = ::RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
  MOZ_ASSERT(format);
  return format;
}

/* static */
UINT nsClipboard::GetHtmlClipboardFormat() {
  static UINT format = ::RegisterClipboardFormatW(L"HTML Format");
  return format;
}

/* static */
UINT nsClipboard::GetCustomClipboardFormat() {
  static UINT format =
      ::RegisterClipboardFormatW(L"application/x-moz-custom-clipdata");
  return format;
}

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard()
    : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
          false /* supportsSelectionClipboard */,
          false /* supportsFindClipboard */,
          false /* supportsSelectionCache */)) {
  mWindow = nullptr;

  // Register for a shutdown notification so that we can flush data
  // to the OS clipboard.
  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                 false);
  }
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard() {}

NS_IMPL_ISUPPORTS_INHERITED(nsClipboard, nsBaseClipboard, nsIObserver)

NS_IMETHODIMP
nsClipboard::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  // This will be called on shutdown.
  ::OleFlushClipboard();
  ::CloseClipboard();

  return NS_OK;
}

//-------------------------------------------------------------------------
UINT nsClipboard::GetFormat(const char* aMimeStr, bool aMapHTMLMime) {
  UINT format;

  if (strcmp(aMimeStr, kTextMime) == 0) {
    format = CF_UNICODETEXT;
  } else if (strcmp(aMimeStr, kRTFMime) == 0) {
    format = ::RegisterClipboardFormat(L"Rich Text Format");
  } else if (strcmp(aMimeStr, kJPEGImageMime) == 0 ||
             strcmp(aMimeStr, kJPGImageMime) == 0) {
    format = CF_DIBV5;
  } else if (strcmp(aMimeStr, kPNGImageMime) == 0) {
    format = ::RegisterClipboardFormat(TEXT("PNG"));
  } else if (strcmp(aMimeStr, kFileMime) == 0 ||
             strcmp(aMimeStr, kFilePromiseMime) == 0) {
    format = CF_HDROP;
  } else if ((strcmp(aMimeStr, kNativeHTMLMime) == 0) ||
             (aMapHTMLMime && strcmp(aMimeStr, kHTMLMime) == 0)) {
    format = GetHtmlClipboardFormat();
  } else if (strcmp(aMimeStr, kCustomTypesMime) == 0) {
    format = GetCustomClipboardFormat();
  } else {
    format = ::RegisterClipboardFormatW(NS_ConvertASCIItoUTF16(aMimeStr).get());
  }

  return format;
}

mozilla::Maybe<UINT> nsClipboard::GetSecondaryFormat(const char* aMimeStr) {
  if (strcmp(aMimeStr, kPNGImageMime) == 0) {
    // Fall back to DIBV5 format
    return mozilla::Some(CF_DIBV5);
  }
  return mozilla::Nothing();
}

//-------------------------------------------------------------------------
// static
nsresult nsClipboard::CreateNativeDataObject(
    nsITransferable* aTransferable, IDataObject** aDataObj, nsIURI* aUri,
    MightNeedToFlush* aMightNeedToFlush) {
  MOZ_ASSERT(aTransferable);
  if (!aTransferable) {
    return NS_ERROR_FAILURE;
  }

  // Create our native DataObject that implements the OLE IDataObject interface
  RefPtr<nsDataObj> dataObj = new nsDataObj(aUri);

  // Now set it up with all the right data flavors & enums
  nsresult res =
      SetupNativeDataObject(aTransferable, dataObj, aMightNeedToFlush);
  if (NS_SUCCEEDED(res)) {
    dataObj.forget(aDataObj);
  }
  return res;
}

static nsresult StoreValueInDataObject(nsDataObj* aObj,
                                       LPCWSTR aClipboardFormat, DWORD value) {
  ScopedOLEMemory<DWORD> hGlobalMemory;
  if (!hGlobalMemory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *hGlobalMemory.lock() = value;

  STGMEDIUM stg;
  stg.tymed = TYMED_HGLOBAL;
  stg.pUnkForRelease = nullptr;
  stg.hGlobal = hGlobalMemory.forget();

  FORMATETC fe;
  SET_FORMATETC(fe, ::RegisterClipboardFormat(aClipboardFormat), 0,
                DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
  aObj->SetData(&fe, &stg, TRUE);

  return NS_OK;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::SetupNativeDataObject(
    nsITransferable* aTransferable, IDataObject* aDataObj,
    MightNeedToFlush* aMightNeedToFlush) {
  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(aDataObj);
  if (!aTransferable || !aDataObj) {
    return NS_ERROR_FAILURE;
  }

  auto* dObj = static_cast<nsDataObj*>(aDataObj);
  if (aMightNeedToFlush) {
    *aMightNeedToFlush = MightNeedToFlush::No;
  }

  // Now give the Transferable to the DataObject
  // for getting the data out of it
  dObj->SetTransferable(aTransferable);

  // Get the transferable list of data flavors
  nsTArray<nsCString> flavors;
  aTransferable->FlavorsTransferableCanExport(flavors);

  // Walk through flavors that contain data and register them
  // into the DataObj as supported flavors
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];

    // When putting data onto the clipboard, we want to maintain kHTMLMime
    // ("text/html") and not map it to CF_HTML here since this will be done
    // below.
    UINT format = GetFormat(flavorStr.get(), false);

    // Now tell the native IDataObject about both our mime type and
    // the native data format
    FORMATETC fe;
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
    dObj->AddDataFlavor(flavorStr.get(), &fe);

    // Do various things internal to the implementation, like map one
    // flavor to another or add additional flavors based on what's required
    // for the win32 impl.
    if (flavorStr.EqualsLiteral(kTextMime)) {
      // if we find text/plain, also add CF_TEXT, but we can add it for
      // text/plain as well.
      FORMATETC textFE;
      SET_FORMATETC(textFE, CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
      dObj->AddDataFlavor(kTextMime, &textFE);
      if (aMightNeedToFlush) {
        *aMightNeedToFlush = MightNeedToFlush::Yes;
      }
    } else if (flavorStr.EqualsLiteral(kHTMLMime)) {
      // if we find text/html, also advertise win32's html flavor (which we will
      // convert on our own in nsDataObj::GetText().
      FORMATETC htmlFE;
      SET_FORMATETC(htmlFE, GetHtmlClipboardFormat(), 0, DVASPECT_CONTENT, -1,
                    TYMED_HGLOBAL);
      dObj->AddDataFlavor(kHTMLMime, &htmlFE);
    } else if (flavorStr.EqualsLiteral(kURLMime)) {
      // if we're a url, in addition to also being text, we need to register
      // the "file" flavors so that the win32 shell knows to create an internet
      // shortcut when it sees one of these beasts.
      FORMATETC shortcutFE;
      SET_FORMATETC(shortcutFE,
                    ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE,
                    ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_FILECONTENTS),
                    0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_INETURLA), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
      SET_FORMATETC(shortcutFE, ::RegisterClipboardFormat(CFSTR_INETURLW), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kURLMime, &shortcutFE);
    } else if (flavorStr.EqualsLiteral(kPNGImageMime) ||
               flavorStr.EqualsLiteral(kJPEGImageMime) ||
               flavorStr.EqualsLiteral(kJPGImageMime) ||
               flavorStr.EqualsLiteral(kGIFImageMime) ||
               flavorStr.EqualsLiteral(kNativeImageMime)) {
      // if we're an image, register the relevant bitmap flavors
      FORMATETC imageFE;

      // Add PNG, depending on prefs
      if (mozilla::StaticPrefs::clipboard_copy_image_as_png()) {
        static const CLIPFORMAT CF_PNG = ::RegisterClipboardFormat(TEXT("PNG"));
        SET_FORMATETC(imageFE, CF_PNG, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
        dObj->AddDataFlavor(flavorStr.get(), &imageFE);
      }

      // Add DIBv5
      SET_FORMATETC(imageFE, CF_DIBV5, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(flavorStr.get(), &imageFE);

      // Add DIBv3
      SET_FORMATETC(imageFE, CF_DIB, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(flavorStr.get(), &imageFE);
    } else if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
      // if we're a file promise flavor, also register the
      // CFSTR_PREFERREDDROPEFFECT format.  The data object
      // returns a value of DROPEFFECTS_MOVE to the drop target
      // when it asks for the value of this format.  This causes
      // the file to be moved from the temporary location instead
      // of being copied.  The right thing to do here is to call
      // SetData() on the data object and set the value of this format
      // to DROPEFFECTS_MOVE on this particular data object.  But,
      // since all the other clipboard formats follow the model of setting
      // data on the data object only when the drop object calls GetData(),
      // I am leaving this format's value hard coded in the data object.
      // We can change this if other consumers of this format get added to this
      // codebase and they need different values.
      FORMATETC shortcutFE;
      SET_FORMATETC(shortcutFE,
                    ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT), 0,
                    DVASPECT_CONTENT, -1, TYMED_HGLOBAL)
      dObj->AddDataFlavor(kFilePromiseMime, &shortcutFE);
    }
  }

  if (!mozilla::StaticPrefs::
          clipboard_copyPrivateDataToClipboardCloudOrHistory()) {
    // Let Clipboard know that data is sensitive and must not be copied to
    // the Cloud Clipboard, Clipboard History and similar.
    // https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats#cloud-clipboard-and-clipboard-history-formats
    if (aTransferable->GetIsPrivateData()) {
      nsresult rv =
          StoreValueInDataObject(dObj, TEXT("CanUploadToCloudClipboard"), 0);
      NS_ENSURE_SUCCESS(rv, rv);
      rv =
          StoreValueInDataObject(dObj, TEXT("CanIncludeInClipboardHistory"), 0);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = StoreValueInDataObject(
          dObj, TEXT("ExcludeClipboardContentFromMonitorProcessing"), 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// See methods listed at
// <https://docs.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-idataobject#methods>.
static void IDataObjectMethodResultToString(const HRESULT aHres,
                                            nsACString& aResult) {
  switch (aHres) {
    case E_INVALIDARG:
      aResult = "E_INVALIDARG";
      break;
    case E_UNEXPECTED:
      aResult = "E_UNEXPECTED";
      break;
    case E_OUTOFMEMORY:
      aResult = "E_OUTOFMEMORY";
      break;
    case DV_E_LINDEX:
      aResult = "DV_E_LINDEX";
      break;
    case DV_E_FORMATETC:
      aResult = "DV_E_FORMATETC";
      break;
    case DV_E_TYMED:
      aResult = "DV_E_TYMED";
      break;
    case DV_E_DVASPECT:
      aResult = "DV_E_DVASPECT";
      break;
    case OLE_E_NOTRUNNING:
      aResult = "OLE_E_NOTRUNNING";
      break;
    case STG_E_MEDIUMFULL:
      aResult = "STG_E_MEDIUMFULL";
      break;
    case DV_E_CLIPFORMAT:
      aResult = "DV_E_CLIPFORMAT";
      break;
    case S_OK:
      aResult = "S_OK";
      break;
    default:
      // Explicit template instantiaton, because otherwise the call is
      // ambiguous.
      constexpr int kRadix = 16;
      aResult = IntToCString<int32_t>(aHres, kRadix);
      break;
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olegetclipboard>.
static void OleGetClipboardResultToString(const HRESULT aHres,
                                          nsACString& aResult) {
  switch (aHres) {
    case S_OK:
      aResult = "S_OK";
      break;
    case CLIPBRD_E_CANT_OPEN:
      aResult = "CLIPBRD_E_CANT_OPEN";
      break;
    case CLIPBRD_E_CANT_CLOSE:
      aResult = "CLIPBRD_E_CANT_CLOSE";
      break;
    default:
      // Explicit template instantiaton, because otherwise the call is
      // ambiguous.
      constexpr int kRadix = 16;
      aResult = IntToCString<int32_t>(aHres, kRadix);
      break;
  }
}

static void MaybeLogClipboardCurrentOwner(
    const HRESULT aHres, const mozilla::StaticString& aMethodName) {
  if (!MOZ_CLIPBOARD_LOG_ENABLED()) {
    return;
  }

  if (aHres != CLIPBRD_E_CANT_OPEN) {
    return;
  }
  auto hwnd = ::GetOpenClipboardWindow();
  if (!hwnd) {
    MOZ_CLIPBOARD_LOG(
        "IDataObject::%s | Clipboard already opened by unknown process",
        aMethodName.get());
    return;
  }
  DWORD procId;
  DWORD threadId = ::GetWindowThreadProcessId(hwnd, &procId);
  NS_ENSURE_TRUE_VOID(threadId);
  nsAutoString procName;
  NS_ENSURE_SUCCESS_VOID(
      mozilla::widget::WinUtils::GetProcessImageName(procId, procName));
  MOZ_CLIPBOARD_LOG(
      "IDataObject::%s | Clipboard already opened by HWND: %p | "
      "Process ID: %lu | Thread ID: %lu | App name: %s",
      aMethodName.get(), hwnd, procId, threadId,
      NS_ConvertUTF16toUTF8(procName).get());
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olegetclipboard>.
static void LogOleGetClipboardResult(const HRESULT aHres) {
  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    nsAutoCString hresString;
    OleGetClipboardResultToString(aHres, hresString);
    MOZ_CLIPBOARD_LOG("OleGetClipboard result: %s", hresString.get());
    MaybeLogClipboardCurrentOwner(aHres, "OleGetClipboard");
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olesetclipboard>.
static void OleSetClipboardResultToString(HRESULT aHres, nsACString& aResult) {
  switch (aHres) {
    case S_OK:
      aResult = "S_OK";
      break;
    case CLIPBRD_E_CANT_OPEN:
      aResult = "CLIPBRD_E_CANT_OPEN";
      break;
    case CLIPBRD_E_CANT_EMPTY:
      aResult = "CLIPBRD_E_CANT_EMPTY";
      break;
    case CLIPBRD_E_CANT_CLOSE:
      aResult = "CLIPBRD_E_CANT_CLOSE";
      break;
    case CLIPBRD_E_CANT_SET:
      aResult = "CLIPBRD_E_CANT_SET";
      break;
    default:
      // Explicit template instantiaton, because otherwise the call is
      // ambiguous.
      constexpr int kRadix = 16;
      aResult = IntToCString<int32_t>(aHres, kRadix);
      break;
  }
}

// See
// <https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-olesetclipboard>.
static void LogOleSetClipboardResult(const HRESULT aHres) {
  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    nsAutoCString hresString;
    OleSetClipboardResultToString(aHres, hresString);
    MOZ_CLIPBOARD_LOG("OleSetClipboard result: %s", hresString.get());
    MaybeLogClipboardCurrentOwner(aHres, "OleSetClipboard");
  }
}

template <typename Function, typename LogFunction, typename... Args>
static HRESULT RepeatedlyTry(Function aFunction, LogFunction aLogFunction,
                             Args... aArgs) {
  // These are magic values based on local testing. They are chosen not higher
  // to avoid jank (<https://developer.mozilla.org/en-US/docs/Glossary/Jank>).
  // When changing them, be careful.
  static constexpr int kNumberOfTries = 3;
  static constexpr int kDelayInMs = 3;

  HRESULT hres;
  for (int i = 0; i < kNumberOfTries; ++i) {
    hres = aFunction(aArgs...);
    aLogFunction(hres);

    if (hres == S_OK) {
      break;
    }

    // TODO: This was formerly std::sleep_for, which wasn't actually sleeping
    // in tests (bug 1927664).
    ::SleepEx(kDelayInMs, TRUE);
  }

  return hres;
}

// Other apps can block access to the clipboard. This repeatedly
// calls `::OleSetClipboard` for a fixed number of times and should be called
// instead of `::OleSetClipboard`.
static void RepeatedlyTryOleSetClipboard(IDataObject* aDataObj) {
  RepeatedlyTry(::OleSetClipboard, LogOleSetClipboardResult, aDataObj);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(
    nsITransferable* aTransferable, ClipboardType aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_FAILURE;
  }

  // make sure we have a good transferable
  if (!aTransferable) {
    return NS_ERROR_FAILURE;
  }

#ifdef ACCESSIBILITY
  mozilla::a11y::Compatibility::SuppressA11yForClipboardCopy();
#endif

  RefPtr<IDataObject> dataObj;
  auto mightNeedToFlush = MightNeedToFlush::No;
  if (NS_SUCCEEDED(CreateNativeDataObject(aTransferable,
                                          getter_AddRefs(dataObj), nullptr,
                                          &mightNeedToFlush))) {
    RepeatedlyTryOleSetClipboard(dataObj);

    const bool doFlush = [&] {
      switch (mozilla::StaticPrefs::widget_windows_sync_clipboard_flush()) {
        case 0:
          return false;
        case 1:
          return true;
        default:
          // Bug 1774285: Windows Suggested Actions (introduced in Windows 11
          // 22H2) walks the entire a11y tree using UIA if something is placed
          // on the clipboard using delayed rendering. (The OLE clipboard always
          // uses delayed rendering.) This a11y tree walk causes an unacceptable
          // hang, particularly when the a11y cache is disabled. We choose the
          // lesser of the two performance/memory evils here and force immediate
          // rendering as part of our workaround.
          return mightNeedToFlush == MightNeedToFlush::Yes &&
                 mozilla::IsWin1122H2OrLater();
      }
    }();
    if (doFlush) {
      RepeatedlyTry(::OleFlushClipboard, [](HRESULT) {});
    }
  } else {
    // Clear the native clipboard
    RepeatedlyTryOleSetClipboard(nullptr);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetGlobalData(HGLOBAL aHGBL, void** aData,
                                    uint32_t* aLen) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  // Allocate a new memory buffer and copy the data from global memory.
  //
  // Some callers of this function call `NS_strlen(char16_t*)` on the returned
  // data buffer -- even though there's no guarantee that the data is a wide
  // string, let alone NUL-terminated. As a safety precaution, allocate a
  // slightly longer buffer than necessary, and append three bytes' worth of
  // NUL.
  //
  // (These bytes are not reported in *aLen, so callers which sensibly use that
  // as a limit will not need to worry about stray trailing bytes.)

  if (aHGBL != nullptr) {
    ScopedOLELock<CHAR[]> lpStr(aHGBL);
    mozilla::CheckedInt<uint32_t> allocSize =
        mozilla::CheckedInt<uint32_t>(lpStr.size()) + 3;
    if (!allocSize.isValid()) {
      return NS_ERROR_INVALID_ARG;
    }
    char* data = static_cast<char*>(malloc(allocSize.value()));
    if (!data) return NS_ERROR_FAILURE;

    std::copy(lpStr.begin(), lpStr.end(), data);

    // null terminate for safety
    std::fill_n(data + lpStr.size(), 3, '\0');

    *aData = data;
    *aLen = lpStr.size();

    return NS_OK;
  }

  // We really shouldn't ever get here
  // but just in case
  *aData = nullptr;
  *aLen = 0;

  LPVOID lpMsgBuf;
  ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
      (LPWSTR)&lpMsgBuf, 0, nullptr);

  // Display the string.
  ::MessageBoxW(nullptr, (LPCWSTR)lpMsgBuf, L"GetLastError",
                MB_OK | MB_ICONINFORMATION);

  // Free the buffer.
  ::LocalFree(lpMsgBuf);

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetNativeDataOffClipboard(nsIWidget* aWidget,
                                                UINT /*aIndex*/, UINT aFormat,
                                                void** aData, uint32_t* aLen) {
  MOZ_CLIPBOARD_LOG("%s: overload taking nsIWidget*.", __FUNCTION__);

  HGLOBAL hglb;
  nsresult result = NS_ERROR_FAILURE;

  HWND nativeWin = nullptr;
  if (::OpenClipboard(nativeWin)) {
    hglb = ::GetClipboardData(aFormat);
    result = GetGlobalData(hglb, aData, aLen);
    ::CloseClipboard();
  }
  return result;
}

// See methods listed at
// <https://docs.microsoft.com/en-us/windows/win32/api/objidl/nn-objidl-idataobject#methods>.
static void LogIDataObjectMethodResult(const HRESULT aHres,
                                       mozilla::StaticString aMethodName) {
  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    nsAutoCString hresString;
    IDataObjectMethodResultToString(aHres, hresString);
    MOZ_CLIPBOARD_LOG("IDataObject::%s result : %s", aMethodName.get(),
                      hresString.get());
    MaybeLogClipboardCurrentOwner(aHres, aMethodName);
  }
}

// Other apps can block access to the clipboard. This repeatedly calls
// `GetData` for a fixed number of times and should be called instead of
// `GetData`. See
// <https://docs.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-idataobject-getdata>.
// While Microsoft's documentation doesn't include `CLIPBRD_E_CANT_OPEN`
// explicitly, it allows it implicitly and in local experiments it was indeed
// returned.
static HRESULT RepeatedlyTryGetData(IDataObject& aDataObject, LPFORMATETC pFE,
                                    LPSTGMEDIUM pSTM) {
  return RepeatedlyTry(
      [&aDataObject, &pFE, &pSTM]() { return aDataObject.GetData(pFE, pSTM); },
      [](HRESULT hres) { LogIDataObjectMethodResult(hres, "GetData"); });
}

//-------------------------------------------------------------------------
// static
HRESULT nsClipboard::FillSTGMedium(IDataObject* aDataObject, UINT aFormat,
                                   LPFORMATETC pFE, LPSTGMEDIUM pSTM,
                                   DWORD aTymed) {
  SET_FORMATETC(*pFE, aFormat, 0, DVASPECT_CONTENT, -1, aTymed);

  // Starting by querying for the data to see if we can get it as from global
  // memory
  HRESULT hres = S_FALSE;
  hres = aDataObject->QueryGetData(pFE);
  LogIDataObjectMethodResult(hres, "QueryGetData");
  if (S_OK == hres) {
    hres = RepeatedlyTryGetData(*aDataObject, pFE, pSTM);
  }
  return hres;
}

//-------------------------------------------------------------------------
// If aFormat is CF_DIBV5, aMIMEImageFormat must be a type for which we have
// an image encoder (e.g. image/png).
// For other values of aFormat, it is OK to pass null for aMIMEImageFormat.
nsresult nsClipboard::GetNativeDataOffClipboard(IDataObject* aDataObject,
                                                UINT aIndex, UINT aFormat,
                                                const char* aMIMEImageFormat,
                                                void** aData, uint32_t* aLen) {
  MOZ_CLIPBOARD_LOG("%s: overload taking IDataObject*.", __FUNCTION__);

  *aData = nullptr;
  *aLen = 0;

  if (!aDataObject) {
    return NS_ERROR_FAILURE;
  }

  UINT const format = aFormat;

  FORMATETC fe;
  STGMEDIUM stm;
  HRESULT hres = FillSTGMedium(aDataObject, format, &fe, &stm, TYMED_HGLOBAL);

  // If the format is CF_HDROP and we haven't found any files we can try looking
  // for virtual files with FILEDESCRIPTOR.
  if (FAILED(hres) && format == CF_HDROP) {
    hres = FillSTGMedium(aDataObject,
                         nsClipboard::GetClipboardFileDescriptorFormatW(), &fe,
                         &stm, TYMED_HGLOBAL);
    if (FAILED(hres)) {
      hres = FillSTGMedium(aDataObject,
                           nsClipboard::GetClipboardFileDescriptorFormatA(),
                           &fe, &stm, TYMED_HGLOBAL);
    }
  }

  // N.B.: not `FAILED(hres)`, as this can be `S_FALSE`!
  if (hres != S_OK) {
    return NS_ERROR_FAILURE;
  }

  // otherwise, there is something in stm; make sure we delete it on exit
  auto const _release_stm =
      mozilla::MakeScopeExit([&stm] { ::ReleaseStgMedium(&stm); });

  static CLIPFORMAT fileDescriptorFlavorA =
      ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA);
  static CLIPFORMAT fileDescriptorFlavorW =
      ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
  static CLIPFORMAT fileFlavor = ::RegisterClipboardFormat(CFSTR_FILECONTENTS);
  static CLIPFORMAT preferredDropEffect =
      ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
  static CLIPFORMAT pngFlavor = ::RegisterClipboardFormat(TEXT("PNG"));

  // Historical note: when this code was first written (bug #9367, 1999-07-09),
  // it was believed we would need to handle other values of stm.tymed. As of
  // 2024-01-09, such a need has not yet materialized.
  if (stm.tymed != TYMED_HGLOBAL) {
    MOZ_CLIPBOARD_LOG("unhandled TYMED_* value: %lu", stm.tymed);
    return NS_ERROR_FAILURE;
  }

  // compile-time-constant format indicators:
  switch (fe.cfFormat) {
    case CF_TEXT: {
      // Get the data out of the global data handle. The size we
      // return should not include the null because the other
      // platforms don't use nulls, so just return the length we get
      // back from strlen(), since we know CF_TEXT is null
      // terminated. Recall that GetGlobalData() returns the size of
      // the allocated buffer, not the size of the data (on 98, these
      // are not the same) so we can't use that.
      uint32_t allocLen = 0;
      MOZ_TRY(GetGlobalData(stm.hGlobal, aData, &allocLen));
      *aLen = strlen(reinterpret_cast<char*>(*aData));
      return NS_OK;
    }

    case CF_UNICODETEXT: {
      // Get the data out of the global data handle. The size we
      // return should not include the null because the other
      // platforms don't use nulls, so just return the length we get
      // back from strlen(), since we know CF_UNICODETEXT is null
      // terminated. Recall that GetGlobalData() returns the size of
      // the allocated buffer, not the size of the data (on 98, these
      // are not the same) so we can't use that.
      uint32_t allocLen = 0;
      MOZ_TRY(GetGlobalData(stm.hGlobal, aData, &allocLen));
      *aLen = NS_strlen(reinterpret_cast<char16_t*>(*aData)) * 2;
      return NS_OK;
    }

    case CF_DIBV5: {
      if (!aMIMEImageFormat) {
        return NS_ERROR_FAILURE;
      }
      uint32_t allocLen = 0;
      const char* clipboardData = nullptr;
      auto const _freeClipboardData =
          mozilla::MakeScopeExit([&]() { free((void*)clipboardData); });

      MOZ_TRY(GetGlobalData(stm.hGlobal, (void**)&clipboardData, &allocLen));
      nsCOMPtr<imgIContainer> container;
      nsCOMPtr<imgITools> imgTools =
          do_CreateInstance("@mozilla.org/image/tools;1");
      MOZ_TRY(imgTools->DecodeImageFromBuffer(
          clipboardData, allocLen, nsLiteralCString(IMAGE_BMP_MS_CLIPBOARD),
          getter_AddRefs(container)));

      nsAutoCString mimeType;
      if (strcmp(aMIMEImageFormat, kJPGImageMime) == 0) {
        mimeType.Assign(IMAGE_JPEG);
      } else {
        mimeType.Assign(aMIMEImageFormat);
      }

      nsCOMPtr<nsIInputStream> inputStream;
      MOZ_TRY(imgTools->EncodeImage(container, mimeType, u""_ns,
                                    getter_AddRefs(inputStream)));

      if (!inputStream) {
        return NS_ERROR_FAILURE;
      }

      *aData = inputStream.forget().take();
      *aLen = sizeof(nsIInputStream*);
      return NS_OK;
    }

    case CF_HDROP: {
      // in the case of a file drop, multiple files are stashed within a
      // single data object. In order to match mozilla's D&D apis, we
      // just pull out the file at the requested index, pretending as
      // if there really are multiple drag items.
      ScopedOLELock<HDROP> dropFiles(stm.hGlobal);

      UINT numFiles = ::DragQueryFileW(dropFiles.get(), 0xFFFFFFFF, nullptr, 0);

      if (numFiles == 0) {
        NS_WARNING("CF_HDROP received with empty file list");
        return NS_ERROR_FAILURE;
      }

      // Note that (partly for historical reasons) we do not consider it to be
      // an error on Gecko's part to request index 0 of a data object that turns
      // out to be empty. That case is handled above.
      if (aIndex >= numFiles) {
        MOZ_ASSERT(false, "Asked for a file index out of range of list");
        return NS_ERROR_INVALID_ARG;
      }

      UINT fileNameLen = ::DragQueryFileW(dropFiles.get(), aIndex, nullptr, 0);
      wchar_t* buffer = reinterpret_cast<wchar_t*>(
          moz_xmalloc((fileNameLen + 1) * sizeof(wchar_t)));
      ::DragQueryFileW(dropFiles.get(), aIndex, buffer, fileNameLen + 1);
      *aData = buffer;
      *aLen = fileNameLen * sizeof(char16_t);
      return NS_OK;
    }

    default: /* fallthrough */;
  }  // switch (fe.cfFormat)

  // non-compile-time-constant format indicators:

  if (fe.cfFormat == fileDescriptorFlavorA ||
      fe.cfFormat == fileDescriptorFlavorW) {
    nsAutoString tempPath;

    // BUG(?): this should probably use FILEGROUPDESCRIPTOR[A,W] depending on
    // the above
    ScopedOLELock<LPFILEGROUPDESCRIPTOR> fgdesc(stm.hGlobal);
    if (fgdesc) {
      MOZ_TRY(GetTempFilePath(
          nsDependentString((fgdesc->fgd)[aIndex].cFileName), tempPath));
    }

    MOZ_TRY(SaveStorageOrStream(aDataObject, aIndex, tempPath));

    wchar_t* buffer = reinterpret_cast<wchar_t*>(
        moz_xmalloc((tempPath.Length() + 1) * sizeof(wchar_t)));
    wcscpy(buffer, tempPath.get());
    *aData = buffer;
    *aLen = tempPath.Length() * sizeof(wchar_t);
    return NS_OK;
  }

  if (fe.cfFormat == pngFlavor) {
    MOZ_ASSERT(!strcmp(aMIMEImageFormat, kPNGImageMime));
    uint32_t allocLen = 0;
    const char* clipboardData = nullptr;
    auto const _freeClipboardData =
        mozilla::MakeScopeExit([&]() { free((void*)clipboardData); });

    MOZ_TRY(GetGlobalData(stm.hGlobal, (void**)&clipboardData, &allocLen));
    nsCOMPtr<imgIContainer> container;
    nsCOMPtr<imgITools> imgTools =
        do_CreateInstance("@mozilla.org/image/tools;1");
    MOZ_TRY(imgTools->DecodeImageFromBuffer(clipboardData, allocLen,
                                            nsLiteralCString(kPNGImageMime),
                                            getter_AddRefs(container)));

    nsCOMPtr<nsIInputStream> inputStream;
    MOZ_TRY(imgTools->EncodeImage(container, nsLiteralCString(kPNGImageMime),
                                  u""_ns, getter_AddRefs(inputStream)));

    if (!inputStream) {
      return NS_ERROR_FAILURE;
    }

    *aData = inputStream.forget().take();
    *aLen = sizeof(nsIInputStream*);
    return NS_OK;
  }

  if (fe.cfFormat == fileFlavor) {
    NS_WARNING(
        "Mozilla doesn't yet understand how to read this type of "
        "file flavor");
    return NS_ERROR_FAILURE;
  }

  // Get the data out of the global data handle. The size we
  // return should not include the null because the other
  // platforms don't use nulls, so just return the length we get
  // back from strlen(), since we know CF_UNICODETEXT is null
  // terminated. Recall that GetGlobalData() returns the size of
  // the allocated buffer, not the size of the data (on 98, these
  // are not the same) so we can't use that.
  //
  // NOTE: we are assuming that anything that falls into this
  //        default case is unicode. As we start to get more
  //        kinds of binary data, this may become an incorrect
  //        assumption. Stay tuned.
  uint32_t allocLen = 0;
  MOZ_TRY(GetGlobalData(stm.hGlobal, aData, &allocLen));
  if (fe.cfFormat == GetHtmlClipboardFormat()) {
    // CF_HTML is actually UTF8, not unicode, so disregard the
    // assumption above. We have to check the header for the
    // actual length, and we'll do that in FindPlatformHTML().
    // For now, return the allocLen. This case is mostly to
    // ensure we don't try to call strlen on the buffer.
    *aLen = allocLen;
  } else if (fe.cfFormat == GetCustomClipboardFormat()) {
    // Binary data
    *aLen = allocLen;
  } else if (fe.cfFormat == preferredDropEffect) {
    // As per the MSDN doc entitled: "Shell Clipboard Formats"
    // CFSTR_PREFERREDDROPEFFECT should return a DWORD
    // Reference:
    // http://msdn.microsoft.com/en-us/library/bb776902(v=vs.85).aspx
    NS_ASSERTION(allocLen == sizeof(DWORD),
                 "CFSTR_PREFERREDDROPEFFECT should return a DWORD");
    *aLen = allocLen;
  } else {
    *aLen = NS_strlen(reinterpret_cast<char16_t*>(*aData)) * sizeof(char16_t);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
mozilla::Result<nsCOMPtr<nsISupports>, nsresult>
nsClipboard::GetDataFromDataObject(IDataObject* aDataObject, UINT anIndex,
                                   nsIWidget* aWindow,
                                   const nsCString& aFlavor) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  UINT format = GetFormat(aFlavor.get());

  // Try to get the data using the desired flavor. This might fail, but all is
  // not lost.
  void* data = nullptr;
  uint32_t dataLen = 0;
  bool dataFound = false;
  if (nullptr != aDataObject) {
    if (NS_SUCCEEDED(GetNativeDataOffClipboard(
            aDataObject, anIndex, format, aFlavor.get(), &data, &dataLen))) {
      dataFound = true;
    }
  } else if (nullptr != aWindow) {
    if (NS_SUCCEEDED(GetNativeDataOffClipboard(aWindow, anIndex, format, &data,
                                               &dataLen))) {
      dataFound = true;
    }
  }

  // This is our second chance to try to find some data, having not found it
  // when directly asking for the flavor. Let's try digging around in other
  // flavors to help satisfy our craving for data.
  if (!dataFound) {
    if (aFlavor.EqualsLiteral(kTextMime)) {
      dataFound =
          FindUnicodeFromPlainText(aDataObject, anIndex, &data, &dataLen);
    } else if (aFlavor.EqualsLiteral(kURLMime)) {
      // drags from other windows apps expose the native
      // CFSTR_INETURL{A,W} flavor
      dataFound = FindURLFromNativeURL(aDataObject, anIndex, &data, &dataLen);
      if (!dataFound) {
        dataFound = FindURLFromLocalFile(aDataObject, anIndex, &data, &dataLen);
      }
    } else {
      mozilla::Maybe<UINT> secondaryFormat = GetSecondaryFormat(aFlavor.get());
      if (secondaryFormat) {
        // Fall back to secondary format
        dataFound = NS_SUCCEEDED(GetNativeDataOffClipboard(
            aDataObject, anIndex, secondaryFormat.value(), aFlavor.get(), &data,
            &dataLen));
      }
    }
  }  // if we try one last ditch effort to find our data

  if (!dataFound) {
    return nsCOMPtr<nsISupports>{};
  }

  // Hopefully by this point we've found it and can go about our business
  nsCOMPtr<nsISupports> genericDataWrapper;
  if (aFlavor.EqualsLiteral(kFileMime)) {
    // we have a file path in |data|. Create an nsLocalFile object.
    nsDependentString filepath(reinterpret_cast<char16_t*>(data));
    nsCOMPtr<nsIFile> file;
    if (NS_SUCCEEDED(NS_NewLocalFile(filepath, getter_AddRefs(file)))) {
      genericDataWrapper = do_QueryInterface(file);
    }
    free(data);
  } else if (aFlavor.EqualsLiteral(kNativeHTMLMime)) {
    uint32_t dummy;
    // the editor folks want CF_HTML exactly as it's on the clipboard, no
    // conversions, no fancy stuff. Pull it off the clipboard, stuff it into
    // a wrapper and hand it back to them.
    if (FindPlatformHTML(aDataObject, anIndex, &data, &dummy, &dataLen)) {
      nsPrimitiveHelpers::CreatePrimitiveForData(
          aFlavor, data, dataLen, getter_AddRefs(genericDataWrapper));
    }
    free(data);
  } else if (aFlavor.EqualsLiteral(kHTMLMime)) {
    uint32_t startOfData = 0;
    // The JS folks want CF_HTML exactly as it is on the clipboard, but
    // minus the CF_HTML header index information.
    // It also needs to be converted to UTF16 and have linebreaks changed.
    if (FindPlatformHTML(aDataObject, anIndex, &data, &startOfData, &dataLen)) {
      dataLen -= startOfData;
      nsPrimitiveHelpers::CreatePrimitiveForCFHTML(
          static_cast<char*>(data) + startOfData, &dataLen,
          getter_AddRefs(genericDataWrapper));
    }
    free(data);
  } else if (aFlavor.EqualsLiteral(kJPEGImageMime) ||
             aFlavor.EqualsLiteral(kJPGImageMime) ||
             aFlavor.EqualsLiteral(kPNGImageMime)) {
    nsIInputStream* imageStream = reinterpret_cast<nsIInputStream*>(data);
    genericDataWrapper = do_QueryInterface(imageStream);
    NS_IF_RELEASE(imageStream);
  } else {
    // Treat custom types as a string of bytes.
    if (!aFlavor.EqualsLiteral(kCustomTypesMime)) {
      bool isRTF = aFlavor.EqualsLiteral(kRTFMime);
      // we probably have some form of text. The DOM only wants LF, so
      // convert from Win32 line endings to DOM line endings.
      int32_t signedLen = static_cast<int32_t>(dataLen);
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(isRTF, &data,
                                                         &signedLen);
      dataLen = signedLen;

      if (isRTF) {
        // RTF on Windows is known to sometimes deliver an extra null byte.
        if (dataLen > 0 && static_cast<char*>(data)[dataLen - 1] == '\0') {
          dataLen--;
        }
      }
    }

    nsPrimitiveHelpers::CreatePrimitiveForData(
        aFlavor, data, dataLen, getter_AddRefs(genericDataWrapper));
    free(data);
  }

  return std::move(genericDataWrapper);
}

nsresult nsClipboard::GetDataFromDataObject(IDataObject* aDataObject,
                                            UINT anIndex, nsIWidget* aWindow,
                                            nsITransferable* aTransferable) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  // make sure we have a good transferable
  if (!aTransferable) {
    return NS_ERROR_INVALID_ARG;
  }

  // get flavor list that includes all flavors that can be written (including
  // ones obtained through conversion)
  nsTArray<nsCString> flavors;
  nsresult res = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(res)) {
    return NS_ERROR_FAILURE;
  }

  // Walk through flavors and see which flavor is on the clipboard them on the
  // native clipboard,
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    const nsCString& flavorStr = flavors[i];

    auto dataOrError =
        GetDataFromDataObject(aDataObject, anIndex, aWindow, flavorStr);
    if (dataOrError.isErr() || !dataOrError.inspect()) {
      continue;
    }

    NS_ASSERTION(dataOrError.inspect(),
                 "About to put null data into the transferable");
    aTransferable->SetTransferData(flavorStr.get(), dataOrError.inspect());
    // we found one, get out of the loop
    break;
  }  // foreach flavor

  return NS_OK;
}

//
// FindPlatformHTML
//
// Someone asked for the OS CF_HTML flavor. We give it back to them exactly
// as-is.
//
bool nsClipboard ::FindPlatformHTML(IDataObject* inDataObject, UINT inIndex,
                                    void** outData, uint32_t* outStartOfData,
                                    uint32_t* outDataLen) {
  // Reference: MSDN doc entitled "HTML Clipboard Format"
  // http://msdn.microsoft.com/en-us/library/aa767917(VS.85).aspx#unknown_854
  // CF_HTML is UTF8, not unicode. We also can't rely on it being
  // null-terminated so we have to check the CF_HTML header for the correct
  // length. The length we return is the bytecount from the beginning of the
  // selected data to the end of the selected data, without the null
  // termination. Because it's UTF8, we're guaranteed the header is ASCII.

  if (!outData || !*outData) {
    return false;
  }

  char version[8] = {0};
  int32_t startOfData = 0;
  int32_t endOfData = 0;
  int numFound =
      sscanf((char*)*outData, "Version:%7s\nStartHTML:%d\nEndHTML:%d", version,
             &startOfData, &endOfData);

  if (numFound != 3 || startOfData < -1 || endOfData < -1) {
    return false;
  }

  // Fixup the start and end markers if they have no context (set to -1)
  if (startOfData == -1) {
    startOfData = 0;
  }
  if (endOfData == -1) {
    endOfData = *outDataLen;
  }

  // Make sure we were passed sane values within our buffer size.
  // (Note that we've handled all cases of negative endOfData above, so we can
  // safely cast it to be unsigned here.)
  if (!endOfData || startOfData >= endOfData ||
      static_cast<uint32_t>(endOfData) > *outDataLen) {
    return false;
  }

  // We want to return the buffer not offset by startOfData because it will be
  // parsed out later (probably by HTMLEditor::ParseCFHTML) when it is still
  // in CF_HTML format.

  // We return the byte offset from the start of the data buffer to where the
  // HTML data starts. The caller might want to extract the HTML only.
  *outStartOfData = startOfData;
  *outDataLen = endOfData;
  return true;
}

//
// FindUnicodeFromPlainText
//
// Looks for CF_TEXT on the clipboard and converts it into an UTF-16 string
// if present. Returns this string in outData, and its length in outDataLen.
// XXXndeakin Windows converts between CF_UNICODE and CF_TEXT automatically
// so it doesn't seem like this is actually needed.
//
bool nsClipboard ::FindUnicodeFromPlainText(IDataObject* inDataObject,
                                            UINT inIndex, void** outData,
                                            uint32_t* outDataLen) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  // We are looking for text/plain and we failed to find it on the clipboard
  // first, so try again with CF_TEXT. If that is present, convert it to
  // unicode.
  nsresult rv = GetNativeDataOffClipboard(inDataObject, inIndex, CF_TEXT,
                                          nullptr, outData, outDataLen);
  if (NS_FAILED(rv) || !*outData) {
    return false;
  }

  const char* castedText = static_cast<char*>(*outData);
  nsAutoString tmp;
  rv = NS_CopyNativeToUnicode(nsDependentCSubstring(castedText, *outDataLen),
                              tmp);
  if (NS_FAILED(rv)) {
    return false;
  }

  // out with the old, in with the new
  free(*outData);
  *outData = ToNewUnicode(tmp);
  *outDataLen = tmp.Length() * sizeof(char16_t);

  return true;

}  // FindUnicodeFromPlainText

//
// FindURLFromLocalFile
//
// we are looking for a URL and couldn't find it, try again with looking for
// a local file. If we have one, it may either be a normal file or an internet
// shortcut. In both cases, however, we can get a URL (it will be a file:// url
// in the local file case).
//
bool nsClipboard ::FindURLFromLocalFile(IDataObject* inDataObject, UINT inIndex,
                                        void** outData, uint32_t* outDataLen) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  bool dataFound = false;

  nsresult loadResult =
      GetNativeDataOffClipboard(inDataObject, inIndex, GetFormat(kFileMime),
                                nullptr, outData, outDataLen);
  if (NS_SUCCEEDED(loadResult) && *outData) {
    // we have a file path in |data|. Is it an internet shortcut or a normal
    // file?
    const nsDependentString filepath(static_cast<char16_t*>(*outData));
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_NewLocalFile(filepath, getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      free(*outData);
      return dataFound;
    }

    if (IsInternetShortcut(filepath)) {
      free(*outData);
      nsAutoCString url;
      ResolveShortcut(file, url);
      if (!url.IsEmpty()) {
        // convert it to unicode and pass it out
        NS_ConvertUTF8toUTF16 urlString(url);
        // the internal mozilla URL format, text/x-moz-url, contains
        // URL\ntitle.  We can guess the title from the file's name.
        nsAutoString title;
        file->GetLeafName(title);
        // We rely on IsInternetShortcut check that file has a .url extension.
        title.SetLength(title.Length() - 4);
        if (title.IsEmpty()) {
          title = urlString;
        }
        *outData = ToNewUnicode(urlString + u"\n"_ns + title);
        *outDataLen =
            NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);

        dataFound = true;
      }
    } else {
      // we have a normal file, use some Necko objects to get our file path
      nsAutoCString urlSpec;
      NS_GetURLSpecFromFile(file, urlSpec);

      // convert it to unicode and pass it out
      free(*outData);
      *outData = UTF8ToNewUnicode(urlSpec);
      *outDataLen =
          NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);
      dataFound = true;
    }  // else regular file
  }

  return dataFound;
}  // FindURLFromLocalFile

//
// FindURLFromNativeURL
//
// we are looking for a URL and couldn't find it using our internal
// URL flavor, so look for it using the native URL flavor,
// CF_INETURLSTRW (We don't handle CF_INETURLSTRA currently)
//
bool nsClipboard ::FindURLFromNativeURL(IDataObject* inDataObject, UINT inIndex,
                                        void** outData, uint32_t* outDataLen) {
  MOZ_CLIPBOARD_LOG("%s", __FUNCTION__);

  bool dataFound = false;

  void* tempOutData = nullptr;
  uint32_t tempDataLen = 0;

  nsresult loadResult = GetNativeDataOffClipboard(
      inDataObject, inIndex, ::RegisterClipboardFormat(CFSTR_INETURLW), nullptr,
      &tempOutData, &tempDataLen);
  if (NS_SUCCEEDED(loadResult) && tempOutData) {
    nsDependentString urlString(static_cast<char16_t*>(tempOutData));
    // the internal mozilla URL format, text/x-moz-url, contains
    // URL\ntitle.  Since we don't actually have a title here,
    // just repeat the URL to fake it.
    *outData = ToNewUnicode(urlString + u"\n"_ns + urlString);
    *outDataLen =
        NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);
    free(tempOutData);
    dataFound = true;
  } else {
    loadResult = GetNativeDataOffClipboard(
        inDataObject, inIndex, ::RegisterClipboardFormat(CFSTR_INETURLA),
        nullptr, &tempOutData, &tempDataLen);
    if (NS_SUCCEEDED(loadResult) && tempOutData) {
      // CFSTR_INETURLA is (currently) equal to CFSTR_SHELLURL which is equal to
      // CF_TEXT which is by definition ANSI encoded.
      nsCString urlUnescapedA;
      bool unescaped =
          NS_UnescapeURL(static_cast<char*>(tempOutData), tempDataLen,
                         esc_OnlyNonASCII | esc_SkipControl, urlUnescapedA);

      nsString urlString;
      if (unescaped) {
        NS_CopyNativeToUnicode(urlUnescapedA, urlString);
      } else {
        NS_CopyNativeToUnicode(
            nsDependentCString(static_cast<char*>(tempOutData), tempDataLen),
            urlString);
      }

      // the internal mozilla URL format, text/x-moz-url, contains
      // URL\ntitle.  Since we don't actually have a title here,
      // just repeat the URL to fake it.
      *outData = ToNewUnicode(urlString + u"\n"_ns + urlString);
      *outDataLen =
          NS_strlen(static_cast<char16_t*>(*outData)) * sizeof(char16_t);
      free(tempOutData);
      dataFound = true;
    }
  }

  return dataFound;
}  // FindURLFromNativeURL

// Other apps can block access to the clipboard. This repeatedly
// calls `::OleGetClipboard` for a fixed number of times and should be called
// instead of `::OleGetClipboard`.
static HRESULT RepeatedlyTryOleGetClipboard(IDataObject** aDataObj) {
  return RepeatedlyTry(::OleGetClipboard, LogOleGetClipboardResult, aDataObj);
}

//
// ResolveShortcut
//
void nsClipboard ::ResolveShortcut(nsIFile* aFile, nsACString& outURL) {
  nsCOMPtr<nsIFileProtocolHandler> fph;
  nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fph));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  rv = fph->ReadURLFile(aFile, getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return;
  }

  uri->GetSpec(outURL);
}  // ResolveShortcut

//
// IsInternetShortcut
//
// A file is an Internet Shortcut if it ends with .URL
//
bool nsClipboard ::IsInternetShortcut(const nsAString& inFileName) {
  return StringEndsWith(inFileName, u".url"_ns,
                        nsCaseInsensitiveStringComparator);
}  // IsInternetShortcut

//-------------------------------------------------------------------------
mozilla::Result<nsCOMPtr<nsISupports>, nsresult>
nsClipboard::GetNativeClipboardData(const nsACString& aFlavor,
                                    ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  MOZ_CLIPBOARD_LOG("%s aWhichClipboard=%i", __FUNCTION__, aWhichClipboard);

  // This makes sure we can use the OLE functionality for the clipboard
  IDataObject* dataObj;
  if (S_OK == RepeatedlyTryOleGetClipboard(&dataObj)) {
    auto dataObjRelease = mozilla::MakeScopeExit([&] { dataObj->Release(); });
    // Use OLE IDataObject for clipboard operations
    MOZ_CLIPBOARD_LOG("    use OLE IDataObject:");
    if (MOZ_CLIPBOARD_LOG_ENABLED()) {
      IEnumFORMATETC* pEnum = nullptr;
      if (S_OK == dataObj->EnumFormatEtc(DATADIR_GET, &pEnum)) {
        auto pEnumRelease = mozilla::MakeScopeExit([&] { pEnum->Release(); });
        FORMATETC fEtc;
        while (S_OK == pEnum->Next(1, &fEtc, nullptr)) {
          nsAutoString format;
          mozilla::widget::WinUtils::GetClipboardFormatAsString(fEtc.cfFormat,
                                                                format);
          MOZ_CLIPBOARD_LOG("        FORMAT %s",
                            NS_ConvertUTF16toUTF8(format).get());
        }
      }
    }

    return GetDataFromDataObject(dataObj, 0, nullptr,
                                 PromiseFlatCString(aFlavor));
  }

  // do it the old manual way
  return GetDataFromDataObject(nullptr, 0, mWindow,
                               PromiseFlatCString(aFlavor));
}

nsresult nsClipboard::EmptyNativeClipboardData(ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  // Some programs such as ZoneAlarm monitor clipboard usage and then open the
  // clipboard to scan it.  If we i) empty and then ii) set data, then the
  // 'set data' can sometimes fail with access denied becacuse another program
  // has the clipboard open.  So to avoid this race condition for OpenClipboard
  // we do not empty the clipboard when we're setting it.
  RepeatedlyTryOleSetClipboard(nullptr);
  return NS_OK;
}

mozilla::Result<int32_t, nsresult>
nsClipboard::GetNativeClipboardSequenceNumber(ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(kGlobalClipboard == aWhichClipboard);
  return (int32_t)::GetClipboardSequenceNumber();
}

//-------------------------------------------------------------------------
mozilla::Result<bool, nsresult>
nsClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  for (const auto& flavor : aFlavorList) {
    UINT format = GetFormat(flavor.get());
    if (IsClipboardFormatAvailable(format)) {
      return true;
    }
    mozilla::Maybe<UINT> secondaryFormat = GetSecondaryFormat(flavor.get());
    if (secondaryFormat &&
        IsClipboardFormatAvailable(secondaryFormat.value())) {
      return true;
    }
  }
  return false;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::GetTempFilePath(const nsAString& aFileName,
                                      nsAString& aFilePath) {
  nsresult result = NS_OK;

  nsCOMPtr<nsIFile> tmpFile;
  result =
      GetSpecialSystemDirectory(OS_TemporaryDirectory, getter_AddRefs(tmpFile));
  NS_ENSURE_SUCCESS(result, result);

  result = tmpFile->Append(aFileName);
  NS_ENSURE_SUCCESS(result, result);

  result = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0660);
  NS_ENSURE_SUCCESS(result, result);
  result = tmpFile->GetPath(aFilePath);

  return result;
}

//-------------------------------------------------------------------------
nsresult nsClipboard::SaveStorageOrStream(IDataObject* aDataObject, UINT aIndex,
                                          const nsAString& aFileName) {
  NS_ENSURE_ARG_POINTER(aDataObject);

  FORMATETC fe = {0};
  SET_FORMATETC(fe, RegisterClipboardFormat(CFSTR_FILECONTENTS), 0,
                DVASPECT_CONTENT, aIndex, TYMED_ISTORAGE | TYMED_ISTREAM);

  STGMEDIUM stm = {0};
  HRESULT hres = aDataObject->GetData(&fe, &stm);
  if (FAILED(hres)) {
    return NS_ERROR_FAILURE;
  }

  auto releaseMediumGuard =
      mozilla::MakeScopeExit([&] { ReleaseStgMedium(&stm); });

  // We do this check because, even though we *asked* for IStorage or IStream,
  // it seems that IDataObject providers can just hand us back whatever they
  // feel like. See Bug 1824644 for a fun example of that!
  if (stm.tymed != TYMED_ISTORAGE && stm.tymed != TYMED_ISTREAM) {
    return NS_ERROR_FAILURE;
  }

  if (stm.tymed == TYMED_ISTORAGE) {
    // should never happen -- but theoretically possible, given an ill-behaved
    // data-source
    if (stm.pstg == nullptr) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<IStorage> file;
    hres = StgCreateStorageEx(
        aFileName.Data(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
        STGFMT_STORAGE, 0, NULL, NULL, IID_IStorage, getter_AddRefs(file));
    if (FAILED(hres)) {
      return NS_ERROR_FAILURE;
    }

    hres = stm.pstg->CopyTo(0, NULL, NULL, file);
    if (FAILED(hres)) {
      return NS_ERROR_FAILURE;
    }

    file->Commit(STGC_DEFAULT);

    return NS_OK;
  }

  MOZ_ASSERT(stm.tymed == TYMED_ISTREAM);
  // should never happen -- but possible given an ill-behaved data-source, and
  // has been seen in the wild (bug 1895681)
  if (stm.pstm == nullptr) {
    return NS_ERROR_FAILURE;
  }

  HANDLE handle = CreateFile(aFileName.Data(), GENERIC_WRITE, FILE_SHARE_READ,
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    return NS_ERROR_FAILURE;
  }

  auto fileCloseGuard = mozilla::MakeScopeExit([&] { CloseHandle(handle); });

  const ULONG bufferSize = 4096;
  char buffer[bufferSize] = {0};
  ULONG bytesRead = 0;
  DWORD bytesWritten = 0;
  while (true) {
    HRESULT result = stm.pstm->Read(buffer, bufferSize, &bytesRead);
    if (FAILED(result)) {
      return NS_ERROR_FAILURE;
    }
    if (bytesRead == 0) {
      break;
    }
    if (!WriteFile(handle, buffer, static_cast<DWORD>(bytesRead), &bytesWritten,
                   NULL)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}
