/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/intl/LanguageTag.h"

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Range.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Variant.h"

#include <algorithm>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <type_traits>
#include <utility>

#include "jsapi.h"
#include "jsfriendapi.h"

#include "builtin/intl/CommonFunctions.h"
#include "ds/Sort.h"
#include "js/Result.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "unicode/uloc.h"
#include "unicode/utypes.h"
#include "util/StringBuffer.h"
#include "vm/JSContext.h"
#include "vm/Printer.h"
#include "vm/StringType.h"

namespace js {
namespace intl {

using namespace js::intl::LanguageTagLimits;

using ConstCharRange = mozilla::Range<const char>;

#ifdef DEBUG
template <typename CharT>
bool IsStructurallyValidLanguageTag(
    const mozilla::Range<const CharT>& language) {
  // Tell the analysis the |std::all_of| function can't GC.
  JS::AutoSuppressGCAnalysis nogc;

  // unicode_language_subtag = alpha{2,3} | alpha{5,8};
  size_t length = language.length();
  const CharT* str = language.begin().get();
  return ((2 <= length && length <= 3) || (5 <= length && length <= 8)) &&
         std::all_of(str, str + length, mozilla::IsAsciiLowercaseAlpha<CharT>);
}

template bool IsStructurallyValidLanguageTag(
    const mozilla::Range<const Latin1Char>& language);
template bool IsStructurallyValidLanguageTag(
    const mozilla::Range<const char16_t>& language);

template <typename CharT>
bool IsStructurallyValidScriptTag(const mozilla::Range<const CharT>& script) {
  // Tell the analysis the |std::all_of| function can't GC.
  JS::AutoSuppressGCAnalysis nogc;

  // unicode_script_subtag = alpha{4} ;
  size_t length = script.length();
  const CharT* str = script.begin().get();
  return length == 4 && mozilla::IsAsciiUppercaseAlpha<CharT>(str[0]) &&
         std::all_of(str + 1, str + length,
                     mozilla::IsAsciiLowercaseAlpha<CharT>);
}

template bool IsStructurallyValidScriptTag(
    const mozilla::Range<const Latin1Char>& script);
template bool IsStructurallyValidScriptTag(
    const mozilla::Range<const char16_t>& script);

template <typename CharT>
bool IsStructurallyValidRegionTag(const mozilla::Range<const CharT>& region) {
  // Tell the analysis the |std::all_of| function can't GC.
  JS::AutoSuppressGCAnalysis nogc;

  // unicode_region_subtag = (alpha{2} | digit{3}) ;
  size_t length = region.length();
  const CharT* str = region.begin().get();
  return (length == 2 && std::all_of(str, str + length,
                                     mozilla::IsAsciiUppercaseAlpha<CharT>)) ||
         (length == 3 &&
          std::all_of(str, str + length, mozilla::IsAsciiDigit<CharT>));
}

template bool IsStructurallyValidRegionTag(
    const mozilla::Range<const Latin1Char>& region);
template bool IsStructurallyValidRegionTag(
    const mozilla::Range<const char16_t>& region);

bool IsStructurallyValidVariantTag(const ConstCharRange& variant) {
  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  auto isAsciiLowercaseAlphanumeric = [](char c) {
    return mozilla::IsAsciiLowercaseAlpha(c) || mozilla::IsAsciiDigit(c);
  };
  size_t length = variant.length();
  const char* str = variant.begin().get();
  return ((5 <= length && length <= 8) ||
          (length == 4 && mozilla::IsAsciiDigit(str[0]))) &&
         std::all_of(str, str + length, isAsciiLowercaseAlphanumeric);
}

bool IsStructurallyValidUnicodeExtensionTag(const ConstCharRange& extension) {
  auto isAsciiLowercaseAlphanumericOrDash = [](char c) {
    return mozilla::IsAsciiLowercaseAlpha(c) || mozilla::IsAsciiDigit(c) ||
           c == '-';
  };

  size_t length = extension.length();
  const char* str = extension.begin().get();
  return LanguageTagParser::canParseUnicodeExtension(extension) &&
         std::all_of(str, str + length, isAsciiLowercaseAlphanumericOrDash);
}

static bool IsStructurallyValidExtensionTag(const ConstCharRange& extension) {
  // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
  // NB: Allow any extension, including Unicode and Transform here, because
  // this function is only used for an assertion.
  auto isAsciiDigitOrLowercaseAlpha = [](char c) {
    return mozilla::IsAsciiDigit(c) || mozilla::IsAsciiLowercaseAlpha(c);
  };

  size_t length = extension.length();
  const char* str = extension.begin().get();
  if (length <= 2) {
    return false;
  }
  if (!isAsciiDigitOrLowercaseAlpha(str[0]) || str[0] == 'x') {
    return false;
  }
  str++;
  if (*str++ != '-') {
    return false;
  }
  while (true) {
    const char* sep = reinterpret_cast<const char*>(
        memchr(str, '-', extension.end().get() - str));
    size_t len = (sep ? sep : extension.end().get()) - str;
    if (len < 2 || len > 8 ||
        !std::all_of(str, str + len, isAsciiDigitOrLowercaseAlpha)) {
      return false;
    }
    if (!sep) {
      return true;
    }
    str = sep + 1;
  }
}

bool IsStructurallyValidPrivateUseTag(const ConstCharRange& privateUse) {
  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
  auto isAsciiDigitOrLowercaseAlpha = [](char c) {
    return mozilla::IsAsciiDigit(c) || mozilla::IsAsciiLowercaseAlpha(c);
  };

  size_t length = privateUse.length();
  const char* str = privateUse.begin().get();
  if (length <= 2 || *str++ != 'x' || *str++ != '-') {
    return false;
  }
  while (true) {
    const char* sep = reinterpret_cast<const char*>(
        memchr(str, '-', privateUse.end().get() - str));
    size_t len = (sep ? sep : privateUse.end().get()) - str;
    if (len == 0 || len > 8 ||
        !std::all_of(str, str + len, isAsciiDigitOrLowercaseAlpha)) {
      return false;
    }
    if (!sep) {
      return true;
    }
    str = sep + 1;
  }
}
#endif

bool LanguageTag::setUnicodeExtension(UniqueChars extension) {
  MOZ_ASSERT(IsStructurallyValidUnicodeExtensionTag(
      {extension.get(), strlen(extension.get())}));

  // Replace the existing Unicode extension subtag or append a new one.
  auto p = std::find_if(extensions().begin(), extensions().end(),
                        [](const auto& ext) { return ext[0] == 'u'; });
  if (p != extensions().end()) {
    size_t index = std::distance(extensions().begin(), p);
    extensions_[index] = std::move(extension);
    return true;
  }
  return extensions_.append(std::move(extension));
}

void LanguageTag::clearUnicodeExtension() {
  auto p = std::find_if(extensions().begin(), extensions().end(),
                        [](const auto& ext) { return ext[0] == 'u'; });
  if (p != extensions().end()) {
    size_t index = std::distance(extensions().begin(), p);
    extensions_.erase(extensions_.begin() + index);
  }
}

template <size_t InitialCapacity>
static bool SortAlphabetically(JSContext* cx,
                               Vector<UniqueChars, InitialCapacity>& subtags) {
  size_t length = subtags.length();

  // Zero or one element lists are already sorted.
  if (length < 2) {
    return true;
  }

  // Handle two element lists inline.
  if (length == 2) {
    if (strcmp(subtags[0].get(), subtags[1].get()) > 0) {
      subtags[0].swap(subtags[1]);
    }
    return true;
  }

  Vector<char*, 8> scratch(cx);
  if (!scratch.resizeUninitialized(length * 2)) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    scratch[i] = subtags[i].release();
  }

  MOZ_ALWAYS_TRUE(
      MergeSort(scratch.begin(), length, scratch.begin() + length,
                [](const char* a, const char* b, bool* lessOrEqualp) {
                  *lessOrEqualp = strcmp(a, b) <= 0;
                  return true;
                }));

  for (size_t i = 0; i < length; i++) {
    subtags[i] = UniqueChars(scratch[i]);
  }
  return true;
}

bool LanguageTag::canonicalizeBaseName(JSContext* cx) {
  // Per UTS 35, 3.3.1, the very first step is to canonicalize the syntax by
  // normalizing the case and ordering all subtags. The canonical syntax form
  // itself is specified in UTS 35, 3.2.1.

  // The |LanguageTag| fields are already in normalized case, so we can skip
  // this step.
  MOZ_ASSERT(IsStructurallyValidLanguageTag(language().range()));
  MOZ_ASSERT(script().length() == 0 ||
             IsStructurallyValidScriptTag(script().range()));
  MOZ_ASSERT(region().length() == 0 ||
             IsStructurallyValidRegionTag(region().range()));
#ifdef DEBUG
  auto validVariant = [](const auto& variant) {
    const char* str = variant.get();
    return IsStructurallyValidVariantTag({str, strlen(str)});
  };
  MOZ_ASSERT(std::all_of(variants().begin(), variants().end(), validVariant));

  auto validExtension = [](const auto& extension) {
    const char* str = extension.get();
    return IsStructurallyValidExtensionTag({str, strlen(str)});
  };
  MOZ_ASSERT(
      std::all_of(extensions().begin(), extensions().end(), validExtension));
#endif
  MOZ_ASSERT(!privateuse() || IsStructurallyValidPrivateUseTag(
                                  {privateuse(), strlen(privateuse())}));

  // The second step in UTS 35, 3.2.1, is to order all subtags.

  // 1. Any variants are in alphabetical order.
  if (!SortAlphabetically(cx, variants_)) {
    return false;
  }

  // 2. Any extensions are in alphabetical order by their singleton.
  // - A subsequent call to canonicalizeExtensions() will perform this.

  // The next two steps in 3.3.1 replace deprecated language and region
  // subtags with their preferred mappings.

  if (!updateGrandfatheredMappings(cx)) {
    return false;
  }

  // Replace deprecated language subtags with their preferred values.
  if (!languageMapping(language_) && complexLanguageMapping(language_)) {
    performComplexLanguageMappings();
  }

  // No script replacements are currently present.

  // Replace deprecated region subtags with their preferred values.
  if (region().length() > 0) {
    if (!regionMapping(region_) && complexRegionMapping(region_)) {
      performComplexRegionMappings();
    }
  }

  // No variant subtag replacements are currently present.
  // No extension replacements are currently present.
  // Private use sequences are left as is.

  // The two final steps in 3.3.1, handling irregular grandfathered and
  // private-use only language tags, don't apply, because these two forms
  // can't occur in Unicode BCP 47 locale identifiers.

  return true;
}

bool LanguageTag::canonicalizeExtensions(
    JSContext* cx, UnicodeExtensionCanonicalForm canonicalForm) {
  // Any extensions are in alphabetical order by their singleton.
  // "u-ca-chinese-t-zh-latn" -> "t-zh-latn-u-ca-chinese"
  if (!SortAlphabetically(cx, extensions_)) {
    return false;
  }

  for (UniqueChars& extension : extensions_) {
    if (extension[0] == 'u') {
      if (!canonicalizeUnicodeExtension(cx, extension, canonicalForm)) {
        return false;
      }
    } else if (extension[0] == 't') {
      if (!canonicalizeTransformExtension(cx, extension)) {
        return false;
      }
    }
  }
  return true;
}

/**
 * CanonicalizeUnicodeExtension( attributes, keywords )
 *
 * Canonical syntax per
 * <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>:
 *
 * - All attributes and keywords are in lowercase.
 *   - Note: The parser already converted keywords to lowercase.
 * - All attributes are sorted in alphabetical order.
 * - All keywords are sorted by alphabetical order of their keys.
 * - Any type value "true" is removed.
 *
 * Canonical form:
 * - All keys and types use the canonical form (from the name attribute;
 *   see Section 3.6.4 U Extension Data Files).
 */
bool LanguageTag::canonicalizeUnicodeExtension(
    JSContext* cx, JS::UniqueChars& unicodeExtension,
    UnicodeExtensionCanonicalForm canonicalForm) {
  const char* const extension = unicodeExtension.get();
  MOZ_ASSERT(extension[0] == 'u');
  MOZ_ASSERT(extension[1] == '-');
  MOZ_ASSERT(IsStructurallyValidExtensionTag({extension, strlen(extension)}));

  size_t length = strlen(extension);

  LanguageTagParser::AttributesVector attributes(cx);
  LanguageTagParser::KeywordsVector keywords(cx);

  using Attribute = LanguageTagParser::AttributesVector::ElementType;
  using Keyword = LanguageTagParser::KeywordsVector::ElementType;

  bool ok;
  JS_TRY_VAR_OR_RETURN_FALSE(
      cx, ok,
      LanguageTagParser::parseUnicodeExtension(
          cx, ConstCharRange(extension, length), attributes, keywords));
  MOZ_ASSERT(ok, "unexpected invalid Unicode extension subtag");

  auto attributesLessOrEqual = [extension](const Attribute& a,
                                           const Attribute& b) {
    const char* astr = a.begin(extension);
    const char* bstr = b.begin(extension);
    size_t alen = a.length();
    size_t blen = b.length();

    if (int r =
            std::char_traits<char>::compare(astr, bstr, std::min(alen, blen))) {
      return r < 0;
    }
    return alen <= blen;
  };

  // All attributes are sorted in alphabetical order.
  size_t attributesLength = attributes.length();
  if (attributesLength > 1) {
    if (!attributes.growByUninitialized(attributesLength)) {
      return false;
    }

    MOZ_ALWAYS_TRUE(
        MergeSort(attributes.begin(), attributesLength,
                  attributes.begin() + attributesLength,
                  [&](const auto& a, const auto& b, bool* lessOrEqualp) {
                    *lessOrEqualp = attributesLessOrEqual(a, b);
                    return true;
                  }));

    attributes.shrinkBy(attributesLength);
  }

  auto keywordsLessOrEqual = [extension](const Keyword& a, const Keyword& b) {
    const char* astr = a.begin(extension);
    const char* bstr = b.begin(extension);
    MOZ_ASSERT(a.length() >= UnicodeKeyLength);
    MOZ_ASSERT(b.length() >= UnicodeKeyLength);

    return std::char_traits<char>::compare(astr, bstr, UnicodeKeyLength) <= 0;
  };

  // All keywords are sorted by alphabetical order of keys.
  size_t keywordsLength = keywords.length();
  if (keywordsLength > 1) {
    if (!keywords.growByUninitialized(keywordsLength)) {
      return false;
    }

    // Using merge sort, being a stable sort algorithm, guarantees that two
    // keywords using the same key are never reordered. That means for example
    // when we have the input "u-nu-thai-kf-false-nu-latn", we are guaranteed to
    // get the result "u-kf-false-nu-thai-nu-latn", i.e. "nu-thai" still occurs
    // before "nu-latn".
    // This is required so that deduplication below preserves the first keyword
    // for a given key and discards the rest.
    MOZ_ALWAYS_TRUE(MergeSort(
        keywords.begin(), keywordsLength, keywords.begin() + keywordsLength,
        [&](const auto& a, const auto& b, bool* lessOrEqualp) {
          *lessOrEqualp = keywordsLessOrEqual(a, b);
          return true;
        }));

    keywords.shrinkBy(keywordsLength);
  }

  Vector<char, 32> sb(cx);
  if (!sb.append('u')) {
    return false;
  }

  // Append all Unicode extension attributes.
  for (size_t i = 0; i < attributes.length(); i++) {
    const auto& attribute = attributes[i];

    // Skip duplicate attributes.
    if (canonicalForm == UnicodeExtensionCanonicalForm::Yes && i > 0) {
      const auto& lastAttribute = attributes[i - 1];
      if (attribute.length() == lastAttribute.length() &&
          std::char_traits<char>::compare(attribute.begin(extension),
                                          lastAttribute.begin(extension),
                                          attribute.length()) == 0) {
        continue;
      }
      MOZ_ASSERT(!attributesLessOrEqual(attribute, lastAttribute));
    }

    if (!sb.append('-')) {
      return false;
    }
    if (!sb.append(attribute.begin(extension), attribute.length())) {
      return false;
    }
  }

  static constexpr size_t UnicodeKeyWithSepLength = UnicodeKeyLength + 1;

  static auto isTrue = [](const ConstCharRange& type) {
    constexpr char True[] = "true";
    const size_t TrueLength = strlen(True);
    return type.length() == TrueLength &&
           std::char_traits<char>::compare(type.begin().get(), True,
                                           TrueLength) == 0;
  };

  auto appendKey = [&sb, extension](const Keyword& keyword) {
    MOZ_ASSERT(keyword.length() == UnicodeKeyLength);
    return sb.append(keyword.begin(extension), UnicodeKeyLength);
  };

  auto appendKeyword = [&sb, extension](const Keyword& keyword,
                                        const ConstCharRange& type) {
    MOZ_ASSERT(keyword.length() > UnicodeKeyLength);

    // Elide the Unicode extension type "true".
    if (isTrue(type)) {
      return sb.append(keyword.begin(extension), UnicodeKeyLength);
    }
    // Otherwise append the complete Unicode extension keyword.
    return sb.append(keyword.begin(extension), keyword.length());
  };

  auto appendReplacement = [&sb, extension](const Keyword& keyword,
                                            const ConstCharRange& replacement) {
    MOZ_ASSERT(keyword.length() > UnicodeKeyLength);

    // Elide the type "true" if present in the replacement.
    if (isTrue(replacement)) {
      return sb.append(keyword.begin(extension), UnicodeKeyLength);
    }
    // Otherwise append the Unicode key (including the separator) and the
    // replaced type.
    return sb.append(keyword.begin(extension), UnicodeKeyWithSepLength) &&
           sb.append(replacement.begin().get(), replacement.length());
  };

  // Append all Unicode extension keywords.
  for (size_t i = 0; i < keywords.length(); i++) {
    const auto& keyword = keywords[i];

    // Skip duplicate keywords.
    if (canonicalForm == UnicodeExtensionCanonicalForm::Yes && i > 0) {
      const auto& lastKeyword = keywords[i - 1];
      if (std::char_traits<char>::compare(keyword.begin(extension),
                                          lastKeyword.begin(extension),
                                          UnicodeKeyLength) == 0) {
        continue;
      }
      MOZ_ASSERT(!keywordsLessOrEqual(keyword, lastKeyword));
    }

    if (!sb.append('-')) {
      return false;
    }

    if (keyword.length() == UnicodeKeyLength) {
      // Keyword without type value.
      if (!appendKey(keyword)) {
        return false;
      }
    } else {
      ConstCharRange key(keyword.begin(extension), UnicodeKeyLength);
      ConstCharRange type(keyword.begin(extension) + UnicodeKeyWithSepLength,
                          keyword.length() - UnicodeKeyWithSepLength);

      if (canonicalForm == UnicodeExtensionCanonicalForm::Yes) {
        // Search if there's a replacement for the current Unicode keyword.
        if (const char* replacement = replaceUnicodeExtensionType(key, type)) {
          if (!appendReplacement(
                  keyword, ConstCharRange(replacement, strlen(replacement)))) {
            return false;
          }
        } else {
          if (!appendKeyword(keyword, type)) {
            return false;
          }
        }
      } else {
        if (!appendKeyword(keyword, type)) {
          return false;
        }
      }
    }
  }

  // We can keep the previous extension when canonicalization didn't modify it.
  if (sb.length() != length ||
      std::char_traits<char>::compare(sb.begin(), extension, length) != 0) {
    // Null-terminate the new string and replace the previous extension.
    if (!sb.append('\0')) {
      return false;
    }
    UniqueChars canonical(sb.extractOrCopyRawBuffer());
    if (!canonical) {
      return false;
    }
    unicodeExtension = std::move(canonical);
  }

  return true;
}

template <class Buffer>
static bool LanguageTagToString(JSContext* cx, const LanguageTag& tag,
                                Buffer& sb) {
  auto appendSubtag = [&sb](const auto& subtag) {
    auto range = subtag.range();
    MOZ_ASSERT(range.length() > 0);
    return sb.append(range.begin().get(), range.length());
  };

  auto appendSubtagZ = [&sb](const char* subtag) {
    MOZ_ASSERT(strlen(subtag) > 0);
    return sb.append(subtag, strlen(subtag));
  };

  auto appendSubtagsZ = [&sb, &appendSubtagZ](const auto& subtags) {
    for (const auto& subtag : subtags) {
      if (!sb.append('-') || !appendSubtagZ(subtag.get())) {
        return false;
      }
    }
    return true;
  };

  // Append the language subtag.
  if (!appendSubtag(tag.language())) {
    return false;
  }

  // Append the script subtag if present.
  if (tag.script().length() > 0) {
    if (!sb.append('-') || !appendSubtag(tag.script())) {
      return false;
    }
  }

  // Append the region subtag if present.
  if (tag.region().length() > 0) {
    if (!sb.append('-') || !appendSubtag(tag.region())) {
      return false;
    }
  }

  // Append the variant subtags if present.
  if (!appendSubtagsZ(tag.variants())) {
    return false;
  }

  // Append the extensions subtags if present.
  if (!appendSubtagsZ(tag.extensions())) {
    return false;
  }

  // Append the private-use subtag if present.
  if (tag.privateuse()) {
    if (!sb.append('-') || !appendSubtagZ(tag.privateuse())) {
      return false;
    }
  }

  return true;
}

/**
 * CanonicalizeTransformExtension
 *
 * Canonical form per <https://unicode.org/reports/tr35/#BCP47_T_Extension>:
 *
 * - These subtags are all in lowercase (that is the canonical casing for these
 *   subtags), [...].
 *
 * And per
 * <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>:
 *
 * - All keywords and tfields are sorted by alphabetical order of their keys,
 *   within their respective extensions.
 */
bool LanguageTag::canonicalizeTransformExtension(
    JSContext* cx, JS::UniqueChars& transformExtension) {
  const char* const extension = transformExtension.get();
  MOZ_ASSERT(extension[0] == 't');
  MOZ_ASSERT(extension[1] == '-');
  MOZ_ASSERT(IsStructurallyValidExtensionTag({extension, strlen(extension)}));

  size_t length = strlen(extension);

  LanguageTag tag(cx);
  LanguageTagParser::TFieldVector fields(cx);

  using TField = LanguageTagParser::TFieldVector::ElementType;

  bool ok;
  JS_TRY_VAR_OR_RETURN_FALSE(
      cx, ok,
      LanguageTagParser::parseTransformExtension(
          cx, ConstCharRange(extension, length), tag, fields));
  MOZ_ASSERT(ok, "unexpected invalid transform extension subtag");

  auto tfieldLessOrEqual = [extension](const TField& a, const TField& b) {
    MOZ_ASSERT(a.length() > TransformKeyLength);
    MOZ_ASSERT(b.length() > TransformKeyLength);
    const char* astr = a.begin(extension);
    const char* bstr = b.begin(extension);
    return std::char_traits<char>::compare(astr, bstr, TransformKeyLength) <= 0;
  };

  // All tfields are sorted by alphabetical order of their keys.
  size_t fieldsLength = fields.length();
  if (fieldsLength > 1) {
    if (!fields.growByUninitialized(fieldsLength)) {
      return false;
    }

    MOZ_ALWAYS_TRUE(
        MergeSort(fields.begin(), fieldsLength, fields.begin() + fieldsLength,
                  [&](const auto& a, const auto& b, bool* lessOrEqualp) {
                    *lessOrEqualp = tfieldLessOrEqual(a, b);
                    return true;
                  }));

    fields.shrinkBy(fieldsLength);
  }

  Vector<char, 32> sb(cx);
  if (!sb.append('t')) {
    return false;
  }

  // Append the language subtag if present.
  //
  // [1] is a bit unclear whether or not the `tlang` subtag also needs to be
  // canonicalized (and case-adjusted). For now simply append it as is.
  // (|parseTransformExtension| doesn't alter case from the lowercased form we
  // have previously taken pains to ensure is present in the extension, so no
  // special effort is required to ensure lowercasing.) If we switch to [2], the
  // `tlang` subtag also needs to be canonicalized according to the same rules
  // as `unicode_language_id` subtags are canonicalized. Also see [3].
  //
  // [1] https://unicode.org/reports/tr35/#Language_Tag_to_Locale_Identifier
  // [2] https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers
  // [3] https://github.com/tc39/ecma402/issues/330
  if (tag.language().length() > 0) {
    if (!sb.append('-')) {
      return false;
    }
    if (!LanguageTagToString(cx, tag, sb)) {
      return false;
    }
  }

  // Append all fields.
  //
  // UTS 35, 3.2.1 specifies:
  // - Any type or tfield value "true" is removed.
  //
  // But the `tvalue` subtag is mandatory in `tfield: tkey tvalue`, so ignore
  // this apparently invalid part of the UTS 35 specification and simply
  // append all `tfield` subtags.
  for (const auto& field : fields) {
    if (!sb.append('-')) {
      return false;
    }
    if (!sb.append(field.begin(extension), field.length())) {
      return false;
    }
  }

  // We can keep the previous extension when canonicalization didn't modify it.
  if (sb.length() != length ||
      std::char_traits<char>::compare(sb.begin(), extension, length) != 0) {
    // Null-terminate the new string and replace the previous extension.
    if (!sb.append('\0')) {
      return false;
    }
    UniqueChars canonical(sb.extractOrCopyRawBuffer());
    if (!canonical) {
      return false;
    }
    transformExtension = std::move(canonical);
  }

  return true;
}

JSString* LanguageTag::toString(JSContext* cx) const {
  JSStringBuilder sb(cx);
  if (!LanguageTagToString(cx, *this, sb)) {
    return nullptr;
  }

  return sb.finishString();
}

// Zero-terminated ICU Locale ID.
using LocaleId =
    js::Vector<char, LanguageLength + 1 + ScriptLength + 1 + RegionLength + 1>;

enum class LikelySubtags : bool { Add, Remove };

// Return true iff the language tag is already maximized resp. minimized.
static bool HasLikelySubtags(LikelySubtags likelySubtags,
                             const LanguageTag& tag) {
  // The language tag is already maximized if the language, script, and region
  // subtags are present and no placeholder subtags ("und", "Zzzz", "ZZ") are
  // used.
  if (likelySubtags == LikelySubtags::Add) {
    return !tag.language().equalTo("und") &&
           (tag.script().length() > 0 && !tag.script().equalTo("Zzzz")) &&
           (tag.region().length() > 0 && !tag.region().equalTo("ZZ"));
  }

  // The language tag is already minimized if it only contains a language
  // subtag whose value is not the placeholder value "und".
  return !tag.language().equalTo("und") && tag.script().length() == 0 &&
         tag.region().length() == 0;
}

// Create an ICU locale ID from the given language tag.
static bool CreateLocaleForLikelySubtags(const LanguageTag& tag,
                                         LocaleId& locale) {
  MOZ_ASSERT(locale.length() == 0);

  auto appendSubtag = [&locale](const auto& subtag) {
    auto range = subtag.range();
    MOZ_ASSERT(range.length() > 0);
    return locale.append(range.begin().get(), range.length());
  };

  // Append the language subtag.
  if (!appendSubtag(tag.language())) {
    return false;
  }

  // Append the script subtag if present.
  if (tag.script().length() > 0) {
    if (!locale.append('_') || !appendSubtag(tag.script())) {
      return false;
    }
  }

  // Append the region subtag if present.
  if (tag.region().length() > 0) {
    if (!locale.append('_') || !appendSubtag(tag.region())) {
      return false;
    }
  }

  // Zero-terminated for use with ICU.
  return locale.append('\0');
}

// Assign the language, script, and region subtags from an ICU locale ID.
//
// ICU provides |uloc_getLanguage|, |uloc_getScript|, and |uloc_getCountry| to
// retrieve these subtags, but unfortunately these functions are rather slow, so
// we use our own implementation.
static bool AssignFromLocaleId(JSContext* cx, LocaleId& localeId,
                               LanguageTag& tag) {
  MOZ_ASSERT(localeId.back() == '\0',
             "Locale ID should be zero-terminated for ICU");

  // Replace the ICU locale ID separator.
  std::replace(localeId.begin(), localeId.end(), '_', '-');

  // ICU replaces "und" with the empty string, which means "und" becomes "" and
  // "und-Latn" becomes "-Latn". Handle this case separately.
  if (localeId[0] == '\0' || localeId[0] == '-') {
    static constexpr char und[] = "und";
    size_t length = strlen(und);

    // Insert "und" in front of the locale ID.
    if (!localeId.growBy(length)) {
      return false;
    }
    memmove(localeId.begin() + length, localeId.begin(), localeId.length());
    memmove(localeId.begin(), und, length);
  }

  ConstCharRange localeRange(localeId.begin(), localeId.length() - 1);

  // Retrieve the language, script, and region subtags from the locale ID, but
  // ignore any other subtags.
  LanguageTag localeTag(cx);
  if (!LanguageTagParser::parseBaseName(cx, localeRange, localeTag)) {
    return false;
  }

  tag.setLanguage(localeTag.language());
  tag.setScript(localeTag.script());
  tag.setRegion(localeTag.region());

  return true;
}

template <decltype(uloc_addLikelySubtags) likelySubtagsFn>
static bool CallLikelySubtags(JSContext* cx, const LocaleId& localeId,
                              LocaleId& result) {
  // Locale ID must be zero-terminated before passing it to ICU.
  MOZ_ASSERT(localeId.back() == '\0');
  MOZ_ASSERT(result.length() == 0);

  // Ensure there's enough room for the result.
  MOZ_ALWAYS_TRUE(result.resize(LocaleId::InlineLength));

  int32_t length = intl::CallICU(
      cx,
      [&localeId](char* chars, int32_t size, UErrorCode* status) {
        return likelySubtagsFn(localeId.begin(), chars, size, status);
      },
      result);
  if (length < 0) {
    return false;
  }

  MOZ_ASSERT(
      size_t(length) <= LocaleId::InlineLength,
      "Unexpected extra subtags were added by ICU. If this assertion ever "
      "fails, simply remove it and move on like nothing ever happended.");

  // Resize the vector to the actual string length.
  result.shrinkTo(length);

  // Zero-terminated for use with ICU.
  return result.append('\0');
}

// The canonical way to compute the Unicode BCP 47 locale identifier with likely
// subtags is as follows:
//
// 1. Call uloc_forLanguageTag() to transform the locale identifer into an ICU
//    locale ID.
// 2. Call uloc_addLikelySubtags() to add the likely subtags to the locale ID.
// 3. Call uloc_toLanguageTag() to transform the resulting locale ID back into
//    a Unicode BCP 47 locale identifier.
//
// Since uloc_forLanguageTag() and uloc_toLanguageTag() are both kind of slow
// and we know, by construction, that the input Unicode BCP 47 locale identifier
// only contains valid language, script, and region subtags, we can avoid both
// calls if we implement them ourselves, see CreateLocaleForLikelySubtags() and
// AssignFromLocaleId(). (Where "slow" means about 50% of the execution time of
// |Intl.Locale.prototype.maximize|.)
static bool LikelySubtags(JSContext* cx, LikelySubtags likelySubtags,
                          LanguageTag& tag) {
  // Return early if the input is already maximized/minimized.
  if (HasLikelySubtags(likelySubtags, tag)) {
    return true;
  }

  // Create the locale ID for the input argument.
  LocaleId locale(cx);
  if (!CreateLocaleForLikelySubtags(tag, locale)) {
    return false;
  }

  // UTS #35 requires that locale ID is maximized before its likely subtags are
  // removed, so we need to call uloc_addLikelySubtags() for both cases.
  // See <https://ssl.icu-project.org/trac/ticket/10220> and
  // <https://ssl.icu-project.org/trac/ticket/12345>.

  LocaleId localeLikelySubtags(cx);

  // Add likely subtags to the locale ID. When minimizing we can skip adding the
  // likely subtags for already maximized tags. (When maximizing we've already
  // verified above that the tag is missing likely subtags.)
  bool addLikelySubtags = likelySubtags == LikelySubtags::Add ||
                          !HasLikelySubtags(LikelySubtags::Add, tag);

  if (addLikelySubtags) {
    if (!CallLikelySubtags<uloc_addLikelySubtags>(cx, locale,
                                                  localeLikelySubtags)) {
      return false;
    }
  }

  // Now that we've succesfully maximized the locale, we can minimize it.
  if (likelySubtags == LikelySubtags::Remove) {
    if (addLikelySubtags) {
      // Copy the maximized subtags back into |locale|.
      locale = std::move(localeLikelySubtags);
      localeLikelySubtags = LocaleId(cx);
    }

    // Remove likely subtags from the locale ID.
    if (!CallLikelySubtags<uloc_minimizeSubtags>(cx, locale,
                                                 localeLikelySubtags)) {
      return false;
    }
  }

  // Assign the language, script, and region subtags from the locale ID.
  if (!AssignFromLocaleId(cx, localeLikelySubtags, tag)) {
    return false;
  }

  // Update mappings in case ICU returned a non-canonical locale.
  return tag.canonicalizeBaseName(cx);
}

bool LanguageTag::addLikelySubtags(JSContext* cx) {
  return LikelySubtags(cx, LikelySubtags::Add, *this);
}

bool LanguageTag::removeLikelySubtags(JSContext* cx) {
  return LikelySubtags(cx, LikelySubtags::Remove, *this);
}

LanguageTagParser::Token LanguageTagParser::nextToken() {
  MOZ_ASSERT(index_ <= length_ + 1, "called after 'None' token was read");

  TokenKind kind = TokenKind::None;
  size_t tokenLength = 0;
  for (size_t i = index_; i < length_; i++) {
    // UTS 35, section 3.1.
    // alpha = [A-Z a-z] ;
    // digit = [0-9] ;
    char16_t c = charAtUnchecked(i);
    if (mozilla::IsAsciiAlpha(c)) {
      kind |= TokenKind::Alpha;
    } else if (mozilla::IsAsciiDigit(c)) {
      kind |= TokenKind::Digit;
    } else if (c == '-' && i > index_ && i + 1 < length_) {
      break;
    } else {
      return {TokenKind::Error, 0, 0};
    }
    tokenLength += 1;
  }

  Token token{kind, index_, tokenLength};
  index_ += tokenLength + 1;
  return token;
}

UniqueChars LanguageTagParser::chars(JSContext* cx, size_t index,
                                     size_t length) const {
  // Add +1 to null-terminate the string.
  auto chars = cx->make_pod_array<char>(length + 1);
  if (chars) {
    char* dest = chars.get();
    if (locale_.is<const JS::Latin1Char*>()) {
      std::copy_n(locale_.as<const JS::Latin1Char*>() + index, length, dest);
    } else {
      std::copy_n(locale_.as<const char16_t*>() + index, length, dest);
    }
    dest[length] = '\0';
  }
  return chars;
}

UniqueChars LanguageTagParser::extension(JSContext* cx, const Token& start,
                                         const Token& end) const {
  MOZ_ASSERT(start.index() < end.index());

  size_t length = end.index() - 1 - start.index();
  UniqueChars extension = chars(cx, start.index(), length);
  if (extension) {
    AsciiToLowerCase(extension.get(), length, extension.get());
  }
  return extension;
}

// Parse the `unicode_language_id` production.
//
// unicode_language_id = unicode_language_subtag
//                       (sep unicode_script_subtag)?
//                       (sep unicode_region_subtag)?
//                       (sep unicode_variant_subtag)* ;
//
// sep                 = "-"
//
// Note: Unicode CLDR locale identifier backward compatibility extensions
//       removed from `unicode_language_id`.
//
// |tok| is the current token from |ts|.
//
// The trailing |parseType| argument corresponds to one of two modes.
//
// In the |BaseNameParsing::Normal| mode, our input is in unknown case and is
// potentially invalid. |tag| will be filled with canonically-cased output, and
// duplicate variants will lead to an error.
//
// In the |BaseNameParsing::WithinTransformExtension| mode, our input is the
// `tlang` in a lowercased `transform_extensions`. |tag| subtags will be
// directly copied from the input (i.e. in lowercase). Variant subtags in the
// `tlang` subtag may contain duplicates.
//
// Do not use this function directly: use |parseBaseName| or
// |parseTlangFromTransformExtension| instead.
JS::Result<bool> LanguageTagParser::internalParseBaseName(
    JSContext* cx, LanguageTagParser& ts, LanguageTag& tag, Token& tok,
    BaseNameParsing parseType) {
#ifdef DEBUG
  auto isAsciiLowerCase = [](const auto& range) {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    const char* ptr = range.begin().get();
    size_t length = range.length();
    return std::all_of(ptr, ptr + length, mozilla::IsAsciiLowercaseAlpha<char>);
  };
  auto isAsciiDigit = [](const auto& range) {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    const char* ptr = range.begin().get();
    size_t length = range.length();
    return std::all_of(ptr, ptr + length, mozilla::IsAsciiDigit<char>);
  };
#endif

  if (ts.isLanguage(tok)) {
    ts.copyChars(tok, tag.language_);

    // Language codes need to be in lower case. "JA" -> "ja"
    if (parseType == BaseNameParsing::Normal) {
      tag.language_.toLowerCase();
    } else {
      MOZ_ASSERT(isAsciiLowerCase(tag.language_.range()));
    }

    tok = ts.nextToken();
  } else {
    MOZ_ASSERT(parseType == BaseNameParsing::Normal);

    // The language subtag is mandatory.
    return false;
  }

  if (ts.isScript(tok)) {
    ts.copyChars(tok, tag.script_);

    // The first character of a script code needs to be capitalized.
    // "hans" -> "Hans"
    if (parseType == BaseNameParsing::Normal) {
      tag.script_.toTitleCase();
    } else {
      MOZ_ASSERT(isAsciiLowerCase(tag.script_.range()));
    }

    tok = ts.nextToken();
  }

  if (ts.isRegion(tok)) {
    ts.copyChars(tok, tag.region_);

    // Region codes need to be in upper case. "bu" -> "BU"
    if (parseType == BaseNameParsing::Normal) {
      tag.region_.toUpperCase();
    } else {
      MOZ_ASSERT_IF(tok.length() == 2, isAsciiLowerCase(tag.region_.range()));
      MOZ_ASSERT_IF(tok.length() == 3, isAsciiDigit(tag.region_.range()));
    }

    tok = ts.nextToken();
  }

  auto& variants = tag.variants_;
  MOZ_ASSERT(variants.length() == 0);
  while (ts.isVariant(tok)) {
    auto variant = ts.chars(cx, tok);
    if (!variant) {
      return cx->alreadyReportedOOM();
    }

    if (parseType == BaseNameParsing::Normal) {
      // Locale identifiers are case insensitive (UTS 35, section 3.2).
      // All seen variants are compared ignoring case differences by using the
      // lower case form. This allows to properly detect and reject variant
      // repetitions with differing case, e.g. "en-variant-Variant".
      AsciiToLowerCase(variant.get(), tok.length(), variant.get());

      // Reject the Locale identifier if a duplicate variant was found.
      //
      // This linear-time verification step means the whole variant subtag
      // checking is potentially quadratic. Language tags are unlikely to be
      // deliberately pathological, so this is okay at least for now.
      for (const auto& seenVariant : variants) {
        if (strcmp(variant.get(), seenVariant.get()) == 0) {
          return false;
        }
      }
    } else {
      // When parsing variants in a `tlang` subtag, duplicates are allowed.
    }

    if (!variants.append(std::move(variant))) {
      return cx->alreadyReportedOOM();
    }

    tok = ts.nextToken();
  }

  return true;
}

static mozilla::Variant<const Latin1Char*, const char16_t*> StringChars(
    const char* locale) {
  return mozilla::AsVariant(reinterpret_cast<const JS::Latin1Char*>(locale));
}

static mozilla::Variant<const Latin1Char*, const char16_t*> StringChars(
    JSLinearString* linear, JS::AutoCheckCannotGC& nogc) {
  if (linear->hasLatin1Chars()) {
    return mozilla::AsVariant(linear->latin1Chars(nogc));
  }
  return mozilla::AsVariant(linear->twoByteChars(nogc));
}

JS::Result<bool> LanguageTagParser::tryParse(JSContext* cx,
                                             JSLinearString* locale,
                                             LanguageTag& tag) {
  JS::AutoCheckCannotGC nogc;
  LocaleChars localeChars = StringChars(locale, nogc);

  // unicode_locale_id = unicode_language_id
  //                     extensions*
  //                     pu_extensions? ;

  LanguageTagParser ts(localeChars, locale->length());
  Token tok = ts.nextToken();

  bool ok;
  MOZ_TRY_VAR(ok, parseBaseName(cx, ts, tag, tok));
  if (!ok) {
    return false;
  }

  // extensions = unicode_locale_extensions
  //            | transformed_extensions
  //            | other_extensions ;

  // Bit set of seen singletons.
  uint64_t seenSingletons = 0;

  auto& extensions = tag.extensions_;
  while (ts.isExtensionStart(tok)) {
    char singleton = ts.singletonKey(tok);

    // Reject the input if a duplicate singleton was found.
    uint64_t hash = 1ULL << (mozilla::AsciiAlphanumericToNumber(singleton) + 1);
    if (seenSingletons & hash) {
      return false;
    }
    seenSingletons |= hash;

    Token start = tok;
    tok = ts.nextToken();

    // We'll check for missing non-singleton subtags after this block by
    // comparing |startValue| with the then-current position.
    size_t startValue = tok.index();

    if (singleton == 'u') {
      while (ts.isUnicodeExtensionPart(tok)) {
        tok = ts.nextToken();
      }
    } else if (singleton == 't') {
      // transformed_extensions = sep [tT]
      //                          ((sep tlang (sep tfield)*)
      //                           | (sep tfield)+) ;

      // tlang = unicode_language_subtag
      //         (sep unicode_script_subtag)?
      //         (sep unicode_region_subtag)?
      //         (sep unicode_variant_subtag)* ;
      if (ts.isLanguage(tok)) {
        tok = ts.nextToken();

        if (ts.isScript(tok)) {
          tok = ts.nextToken();
        }

        if (ts.isRegion(tok)) {
          tok = ts.nextToken();
        }

        while (ts.isVariant(tok)) {
          tok = ts.nextToken();
        }
      }

      // tfield = tkey tvalue;
      while (ts.isTransformExtensionKey(tok)) {
        tok = ts.nextToken();

        size_t startTValue = tok.index();
        while (ts.isTransformExtensionPart(tok)) {
          tok = ts.nextToken();
        }

        // `tfield` requires at least one `tvalue`.
        if (tok.index() <= startTValue) {
          return false;
        }
      }
    } else {
      while (ts.isOtherExtensionPart(tok)) {
        tok = ts.nextToken();
      }
    }

    // Singletons must be followed by a non-singleton subtag, "en-a-b" is not
    // allowed.
    if (tok.index() <= startValue) {
      return false;
    }

    UniqueChars extension = ts.extension(cx, start, tok);
    if (!extension) {
      return cx->alreadyReportedOOM();
    }
    if (!extensions.append(std::move(extension))) {
      return cx->alreadyReportedOOM();
    }
  }

  // Trailing `pu_extension` component of the `unicode_locale_id` production.
  if (ts.isPrivateUseStart(tok)) {
    Token start = tok;
    tok = ts.nextToken();

    size_t startValue = tok.index();
    while (ts.isPrivateUsePart(tok)) {
      tok = ts.nextToken();
    }

    // There must be at least one subtag after the "-x-".
    if (tok.index() <= startValue) {
      return false;
    }

    UniqueChars privateUse = ts.extension(cx, start, tok);
    if (!privateUse) {
      return cx->alreadyReportedOOM();
    }
    tag.privateuse_ = std::move(privateUse);
  }

  // Return true if the complete input was successfully parsed.
  return tok.isNone();
}

bool LanguageTagParser::parse(JSContext* cx, JSLinearString* locale,
                              LanguageTag& tag) {
  bool ok;
  JS_TRY_VAR_OR_RETURN_FALSE(cx, ok, tryParse(cx, locale, tag));
  if (ok) {
    return true;
  }
  if (UniqueChars localeChars = QuoteString(cx, locale, '"')) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INVALID_LANGUAGE_TAG, localeChars.get());
  }
  return false;
}

bool LanguageTagParser::parseBaseName(JSContext* cx, ConstCharRange locale,
                                      LanguageTag& tag) {
  LocaleChars localeChars = StringChars(locale.begin().get());
  LanguageTagParser ts(localeChars, locale.length());
  Token tok = ts.nextToken();

  // Parse only the base-name part and ignore any trailing characters.
  bool ok;
  JS_TRY_VAR_OR_RETURN_FALSE(cx, ok, parseBaseName(cx, ts, tag, tok));
  if (ok) {
    return true;
  }
  if (UniqueChars localeChars =
          DuplicateString(locale.begin().get(), locale.length())) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INVALID_LANGUAGE_TAG, localeChars.get());
  } else {
    JS_ReportOutOfMemory(cx);
  }
  return false;
}

// Parse |extension|, which must be a valid `transformed_extensions` subtag, and
// fill |tag| and |fields| from the `tlang` and `tfield` components.
JS::Result<bool> LanguageTagParser::parseTransformExtension(
    JSContext* cx, ConstCharRange extension, LanguageTag& tag,
    TFieldVector& fields) {
  LocaleChars extensionChars = StringChars(extension.begin().get());
  LanguageTagParser ts(extensionChars, extension.length());
  Token tok = ts.nextToken();

  if (!ts.isExtensionStart(tok) || ts.singletonKey(tok) != 't') {
    return false;
  }

  tok = ts.nextToken();

  if (tok.isNone()) {
    return false;
  }

  if (ts.isLanguage(tok)) {
    // We're parsing a possible `tlang` in a known-valid transform extension, so
    // use the special-purpose function that takes advantage of this to compute
    // lowercased |tag| contents in an optimal manner.
    MOZ_TRY(parseTlangInTransformExtension(cx, ts, tag, tok));

    // After `tlang` we must have a `tfield` and its `tkey`, or we're at the end
    // of the transform extension.
    MOZ_ASSERT(ts.isTransformExtensionKey(tok) || tok.isNone());
  } else {
    // If there's no `tlang` subtag, at least one `tfield` must be present.
    MOZ_ASSERT(ts.isTransformExtensionKey(tok));
  }

  // Trailing `tfield` subtags. (Any other trailing subtags are an error,
  // because we're guaranteed to only see a valid tranform extension here.)
  while (ts.isTransformExtensionKey(tok)) {
    size_t begin = tok.index();
    tok = ts.nextToken();

    size_t startTValue = tok.index();
    while (ts.isTransformExtensionPart(tok)) {
      tok = ts.nextToken();
    }

    // `tfield` requires at least one `tvalue`.
    if (tok.index() <= startTValue) {
      return false;
    }

    size_t length = tok.index() - 1 - begin;
    if (!fields.emplaceBack(begin, length)) {
      return cx->alreadyReportedOOM();
    }
  }

  // Return true if the complete input was successfully parsed.
  return tok.isNone();
}

// Parse |extension|, which must be a valid `unicode_locale_extensions` subtag,
// and fill |attributes| and |keywords| from the `attribute` and `keyword`
// components.
JS::Result<bool> LanguageTagParser::parseUnicodeExtension(
    JSContext* cx, ConstCharRange extension, AttributesVector& attributes,
    KeywordsVector& keywords) {
  LocaleChars extensionChars = StringChars(extension.begin().get());
  LanguageTagParser ts(extensionChars, extension.length());
  Token tok = ts.nextToken();

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;

  if (!ts.isExtensionStart(tok) || ts.singletonKey(tok) != 'u') {
    return false;
  }

  tok = ts.nextToken();

  if (tok.isNone()) {
    return false;
  }

  while (ts.isUnicodeExtensionAttribute(tok)) {
    if (!attributes.emplaceBack(tok.index(), tok.length())) {
      return cx->alreadyReportedOOM();
    }

    tok = ts.nextToken();
  }

  // keyword = key (sep type)? ;
  while (ts.isUnicodeExtensionKey(tok)) {
    size_t begin = tok.index();
    tok = ts.nextToken();

    while (ts.isUnicodeExtensionType(tok)) {
      tok = ts.nextToken();
    }

    if (tok.isError()) {
      return false;
    }

    size_t length = tok.index() - 1 - begin;
    if (!keywords.emplaceBack(begin, length)) {
      return cx->alreadyReportedOOM();
    }
  }

  // Return true if the complete input was successfully parsed.
  return tok.isNone();
}

bool LanguageTagParser::canParseUnicodeExtension(ConstCharRange extension) {
  LocaleChars extensionChars = StringChars(extension.begin().get());
  LanguageTagParser ts(extensionChars, extension.length());
  Token tok = ts.nextToken();

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;

  if (!ts.isExtensionStart(tok) || ts.singletonKey(tok) != 'u') {
    return false;
  }

  tok = ts.nextToken();

  if (tok.isNone()) {
    return false;
  }

  while (ts.isUnicodeExtensionAttribute(tok)) {
    tok = ts.nextToken();
  }

  // keyword = key (sep type)? ;
  while (ts.isUnicodeExtensionKey(tok)) {
    tok = ts.nextToken();

    while (ts.isUnicodeExtensionType(tok)) {
      tok = ts.nextToken();
    }

    if (tok.isError()) {
      return false;
    }
  }

  // Return true if the complete input was successfully parsed.
  return tok.isNone();
}

bool LanguageTagParser::canParseUnicodeExtensionType(
    JSLinearString* unicodeType) {
  JS::AutoCheckCannotGC nogc;
  LocaleChars unicodeTypeChars = StringChars(unicodeType, nogc);

  LanguageTagParser ts(unicodeTypeChars, unicodeType->length());
  Token tok = ts.nextToken();

  while (ts.isUnicodeExtensionType(tok)) {
    tok = ts.nextToken();
  }

  // Return true if the complete input was successfully parsed.
  return tok.isNone();
}

bool ParseStandaloneLanguagTag(HandleLinearString str, LanguageSubtag& result) {
  auto isLanguage = [](const auto* language, size_t length) {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    using T = std::remove_pointer_t<decltype(language)>;
    return length >= 2 && length != 4 && length <= 8 &&
           std::all_of(language, language + length, mozilla::IsAsciiAlpha<T>);
  };

  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    if (!isLanguage(str->latin1Chars(nogc), str->length())) {
      return false;
    }
    result.set(str->latin1Range(nogc));
  } else {
    if (!isLanguage(str->twoByteChars(nogc), str->length())) {
      return false;
    }
    result.set(str->twoByteRange(nogc));
  }
  result.toLowerCase();
  return true;
}

bool ParseStandaloneScriptTag(HandleLinearString str, ScriptSubtag& result) {
  auto isScript = [](const auto* script, size_t length) {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    using T = std::remove_pointer_t<decltype(script)>;
    return length == ScriptLength &&
           std::all_of(script, script + ScriptLength, mozilla::IsAsciiAlpha<T>);
  };

  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    if (!isScript(str->latin1Chars(nogc), str->length())) {
      return false;
    }
    result.set(str->latin1Range(nogc));
  } else {
    if (!isScript(str->twoByteChars(nogc), str->length())) {
      return false;
    }
    result.set(str->twoByteRange(nogc));
  }
  result.toTitleCase();
  return true;
}

bool ParseStandaloneRegionTag(HandleLinearString str, RegionSubtag& result) {
  auto isRegion = [](const auto* region, size_t length) {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    using T = std::remove_pointer_t<decltype(region)>;
    return (length == AlphaRegionLength &&
            std::all_of(region, region + AlphaRegionLength,
                        mozilla::IsAsciiAlpha<T>)) ||
           (length == DigitRegionLength &&
            std::all_of(region, region + DigitRegionLength,
                        mozilla::IsAsciiDigit<T>));
  };

  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    if (!isRegion(str->latin1Chars(nogc), str->length())) {
      return false;
    }
    result.set(str->latin1Range(nogc));
  } else {
    if (!isRegion(str->twoByteChars(nogc), str->length())) {
      return false;
    }
    result.set(str->twoByteRange(nogc));
  }
  result.toUpperCase();
  return true;
}

template <typename CharT>
static bool IsAsciiLowercaseAlpha(const mozilla::Range<const CharT>& range) {
  // Tell the analysis the |std::all_of| function can't GC.
  JS::AutoSuppressGCAnalysis nogc;

  const CharT* ptr = range.begin().get();
  size_t length = range.length();
  return std::all_of(ptr, ptr + length, mozilla::IsAsciiLowercaseAlpha<CharT>);
}

static bool IsAsciiLowercaseAlpha(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  return str->hasLatin1Chars() ? IsAsciiLowercaseAlpha(str->latin1Range(nogc))
                               : IsAsciiLowercaseAlpha(str->twoByteRange(nogc));
}

template <typename CharT>
static bool IsAsciiAlpha(const mozilla::Range<const CharT>& range) {
  // Tell the analysis the |std::all_of| function can't GC.
  JS::AutoSuppressGCAnalysis nogc;

  const CharT* ptr = range.begin().get();
  size_t length = range.length();
  return std::all_of(ptr, ptr + length, mozilla::IsAsciiAlpha<CharT>);
}

static bool IsAsciiAlpha(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  return str->hasLatin1Chars() ? IsAsciiAlpha(str->latin1Range(nogc))
                               : IsAsciiAlpha(str->twoByteRange(nogc));
}

JS::Result<JSString*> ParseStandaloneISO639LanguageTag(JSContext* cx,
                                                       HandleLinearString str) {
  // ISO-639 language codes contain either two or three characters.
  size_t length = str->length();
  if (length != 2 && length != 3) {
    return nullptr;
  }

  // We can directly the return the input below if it's in the correct case.
  bool isLowerCase = IsAsciiLowercaseAlpha(str);
  if (!isLowerCase) {
    // Must be an ASCII alpha string.
    if (!IsAsciiAlpha(str)) {
      return nullptr;
    }
  }

  LanguageSubtag languageTag;
  if (str->hasLatin1Chars()) {
    JS::AutoCheckCannotGC nogc;
    languageTag.set(str->latin1Range(nogc));
  } else {
    JS::AutoCheckCannotGC nogc;
    languageTag.set(str->twoByteRange(nogc));
  }

  if (!isLowerCase) {
    // The language subtag is canonicalized to lower case.
    languageTag.toLowerCase();
  }

  // Reject the input if the canonical tag contains more than just a single
  // language subtag.
  if (LanguageTag::complexLanguageMapping(languageTag)) {
    return nullptr;
  }

  // Take care to replace deprecated subtags with their preferred values.
  JSString* result;
  if (LanguageTag::languageMapping(languageTag) || !isLowerCase) {
    auto range = languageTag.range();
    result = NewStringCopyN<CanGC>(cx, range.begin().get(), range.length());
  } else {
    result = str;
  }
  if (!result) {
    return cx->alreadyReportedOOM();
  }
  return result;
}

}  // namespace intl
}  // namespace js
