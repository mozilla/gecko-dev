/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDNService_h__
#define nsIDNService_h__

#include "nsIIDNService.h"

#include "mozilla/RWLock.h"
#include "mozilla/intl/UnicodeScriptCodes.h"
#include "mozilla/net/IDNBlocklistUtils.h"
#include "mozilla/Span.h"
#include "nsTHashSet.h"

class nsIPrefBranch;

//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

namespace mozilla::net {
enum ScriptCombo : int32_t;
}

class nsIDNService final : public nsIIDNService {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIDNSERVICE

  nsIDNService();

  nsresult Init();

 protected:
  virtual ~nsIDNService();

 private:
  void InitCJKSlashConfusables();
  void InitCJKIdeographs();
  void InitDigitConfusables();
  void InitCyrillicLatinConfusables();
  void InitThaiLatinConfusables();

 public:
  /**
   * Determine whether a label is considered safe to display to the user
   * according to the algorithm defined in UTR 39.
   *
   * For the ASCII-only profile, returns false for all labels containing
   * non-ASCII characters.
   *
   * For the other profiles, returns false for labels containing any of
   * the following:
   *
   *  Characters in scripts other than the "recommended scripts" and
   *   "aspirational scripts" defined in
   *   http://www.unicode.org/reports/tr31/#Table_Recommended_Scripts
   *   and http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts
   *  This includes codepoints that are not defined as Unicode
   *   characters
   *
   *  Illegal combinations of scripts (@see illegalScriptCombo)
   *
   *  Numbers from more than one different numbering system
   *
   *  Sequences of the same non-spacing mark
   *
   *  Both simplified-only and traditional-only Chinese characters
   *   XXX this test was disabled by bug 857481
   */
  bool IsLabelSafe(mozilla::Span<const char32_t> aLabel,
                   mozilla::Span<const char32_t> aTLD);

 private:
  /**
   * Determine whether a combination of scripts in a single label is
   * permitted according to the algorithm defined in UTR 39.
   *
   * All characters in each identifier must be from a single script,
   * or from the combinations:
   *  Latin + Han + Hiragana + Katakana;
   *  Latin + Han + Bopomofo; or
   *  Latin + Han + Hangul
   */
  bool illegalScriptCombo(mozilla::intl::Script script,
                          mozilla::net::ScriptCombo& savedScript);

  bool isCJKSlashConfusable(char32_t aChar);
  bool isCJKIdeograph(char32_t aChar);

  nsTArray<mozilla::net::BlocklistRange> mIDNBlocklist;

  // Confusables that we would like to check for IDN spoofing detection.
  nsTHashSet<char32_t> mCJKSlashConfusables;
  nsTHashSet<char32_t> mCJKIdeographs;
  nsTHashSet<char32_t> mDigitConfusables;
  nsTHashSet<char32_t> mCyrillicLatinConfusables;
  nsTHashSet<char32_t> mThaiLatinConfusables;
};

extern "C" MOZ_EXPORT bool mozilla_net_is_label_safe(const char32_t* aLabel,
                                                     size_t aLabelLen,
                                                     const char32_t* aTld,
                                                     size_t aTldLen);

#endif  // nsIDNService_h__
