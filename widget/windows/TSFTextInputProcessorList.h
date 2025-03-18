/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TSFTextInputProcessorList_h
#define TSFTextInputProcessorList_h

namespace mozilla::widget {

enum class TextInputProcessorID {
  // Internal use only.  This won't be returned by TSFStaticSink::ActiveTIP().
  NotComputed,

  // Not a TIP.  E.g., simple keyboard layout or IMM-IME.
  None,

  // Used for other TIPs, i.e., any TIPs which we don't support specifically.
  Unknown,

  // TIP for Japanese.
  MicrosoftIMEForJapanese,
  MicrosoftOfficeIME2010ForJapanese,
  GoogleJapaneseInput,
  ATOK2011,
  ATOK2012,
  ATOK2013,
  ATOK2014,
  ATOK2015,
  ATOK2016,
  ATOKUnknown,
  Japanist10,

  // TIP for Traditional Chinese.
  MicrosoftBopomofo,
  MicrosoftChangJie,
  MicrosoftPhonetic,
  MicrosoftQuick,
  MicrosoftNewChangJie,
  MicrosoftNewPhonetic,
  MicrosoftNewQuick,
  FreeChangJie,

  // TIP for Simplified Chinese.
  MicrosoftPinyin,
  MicrosoftPinyinNewExperienceInputStyle,
  MicrosoftWubi,

  // TIP for Korean.
  MicrosoftIMEForKorean,
  MicrosoftOldHangul,

  // Keyman Desktop, which can install various language keyboards.
  KeymanDesktop,
};

}  // namespace mozilla::widget

#endif  // #ifndef TSFTextInputProcessorList_h
