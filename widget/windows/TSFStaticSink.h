/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TSFStaticSink_h
#define TSFStaticSink_h

#include <msctf.h>
#include <windows.h>
#include <winuser.h>

#include "TSFTextInputProcessorList.h"
#include "WinUtils.h"

#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla::widget {

class TSFStaticSink final : public ITfInputProcessorProfileActivationSink {
 public:
  static TSFStaticSink* GetInstance();

  static void Shutdown() {
    if (sInstance) {
      sInstance->Destroy();
      sInstance = nullptr;
    }
  }

  bool Init(ITfThreadMgr* aThreadMgr,
            ITfInputProcessorProfiles* aInputProcessorProfiles);
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
    *ppv = nullptr;
    if (IID_IUnknown == riid ||
        IID_ITfInputProcessorProfileActivationSink == riid) {
      *ppv = static_cast<ITfInputProcessorProfileActivationSink*>(this);
    }
    if (*ppv) {
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(TSFStaticSink)

  [[nodiscard]] const nsString& GetActiveTIPKeyboardDescription() const {
    return mActiveTIPKeyboardDescription;
  }

  [[nodiscard]] static bool IsIMM_IMEActive() {
    // Use IMM API until TSFStaticSink starts to work.
    if (!sInstance || !sInstance->EnsureInitActiveTIPKeyboard()) {
      return IsIMM_IME(::GetKeyboardLayout(0));
    }
    return sInstance->mIsIMM_IME;
  }

  [[nodiscard]] static bool IsIMM_IME(HKL aHKL) {
    return (::ImmGetIMEFileNameW(aHKL, nullptr, 0) > 0);
  }

  [[nodiscard]] static bool IsTraditionalChinese() {
    EnsureInstance();
    return sInstance && sInstance->IsTraditionalChineseInternal();
  }
  [[nodiscard]] static bool IsSimplifiedChinese() {
    EnsureInstance();
    return sInstance && sInstance->IsSimplifiedChineseInternal();
  }
  [[nodiscard]] static bool IsJapanese() {
    EnsureInstance();
    return sInstance && sInstance->IsJapaneseInternal();
  }
  [[nodiscard]] static bool IsKorean() {
    EnsureInstance();
    return sInstance && sInstance->IsKoreanInternal();
  }

  /**
   * ActiveTIP() returns an ID for currently active TIP.
   * Please note that this method is expensive due to needs a lot of GUID
   * comparisons if active language ID is one of CJKT.  If you need to
   * check TIPs for a specific language, you should check current language
   * first.
   */
  [[nodiscard]] static TextInputProcessorID ActiveTIP() {
    EnsureInstance();
    if (!sInstance || !sInstance->EnsureInitActiveTIPKeyboard()) {
      return TextInputProcessorID::Unknown;
    }
    sInstance->ComputeActiveTextInputProcessor();
    if (NS_WARN_IF(sInstance->mActiveTIP ==
                   TextInputProcessorID::NotComputed)) {
      return TextInputProcessorID::Unknown;
    }
    return sInstance->mActiveTIP;
  }

  static bool GetActiveTIPNameForTelemetry(nsAString& aName);

  static bool IsMSChangJieOrMSQuickActive();
  static bool IsMSPinyinOrMSWubiActive();
  static bool IsMSJapaneseIMEActive();
  static bool IsGoogleJapaneseInputActive();
  static bool IsATOKActive();

  // Note that ATOK 2011 - 2016 refers native caret position for deciding its
  // popup window position.
  static bool IsATOKReferringNativeCaretActive();

 private:
  static void EnsureInstance() {
    if (!sInstance) {
      RefPtr<TSFStaticSink> staticSink = GetInstance();
      Unused << staticSink;
    }
  }

  [[nodiscard]] bool IsTraditionalChineseInternal() const {
    return mLangID == 0x0404;
  }
  [[nodiscard]] bool IsSimplifiedChineseInternal() const {
    return mLangID == 0x0804;
  }
  [[nodiscard]] bool IsJapaneseInternal() const { return mLangID == 0x0411; }
  [[nodiscard]] bool IsKoreanInternal() const { return mLangID == 0x0412; }
  [[nodiscard]] bool IsATOKActiveInternal();

  void ComputeActiveTextInputProcessor();

  [[nodiscard]] TextInputProcessorID ComputeActiveTIPAsJapanese();
  [[nodiscard]] TextInputProcessorID ComputeActiveTIPAsTraditionalChinese();
  [[nodiscard]] TextInputProcessorID ComputeActiveTIPAsSimplifiedChinese();
  [[nodiscard]] TextInputProcessorID ComputeActiveTIPAsKorean();

 public:  // ITfInputProcessorProfileActivationSink
  STDMETHODIMP OnActivated(DWORD, LANGID, REFCLSID, REFGUID, REFGUID, HKL,
                           DWORD);

 private:
  TSFStaticSink() = default;
  virtual ~TSFStaticSink() = default;

  bool EnsureInitActiveTIPKeyboard();

  void Destroy();

  void GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                         REFGUID aProfile, nsAString& aDescription);
  [[nodiscard]] bool IsTIPCategoryKeyboard(REFCLSID aTextService,
                                           LANGID aLangID, REFGUID aProfile);

  TextInputProcessorID mActiveTIP = TextInputProcessorID::NotComputed;

  // Cookie of installing ITfInputProcessorProfileActivationSink
  DWORD mIPProfileCookie = TF_INVALID_COOKIE;

  LANGID mLangID = 0;

  // True if current IME is implemented with IMM.
  bool mIsIMM_IME = false;
  // True if OnActivated() is already called
  bool mOnActivatedCalled = false;

  RefPtr<ITfThreadMgr> mThreadMgr;
  RefPtr<ITfInputProcessorProfiles> mInputProcessorProfiles;

  // Active TIP keyboard's description.  If active language profile isn't TIP,
  // i.e., IMM-IME or just a keyboard layout, this is empty.
  nsString mActiveTIPKeyboardDescription;

  // Active TIP's GUID and CLSID
  GUID mActiveTIPGUID = GUID_NULL;
  CLSID mActiveTIPCLSID = CLSID_NULL;

  static StaticRefPtr<TSFStaticSink> sInstance;
};

}  // namespace mozilla::widget

#endif  // #ifndef TSFStaticSink_h
