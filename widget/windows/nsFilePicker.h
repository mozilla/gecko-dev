/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include <windows.h>

#include "mozilla/MozPromise.h"
#include "nsCOMArray.h"
#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsdefs.h"
#include <commdlg.h>
#include <shobjidl.h>
#undef LogSeverity  // SetupAPI.h #defines this as DWORD

class nsIFile;
class nsILoadContext;
class nsISimpleEnumerator;

namespace mozilla {

namespace dom {
class BrowsingContext;
}  // namespace dom

namespace widget::filedialog {
class Command;
class Results;
enum class FileDialogType : uint8_t;
struct Error;
}  // namespace widget::filedialog

}  // namespace mozilla

class nsBaseWinFilePicker : public nsBaseFilePicker {
 public:
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString) override;
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString) override;
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension) override;
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension) override;

 protected:
  nsString mDefaultFilePath;
  nsString mDefaultFilename;
  nsString mDefaultExtension;
};

/**
 * Native Windows FileSelector wrapper
 */

class nsFilePicker final : public nsBaseWinFilePicker {
  virtual ~nsFilePicker() = default;

  template <typename T>
  using Maybe = mozilla::Maybe<T>;
  template <typename T>
  using Result = mozilla::Result<T, HRESULT>;

  using Command = mozilla::widget::filedialog::Command;
  using Results = mozilla::widget::filedialog::Results;
  using FileDialogType = mozilla::widget::filedialog::FileDialogType;
  using Error = mozilla::widget::filedialog::Error;

 public:
  nsFilePicker();

  NS_IMETHOD Init(mozilla::dom::BrowsingContext* aBrowsingContext,
                  const nsAString& aTitle, nsIFilePicker::Mode aMode) override;

  NS_DECL_ISUPPORTS

  // nsIFilePicker (less what's in nsBaseFilePicker and nsBaseWinFilePicker)
  NS_IMETHOD GetFilterIndex(int32_t* aFilterIndex) override;
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
  NS_IMETHOD GetFile(nsIFile** aFile) override;
  NS_IMETHOD GetFileURL(nsIURI** aFileURL) override;
  NS_IMETHOD GetFiles(nsISimpleEnumerator** aFiles) override;
  NS_IMETHOD AppendFilter(const nsAString& aTitle,
                          const nsAString& aFilter) override;

 protected:
  /* method from nsBaseFilePicker */
  virtual void InitNative(nsIWidget* aParent, const nsAString& aTitle) override;
  void GetFilterListArray(nsString& aFilterList);

  NS_IMETHOD Open(nsIFilePickerShownCallback* aCallback) override;

 private:
  using Unit = mozilla::Ok;
  RefPtr<mozilla::MozPromise<bool, Error, true>> ShowFolderPicker(
      const nsString& aInitialDir);
  RefPtr<mozilla::MozPromise<bool, Error, true>> ShowFilePicker(
      const nsString& aInitialDir);

  void ClearFiles();
  using ContentAnalysisResponse = mozilla::MozPromise<bool, nsresult, true>;
  RefPtr<ContentAnalysisResponse> CheckContentAnalysisService();

 protected:
  void RememberLastUsedDirectory();
  bool IsPrivacyModeEnabled();
  bool IsDefaultPathLink();
  bool IsDefaultPathHtml();

  using FallbackResult = mozilla::Result<RefPtr<nsIFile>, nsresult>;
  FallbackResult ComputeFallbackSavePath() const;
  void SendFailureNotification(ResultCode aResult, Error error,
                               FallbackResult fallback) const;

  nsCOMPtr<nsIWidget> mParentWidget;
  nsString mTitle;
  nsCString mFile;
  int32_t mSelectedType = 1;
  nsCOMArray<nsIFile> mFiles;
  nsString mUnicodeFile;

  struct FreeDeleter {
    void operator()(void* aPtr) { ::free(aPtr); }
  };
  static mozilla::UniquePtr<char16_t[], FreeDeleter> sLastUsedUnicodeDirectory;

  struct Filter {
    nsString title;
    nsString filter;
  };
  AutoTArray<Filter, 1> mFilterList;
};

#endif  // nsFilePicker_h__
