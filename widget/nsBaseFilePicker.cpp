/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWidget.h"

#include "nsArrayEnumerator.h"
#include "nsIStringBundle.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsIFile.h"
#include "nsEnumeratorUtils.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_widget.h"
#include "WidgetUtils.h"
#include "nsSimpleEnumerator.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"

#include "nsBaseFilePicker.h"

using namespace mozilla::widget;
using namespace mozilla::dom;
using mozilla::ErrorResult;

#define FILEPICKER_TITLES "chrome://global/locale/filepicker.properties"
#define FILEPICKER_FILTERS "chrome://global/content/filepicker.properties"

namespace {

nsresult LocalFileToDirectoryOrBlob(nsPIDOMWindowInner* aWindow,
                                    bool aIsDirectory, nsIFile* aFile,
                                    nsISupports** aResult) {
  MOZ_ASSERT(aWindow);

  if (aIsDirectory) {
#ifdef DEBUG
    bool isDir;
    aFile->IsDirectory(&isDir);
    MOZ_ASSERT(isDir);
#endif

    RefPtr<Directory> directory = Directory::Create(aWindow->AsGlobal(), aFile);
    MOZ_ASSERT(directory);

    directory.forget(aResult);
    return NS_OK;
  }

  RefPtr<File> file = File::CreateFromFile(aWindow->AsGlobal(), aFile);
  if (NS_WARN_IF(!file)) {
    return NS_ERROR_FAILURE;
  }

  file.forget(aResult);
  return NS_OK;
}

}  // anonymous namespace

class nsBaseFilePickerEnumerator : public nsSimpleEnumerator {
 public:
  nsBaseFilePickerEnumerator(nsPIDOMWindowOuter* aParent,
                             nsISimpleEnumerator* iterator,
                             nsIFilePicker::Mode aMode)
      : mIterator(iterator),
        mParent(aParent->GetCurrentInnerWindow()),
        mMode(aMode) {}

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIFile); }

  NS_IMETHOD
  GetNext(nsISupports** aResult) override {
    nsCOMPtr<nsISupports> tmp;
    nsresult rv = mIterator->GetNext(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!tmp) {
      return NS_OK;
    }

    nsCOMPtr<nsIFile> localFile = do_QueryInterface(tmp);
    if (!localFile) {
      return NS_ERROR_FAILURE;
    }

    if (!mParent) {
      return NS_ERROR_FAILURE;
    }

    return LocalFileToDirectoryOrBlob(
        mParent, mMode == nsIFilePicker::modeGetFolder, localFile, aResult);
  }

  NS_IMETHOD
  HasMoreElements(bool* aResult) override {
    return mIterator->HasMoreElements(aResult);
  }

 private:
  nsCOMPtr<nsISimpleEnumerator> mIterator;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsIFilePicker::Mode mMode;
};

nsBaseFilePicker::nsBaseFilePicker() = default;

nsBaseFilePicker::~nsBaseFilePicker() = default;

NS_IMETHODIMP nsBaseFilePicker::Init(BrowsingContext* aBrowsingContext,
                                     const nsAString& aTitle,
                                     nsIFilePicker::Mode aMode) {
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_ARG_POINTER(aBrowsingContext);

  nsCOMPtr<nsIWidget> widget =
      aBrowsingContext->Canonical()->GetParentProcessWidgetContaining();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  mBrowsingContext = aBrowsingContext;
  mMode = aMode;
  InitNative(widget, aTitle);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::IsModeSupported(nsIFilePicker::Mode aMode, JSContext* aCx,
                                  Promise** aPromise) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aPromise);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  promise->MaybeResolve(true);
  promise.forget(aPromise);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::AppendFilters(int32_t aFilterMask) {
  nsCOMPtr<nsIStringBundleService> stringService =
      mozilla::components::StringBundle::Service();
  if (!stringService) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStringBundle> titleBundle, filterBundle;

  nsresult rv = stringService->CreateBundle(FILEPICKER_TITLES,
                                            getter_AddRefs(titleBundle));
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  rv = stringService->CreateBundle(FILEPICKER_FILTERS,
                                   getter_AddRefs(filterBundle));
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString title;
  nsAutoString filter;

  if (aFilterMask & filterAll) {
    titleBundle->GetStringFromName("allTitle", title);
    filterBundle->GetStringFromName("allFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterHTML) {
    titleBundle->GetStringFromName("htmlTitle", title);
    filterBundle->GetStringFromName("htmlFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterText) {
    titleBundle->GetStringFromName("textTitle", title);
    filterBundle->GetStringFromName("textFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterImages) {
    titleBundle->GetStringFromName("imageTitle", title);
    filterBundle->GetStringFromName("imageFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterAudio) {
    titleBundle->GetStringFromName("audioTitle", title);
    filterBundle->GetStringFromName("audioFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterVideo) {
    titleBundle->GetStringFromName("videoTitle", title);
    filterBundle->GetStringFromName("videoFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterXML) {
    titleBundle->GetStringFromName("xmlTitle", title);
    filterBundle->GetStringFromName("xmlFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterXUL) {
    titleBundle->GetStringFromName("xulTitle", title);
    filterBundle->GetStringFromName("xulFilter", filter);
    AppendFilter(title, filter);
  }
  if (aFilterMask & filterApps) {
    titleBundle->GetStringFromName("appsTitle", title);
    // Pass the magic string "..apps" to the platform filepicker, which it
    // should recognize and do the correct platform behavior for.
    AppendFilter(title, u"..apps"_ns);
  }
  if (aFilterMask & filterPDF) {
    titleBundle->GetStringFromName("pdfTitle", title);
    filterBundle->GetStringFromName("pdfFilter", filter);
    AppendFilter(title, filter);
  }
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::AppendRawFilter(const nsAString& aFilter) {
  mRawFilters.AppendElement(aFilter);
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::GetCapture(
    nsIFilePicker::CaptureTarget* aCapture) {
  *aCapture = nsIFilePicker::CaptureTarget::captureNone;
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::SetCapture(
    nsIFilePicker::CaptureTarget aCapture) {
  return NS_OK;
}

// Set the filter index
NS_IMETHODIMP nsBaseFilePicker::GetFilterIndex(int32_t* aFilterIndex) {
  *aFilterIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::SetFilterIndex(int32_t aFilterIndex) {
  return NS_OK;
}

NS_IMETHODIMP nsBaseFilePicker::GetFiles(nsISimpleEnumerator** aFiles) {
  NS_ENSURE_ARG_POINTER(aFiles);
  nsCOMArray<nsIFile> files;
  nsresult rv;

  // if we get into the base class, the platform
  // doesn't implement GetFiles() yet.
  // so we fake it.
  nsCOMPtr<nsIFile> file;
  rv = GetFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  files.AppendObject(file);

  return NS_NewArrayEnumerator(aFiles, files, NS_GET_IID(nsIFile));
}

// Set the display directory
NS_IMETHODIMP nsBaseFilePicker::SetDisplayDirectory(nsIFile* aDirectory) {
  // if displaySpecialDirectory has been previously called, let's abort this
  // operation.
  if (!mDisplaySpecialDirectory.IsEmpty()) {
    return NS_OK;
  }

  if (!aDirectory) {
    mDisplayDirectory = nullptr;
    return NS_OK;
  }
  nsCOMPtr<nsIFile> directory;
  nsresult rv = aDirectory->Clone(getter_AddRefs(directory));
  if (NS_FAILED(rv)) {
    return rv;
  }

  mDisplayDirectory = directory;
  return NS_OK;
}

// Get the display directory
NS_IMETHODIMP nsBaseFilePicker::GetDisplayDirectory(nsIFile** aDirectory) {
  *aDirectory = nullptr;

  // if displaySpecialDirectory has been previously called, let's abort this
  // operation.
  if (!mDisplaySpecialDirectory.IsEmpty()) {
    return NS_OK;
  }

  if (!mDisplayDirectory) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> directory;
  nsresult rv = mDisplayDirectory->Clone(getter_AddRefs(directory));
  if (NS_FAILED(rv)) {
    return rv;
  }
  directory.forget(aDirectory);
  return NS_OK;
}

// Set the display special directory
NS_IMETHODIMP nsBaseFilePicker::SetDisplaySpecialDirectory(
    const nsAString& aDirectory) {
  // if displayDirectory has been previously called, let's abort this operation.
  if (mDisplayDirectory && mDisplaySpecialDirectory.IsEmpty()) {
    return NS_OK;
  }

  mDisplaySpecialDirectory = aDirectory;
  if (mDisplaySpecialDirectory.IsEmpty()) {
    mDisplayDirectory = nullptr;
    return NS_OK;
  }

  return ResolveSpecialDirectory(aDirectory);
}

bool nsBaseFilePicker::MaybeBlockFilePicker(
    nsIFilePickerShownCallback* aCallback) {
  MOZ_ASSERT(mBrowsingContext);
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mozilla::StaticPrefs::widget_disable_file_pickers()) {
    if (aCallback) {
      // File pickers are disabled, so we answer the callback with returnCancel.
      NS_DispatchToCurrentThread(
          mozilla::NewRunnableMethod<nsIFilePicker::ResultCode>(
              "nsBaseFilePicker::CallbackWithCancelResult", aCallback,
              &nsIFilePickerShownCallback::Done, nsIFilePicker::returnCancel));
    }

    RefPtr<Element> topFrameElement = mBrowsingContext->GetTopFrameElement();
    if (topFrameElement) {
      // Dispatch an event that the frontend may use.
      nsContentUtils::DispatchEventOnlyToChrome(
          topFrameElement->OwnerDoc(), topFrameElement, u"FilePickerBlocked"_ns,
          mozilla::CanBubble::eYes, mozilla::Cancelable::eNo);
    }

    return true;
  }

  if (mBrowsingContext->Canonical()->CanOpenModalPicker()) {
    return false;
  }

  if (aCallback) {
    // File pickers are not allowed to open, so we respond to the callback with
    // returnCancel.
    NS_DispatchToCurrentThread(
        mozilla::NewRunnableMethod<nsIFilePicker::ResultCode>(
            "nsBaseFilePicker::CallbackWithCancelResult", aCallback,
            &nsIFilePickerShownCallback::Done, nsIFilePicker::returnCancel));
  }

  return true;
}

nsresult nsBaseFilePicker::ResolveSpecialDirectory(
    const nsAString& aSpecialDirectory) {
  // Only perform special-directory name resolution in the parent process.
  // (Subclasses of `nsBaseFilePicker` used in other processes must override
  // this function.)
  MOZ_ASSERT(XRE_IsParentProcess());
  return NS_GetSpecialDirectory(NS_ConvertUTF16toUTF8(aSpecialDirectory).get(),
                                getter_AddRefs(mDisplayDirectory));
}

// Get the display special directory
NS_IMETHODIMP nsBaseFilePicker::GetDisplaySpecialDirectory(
    nsAString& aDirectory) {
  aDirectory = mDisplaySpecialDirectory;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetAddToRecentDocs(bool* aFlag) {
  *aFlag = mAddToRecentDocs;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::SetAddToRecentDocs(bool aFlag) {
  mAddToRecentDocs = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetMode(nsIFilePicker::Mode* aMode) {
  *aMode = mMode;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::SetOkButtonLabel(const nsAString& aLabel) {
  mOkButtonLabel = aLabel;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetOkButtonLabel(nsAString& aLabel) {
  aLabel = mOkButtonLabel;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomFileOrDirectory(nsISupports** aValue) {
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_ARG_POINTER(mBrowsingContext);
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = GetFile(getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!localFile) {
    *aValue = nullptr;
    return NS_OK;
  }

  auto* innerParent =
      mBrowsingContext->GetDOMWindow()
          ? mBrowsingContext->GetDOMWindow()->GetCurrentInnerWindow()
          : nullptr;

  if (!innerParent) {
    return NS_ERROR_FAILURE;
  }

  return LocalFileToDirectoryOrBlob(
      innerParent, mMode == nsIFilePicker::modeGetFolder, localFile, aValue);
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomFileOrDirectoryEnumerator(
    nsISimpleEnumerator** aValue) {
  nsCOMPtr<nsISimpleEnumerator> iter;
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_ARG_POINTER(mBrowsingContext);
  nsresult rv = GetFiles(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);

  auto* parent = mBrowsingContext->GetDOMWindow();

  if (!parent) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsBaseFilePickerEnumerator> retIter =
      new nsBaseFilePickerEnumerator(parent, iter, mMode);

  retIter.forget(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseFilePicker::GetDomFilesInWebKitDirectory(nsISimpleEnumerator** aValue) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
