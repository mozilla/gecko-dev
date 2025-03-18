/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TSFStaticSink.h"

#include "TSFTextStore.h"
#include "TSFUtils.h"
#include "WinIMEHandler.h"
#include "WinMessages.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/glean/WidgetWindowsMetrics.h"

#include <comutil.h>  // for _bstr_t
#include <oleauto.h>  // for SysAllocString
#include <olectl.h>

extern mozilla::LazyLogModule gIMELog;  // defined in TSFUtils.cpp

namespace mozilla::widget {

StaticRefPtr<TSFStaticSink> TSFStaticSink::sInstance;

// static
TSFStaticSink* TSFStaticSink::GetInstance() {
  if (!sInstance) {
    RefPtr<ITfThreadMgr> threadMgr = TSFTextStore::GetThreadMgr();
    if (NS_WARN_IF(!threadMgr)) {
      MOZ_LOG(
          gIMELog, LogLevel::Error,
          ("TSFStaticSink::GetInstance() FAILED to initialize TSFStaticSink "
           "instance due to no ThreadMgr instance"));
      return nullptr;
    }
    RefPtr<ITfInputProcessorProfiles> inputProcessorProfiles =
        TSFTextStore::GetInputProcessorProfiles();
    if (NS_WARN_IF(!inputProcessorProfiles)) {
      MOZ_LOG(
          gIMELog, LogLevel::Error,
          ("TSFStaticSink::GetInstance() FAILED to initialize TSFStaticSink "
           "instance due to no InputProcessorProfiles instance"));
      return nullptr;
    }
    RefPtr<TSFStaticSink> staticSink = new TSFStaticSink();
    if (NS_WARN_IF(!staticSink->Init(threadMgr, inputProcessorProfiles))) {
      staticSink->Destroy();
      MOZ_LOG(
          gIMELog, LogLevel::Error,
          ("TSFStaticSink::GetInstance() FAILED to initialize TSFStaticSink "
           "instance"));
      return nullptr;
    }
    sInstance = staticSink.forget();
  }
  return sInstance;
}

// static
bool TSFStaticSink::GetActiveTIPNameForTelemetry(nsAString& aName) {
  if (!sInstance || !sInstance->EnsureInitActiveTIPKeyboard()) {
    return false;
  }
  if (sInstance->mActiveTIPGUID == GUID_NULL) {
    aName.Truncate();
    aName.AppendPrintf("0x%04X", sInstance->mLangID);
    return true;
  }
  // key should be "LocaleID|Description".  Although GUID of the
  // profile is unique key since description may be localized for system
  // language, unfortunately, it's too long to record as key with its
  // description.  Therefore, we should record only the description with
  // LocaleID because Microsoft IME may not include language information.
  // 72 is kMaximumKeyStringLength in TelemetryScalar.cpp
  aName.Truncate();
  aName.AppendPrintf("0x%04X|", sInstance->mLangID);
  nsAutoString description;
  description.Assign(sInstance->mActiveTIPKeyboardDescription);
  static const uint32_t kMaxDescriptionLength = 72 - aName.Length();
  if (description.Length() > kMaxDescriptionLength) {
    if (NS_IS_LOW_SURROGATE(description[kMaxDescriptionLength - 1]) &&
        NS_IS_HIGH_SURROGATE(description[kMaxDescriptionLength - 2])) {
      description.Truncate(kMaxDescriptionLength - 2);
    } else {
      description.Truncate(kMaxDescriptionLength - 1);
    }
    // U+2026 is "..."
    description.Append(char16_t(0x2026));
  }
  aName.Append(description);
  return true;
}

// static
bool TSFStaticSink::IsMSChangJieOrMSQuickActive() {
  // ActiveTIP() is expensive if it hasn't computed active TIP yet.
  // For avoiding unnecessary computation, we should check if the language
  // for current TIP is Traditional Chinese.
  if (!IsTraditionalChinese()) {
    return false;
  }
  switch (ActiveTIP()) {
    case TextInputProcessorID::MicrosoftChangJie:
    case TextInputProcessorID::MicrosoftQuick:
      return true;
    default:
      return false;
  }
}

// static
bool TSFStaticSink::IsMSPinyinOrMSWubiActive() {
  // ActiveTIP() is expensive if it hasn't computed active TIP yet.
  // For avoiding unnecessary computation, we should check if the language
  // for current TIP is Simplified Chinese.
  if (!IsSimplifiedChinese()) {
    return false;
  }
  switch (ActiveTIP()) {
    case TextInputProcessorID::MicrosoftPinyin:
    case TextInputProcessorID::MicrosoftWubi:
      return true;
    default:
      return false;
  }
}

// static
bool TSFStaticSink::IsMSJapaneseIMEActive() {
  // ActiveTIP() is expensive if it hasn't computed active TIP yet.
  // For avoiding unnecessary computation, we should check if the language
  // for current TIP is Japanese.
  if (!IsJapanese()) {
    return false;
  }
  return ActiveTIP() == TextInputProcessorID::MicrosoftIMEForJapanese;
}

// static
bool TSFStaticSink::IsGoogleJapaneseInputActive() {
  // ActiveTIP() is expensive if it hasn't computed active TIP yet.
  // For avoiding unnecessary computation, we should check if the language
  // for current TIP is Japanese.
  if (!IsJapanese()) {
    return false;
  }
  return ActiveTIP() == TextInputProcessorID::GoogleJapaneseInput;
}

// static
bool TSFStaticSink::IsATOKActive() {
  // ActiveTIP() is expensive if it hasn't computed active TIP yet.
  // For avoiding unnecessary computation, we should check if active TIP is
  // ATOK first since it's cheaper.
  return IsJapanese() && sInstance->IsATOKActiveInternal();
}

// static
bool TSFStaticSink::IsATOKReferringNativeCaretActive() {
  // ActiveTIP() is expensive if it hasn't computed active TIP yet.
  // For avoiding unnecessary computation, we should check if active TIP is
  // ATOK first since it's cheaper.
  if (!IsJapanese() || !sInstance->IsATOKActiveInternal()) {
    return false;
  }
  switch (ActiveTIP()) {
    case TextInputProcessorID::ATOK2011:
    case TextInputProcessorID::ATOK2012:
    case TextInputProcessorID::ATOK2013:
    case TextInputProcessorID::ATOK2014:
    case TextInputProcessorID::ATOK2015:
      return true;
    default:
      return false;
  }
}

bool TSFStaticSink::IsATOKActiveInternal() {
  EnsureInitActiveTIPKeyboard();
  // FYI: Name of packaged ATOK includes the release year like "ATOK 2015".
  //      Name of ATOK Passport (subscription) equals "ATOK".
  return StringBeginsWith(mActiveTIPKeyboardDescription, u"ATOK "_ns) ||
         mActiveTIPKeyboardDescription.EqualsLiteral("ATOK");
}

void TSFStaticSink::ComputeActiveTextInputProcessor() {
  if (mActiveTIP != TextInputProcessorID::NotComputed) {
    return;
  }

  if (mActiveTIPGUID == GUID_NULL) {
    mActiveTIP = TextInputProcessorID::None;
    return;
  }

  // Comparing GUID is slow. So, we should use language information to
  // reduce the comparing cost for TIP which is not we do not support
  // specifically since they are always compared with all supported TIPs.
  switch (mLangID) {
    case 0x0404:
      mActiveTIP = ComputeActiveTIPAsTraditionalChinese();
      break;
    case 0x0411:
      mActiveTIP = ComputeActiveTIPAsJapanese();
      break;
    case 0x0412:
      mActiveTIP = ComputeActiveTIPAsKorean();
      break;
    case 0x0804:
      mActiveTIP = ComputeActiveTIPAsSimplifiedChinese();
      break;
    default:
      mActiveTIP = TextInputProcessorID::Unknown;
      break;
  }
  // Special case for Keyman Desktop, it is available for any languages.
  // Therefore, we need to check it only if we don't know the active TIP.
  if (mActiveTIP != TextInputProcessorID::Unknown) {
    return;
  }

  // Note that keyboard layouts for Keyman assign its GUID on install
  // randomly, but CLSID is constant in any environments.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1670834#c7
  // https://github.com/keymanapp/keyman/blob/318c73a9e1d571d942837ff9964590626e5bd5aa/windows/src/engine/kmtip/globals.cpp#L37
  // {FE0420F1-38D1-4B4C-96BF-E7E20A74CFB7}
  static constexpr CLSID kKeymanDesktop_CLSID = {
      0xFE0420F1,
      0x38D1,
      0x4B4C,
      {0x96, 0xBF, 0xE7, 0xE2, 0x0A, 0x74, 0xCF, 0xB7}};
  if (mActiveTIPCLSID == kKeymanDesktop_CLSID) {
    mActiveTIP = TextInputProcessorID::KeymanDesktop;
  }
}

TextInputProcessorID TSFStaticSink::ComputeActiveTIPAsJapanese() {
  // {A76C93D9-5523-4E90-AAFA-4DB112F9AC76} (Win7, Win8.1, Win10)
  static constexpr GUID kMicrosoftIMEForJapaneseGUID = {
      0xA76C93D9,
      0x5523,
      0x4E90,
      {0xAA, 0xFA, 0x4D, 0xB1, 0x12, 0xF9, 0xAC, 0x76}};
  if (mActiveTIPGUID == kMicrosoftIMEForJapaneseGUID) {
    return TextInputProcessorID::MicrosoftIMEForJapanese;
  }
  // {54EDCC94-1524-4BB1-9FB7-7BABE4F4CA64}
  static constexpr GUID kMicrosoftOfficeIME2010ForJapaneseGUID = {
      0x54EDCC94,
      0x1524,
      0x4BB1,
      {0x9F, 0xB7, 0x7B, 0xAB, 0xE4, 0xF4, 0xCA, 0x64}};
  if (mActiveTIPGUID == kMicrosoftOfficeIME2010ForJapaneseGUID) {
    return TextInputProcessorID::MicrosoftOfficeIME2010ForJapanese;
  }
  // {773EB24E-CA1D-4B1B-B420-FA985BB0B80D}
  static constexpr GUID kGoogleJapaneseInputGUID = {
      0x773EB24E,
      0xCA1D,
      0x4B1B,
      {0xB4, 0x20, 0xFA, 0x98, 0x5B, 0xB0, 0xB8, 0x0D}};
  if (mActiveTIPGUID == kGoogleJapaneseInputGUID) {
    return TextInputProcessorID::GoogleJapaneseInput;
  }
  // {F9C24A5C-8A53-499D-9572-93B2FF582115}
  static const GUID kATOK2011GUID = {
      0xF9C24A5C,
      0x8A53,
      0x499D,
      {0x95, 0x72, 0x93, 0xB2, 0xFF, 0x58, 0x21, 0x15}};
  if (mActiveTIPGUID == kATOK2011GUID) {
    return TextInputProcessorID::ATOK2011;
  }
  // {1DE01562-F445-401B-B6C3-E5B18DB79461}
  static constexpr GUID kATOK2012GUID = {
      0x1DE01562,
      0xF445,
      0x401B,
      {0xB6, 0xC3, 0xE5, 0xB1, 0x8D, 0xB7, 0x94, 0x61}};
  if (mActiveTIPGUID == kATOK2012GUID) {
    return TextInputProcessorID::ATOK2012;
  }
  // {3C4DB511-189A-4168-B6EA-BFD0B4C85615}
  static constexpr GUID kATOK2013GUID = {
      0x3C4DB511,
      0x189A,
      0x4168,
      {0xB6, 0xEA, 0xBF, 0xD0, 0xB4, 0xC8, 0x56, 0x15}};
  if (mActiveTIPGUID == kATOK2013GUID) {
    return TextInputProcessorID::ATOK2013;
  }
  // {4EF33B79-6AA9-4271-B4BF-9321C279381B}
  static constexpr GUID kATOK2014GUID = {
      0x4EF33B79,
      0x6AA9,
      0x4271,
      {0xB4, 0xBF, 0x93, 0x21, 0xC2, 0x79, 0x38, 0x1B}};
  if (mActiveTIPGUID == kATOK2014GUID) {
    return TextInputProcessorID::ATOK2014;
  }
  // {EAB4DC00-CE2E-483D-A86A-E6B99DA9599A}
  static constexpr GUID kATOK2015GUID = {
      0xEAB4DC00,
      0xCE2E,
      0x483D,
      {0xA8, 0x6A, 0xE6, 0xB9, 0x9D, 0xA9, 0x59, 0x9A}};
  if (mActiveTIPGUID == kATOK2015GUID) {
    return TextInputProcessorID::ATOK2015;
  }
  // {0B557B4C-5740-4110-A60A-1493FA10BF2B}
  static constexpr GUID kATOK2016GUID = {
      0x0B557B4C,
      0x5740,
      0x4110,
      {0xA6, 0x0A, 0x14, 0x93, 0xFA, 0x10, 0xBF, 0x2B}};
  if (mActiveTIPGUID == kATOK2016GUID) {
    return TextInputProcessorID::ATOK2016;
  }

  // * ATOK 2017
  //   - {6DBFD8F5-701D-11E6-920F-782BCBA6348F}
  // * ATOK Passport (confirmed with version 31.1.2)
  //   - {A38F2FD9-7199-45E1-841C-BE0313D8052F}

  if (IsATOKActiveInternal()) {
    return TextInputProcessorID::ATOKUnknown;
  }

  // {E6D66705-1EDA-4373-8D01-1D0CB2D054C7}
  static constexpr GUID kJapanist10GUID = {
      0xE6D66705,
      0x1EDA,
      0x4373,
      {0x8D, 0x01, 0x1D, 0x0C, 0xB2, 0xD0, 0x54, 0xC7}};
  if (mActiveTIPGUID == kJapanist10GUID) {
    return TextInputProcessorID::Japanist10;
  }

  return TextInputProcessorID::Unknown;
}

TextInputProcessorID TSFStaticSink::ComputeActiveTIPAsTraditionalChinese() {
  // {B2F9C502-1742-11D4-9790-0080C882687E} (Win8.1, Win10)
  static constexpr GUID kMicrosoftBopomofoGUID = {
      0xB2F9C502,
      0x1742,
      0x11D4,
      {0x97, 0x90, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftBopomofoGUID) {
    return TextInputProcessorID::MicrosoftBopomofo;
  }
  // {4BDF9F03-C7D3-11D4-B2AB-0080C882687E} (Win7, Win8.1, Win10)
  static const GUID kMicrosoftChangJieGUID = {
      0x4BDF9F03,
      0xC7D3,
      0x11D4,
      {0xB2, 0xAB, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftChangJieGUID) {
    return TextInputProcessorID::MicrosoftChangJie;
  }
  // {761309DE-317A-11D4-9B5D-0080C882687E} (Win7)
  static constexpr GUID kMicrosoftPhoneticGUID = {
      0x761309DE,
      0x317A,
      0x11D4,
      {0x9B, 0x5D, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftPhoneticGUID) {
    return TextInputProcessorID::MicrosoftPhonetic;
  }
  // {6024B45F-5C54-11D4-B921-0080C882687E} (Win7, Win8.1, Win10)
  static constexpr GUID kMicrosoftQuickGUID = {
      0x6024B45F,
      0x5C54,
      0x11D4,
      {0xB9, 0x21, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftQuickGUID) {
    return TextInputProcessorID::MicrosoftQuick;
  }
  // {F3BA907A-6C7E-11D4-97FA-0080C882687E} (Win7)
  static constexpr GUID kMicrosoftNewChangJieGUID = {
      0xF3BA907A,
      0x6C7E,
      0x11D4,
      {0x97, 0xFA, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftNewChangJieGUID) {
    return TextInputProcessorID::MicrosoftNewChangJie;
  }
  // {B2F9C502-1742-11D4-9790-0080C882687E} (Win7)
  static constexpr GUID kMicrosoftNewPhoneticGUID = {
      0xB2F9C502,
      0x1742,
      0x11D4,
      {0x97, 0x90, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftNewPhoneticGUID) {
    return TextInputProcessorID::MicrosoftNewPhonetic;
  }
  // {0B883BA0-C1C7-11D4-87F9-0080C882687E} (Win7)
  static constexpr GUID kMicrosoftNewQuickGUID = {
      0x0B883BA0,
      0xC1C7,
      0x11D4,
      {0x87, 0xF9, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftNewQuickGUID) {
    return TextInputProcessorID::MicrosoftNewQuick;
  }

  // NOTE: There are some other Traditional Chinese TIPs installed in Windows:
  // * Chinese Traditional Array (version 6.0)
  //   - {D38EFF65-AA46-4FD5-91A7-67845FB02F5B} (Win7, Win8.1)
  // * Chinese Traditional DaYi (version 6.0)
  //   - {037B2C25-480C-4D7F-B027-D6CA6B69788A} (Win7, Win8.1)

  // {B58630B5-0ED3-4335-BBC9-E77BBCB43CAD}
  static const GUID kFreeChangJieGUID = {
      0xB58630B5,
      0x0ED3,
      0x4335,
      {0xBB, 0xC9, 0xE7, 0x7B, 0xBC, 0xB4, 0x3C, 0xAD}};
  if (mActiveTIPGUID == kFreeChangJieGUID) {
    return TextInputProcessorID::FreeChangJie;
  }

  return TextInputProcessorID::Unknown;
}

TextInputProcessorID TSFStaticSink::ComputeActiveTIPAsSimplifiedChinese() {
  // FYI: This matches with neither "Microsoft Pinyin ABC Input Style" nor
  //      "Microsoft Pinyin New Experience Input Style" on Win7.
  // {FA550B04-5AD7-411F-A5AC-CA038EC515D7} (Win8.1, Win10)
  static constexpr GUID kMicrosoftPinyinGUID = {
      0xFA550B04,
      0x5AD7,
      0x411F,
      {0xA5, 0xAC, 0xCA, 0x03, 0x8E, 0xC5, 0x15, 0xD7}};
  if (mActiveTIPGUID == kMicrosoftPinyinGUID) {
    return TextInputProcessorID::MicrosoftPinyin;
  }

  // {F3BA9077-6C7E-11D4-97FA-0080C882687E} (Win7)
  static constexpr GUID kMicrosoftPinyinNewExperienceInputStyleGUID = {
      0xF3BA9077,
      0x6C7E,
      0x11D4,
      {0x97, 0xFA, 0x00, 0x80, 0xC8, 0x82, 0x68, 0x7E}};
  if (mActiveTIPGUID == kMicrosoftPinyinNewExperienceInputStyleGUID) {
    return TextInputProcessorID::MicrosoftPinyinNewExperienceInputStyle;
  }
  // {82590C13-F4DD-44F4-BA1D-8667246FDF8E} (Win8.1, Win10)
  static constexpr GUID kMicrosoftWubiGUID = {
      0x82590C13,
      0xF4DD,
      0x44F4,
      {0xBA, 0x1D, 0x86, 0x67, 0x24, 0x6F, 0xDF, 0x8E}};
  if (mActiveTIPGUID == kMicrosoftWubiGUID) {
    return TextInputProcessorID::MicrosoftWubi;
  }
  // NOTE: There are some other Simplified Chinese TIPs installed in Windows:
  // * Chinese Simplified QuanPin (version 6.0)
  //   - {54FC610E-6ABD-4685-9DDD-A130BDF1B170} (Win8.1)
  // * Chinese Simplified ZhengMa (version 6.0)
  //   - {733B4D81-3BC3-4132-B91A-E9CDD5E2BFC9} (Win8.1)
  // * Chinese Simplified ShuangPin (version 6.0)
  //   - {EF63706D-31C4-490E-9DBB-BD150ADC454B} (Win8.1)
  // * Microsoft Pinyin ABC Input Style
  //   - {FCA121D2-8C6D-41FB-B2DE-A2AD110D4820} (Win7)
  return TextInputProcessorID::Unknown;
}

TextInputProcessorID TSFStaticSink::ComputeActiveTIPAsKorean() {
  // {B5FE1F02-D5F2-4445-9C03-C568F23C99A1} (Win7, Win8.1, Win10)
  static constexpr GUID kMicrosoftIMEForKoreanGUID = {
      0xB5FE1F02,
      0xD5F2,
      0x4445,
      {0x9C, 0x03, 0xC5, 0x68, 0xF2, 0x3C, 0x99, 0xA1}};
  if (mActiveTIPGUID == kMicrosoftIMEForKoreanGUID) {
    return TextInputProcessorID::MicrosoftIMEForKorean;
  }
  // {B60AF051-257A-46BC-B9D3-84DAD819BAFB} (Win8.1, Win10)
  static constexpr GUID kMicrosoftOldHangulGUID = {
      0xB60AF051,
      0x257A,
      0x46BC,
      {0xB9, 0xD3, 0x84, 0xDA, 0xD8, 0x19, 0xBA, 0xFB}};
  if (mActiveTIPGUID == kMicrosoftOldHangulGUID) {
    return TextInputProcessorID::MicrosoftOldHangul;
  }

  // NOTE: There is the other Korean TIP installed in Windows:
  // * Microsoft IME 2010
  //   - {48878C45-93F9-4aaf-A6A1-272CD863C4F5} (Win7)

  return TextInputProcessorID::Unknown;
}

bool TSFStaticSink::Init(ITfThreadMgr* aThreadMgr,
                         ITfInputProcessorProfiles* aInputProcessorProfiles) {
  MOZ_ASSERT(!mThreadMgr && !mInputProcessorProfiles,
             "TSFStaticSink::Init() must be called only once");

  mThreadMgr = aThreadMgr;
  mInputProcessorProfiles = aInputProcessorProfiles;

  RefPtr<ITfSource> source;
  HRESULT hr =
      mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
  if (FAILED(hr)) {
    MOZ_LOG(gIMELog, LogLevel::Error,
            ("0x%p TSFStaticSink::Init() FAILED to get ITfSource "
             "instance (0x%08lX)",
             this, hr));
    return false;
  }

  // NOTE: On Vista or later, Windows let us know activate IME changed only
  //       with ITfInputProcessorProfileActivationSink.
  hr = source->AdviseSink(
      IID_ITfInputProcessorProfileActivationSink,
      static_cast<ITfInputProcessorProfileActivationSink*>(this),
      &mIPProfileCookie);
  if (FAILED(hr) || mIPProfileCookie == TF_INVALID_COOKIE) {
    MOZ_LOG(gIMELog, LogLevel::Error,
            ("0x%p TSFStaticSink::Init() FAILED to install "
             "ITfInputProcessorProfileActivationSink (0x%08lX)",
             this, hr));
    return false;
  }

  MOZ_LOG(gIMELog, LogLevel::Info,
          ("0x%p TSFStaticSink::Init(), "
           "mIPProfileCookie=0x%08lX",
           this, mIPProfileCookie));
  return true;
}

void TSFStaticSink::Destroy() {
  MOZ_LOG(gIMELog, LogLevel::Info,
          ("0x%p TSFStaticSink::Shutdown() "
           "mIPProfileCookie=0x%08lX",
           this, mIPProfileCookie));

  if (mIPProfileCookie != TF_INVALID_COOKIE) {
    RefPtr<ITfSource> source;
    HRESULT hr =
        mThreadMgr->QueryInterface(IID_ITfSource, getter_AddRefs(source));
    if (FAILED(hr)) {
      MOZ_LOG(gIMELog, LogLevel::Error,
              ("0x%p   TSFStaticSink::Shutdown() FAILED to get "
               "ITfSource instance (0x%08lX)",
               this, hr));
    } else {
      hr = source->UnadviseSink(mIPProfileCookie);
      if (FAILED(hr)) {
        MOZ_LOG(gIMELog, LogLevel::Error,
                ("0x%p   TSFTextStore::Shutdown() FAILED to uninstall "
                 "ITfInputProcessorProfileActivationSink (0x%08lX)",
                 this, hr));
      }
    }
  }

  mThreadMgr = nullptr;
  mInputProcessorProfiles = nullptr;
}

STDMETHODIMP TSFStaticSink::OnActivated(DWORD dwProfileType, LANGID langid,
                                        REFCLSID rclsid, REFGUID catid,
                                        REFGUID guidProfile, HKL hkl,
                                        DWORD dwFlags) {
  if ((dwFlags & TF_IPSINK_FLAG_ACTIVE) &&
      (dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT ||
       catid == GUID_TFCAT_TIP_KEYBOARD)) {
    mOnActivatedCalled = true;
    mActiveTIP = TextInputProcessorID::NotComputed;
    mActiveTIPGUID = guidProfile;
    mActiveTIPCLSID = rclsid;
    mLangID = langid & 0xFFFF;
    mIsIMM_IME = IsIMM_IME(hkl);
    GetTIPDescription(rclsid, langid, guidProfile,
                      mActiveTIPKeyboardDescription);
    if (mActiveTIPGUID != GUID_NULL) {
      // key should be "LocaleID|Description".  Although GUID of the
      // profile is unique key since description may be localized for system
      // language, unfortunately, it's too long to record as key with its
      // description.  Therefore, we should record only the description with
      // LocaleID because Microsoft IME may not include language information.
      // 72 is kMaximumKeyStringLength in TelemetryScalar.cpp
      nsAutoString key;
      TSFStaticSink::GetActiveTIPNameForTelemetry(key);
      glean::widget::ime_name_on_windows.Get(NS_ConvertUTF16toUTF8(key))
          .Set(true);
    }
    // Notify IMEHandler of changing active keyboard layout.
    IMEHandler::OnKeyboardLayoutChanged();
  }
  MOZ_LOG(
      gIMELog, LogLevel::Info,
      ("0x%p TSFStaticSink::OnActivated(dwProfileType=%s (0x%08lX), "
       "langid=0x%08X, rclsid=%s, catid=%s, guidProfile=%s, hkl=0x%p, "
       "dwFlags=0x%08lX (TF_IPSINK_FLAG_ACTIVE: %s)), mIsIMM_IME=%s, "
       "mActiveTIPDescription=\"%s\"",
       this,
       dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR
           ? "TF_PROFILETYPE_INPUTPROCESSOR"
       : dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT
           ? "TF_PROFILETYPE_KEYBOARDLAYOUT"
           : "Unknown",
       dwProfileType, langid, AutoClsidCString(rclsid).get(),
       AutoRawGuidCString(catid).get(), AutoRawGuidCString(guidProfile).get(),
       hkl, dwFlags, TSFUtils::BoolToChar(dwFlags & TF_IPSINK_FLAG_ACTIVE),
       TSFUtils::BoolToChar(mIsIMM_IME),
       NS_ConvertUTF16toUTF8(mActiveTIPKeyboardDescription).get()));
  return S_OK;
}

bool TSFStaticSink::EnsureInitActiveTIPKeyboard() {
  if (mOnActivatedCalled) {
    return true;
  }

  RefPtr<ITfInputProcessorProfileMgr> profileMgr;
  HRESULT hr = mInputProcessorProfiles->QueryInterface(
      IID_ITfInputProcessorProfileMgr, getter_AddRefs(profileMgr));
  if (FAILED(hr) || !profileMgr) {
    MOZ_LOG(gIMELog, LogLevel::Error,
            ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
             "to get input processor profile manager, hr=0x%08lX",
             this, hr));
    return false;
  }

  TF_INPUTPROCESSORPROFILE profile;
  hr = profileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  if (hr == S_FALSE) {
    MOZ_LOG(gIMELog, LogLevel::Info,
            ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
             "to get active keyboard layout profile due to no active profile, "
             "hr=0x%08lX",
             this, hr));
    // XXX Should we call OnActivated() with arguments like non-TIP in this
    //     case?
    return false;
  }
  if (FAILED(hr)) {
    MOZ_LOG(gIMELog, LogLevel::Error,
            ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), FAILED "
             "to get active TIP keyboard, hr=0x%08lX",
             this, hr));
    return false;
  }

  MOZ_LOG(gIMELog, LogLevel::Info,
          ("0x%p   TSFStaticSink::EnsureInitActiveLanguageProfile(), "
           "calling OnActivated() manually...",
           this));
  OnActivated(profile.dwProfileType, profile.langid, profile.clsid,
              profile.catid, profile.guidProfile, ::GetKeyboardLayout(0),
              TF_IPSINK_FLAG_ACTIVE);
  return true;
}

void TSFStaticSink::GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                                      REFGUID aProfile,
                                      nsAString& aDescription) {
  aDescription.Truncate();

  if (aTextService == CLSID_NULL || aProfile == GUID_NULL) {
    return;
  }

  BSTR description = nullptr;
  HRESULT hr = mInputProcessorProfiles->GetLanguageProfileDescription(
      aTextService, aLangID, aProfile, &description);
  if (FAILED(hr)) {
    MOZ_LOG(gIMELog, LogLevel::Error,
            ("0x%p   TSFStaticSink::InitActiveTIPDescription() FAILED "
             "due to GetLanguageProfileDescription() failure, hr=0x%08lX",
             this, hr));
    return;
  }

  if (description && description[0]) {
    aDescription.Assign(description);
  }
  ::SysFreeString(description);
}

bool TSFStaticSink::IsTIPCategoryKeyboard(REFCLSID aTextService, LANGID aLangID,
                                          REFGUID aProfile) {
  if (aTextService == CLSID_NULL || aProfile == GUID_NULL) {
    return false;
  }

  RefPtr<IEnumTfLanguageProfiles> enumLangProfiles;
  HRESULT hr = mInputProcessorProfiles->EnumLanguageProfiles(
      aLangID, getter_AddRefs(enumLangProfiles));
  if (FAILED(hr) || !enumLangProfiles) {
    MOZ_LOG(gIMELog, LogLevel::Error,
            ("0x%p   TSFStaticSink::IsTIPCategoryKeyboard(), FAILED "
             "to get language profiles enumerator, hr=0x%08lX",
             this, hr));
    return false;
  }

  TF_LANGUAGEPROFILE profile;
  ULONG fetch = 0;
  while (SUCCEEDED(enumLangProfiles->Next(1, &profile, &fetch)) && fetch) {
    // XXX We're not sure a profile is registered with two or more categories.
    if (profile.clsid == aTextService && profile.guidProfile == aProfile &&
        profile.catid == GUID_TFCAT_TIP_KEYBOARD) {
      return true;
    }
  }
  return false;
}

}  // namespace mozilla::widget
