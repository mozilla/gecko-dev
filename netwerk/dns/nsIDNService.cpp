/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "nsIDNService.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "harfbuzz/hb.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"
#include "mozilla/intl/UnicodeProperties.h"
#include "mozilla/intl/UnicodeScriptCodes.h"
#include "nsNetUtil.h"
#include "nsStandardURL.h"

using namespace mozilla;
using namespace mozilla::intl;
using namespace mozilla::unicode;
using namespace mozilla::net;
using mozilla::Preferences;

//-----------------------------------------------------------------------------

#define ISNUMERIC(c) ((c) >= '0' && (c) <= '9')

template <int N>
static inline bool TLDEqualsLiteral(mozilla::Span<const char32_t> aTLD,
                                    const char (&aStr)[N]) {
  if (aTLD.Length() != N - 1) {
    return false;
  }
  const char* a = aStr;
  for (const char32_t c : aTLD) {
    if (c != char32_t(*a)) {
      return false;
    }
    ++a;
  }
  return true;
}

static inline bool isOnlySafeChars(mozilla::Span<const char32_t> aLabel,
                                   const nsTArray<BlocklistRange>& aBlocklist) {
  if (aBlocklist.IsEmpty()) {
    return true;
  }
  for (const char32_t c : aLabel) {
    if (c > 0xFFFF) {
      // The blocklist only support BMP!
      continue;
    }
    if (CharInBlocklist(char16_t(c), aBlocklist)) {
      return false;
    }
  }
  return true;
}

static bool isCJKSlashConfusable(char32_t aChar) {
  switch (aChar) {
    case 0x30CE:  // ノ
    case 0x30BD:  // ソ
    case 0x30BE:  // ゾ
    case 0x30F3:  // ン
    case 0x4E36:  // 丶
    case 0x4E40:  // 乀
    case 0x4E41:  // 乁
    case 0x4E3F:  // 丿
      return true;
    default:
      return false;
  }
}

static bool isCJKIdeograph(char32_t aChar) {
  switch (aChar) {
    case 0x4E00:  // 一
    case 0x3127:  // ㄧ
    case 0x4E28:  // 丨
    case 0x4E5B:  // 乛
    case 0x4E03:  // 七
    case 0x4E05:  // 丅
    case 0x5341:  // 十
    case 0x3007:  // 〇
    case 0x3112:  // ㄒ
    case 0x311A:  // ㄚ
    case 0x311F:  // ㄟ
    case 0x3128:  // ㄨ
    case 0x3129:  // ㄩ
    case 0x3108:  // ㄈ
    case 0x31BA:  // ㆺ
    case 0x31B3:  // ㆳ
    case 0x5DE5:  // 工
    case 0x31B2:  // ㆲ
    case 0x8BA0:  // 讠
    case 0x4E01:  // 丁
      return true;
    default:
      return false;
  }
}

static bool isDigitLookalike(char32_t aChar) {
  switch (aChar) {
    case 0x03B8:  // θ
    case 0x0968:  // २
    case 0x09E8:  // ২
    case 0x0A68:  // ੨
    case 0x0AE8:  // ૨
    case 0x0CE9:  // ೩
    case 0x0577:  // շ
    case 0x0437:  // з
    case 0x0499:  // ҙ
    case 0x04E1:  // ӡ
    case 0x0909:  // उ
    case 0x0993:  // ও
    case 0x0A24:  // ਤ
    case 0x0A69:  // ੩
    case 0x0AE9:  // ૩
    case 0x0C69:  // ౩
    case 0x1012:  // ဒ
    case 0x10D5:  // ვ
    case 0x10DE:  // პ
    case 0x0A5C:  // ੜ
    case 0x10D9:  // კ
    case 0x0A6B:  // ੫
    case 0x4E29:  // 丩
    case 0x3110:  // ㄐ
    case 0x0573:  // ճ
    case 0x09EA:  // ৪
    case 0x0A6A:  // ੪
    case 0x0B6B:  // ୫
    case 0x0AED:  // ૭
    case 0x0B68:  // ୨
    case 0x0C68:  // ౨
      return true;
    default:
      return false;
  }
}

static bool isCyrillicLatinConfusable(char32_t aChar) {
  switch (aChar) {
    case 0x0430:  // а CYRILLIC SMALL LETTER A
    case 0x044B:  // ы CYRILLIC SMALL LETTER YERU
    case 0x0441:  // с CYRILLIC SMALL LETTER ES
    case 0x0501:  // ԁ CYRILLIC SMALL LETTER KOMI DE
    case 0x0435:  // е CYRILLIC SMALL LETTER IE
    case 0x050D:  // ԍ CYRILLIC SMALL LETTER KOMI SJE
    case 0x04BB:  // һ CYRILLIC SMALL LETTER SHHA
    case 0x0456:  // і CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I {Old
                  // Cyrillic i}
    case 0x044E:  // ю CYRILLIC SMALL LETTER YU
    case 0x043A:  // к CYRILLIC SMALL LETTER KA
    case 0x0458:  // ј CYRILLIC SMALL LETTER JE
    case 0x04CF:  // ӏ CYRILLIC SMALL LETTER PALOCHKA
    case 0x043C:  // м CYRILLIC SMALL LETTER EM
    case 0x043E:  // о CYRILLIC SMALL LETTER O
    case 0x0440:  // р CYRILLIC SMALL LETTER ER
    case 0x0517:  // ԗ CYRILLIC SMALL LETTER RHA {voiceless r}
    case 0x051B:  // ԛ CYRILLIC SMALL LETTER QA
    case 0x0455:  // ѕ CYRILLIC SMALL LETTER DZE
    case 0x051D:  // ԝ CYRILLIC SMALL LETTER WE
    case 0x0445:  // х CYRILLIC SMALL LETTER HA
    case 0x0443:  // у CYRILLIC SMALL LETTER U
    case 0x044A:  // ъ CYRILLIC SMALL LETTER HARD SIGN
    case 0x044C:  // ь CYRILLIC SMALL LETTER SOFT SIGN
    case 0x04BD:  // ҽ CYRILLIC SMALL LETTER ABKHASIAN CHE
    case 0x043F:  // п CYRILLIC SMALL LETTER PE
    case 0x0433:  // г CYRILLIC SMALL LETTER GHE
    case 0x0475:  // ѵ CYRILLIC SMALL LETTER IZHITSA
    case 0x0461:  // ѡ CYRILLIC SMALL LETTER OMEGA
      return true;
    default:
      return false;
  }
}

static bool isCyrillicDomain(mozilla::Span<const char32_t>& aTLD) {
  mozilla::Span<const char32_t>::const_iterator current = aTLD.cbegin();
  mozilla::Span<const char32_t>::const_iterator end = aTLD.cend();

  while (current != end) {
    char32_t ch = *current++;
    if (UnicodeProperties::GetScriptCode(ch) == Script::CYRILLIC) {
      return true;
    }
  }

  return TLDEqualsLiteral(aTLD, "bg") || TLDEqualsLiteral(aTLD, "by") ||
         TLDEqualsLiteral(aTLD, "kz") || TLDEqualsLiteral(aTLD, "pyc") ||
         TLDEqualsLiteral(aTLD, "ru") || TLDEqualsLiteral(aTLD, "su") ||
         TLDEqualsLiteral(aTLD, "ua") || TLDEqualsLiteral(aTLD, "uz");
}

//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

/* Implementation file */
NS_IMPL_ISUPPORTS(nsIDNService, nsIIDNService)

nsresult nsIDNService::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  InitializeBlocklist(mIDNBlocklist);
  return NS_OK;
}

nsIDNService::nsIDNService() { MOZ_ASSERT(NS_IsMainThread()); }

nsIDNService::~nsIDNService() = default;

NS_IMETHODIMP nsIDNService::DomainToASCII(const nsACString& input,
                                          nsACString& ace) {
  return NS_DomainToASCII(input, ace);
}

NS_IMETHODIMP nsIDNService::ConvertUTF8toACE(const nsACString& input,
                                             nsACString& ace) {
  return NS_DomainToASCIIAllowAnyGlyphfulASCII(input, ace);
}

NS_IMETHODIMP nsIDNService::ConvertACEtoUTF8(const nsACString& input,
                                             nsACString& _retval) {
  return NS_DomainToUnicodeAllowAnyGlyphfulASCII(input, _retval);
}

NS_IMETHODIMP nsIDNService::DomainToDisplay(const nsACString& input,
                                            nsACString& _retval) {
  nsresult rv = NS_DomainToDisplay(input, _retval);
  return rv;
}

NS_IMETHODIMP nsIDNService::ConvertToDisplayIDN(const nsACString& input,
                                                nsACString& _retval) {
  nsresult rv = NS_DomainToDisplayAllowAnyGlyphfulASCII(input, _retval);
  return rv;
}

//-----------------------------------------------------------------------------

namespace mozilla::net {

enum ScriptCombo : int32_t {
  UNSET = -1,
  BOPO = 0,
  CYRL = 1,
  GREK = 2,
  HANG = 3,
  HANI = 4,
  HIRA = 5,
  KATA = 6,
  LATN = 7,
  OTHR = 8,
  JPAN = 9,   // Latin + Han + Hiragana + Katakana
  CHNA = 10,  // Latin + Han + Bopomofo
  KORE = 11,  // Latin + Han + Hangul
  HNLT = 12,  // Latin + Han (could be any of the above combinations)
  FAIL = 13,
};

// Ignore - set if the label contains a character that makes it
// obvious it's not a lookalike.
// Safe - set if the label contains no lookalike characters.
// Block - set if the label contains lookalike characters.
enum class LookalikeStatus { Ignore, Safe, Block };

}  // namespace mozilla::net

bool nsIDNService::IsLabelSafe(mozilla::Span<const char32_t> aLabel,
                               mozilla::Span<const char32_t> aTLD) {
  if (StaticPrefs::network_IDN_show_punycode()) {
    return false;
  }

  if (!isOnlySafeChars(aLabel, mIDNBlocklist)) {
    return false;
  }

  mozilla::Span<const char32_t>::const_iterator current = aLabel.cbegin();
  mozilla::Span<const char32_t>::const_iterator end = aLabel.cend();

  Script lastScript = Script::INVALID;
  char32_t previousChar = 0;
  char32_t baseChar = 0;  // last non-diacritic seen (base char for marks)
  char32_t savedNumberingSystem = 0;
  LookalikeStatus digitLookalikeStatus = LookalikeStatus::Safe;
  LookalikeStatus cyrillicStatus = LookalikeStatus::Safe;
// Simplified/Traditional Chinese check temporarily disabled -- bug 857481
#if 0
  HanVariantType savedHanVariant = HVT_NotHan;
#endif

  ScriptCombo savedScript = ScriptCombo::UNSET;

  while (current != end) {
    char32_t ch = *current++;

    IdentifierType idType = GetIdentifierType(ch);
    if (idType == IDTYPE_RESTRICTED) {
      return false;
    }
    MOZ_ASSERT(idType == IDTYPE_ALLOWED);

    // Check for mixed script
    Script script = UnicodeProperties::GetScriptCode(ch);
    if (script != Script::COMMON && script != Script::INHERITED &&
        script != lastScript) {
      if (illegalScriptCombo(script, savedScript)) {
        return false;
      }
    }

#ifdef XP_MACOSX
    // U+0620, U+0f8c, U+0f8d, U+0f8e, U+0f8f and are blocked due to a font
    // issue on macOS
    if (ch == 0x620 || ch == 0xf8c || ch == 0xf8d || ch == 0xf8e ||
        ch == 0xf8f) {
      return false;
    }
#endif

    // U+30FC should be preceded by a Hiragana/Katakana.
    if (ch == 0x30fc && lastScript != Script::HIRAGANA &&
        lastScript != Script::KATAKANA) {
      return false;
    }

    Script nextScript = Script::INVALID;
    if (current != end) {
      nextScript = UnicodeProperties::GetScriptCode(*current);
    }

    // U+3078 to U+307A (へ, べ, ぺ) in Hiragana mixed with Katakana should be
    // unsafe
    if (ch >= 0x3078 && ch <= 0x307A &&
        (lastScript == Script::KATAKANA || nextScript == Script::KATAKANA)) {
      return false;
    }
    // U+30D8 to U+30DA (ヘ, ベ, ペ) in Katakana mixed with Hiragana should be
    // unsafe
    if (ch >= 0x30D8 && ch <= 0x30DA &&
        (lastScript == Script::HIRAGANA || nextScript == Script::HIRAGANA)) {
      return false;
    }
    // U+30FD and U+30FE are allowed only after Katakana
    if ((ch == 0x30FD || ch == 0x30FE) && lastScript != Script::KATAKANA) {
      return false;
    }

    // Slash confusables not enclosed by {Han,Hiragana,Katakana} should be
    // unsafe but by itself should be allowed.
    if (isCJKSlashConfusable(ch) && aLabel.Length() > 1 &&
        lastScript != Script::HAN && lastScript != Script::HIRAGANA &&
        lastScript != Script::KATAKANA && nextScript != Script::HAN &&
        nextScript != Script::HIRAGANA && nextScript != Script::KATAKANA) {
      return false;
    }

    if (ch == 0x30FB &&
        (lastScript == Script::LATIN || nextScript == Script::LATIN)) {
      return false;
    }

    // Combining Diacritic marks (U+0300-U+0339) after a script other than
    // Latin-Greek-Cyrillic is unsafe
    if (ch >= 0x300 && ch <= 0x339 && lastScript != Script::LATIN &&
        lastScript != Script::GREEK && lastScript != Script::CYRILLIC) {
      return false;
    }

    if (ch == 0x307 &&
        (previousChar == 'i' || previousChar == 'j' || previousChar == 'l')) {
      return false;
    }

    // U+00B7 is only allowed on Catalan domains between two l's.
    if (ch == 0xB7 && (!TLDEqualsLiteral(aTLD, "cat") || previousChar != 'l' ||
                       current == end || *current != 'l')) {
      return false;
    }

    // Ignore digit lookalikes if there is a non-digit and non-digit lookalike
    // character. If aLabel only consists of digits and digit lookalikes or
    // digit lookalikes, return false.
    if (digitLookalikeStatus != LookalikeStatus::Ignore && !ISNUMERIC(ch)) {
      digitLookalikeStatus = isDigitLookalike(ch) ? LookalikeStatus::Block
                                                  : LookalikeStatus::Ignore;
    }

    // Disallow Icelandic confusables for domains outside Icelandic and Faroese
    // ccTLD (.is, .fo)
    if ((ch == 0xFE || ch == 0xF0) && !TLDEqualsLiteral(aTLD, "is") &&
        !TLDEqualsLiteral(aTLD, "fo")) {
      return false;
    }

    // Disallow U+0259 for domains outside Azerbaijani ccTLD (.az)
    if (ch == 0x259 && !TLDEqualsLiteral(aTLD, "az")) {
      return false;
    }

    // Block single/double-quote-like characters.
    if (ch == 0x2BB || ch == 0x2BC) {
      return false;
    }

    // Check if all the cyrillic letters in the label are confusables
    if (cyrillicStatus != LookalikeStatus::Ignore &&
        script == Script::CYRILLIC && !isCyrillicDomain(aTLD)) {
      cyrillicStatus = isCyrillicLatinConfusable(ch) ? LookalikeStatus::Block
                                                     : LookalikeStatus::Ignore;
    }

    // Block these CJK ideographs if they are adjacent to non-CJK characters.
    // These characters can be used to spoof Latin characters/punctuation marks.
    if (isCJKIdeograph(ch)) {
      // Check if there is a non-Bopomofo, non-Hiragana, non-Katakana, non-Han,
      // and non-Numeric character on the left. previousChar is 0 when ch is the
      // first character.
      if (lastScript != Script::BOPOMOFO && lastScript != Script::HIRAGANA &&
          lastScript != Script::KATAKANA && lastScript != Script::HAN &&
          previousChar && !ISNUMERIC(previousChar)) {
        return false;
      }
      // Check if there is a non-Bopomofo, non-Hiragana, non-Katakana, non-Han,
      // and non-Numeric character on the right.
      if (nextScript != Script::BOPOMOFO && nextScript != Script::HIRAGANA &&
          nextScript != Script::KATAKANA && nextScript != Script::HAN &&
          current != aLabel.end() && !ISNUMERIC(*current)) {
        return false;
      }
    }

    // Check for mixed numbering systems
    auto genCat = GetGeneralCategory(ch);
    if (genCat == HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER) {
      uint32_t zeroCharacter =
          ch - mozilla::intl::UnicodeProperties::GetNumericValue(ch);
      if (savedNumberingSystem == 0) {
        // If we encounter a decimal number, save the zero character from that
        // numbering system.
        savedNumberingSystem = zeroCharacter;
      } else if (zeroCharacter != savedNumberingSystem) {
        return false;
      }
    }

    if (genCat == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) {
      // Check for consecutive non-spacing marks.
      if (previousChar != 0 && previousChar == ch) {
        return false;
      }
      // Check for marks whose expected script doesn't match the base script.
      if (lastScript != Script::INVALID) {
        UnicodeProperties::ScriptExtensionVector scripts;
        auto extResult = UnicodeProperties::GetExtensions(ch, scripts);
        MOZ_ASSERT(extResult.isOk());
        if (extResult.isErr()) {
          return false;
        }

        int nScripts = AssertedCast<int>(scripts.length());

        // nScripts will always be >= 1, because even for undefined characters
        // it will return Script::INVALID.
        // If the mark just has script=COMMON or INHERITED, we can't check any
        // more carefully, but if it has specific scriptExtension codes, then
        // assume those are the only valid scripts to use it with.
        if (nScripts > 1 || (Script(scripts[0]) != Script::COMMON &&
                             Script(scripts[0]) != Script::INHERITED)) {
          while (--nScripts >= 0) {
            if (Script(scripts[nScripts]) == lastScript) {
              break;
            }
          }
          if (nScripts == -1) {
            return false;
          }
        }
      }
      // Check for diacritics on dotless-i, which would be indistinguishable
      // from normal accented letter i.
      if (baseChar == 0x0131 &&
          ((ch >= 0x0300 && ch <= 0x0314) || ch == 0x031a)) {
        return false;
      }
    } else {
      baseChar = ch;
    }

    if (script != Script::COMMON && script != Script::INHERITED) {
      lastScript = script;
    }

    // Simplified/Traditional Chinese check temporarily disabled -- bug 857481
#if 0

    // Check for both simplified-only and traditional-only Chinese characters
    HanVariantType hanVariant = GetHanVariant(ch);
    if (hanVariant == HVT_SimplifiedOnly || hanVariant == HVT_TraditionalOnly) {
      if (savedHanVariant == HVT_NotHan) {
        savedHanVariant = hanVariant;
      } else if (hanVariant != savedHanVariant)  {
        return false;
      }
    }
#endif

    previousChar = ch;
  }
  return digitLookalikeStatus != LookalikeStatus::Block &&
         (!StaticPrefs::network_idn_punycode_cyrillic_confusables() ||
          cyrillicStatus != LookalikeStatus::Block);
}

// Scripts that we care about in illegalScriptCombo
static inline ScriptCombo findScriptIndex(Script aScript) {
  switch (aScript) {
    case Script::BOPOMOFO:
      return ScriptCombo::BOPO;
    case Script::CYRILLIC:
      return ScriptCombo::CYRL;
    case Script::GREEK:
      return ScriptCombo::GREK;
    case Script::HANGUL:
      return ScriptCombo::HANG;
    case Script::HAN:
      return ScriptCombo::HANI;
    case Script::HIRAGANA:
      return ScriptCombo::HIRA;
    case Script::KATAKANA:
      return ScriptCombo::KATA;
    case Script::LATIN:
      return ScriptCombo::LATN;
    default:
      return ScriptCombo::OTHR;
  }
}

static const ScriptCombo scriptComboTable[13][9] = {
    /* thisScript: BOPO  CYRL  GREK  HANG  HANI  HIRA  KATA  LATN  OTHR
     * savedScript */
    /* BOPO */ {BOPO, FAIL, FAIL, FAIL, CHNA, FAIL, FAIL, CHNA, FAIL},
    /* CYRL */ {FAIL, CYRL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL},
    /* GREK */ {FAIL, FAIL, GREK, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL},
    /* HANG */ {FAIL, FAIL, FAIL, HANG, KORE, FAIL, FAIL, KORE, FAIL},
    /* HANI */ {CHNA, FAIL, FAIL, KORE, HANI, JPAN, JPAN, HNLT, FAIL},
    /* HIRA */ {FAIL, FAIL, FAIL, FAIL, JPAN, HIRA, JPAN, JPAN, FAIL},
    /* KATA */ {FAIL, FAIL, FAIL, FAIL, JPAN, JPAN, KATA, JPAN, FAIL},
    /* LATN */ {CHNA, FAIL, FAIL, KORE, HNLT, JPAN, JPAN, LATN, OTHR},
    /* OTHR */ {FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, FAIL, OTHR, FAIL},
    /* JPAN */ {FAIL, FAIL, FAIL, FAIL, JPAN, JPAN, JPAN, JPAN, FAIL},
    /* CHNA */ {CHNA, FAIL, FAIL, FAIL, CHNA, FAIL, FAIL, CHNA, FAIL},
    /* KORE */ {FAIL, FAIL, FAIL, KORE, KORE, FAIL, FAIL, KORE, FAIL},
    /* HNLT */ {CHNA, FAIL, FAIL, KORE, HNLT, JPAN, JPAN, HNLT, FAIL}};

bool nsIDNService::illegalScriptCombo(Script script, ScriptCombo& savedScript) {
  if (savedScript == ScriptCombo::UNSET) {
    savedScript = findScriptIndex(script);
    return false;
  }

  savedScript = scriptComboTable[savedScript][findScriptIndex(script)];

  return savedScript == OTHR || savedScript == FAIL;
}

extern "C" MOZ_EXPORT bool mozilla_net_is_label_safe(const char32_t* aLabel,
                                                     size_t aLabelLen,
                                                     const char32_t* aTld,
                                                     size_t aTldLen) {
  return static_cast<nsIDNService*>(nsStandardURL::GetIDNService())
      ->IsLabelSafe(mozilla::Span<const char32_t>(aLabel, aLabelLen),
                    mozilla::Span<const char32_t>(aTld, aTldLen));
}
