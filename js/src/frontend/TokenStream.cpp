/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// JS lexical scanner.

#include "frontend/TokenStream.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"

#include <algorithm>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <utility>

#include "jsexn.h"
#include "jsnum.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/Parser.h"
#include "frontend/ReservedWords.h"
#include "js/CharacterEncoding.h"
#include "js/UniquePtr.h"
#include "util/StringBuffer.h"
#include "util/Unicode.h"
#include "vm/HelperThreads.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"

using mozilla::ArrayLength;
using mozilla::AssertedCast;
using mozilla::DecodeOneUtf8CodePoint;
using mozilla::IsAscii;
using mozilla::IsAsciiAlpha;
using mozilla::IsAsciiDigit;
using mozilla::IsTrailingUnit;
using mozilla::MakeScopeExit;
using mozilla::MakeSpan;
using mozilla::Maybe;
using mozilla::PointerRangeSize;
using mozilla::Utf8Unit;

using JS::ReadOnlyCompileOptions;

struct ReservedWordInfo {
  const char* chars;  // C string with reserved word text
  js::frontend::TokenKind tokentype;
};

static const ReservedWordInfo reservedWords[] = {
#define RESERVED_WORD_INFO(word, name, type) \
  {js_##word##_str, js::frontend::type},
    FOR_EACH_JAVASCRIPT_RESERVED_WORD(RESERVED_WORD_INFO)
#undef RESERVED_WORD_INFO
};

// Returns a ReservedWordInfo for the specified characters, or nullptr if the
// string is not a reserved word.
template <typename CharT>
static const ReservedWordInfo* FindReservedWord(const CharT* s, size_t length) {
  MOZ_ASSERT(length != 0);

  size_t i;
  const ReservedWordInfo* rw;
  const char* chars;

#define JSRW_LENGTH() length
#define JSRW_AT(column) s[column]
#define JSRW_GOT_MATCH(index) \
  i = (index);                \
  goto got_match;
#define JSRW_TEST_GUESS(index) \
  i = (index);                 \
  goto test_guess;
#define JSRW_NO_MATCH() goto no_match;
#include "frontend/ReservedWordsGenerated.h"
#undef JSRW_NO_MATCH
#undef JSRW_TEST_GUESS
#undef JSRW_GOT_MATCH
#undef JSRW_AT
#undef JSRW_LENGTH

got_match:
  return &reservedWords[i];

test_guess:
  rw = &reservedWords[i];
  chars = rw->chars;
  do {
    if (*s++ != static_cast<unsigned char>(*chars++)) {
      goto no_match;
    }
  } while (--length != 0);
  return rw;

no_match:
  return nullptr;
}

template <>
MOZ_ALWAYS_INLINE const ReservedWordInfo* FindReservedWord<Utf8Unit>(
    const Utf8Unit* units, size_t length) {
  return FindReservedWord(Utf8AsUnsignedChars(units), length);
}

static const ReservedWordInfo* FindReservedWord(
    JSLinearString* str, js::frontend::NameVisibility* visibility) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    const JS::Latin1Char* chars = str->latin1Chars(nogc);
    size_t length = str->length();
    if (length > 0 && chars[0] == '#') {
      *visibility = js::frontend::NameVisibility::Private;
      return nullptr;
    }
    *visibility = js::frontend::NameVisibility::Public;
    return FindReservedWord(chars, length);
  }

  const char16_t* chars = str->twoByteChars(nogc);
  size_t length = str->length();
  if (length > 0 && chars[0] == '#') {
    *visibility = js::frontend::NameVisibility::Private;
    return nullptr;
  }
  *visibility = js::frontend::NameVisibility::Public;
  return FindReservedWord(chars, length);
}

static uint32_t GetSingleCodePoint(const char16_t** p, const char16_t* end) {
  using namespace js;

  uint32_t codePoint;
  if (MOZ_UNLIKELY(unicode::IsLeadSurrogate(**p)) && *p + 1 < end) {
    char16_t lead = **p;
    char16_t maybeTrail = *(*p + 1);
    if (unicode::IsTrailSurrogate(maybeTrail)) {
      *p += 2;
      return unicode::UTF16Decode(lead, maybeTrail);
    }
  }

  codePoint = **p;
  (*p)++;
  return codePoint;
}

namespace js {

namespace frontend {

bool IsIdentifier(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  MOZ_ASSERT(str);
  if (str->hasLatin1Chars()) {
    return IsIdentifier(str->latin1Chars(nogc), str->length());
  }
  return IsIdentifier(str->twoByteChars(nogc), str->length());
}

bool IsIdentifierNameOrPrivateName(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  MOZ_ASSERT(str);
  if (str->hasLatin1Chars()) {
    return IsIdentifierNameOrPrivateName(str->latin1Chars(nogc), str->length());
  }
  return IsIdentifierNameOrPrivateName(str->twoByteChars(nogc), str->length());
}

bool IsIdentifier(const Latin1Char* chars, size_t length) {
  if (length == 0) {
    return false;
  }

  if (!unicode::IsIdentifierStart(char16_t(*chars))) {
    return false;
  }

  const Latin1Char* end = chars + length;
  while (++chars != end) {
    if (!unicode::IsIdentifierPart(char16_t(*chars))) {
      return false;
    }
  }

  return true;
}

bool IsIdentifierNameOrPrivateName(const Latin1Char* chars, size_t length) {
  if (length == 0) {
    return false;
  }

  if (char16_t(*chars) == '#') {
    ++chars;
    --length;
  }

  return IsIdentifier(chars, length);
}

bool IsIdentifier(const char16_t* chars, size_t length) {
  if (length == 0) {
    return false;
  }

  const char16_t* p = chars;
  const char16_t* end = chars + length;
  uint32_t codePoint;

  codePoint = GetSingleCodePoint(&p, end);
  if (!unicode::IsIdentifierStart(codePoint)) {
    return false;
  }

  while (p < end) {
    codePoint = GetSingleCodePoint(&p, end);
    if (!unicode::IsIdentifierPart(codePoint)) {
      return false;
    }
  }

  return true;
}

bool IsIdentifierNameOrPrivateName(const char16_t* chars, size_t length) {
  if (length == 0) {
    return false;
  }

  const char16_t* p = chars;
  const char16_t* end = chars + length;
  uint32_t codePoint;

  codePoint = GetSingleCodePoint(&p, end);
  if (codePoint == '#') {
    if (length == 1) {
      return false;
    }

    codePoint = GetSingleCodePoint(&p, end);
  }

  if (!unicode::IsIdentifierStart(codePoint)) {
    return false;
  }

  while (p < end) {
    codePoint = GetSingleCodePoint(&p, end);
    if (!unicode::IsIdentifierPart(codePoint)) {
      return false;
    }
  }

  return true;
}

bool IsKeyword(JSLinearString* str) {
  NameVisibility visibility;
  if (const ReservedWordInfo* rw = FindReservedWord(str, &visibility)) {
    return TokenKindIsKeyword(rw->tokentype);
  }

  return false;
}

TokenKind ReservedWordTokenKind(PropertyName* str) {
  NameVisibility visibility;
  if (const ReservedWordInfo* rw = FindReservedWord(str, &visibility)) {
    return rw->tokentype;
  }

  return visibility == NameVisibility::Private ? TokenKind::PrivateName
                                               : TokenKind::Name;
}

const char* ReservedWordToCharZ(PropertyName* str) {
  NameVisibility visibility;
  if (const ReservedWordInfo* rw = FindReservedWord(str, &visibility)) {
    return ReservedWordToCharZ(rw->tokentype);
  }

  return nullptr;
}

const char* ReservedWordToCharZ(TokenKind tt) {
  MOZ_ASSERT(tt != TokenKind::Name);
  switch (tt) {
#define EMIT_CASE(word, name, type) \
  case type:                        \
    return js_##word##_str;
    FOR_EACH_JAVASCRIPT_RESERVED_WORD(EMIT_CASE)
#undef EMIT_CASE
    default:
      MOZ_ASSERT_UNREACHABLE("Not a reserved word PropertyName.");
  }
  return nullptr;
}

PropertyName* TokenStreamAnyChars::reservedWordToPropertyName(
    TokenKind tt) const {
  MOZ_ASSERT(tt != TokenKind::Name);
  switch (tt) {
#define EMIT_CASE(word, name, type) \
  case type:                        \
    return cx->names().name;
    FOR_EACH_JAVASCRIPT_RESERVED_WORD(EMIT_CASE)
#undef EMIT_CASE
    default:
      MOZ_ASSERT_UNREACHABLE("Not a reserved word TokenKind.");
  }
  return nullptr;
}

TokenStreamAnyChars::SourceCoords::SourceCoords(JSContext* cx, uint32_t ln,
                                                uint32_t col,
                                                uint32_t initialLineOffset)
    : lineStartOffsets_(cx),
      initialLineNum_(ln),
      initialColumn_(col),
      lastLineIndex_(0) {
  // This is actually necessary!  Removing it causes compile errors on
  // GCC and clang.  You could try declaring this:
  //
  //   const uint32_t TokenStreamAnyChars::SourceCoords::MAX_PTR;
  //
  // which fixes the GCC/clang error, but causes bustage on Windows.  Sigh.
  //
  uint32_t maxPtr = MAX_PTR;

  // The first line begins at buffer offset |initialLineOffset|.  MAX_PTR is
  // the sentinel.  The appends cannot fail because |lineStartOffsets_| has
  // statically-allocated elements.
  MOZ_ASSERT(lineStartOffsets_.capacity() >= 2);
  MOZ_ALWAYS_TRUE(lineStartOffsets_.reserve(2));
  lineStartOffsets_.infallibleAppend(initialLineOffset);
  lineStartOffsets_.infallibleAppend(maxPtr);
}

MOZ_ALWAYS_INLINE bool TokenStreamAnyChars::SourceCoords::add(
    uint32_t lineNum, uint32_t lineStartOffset) {
  uint32_t lineIndex = lineNumToIndex(lineNum);
  uint32_t sentinelIndex = lineStartOffsets_.length() - 1;

  MOZ_ASSERT(lineStartOffsets_[0] <= lineStartOffset &&
             lineStartOffsets_[sentinelIndex] == MAX_PTR);

  if (lineIndex == sentinelIndex) {
    // We haven't seen this newline before.  Update lineStartOffsets_
    // only if lineStartOffsets_.append succeeds, to keep sentinel.
    // Otherwise return false to tell TokenStream about OOM.
    uint32_t maxPtr = MAX_PTR;
    if (!lineStartOffsets_.append(maxPtr)) {
      static_assert(mozilla::IsSame<decltype(lineStartOffsets_.allocPolicy()),
                                    TempAllocPolicy&>::value,
                    "this function's caller depends on it reporting an "
                    "error on failure, as TempAllocPolicy ensures");
      return false;
    }

    lineStartOffsets_[lineIndex] = lineStartOffset;
  } else {
    // We have seen this newline before (and ungot it).  Do nothing (other
    // than checking it hasn't mysteriously changed).
    // This path can be executed after hitting OOM, so check lineIndex.
    MOZ_ASSERT_IF(lineIndex < sentinelIndex,
                  lineStartOffsets_[lineIndex] == lineStartOffset);
  }
  return true;
}

MOZ_ALWAYS_INLINE bool TokenStreamAnyChars::SourceCoords::fill(
    const TokenStreamAnyChars::SourceCoords& other) {
  MOZ_ASSERT(lineStartOffsets_[0] == other.lineStartOffsets_[0]);
  MOZ_ASSERT(lineStartOffsets_.back() == MAX_PTR);
  MOZ_ASSERT(other.lineStartOffsets_.back() == MAX_PTR);

  if (lineStartOffsets_.length() >= other.lineStartOffsets_.length()) {
    return true;
  }

  uint32_t sentinelIndex = lineStartOffsets_.length() - 1;
  lineStartOffsets_[sentinelIndex] = other.lineStartOffsets_[sentinelIndex];

  for (size_t i = sentinelIndex + 1; i < other.lineStartOffsets_.length();
       i++) {
    if (!lineStartOffsets_.append(other.lineStartOffsets_[i])) {
      return false;
    }
  }
  return true;
}

MOZ_ALWAYS_INLINE uint32_t
TokenStreamAnyChars::SourceCoords::lineIndexOf(uint32_t offset) const {
  uint32_t iMin, iMax, iMid;

  if (lineStartOffsets_[lastLineIndex_] <= offset) {
    // If we reach here, offset is on a line the same as or higher than
    // last time.  Check first for the +0, +1, +2 cases, because they
    // typically cover 85--98% of cases.
    if (offset < lineStartOffsets_[lastLineIndex_ + 1]) {
      return lastLineIndex_;  // lineIndex is same as last time
    }

    // If we reach here, there must be at least one more entry (plus the
    // sentinel).  Try it.
    lastLineIndex_++;
    if (offset < lineStartOffsets_[lastLineIndex_ + 1]) {
      return lastLineIndex_;  // lineIndex is one higher than last time
    }

    // The same logic applies here.
    lastLineIndex_++;
    if (offset < lineStartOffsets_[lastLineIndex_ + 1]) {
      return lastLineIndex_;  // lineIndex is two higher than last time
    }

    // No luck.  Oh well, we have a better-than-default starting point for
    // the binary search.
    iMin = lastLineIndex_ + 1;
    MOZ_ASSERT(iMin <
               lineStartOffsets_.length() - 1);  // -1 due to the sentinel

  } else {
    iMin = 0;
  }

  // This is a binary search with deferred detection of equality, which was
  // marginally faster in this case than a standard binary search.
  // The -2 is because |lineStartOffsets_.length() - 1| is the sentinel, and we
  // want one before that.
  iMax = lineStartOffsets_.length() - 2;
  while (iMax > iMin) {
    iMid = iMin + (iMax - iMin) / 2;
    if (offset >= lineStartOffsets_[iMid + 1]) {
      iMin = iMid + 1;  // offset is above lineStartOffsets_[iMid]
    } else {
      iMax = iMid;  // offset is below or within lineStartOffsets_[iMid]
    }
  }
  MOZ_ASSERT(iMax == iMin);
  MOZ_ASSERT(lineStartOffsets_[iMin] <= offset &&
             offset < lineStartOffsets_[iMin + 1]);
  lastLineIndex_ = iMin;
  return iMin;
}

uint32_t TokenStreamAnyChars::SourceCoords::lineNum(uint32_t offset) const {
  uint32_t lineIndex = lineIndexOf(offset);
  return lineIndexToNum(lineIndex);
}

uint32_t TokenStreamAnyChars::SourceCoords::columnIndex(uint32_t offset) const {
  return lineIndexAndOffsetToColumn(lineIndexOf(offset), offset);
}

void TokenStreamAnyChars::SourceCoords::lineNumAndColumnIndex(
    uint32_t offset, uint32_t* lineNum, uint32_t* column) const {
  uint32_t lineIndex = lineIndexOf(offset);
  *lineNum = lineIndexToNum(lineIndex);
  *column = lineIndexAndOffsetToColumn(lineIndex, offset);
}

TokenStreamAnyChars::TokenStreamAnyChars(JSContext* cx,
                                         const ReadOnlyCompileOptions& options,
                                         StrictModeGetter* smg)
    : srcCoords(cx, options.lineno, options.column, options.scriptSourceOffset),
      options_(options),
      tokens(),
      cursor_(0),
      lookahead(),
      lineno(options.lineno),
      flags(),
      linebase(0),
      prevLinebase(size_t(-1)),
      filename_(options.filename()),
      displayURL_(nullptr),
      sourceMapURL_(nullptr),
      cx(cx),
      mutedErrors(options.mutedErrors()),
      strictModeGetter(smg) {
  // |isExprEnding| was initially zeroed: overwrite the true entries here.
  isExprEnding[size_t(TokenKind::Comma)] = true;
  isExprEnding[size_t(TokenKind::Semi)] = true;
  isExprEnding[size_t(TokenKind::Colon)] = true;
  isExprEnding[size_t(TokenKind::RightParen)] = true;
  isExprEnding[size_t(TokenKind::RightBracket)] = true;
  isExprEnding[size_t(TokenKind::RightCurly)] = true;
}

template <typename Unit>
TokenStreamCharsBase<Unit>::TokenStreamCharsBase(JSContext* cx,
                                                 const Unit* units,
                                                 size_t length,
                                                 size_t startOffset)
    : TokenStreamCharsShared(cx), sourceUnits(units, length, startOffset) {}

template <>
MOZ_MUST_USE bool TokenStreamCharsBase<char16_t>::
    fillCharBufferFromSourceNormalizingAsciiLineBreaks(const char16_t* cur,
                                                       const char16_t* end) {
  MOZ_ASSERT(this->charBuffer.length() == 0);

  while (cur < end) {
    char16_t ch = *cur++;
    if (ch == '\r') {
      ch = '\n';
      if (cur < end && *cur == '\n') {
        cur++;
      }
    }

    if (!this->charBuffer.append(ch)) {
      return false;
    }
  }

  MOZ_ASSERT(cur == end);
  return true;
}

template <>
MOZ_MUST_USE bool TokenStreamCharsBase<Utf8Unit>::
    fillCharBufferFromSourceNormalizingAsciiLineBreaks(const Utf8Unit* cur,
                                                       const Utf8Unit* end) {
  MOZ_ASSERT(this->charBuffer.length() == 0);

  while (cur < end) {
    Utf8Unit unit = *cur++;
    if (MOZ_LIKELY(IsAscii(unit))) {
      char16_t ch = unit.toUint8();
      if (ch == '\r') {
        ch = '\n';
        if (cur < end && *cur == Utf8Unit('\n')) {
          cur++;
        }
      }

      if (!this->charBuffer.append(ch)) {
        return false;
      }

      continue;
    }

    Maybe<char32_t> ch = DecodeOneUtf8CodePoint(unit, &cur, end);
    MOZ_ASSERT(ch.isSome(),
               "provided source text should already have been validated");

    if (!appendCodePointToCharBuffer(ch.value())) {
      return false;
    }
  }

  MOZ_ASSERT(cur == end);
  return true;
}

template <typename Unit, class AnyCharsAccess>
TokenStreamSpecific<Unit, AnyCharsAccess>::TokenStreamSpecific(
    JSContext* cx, const ReadOnlyCompileOptions& options, const Unit* units,
    size_t length)
    : TokenStreamChars<Unit, AnyCharsAccess>(cx, units, length,
                                             options.scriptSourceOffset) {}

bool TokenStreamAnyChars::checkOptions() {
  // Constrain starting columns to half of the range of a signed 32-bit value,
  // to avoid overflow.
  if (options().column >= mozilla::MaxValue<int32_t>::value / 2 + 1) {
    reportErrorNoOffset(JSMSG_BAD_COLUMN_NUMBER);
    return false;
  }

  return true;
}

// Use the fastest available getc.
#if defined(HAVE_GETC_UNLOCKED)
#define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
#define fast_getc _getc_nolock
#else
#define fast_getc getc
#endif

MOZ_MUST_USE MOZ_ALWAYS_INLINE bool
TokenStreamAnyChars::internalUpdateLineInfoForEOL(uint32_t lineStartOffset) {
  prevLinebase = linebase;
  linebase = lineStartOffset;
  lineno++;
  return srcCoords.add(lineno, linebase);
}

void TokenStreamAnyChars::undoInternalUpdateLineInfoForEOL() {
  MOZ_ASSERT(prevLinebase !=
             size_t(-1));  // we should never get more than one EOL
  linebase = prevLinebase;
  prevLinebase = size_t(-1);
  lineno--;
}

#ifdef DEBUG

template <>
inline void SourceUnits<char16_t>::assertNextCodePoint(
    const PeekedCodePoint<char16_t>& peeked) {
  char32_t c = peeked.codePoint();
  if (c < unicode::NonBMPMin) {
    MOZ_ASSERT(peeked.lengthInUnits() == 1);
    MOZ_ASSERT(ptr[0] == c);
  } else {
    MOZ_ASSERT(peeked.lengthInUnits() == 2);
    char16_t lead, trail;
    unicode::UTF16Encode(c, &lead, &trail);
    MOZ_ASSERT(ptr[0] == lead);
    MOZ_ASSERT(ptr[1] == trail);
  }
}

template <>
inline void SourceUnits<Utf8Unit>::assertNextCodePoint(
    const PeekedCodePoint<Utf8Unit>& peeked) {
  char32_t c = peeked.codePoint();

  // This is all roughly indulgence of paranoia only for assertions, so the
  // reimplementation of UTF-8 encoding a code point is (we think) a virtue.
  uint8_t expectedUnits[4] = {};
  if (c < 0x80) {
    expectedUnits[0] = AssertedCast<uint8_t>(c);
  } else if (c < 0x800) {
    expectedUnits[0] = 0b1100'0000 | (c >> 6);
    expectedUnits[1] = 0b1000'0000 | (c & 0b11'1111);
  } else if (c < 0x10000) {
    expectedUnits[0] = 0b1110'0000 | (c >> 12);
    expectedUnits[1] = 0b1000'0000 | ((c >> 6) & 0b11'1111);
    expectedUnits[2] = 0b1000'0000 | (c & 0b11'1111);
  } else {
    expectedUnits[0] = 0b1111'0000 | (c >> 18);
    expectedUnits[1] = 0b1000'0000 | ((c >> 12) & 0b11'1111);
    expectedUnits[2] = 0b1000'0000 | ((c >> 6) & 0b11'1111);
    expectedUnits[3] = 0b1000'0000 | (c & 0b11'1111);
  }

  MOZ_ASSERT(peeked.lengthInUnits() <= 4);
  for (uint8_t i = 0; i < peeked.lengthInUnits(); i++) {
    MOZ_ASSERT(expectedUnits[i] == ptr[i].toUint8());
  }
}

#endif  // DEBUG

template <class AnyCharsAccess>
MOZ_COLD void TokenStreamChars<Utf8Unit, AnyCharsAccess>::internalEncodingError(
    uint8_t relevantUnits, unsigned errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);

  do {
    size_t offset = this->sourceUnits.offset();

    ErrorMetadata err;

    TokenStreamAnyChars& anyChars = anyCharsAccess();

    bool hasLineOfContext = anyChars.fillExcludingContext(&err, offset);
    if (hasLineOfContext) {
      if (!internalComputeLineOfContext(&err, offset)) {
        break;
      }

      // As this is an encoding error, the computed window-end must be
      // identical to the location of the error -- any further on and the
      // window would contain invalid Unicode.
      MOZ_ASSERT_IF(err.lineOfContext != nullptr,
                    err.lineLength == err.tokenOffset);
    }

    auto notes = MakeUnique<JSErrorNotes>();
    if (!notes) {
      ReportOutOfMemory(anyChars.cx);
      break;
    }

    // The largest encoding of a UTF-8 code point is 4 units.  (Encoding an
    // obsolete 5- or 6-byte code point will complain only about a bad lead
    // code unit.)
    constexpr size_t MaxWidth = sizeof("0xHH 0xHH 0xHH 0xHH");

    MOZ_ASSERT(relevantUnits > 0);

    char badUnitsStr[MaxWidth];
    char* ptr = badUnitsStr;
    while (relevantUnits > 0) {
      byteToString(this->sourceUnits.getCodeUnit().toUint8(), ptr);
      ptr[4] = ' ';

      ptr += 5;
      relevantUnits--;
    }

    ptr[-1] = '\0';

    uint32_t line, column;
    anyChars.srcCoords.lineNumAndColumnIndex(offset, &line, &column);

    if (!notes->addNoteASCII(anyChars.cx, anyChars.getFilename(), line, column,
                             GetErrorMessage, nullptr, JSMSG_BAD_CODE_UNITS,
                             badUnitsStr)) {
      break;
    }

    ReportCompileError(anyChars.cx, std::move(err), std::move(notes),
                       JSREPORT_ERROR, errorNumber, args);
  } while (false);

  va_end(args);
}

template <class AnyCharsAccess>
MOZ_COLD void TokenStreamChars<Utf8Unit, AnyCharsAccess>::badLeadUnit(
    Utf8Unit lead) {
  uint8_t leadValue = lead.toUint8();

  char leadByteStr[5];
  byteToTerminatedString(leadValue, leadByteStr);

  internalEncodingError(1, JSMSG_BAD_LEADING_UTF8_UNIT, leadByteStr);
}

template <class AnyCharsAccess>
MOZ_COLD void TokenStreamChars<Utf8Unit, AnyCharsAccess>::notEnoughUnits(
    Utf8Unit lead, uint8_t remaining, uint8_t required) {
  uint8_t leadValue = lead.toUint8();

  MOZ_ASSERT(required == 2 || required == 3 || required == 4);
  MOZ_ASSERT(remaining < 4);
  MOZ_ASSERT(remaining < required);

  char leadByteStr[5];
  byteToTerminatedString(leadValue, leadByteStr);

  // |toHexChar| produces the desired decimal numbers for values < 4.
  const char expectedStr[] = {toHexChar(required - 1), '\0'};
  const char actualStr[] = {toHexChar(remaining - 1), '\0'};

  internalEncodingError(remaining, JSMSG_NOT_ENOUGH_CODE_UNITS, leadByteStr,
                        expectedStr, required == 2 ? "" : "s", actualStr,
                        remaining == 2 ? " was" : "s were");
}

template <class AnyCharsAccess>
MOZ_COLD void TokenStreamChars<Utf8Unit, AnyCharsAccess>::badTrailingUnit(
    uint8_t unitsObserved) {
  Utf8Unit badUnit =
      this->sourceUnits.addressOfNextCodeUnit()[unitsObserved - 1];

  char badByteStr[5];
  byteToTerminatedString(badUnit.toUint8(), badByteStr);

  internalEncodingError(unitsObserved, JSMSG_BAD_TRAILING_UTF8_UNIT,
                        badByteStr);
}

template <class AnyCharsAccess>
MOZ_COLD void
TokenStreamChars<Utf8Unit, AnyCharsAccess>::badStructurallyValidCodePoint(
    uint32_t codePoint, uint8_t codePointLength, const char* reason) {
  // Construct a string like "0x203D" (including null terminator) to include
  // in the error message.  Write the string end-to-start from end to start
  // of an adequately sized |char| array, shifting least significant nibbles
  // off the number and writing the corresponding hex digits until done, then
  // prefixing with "0x".  |codePointStr| points at the incrementally
  // computed string, within |codePointCharsArray|'s bounds.

  // 0x1F'FFFF is the maximum value that can fit in 3+6+6+6 unconstrained
  // bits in a four-byte UTF-8 code unit sequence.
  constexpr size_t MaxHexSize = sizeof(
      "0x1F"
      "FFFF");  // including '\0'
  char codePointCharsArray[MaxHexSize];

  char* codePointStr = codePointCharsArray + ArrayLength(codePointCharsArray);
  *--codePointStr = '\0';

  // Note that by do-while looping here rather than while-looping, this
  // writes a '0' when |codePoint == 0|.
  do {
    MOZ_ASSERT(codePointCharsArray < codePointStr);
    *--codePointStr = toHexChar(codePoint & 0xF);
    codePoint >>= 4;
  } while (codePoint);

  MOZ_ASSERT(codePointCharsArray + 2 <= codePointStr);
  *--codePointStr = 'x';
  *--codePointStr = '0';

  internalEncodingError(codePointLength, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
                        codePointStr, reason);
}

template <class AnyCharsAccess>
MOZ_MUST_USE bool
TokenStreamChars<Utf8Unit, AnyCharsAccess>::getNonAsciiCodePointDontNormalize(
    Utf8Unit lead, char32_t* codePoint) {
  auto onBadLeadUnit = [this, &lead]() { this->badLeadUnit(lead); };

  auto onNotEnoughUnits = [this, &lead](uint8_t remaining, uint8_t required) {
    this->notEnoughUnits(lead, remaining, required);
  };

  auto onBadTrailingUnit = [this](uint8_t unitsObserved) {
    this->badTrailingUnit(unitsObserved);
  };

  auto onBadCodePoint = [this](char32_t badCodePoint, uint8_t unitsObserved) {
    this->badCodePoint(badCodePoint, unitsObserved);
  };

  auto onNotShortestForm = [this](char32_t badCodePoint,
                                  uint8_t unitsObserved) {
    this->notShortestForm(badCodePoint, unitsObserved);
  };

  // If a valid code point is decoded, this function call consumes its code
  // units.  If not, it ungets the lead code unit and invokes the right error
  // handler, so on failure we must immediately return false.
  SourceUnitsIterator iter(this->sourceUnits);
  Maybe<char32_t> maybeCodePoint = DecodeOneUtf8CodePointInline(
      lead, &iter, SourceUnitsEnd(), onBadLeadUnit, onNotEnoughUnits,
      onBadTrailingUnit, onBadCodePoint, onNotShortestForm);
  if (maybeCodePoint.isNothing()) {
    return false;
  }

  *codePoint = maybeCodePoint.value();
  return true;
}

template <class AnyCharsAccess>
bool TokenStreamChars<char16_t, AnyCharsAccess>::getNonAsciiCodePoint(
    int32_t lead, int32_t* codePoint) {
  MOZ_ASSERT(lead != EOF);
  MOZ_ASSERT(!isAsciiCodePoint(lead),
             "ASCII code unit/point must be handled separately");
  MOZ_ASSERT(lead == this->sourceUnits.previousCodeUnit(),
             "getNonAsciiCodePoint called incorrectly");

  // The code point is usually |lead|: overwrite later if needed.
  *codePoint = lead;

  // ECMAScript specifically requires that unpaired UTF-16 surrogates be
  // treated as the corresponding code point and not as an error.  See
  // <https://tc39.github.io/ecma262/#sec-ecmascript-language-types-string-type>.
  // Thus this function does not consider any sequence of 16-bit numbers to
  // be intrinsically in error.

  // Dispense with single-unit code points and lone trailing surrogates.
  if (MOZ_LIKELY(!unicode::IsLeadSurrogate(lead))) {
    if (MOZ_UNLIKELY(lead == unicode::LINE_SEPARATOR ||
                     lead == unicode::PARA_SEPARATOR)) {
      if (!updateLineInfoForEOL()) {
#ifdef DEBUG
        *codePoint = EOF;  // sentinel value to hopefully cause errors
#endif
        MOZ_MAKE_MEM_UNDEFINED(codePoint, sizeof(*codePoint));
        return false;
      }

      *codePoint = '\n';
    } else {
      MOZ_ASSERT(!IsLineTerminator(AssertedCast<char32_t>(*codePoint)));
    }

    return true;
  }

  // Also handle a lead surrogate not paired with a trailing surrogate.
  if (MOZ_UNLIKELY(
          this->sourceUnits.atEnd() ||
          !unicode::IsTrailSurrogate(this->sourceUnits.peekCodeUnit()))) {
    MOZ_ASSERT(!IsLineTerminator(AssertedCast<char32_t>(*codePoint)));
    return true;
  }

  // Otherwise we have a multi-unit code point.
  *codePoint = unicode::UTF16Decode(lead, this->sourceUnits.getCodeUnit());
  MOZ_ASSERT(!IsLineTerminator(AssertedCast<char32_t>(*codePoint)));
  return true;
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::getCodePoint(int32_t* cp) {
  int32_t unit = getCodeUnit();
  if (unit == EOF) {
    MOZ_ASSERT(anyCharsAccess().flags.isEOF,
               "flags.isEOF should have been set by getCodeUnit()");
    *cp = EOF;
    return true;
  }

  if (isAsciiCodePoint(unit)) {
    return getFullAsciiCodePoint(unit, cp);
  }

  return getNonAsciiCodePoint(unit, cp);
}

template <class AnyCharsAccess>
bool TokenStreamChars<Utf8Unit, AnyCharsAccess>::getNonAsciiCodePoint(
    int32_t unit, int32_t* codePoint) {
  MOZ_ASSERT(unit != EOF);
  MOZ_ASSERT(!isAsciiCodePoint(unit),
             "ASCII code unit/point must be handled separately");

  Utf8Unit lead = Utf8Unit(static_cast<unsigned char>(unit));
  MOZ_ASSERT(lead == this->sourceUnits.previousCodeUnit(),
             "getNonAsciiCodePoint called incorrectly");

  auto onBadLeadUnit = [this, &lead]() { this->badLeadUnit(lead); };

  auto onNotEnoughUnits = [this, &lead](uint_fast8_t remaining,
                                        uint_fast8_t required) {
    this->notEnoughUnits(lead, remaining, required);
  };

  auto onBadTrailingUnit = [this](uint_fast8_t unitsObserved) {
    this->badTrailingUnit(unitsObserved);
  };

  auto onBadCodePoint = [this](char32_t badCodePoint,
                               uint_fast8_t unitsObserved) {
    this->badCodePoint(badCodePoint, unitsObserved);
  };

  auto onNotShortestForm = [this](char32_t badCodePoint,
                                  uint_fast8_t unitsObserved) {
    this->notShortestForm(badCodePoint, unitsObserved);
  };

  // This consumes the full, valid code point or ungets |lead| and calls the
  // appropriate error functor on failure.
  SourceUnitsIterator iter(this->sourceUnits);
  Maybe<char32_t> maybeCodePoint = DecodeOneUtf8CodePoint(
      lead, &iter, SourceUnitsEnd(), onBadLeadUnit, onNotEnoughUnits,
      onBadTrailingUnit, onBadCodePoint, onNotShortestForm);
  if (maybeCodePoint.isNothing()) {
    return false;
  }

  char32_t cp = maybeCodePoint.value();
  if (MOZ_UNLIKELY(cp == unicode::LINE_SEPARATOR ||
                   cp == unicode::PARA_SEPARATOR)) {
    if (!updateLineInfoForEOL()) {
#ifdef DEBUG
      *codePoint = EOF;  // sentinel value to hopefully cause errors
#endif
      MOZ_MAKE_MEM_UNDEFINED(codePoint, sizeof(*codePoint));
      return false;
    }

    *codePoint = '\n';
  } else {
    MOZ_ASSERT(!IsLineTerminator(cp));
    *codePoint = AssertedCast<int32_t>(cp);
  }

  return true;
}

template <>
size_t SourceUnits<char16_t>::findWindowStart(size_t offset) const {
  // This is JS's understanding of UTF-16 that allows lone surrogates, so
  // we have to exclude lone surrogates from [windowStart, offset) ourselves.

  const char16_t* const earliestPossibleStart = codeUnitPtrAt(startOffset_);

  const char16_t* const initial = codeUnitPtrAt(offset);
  const char16_t* p = initial;

  auto HalfWindowSize = [&p, &initial]() {
    return PointerRangeSize(p, initial);
  };

  while (true) {
    MOZ_ASSERT(earliestPossibleStart <= p);
    MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
    if (p <= earliestPossibleStart || HalfWindowSize() >= WindowRadius) {
      break;
    }

    char16_t c = p[-1];

    // This stops at U+2028 LINE SEPARATOR or U+2029 PARAGRAPH SEPARATOR in
    // string and template literals.  These code points do affect line and
    // column coordinates, even as they encode their literal values.
    if (IsLineTerminator(c)) {
      break;
    }

    // Don't allow invalid UTF-16 in pre-context.  (Current users don't
    // require this, and this behavior isn't currently imposed on
    // pre-context, but these facts might change someday.)

    if (MOZ_UNLIKELY(unicode::IsLeadSurrogate(c))) {
      break;
    }

    // Optimistically include the code unit, reverting below if needed.
    p--;

    // If it's not a surrogate at all, keep going.
    if (MOZ_LIKELY(!unicode::IsTrailSurrogate(c))) {
      continue;
    }

    // Stop if we don't have a usable surrogate pair.
    if (HalfWindowSize() >= WindowRadius ||
        p <= earliestPossibleStart ||      // trail surrogate at low end
        !unicode::IsLeadSurrogate(p[-1]))  // no paired lead surrogate
    {
      p++;
      break;
    }

    p--;
  }

  MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
  return offset - HalfWindowSize();
}

template <>
size_t SourceUnits<Utf8Unit>::findWindowStart(size_t offset) const {
  // |offset| must be the location of the error or somewhere before it, so we
  // know preceding data is valid UTF-8.

  const Utf8Unit* const earliestPossibleStart = codeUnitPtrAt(startOffset_);

  const Utf8Unit* const initial = codeUnitPtrAt(offset);
  const Utf8Unit* p = initial;

  auto HalfWindowSize = [&p, &initial]() {
    return PointerRangeSize(p, initial);
  };

  while (true) {
    MOZ_ASSERT(earliestPossibleStart <= p);
    MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
    if (p <= earliestPossibleStart || HalfWindowSize() >= WindowRadius) {
      break;
    }

    // Peek backward for a line break, and only decrement if there is none.
    uint8_t prev = p[-1].toUint8();

    // First check for the ASCII LineTerminators.
    if (prev == '\r' || prev == '\n') {
      break;
    }

    // Now check for the non-ASCII LineTerminators U+2028 LINE SEPARATOR
    // (0xE2 0x80 0xA8) and U+2029 PARAGRAPH (0xE2 0x80 0xA9).  If there
    // aren't three code units available, some comparison here will fail
    // before we'd underflow.
    if (MOZ_UNLIKELY((prev == 0xA8 || prev == 0xA9) &&
                     p[-2].toUint8() == 0x80 && p[-3].toUint8() == 0xE2)) {
      break;
    }

    // Rewind over the non-LineTerminator.  This can't underflow
    // |earliestPossibleStart| because it begins a code point.
    while (IsTrailingUnit(*--p)) {
      continue;
    }

    MOZ_ASSERT(earliestPossibleStart <= p);

    // But if we underflowed |WindowRadius|, adjust forward and stop.
    if (HalfWindowSize() > WindowRadius) {
      static_assert(WindowRadius > 3,
                    "skipping over non-lead code units below must not "
                    "advance past |offset|");

      while (IsTrailingUnit(*++p)) {
        continue;
      }

      MOZ_ASSERT(HalfWindowSize() < WindowRadius);
      break;
    }
  }

  MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
  return offset - HalfWindowSize();
}

template <>
size_t SourceUnits<char16_t>::findWindowEnd(size_t offset) const {
  const char16_t* const initial = codeUnitPtrAt(offset);
  const char16_t* p = initial;

  auto HalfWindowSize = [&initial, &p]() {
    return PointerRangeSize(initial, p);
  };

  while (true) {
    MOZ_ASSERT(p <= limit_);
    MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
    if (p >= limit_ || HalfWindowSize() >= WindowRadius) {
      break;
    }

    char16_t c = *p;

    // This stops at U+2028 LINE SEPARATOR or U+2029 PARAGRAPH SEPARATOR in
    // string and template literals.  These code points do affect line and
    // column coordinates, even as they encode their literal values.
    if (IsLineTerminator(c)) {
      break;
    }

    // Don't allow invalid UTF-16 in post-context.  (Current users don't
    // require this, and this behavior isn't currently imposed on
    // pre-context, but these facts might change someday.)

    if (MOZ_UNLIKELY(unicode::IsTrailSurrogate(c))) {
      break;
    }

    // Optimistically consume the code unit, ungetting it below if needed.
    p++;

    // If it's not a surrogate at all, keep going.
    if (MOZ_LIKELY(!unicode::IsLeadSurrogate(c))) {
      continue;
    }

    // Retract if the lead surrogate would stand alone at the end of the
    // window.
    if (HalfWindowSize() >= WindowRadius ||  // split pair
        p >= limit_ ||                       // half-pair at end of source
        !unicode::IsTrailSurrogate(*p))      // no paired trail surrogate
    {
      p--;
      break;
    }

    p++;
  }

  return offset + HalfWindowSize();
}

template <>
size_t SourceUnits<Utf8Unit>::findWindowEnd(size_t offset) const {
  const Utf8Unit* const initial = codeUnitPtrAt(offset);
  const Utf8Unit* p = initial;

  auto HalfWindowSize = [&initial, &p]() {
    return PointerRangeSize(initial, p);
  };

  while (true) {
    MOZ_ASSERT(p <= limit_);
    MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
    if (p >= limit_ || HalfWindowSize() >= WindowRadius) {
      break;
    }

    // A non-encoding error might be followed by an encoding error within
    // |maxEnd|, so we must validate as we go to not include invalid UTF-8
    // in the computed window.  What joy!

    Utf8Unit lead = *p;
    if (mozilla::IsAscii(lead)) {
      if (IsSingleUnitLineTerminator(lead)) {
        break;
      }

      p++;
      continue;
    }

    PeekedCodePoint<Utf8Unit> peeked = PeekCodePoint(p, limit_);
    if (peeked.isNone()) {
      break;  // encoding error
    }

    char32_t c = peeked.codePoint();
    if (MOZ_UNLIKELY(c == unicode::LINE_SEPARATOR ||
                     c == unicode::PARA_SEPARATOR)) {
      break;
    }

    MOZ_ASSERT(!IsLineTerminator(c));

    uint8_t len = peeked.lengthInUnits();
    if (HalfWindowSize() + len > WindowRadius) {
      break;
    }

    p += len;
  }

  MOZ_ASSERT(HalfWindowSize() <= WindowRadius);
  return offset + HalfWindowSize();
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::advance(size_t position) {
  const Unit* end = this->sourceUnits.codeUnitPtrAt(position);
  while (this->sourceUnits.addressOfNextCodeUnit() < end) {
    int32_t c;
    if (!getCodePoint(&c)) {
      return false;
    }
  }

  TokenStreamAnyChars& anyChars = anyCharsAccess();
  Token* cur = const_cast<Token*>(&anyChars.currentToken());
  cur->pos.begin = this->sourceUnits.offset();
  MOZ_MAKE_MEM_UNDEFINED(&cur->type, sizeof(cur->type));
  anyChars.lookahead = 0;
  return true;
}

template <typename Unit, class AnyCharsAccess>
void TokenStreamSpecific<Unit, AnyCharsAccess>::seek(const Position& pos) {
  TokenStreamAnyChars& anyChars = anyCharsAccess();

  this->sourceUnits.setAddressOfNextCodeUnit(pos.buf,
                                             /* allowPoisoned = */ true);
  anyChars.flags = pos.flags;
  anyChars.lineno = pos.lineno;
  anyChars.linebase = pos.linebase;
  anyChars.prevLinebase = pos.prevLinebase;
  anyChars.lookahead = pos.lookahead;

  anyChars.tokens[anyChars.cursor()] = pos.currentToken;
  for (unsigned i = 0; i < anyChars.lookahead; i++) {
    anyChars.tokens[anyChars.aheadCursor(1 + i)] = pos.lookaheadTokens[i];
  }
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::seek(
    const Position& pos, const TokenStreamAnyChars& other) {
  if (!anyCharsAccess().srcCoords.fill(other.srcCoords)) {
    return false;
  }

  seek(pos);
  return true;
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::reportStrictModeErrorNumberVA(
    UniquePtr<JSErrorNotes> notes, uint32_t offset, bool strictMode,
    unsigned errorNumber, va_list* args) {
  TokenStreamAnyChars& anyChars = anyCharsAccess();
  if (!strictMode && !anyChars.options().extraWarningsOption) {
    return true;
  }

  ErrorMetadata metadata;
  if (!computeErrorMetadata(&metadata, offset)) {
    return false;
  }

  if (strictMode) {
    ReportCompileError(anyChars.cx, std::move(metadata), std::move(notes),
                       JSREPORT_ERROR, errorNumber, *args);
    return false;
  }

  return anyChars.compileWarning(std::move(metadata), std::move(notes),
                                 JSREPORT_WARNING | JSREPORT_STRICT,
                                 errorNumber, *args);
}

bool TokenStreamAnyChars::compileWarning(ErrorMetadata&& metadata,
                                         UniquePtr<JSErrorNotes> notes,
                                         unsigned flags, unsigned errorNumber,
                                         va_list args) {
  if (options().werrorOption) {
    flags &= ~JSREPORT_WARNING;
    ReportCompileError(cx, std::move(metadata), std::move(notes), flags,
                       errorNumber, args);
    return false;
  }

  return ReportCompileWarning(cx, std::move(metadata), std::move(notes), flags,
                              errorNumber, args);
}

void TokenStreamAnyChars::computeErrorMetadataNoOffset(ErrorMetadata* err) {
  err->isMuted = mutedErrors;
  err->filename = filename_;
  err->lineNumber = 0;
  err->columnNumber = 0;

  MOZ_ASSERT(err->lineOfContext == nullptr);
}

bool TokenStreamAnyChars::fillExcludingContext(ErrorMetadata* err,
                                               uint32_t offset) {
  err->isMuted = mutedErrors;

  // If this TokenStreamAnyChars doesn't have location information, try to
  // get it from the caller.
  if (!filename_ && !cx->helperThread()) {
    NonBuiltinFrameIter iter(cx, FrameIter::FOLLOW_DEBUGGER_EVAL_PREV_LINK,
                             cx->realm()->principals());
    if (!iter.done() && iter.filename()) {
      err->filename = iter.filename();
      err->lineNumber = iter.computeLine(&err->columnNumber);
      return false;
    }
  }

  // Otherwise use this TokenStreamAnyChars's location information.
  err->filename = filename_;
  srcCoords.lineNumAndColumnIndex(offset, &err->lineNumber, &err->columnNumber);
  return true;
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::hasTokenizationStarted() const {
  const TokenStreamAnyChars& anyChars = anyCharsAccess();
  return anyChars.isCurrentTokenType(TokenKind::Eof) && !anyChars.isEOF();
}

void TokenStreamAnyChars::lineAndColumnAt(size_t offset, uint32_t* line,
                                          uint32_t* column) const {
  srcCoords.lineNumAndColumnIndex(offset, line, column);
}

template <typename Unit, class AnyCharsAccess>
void TokenStreamSpecific<Unit, AnyCharsAccess>::currentLineAndColumn(
    uint32_t* line, uint32_t* column) const {
  const TokenStreamAnyChars& anyChars = anyCharsAccess();
  uint32_t offset = anyChars.currentToken().pos.begin;
  anyChars.srcCoords.lineNumAndColumnIndex(offset, line, column);
}

template <>
inline void SourceUnits<char16_t>::computeWindowOffsetAndLength(
    const char16_t* encodedWindow, size_t encodedTokenOffset,
    size_t* utf16TokenOffset, size_t encodedWindowLength,
    size_t* utf16WindowLength) {
  MOZ_ASSERT_UNREACHABLE("shouldn't need to recompute for UTF-16");
}

template <>
inline void SourceUnits<Utf8Unit>::computeWindowOffsetAndLength(
    const Utf8Unit* encodedWindow, size_t encodedTokenOffset,
    size_t* utf16TokenOffset, size_t encodedWindowLength,
    size_t* utf16WindowLength) {
  MOZ_ASSERT(encodedTokenOffset <= encodedWindowLength,
             "token offset must be within the window, and the two lambda "
             "calls below presume this ordering of values");

  const Utf8Unit* const encodedWindowEnd = encodedWindow + encodedWindowLength;

  size_t i = 0;
  auto ComputeUtf16Count = [&i, &encodedWindow](const Utf8Unit* limit) {
    while (encodedWindow < limit) {
      Utf8Unit lead = *encodedWindow++;
      if (MOZ_LIKELY(IsAscii(lead))) {
        // ASCII contributes a single UTF-16 code unit.
        i++;
        continue;
      }

      Maybe<char32_t> cp = DecodeOneUtf8CodePoint(lead, &encodedWindow, limit);
      MOZ_ASSERT(cp.isSome(),
                 "computed window should only contain valid UTF-8");

      i += unicode::IsSupplementary(cp.value()) ? 2 : 1;
    }

    return i;
  };

  // Compute the token offset from |i == 0| and the initial |encodedWindow|.
  const Utf8Unit* token = encodedWindow + encodedTokenOffset;
  MOZ_ASSERT(token <= encodedWindowEnd);
  *utf16TokenOffset = ComputeUtf16Count(token);

  // Compute the window length, picking up from |i| and |encodedWindow| that,
  // in general, were modified just above.
  *utf16WindowLength = ComputeUtf16Count(encodedWindowEnd);
}

template <typename Unit>
bool TokenStreamCharsBase<Unit>::addLineOfContext(ErrorMetadata* err,
                                                  uint32_t offset) {
  // Rename the variable to make meaning clearer: an offset into source units
  // in Unit encoding.
  size_t encodedOffset = offset;

  // These are also offsets into source units in Unit encoding.
  size_t encodedWindowStart = sourceUnits.findWindowStart(encodedOffset);
  size_t encodedWindowEnd = sourceUnits.findWindowEnd(encodedOffset);

  size_t encodedWindowLength = encodedWindowEnd - encodedWindowStart;
  MOZ_ASSERT(encodedWindowLength <= SourceUnits::WindowRadius * 2);

  // Don't add a useless "line" of context when the window ends up empty
  // because of an invalid encoding at the start of a line.
  if (encodedWindowLength == 0) {
    MOZ_ASSERT(err->lineOfContext == nullptr,
               "ErrorMetadata::lineOfContext must be null so we don't "
               "have to set the lineLength/tokenOffset fields");
    return true;
  }

  // We might have hit an error while processing some source code feature
  // that's accumulating text into |this->charBuffer| -- e.g. we could be
  // halfway into a regular expression literal, then encounter invalid UTF-8.
  // Thus we must clear |this->charBuffer| of prior work.
  this->charBuffer.clear();

  const Unit* encodedWindow = sourceUnits.codeUnitPtrAt(encodedWindowStart);
  if (!fillCharBufferFromSourceNormalizingAsciiLineBreaks(
          encodedWindow, encodedWindow + encodedWindowLength)) {
    return false;
  }

  size_t utf16WindowLength = this->charBuffer.length();

  // The windowed string is null-terminated.
  if (!this->charBuffer.append('\0')) {
    return false;
  }

  err->lineOfContext.reset(this->charBuffer.extractOrCopyRawBuffer());
  if (!err->lineOfContext) {
    return false;
  }

  size_t encodedTokenOffset = encodedOffset - encodedWindowStart;

  MOZ_ASSERT(encodedTokenOffset <= encodedWindowLength,
             "token offset must be inside the window");

  // The length in UTF-8 code units of a code point is always greater than or
  // equal to the same code point's length in UTF-16 code points.  ASCII code
  // points are 1 unit in either encoding.  Code points in [U+0080, U+10000)
  // are 2-3 UTF-8 code units to 1 UTF-16 code unit.  And code points in
  // [U+10000, U+10FFFF] are 4 UTF-8 code units to 2 UTF-16 code units.
  //
  // Therefore, if encoded window length equals the length in UTF-16 (this is
  // always the case for Unit=char16_t), the UTF-16 offsets are exactly the
  // encoded offsets.  Otherwise we must convert offset/length from UTF-8 to
  // UTF-16.
  if (std::is_same<Unit, char16_t>::value) {
    MOZ_ASSERT(utf16WindowLength == encodedWindowLength,
               "UTF-16 to UTF-16 shouldn't change window length");
    err->tokenOffset = encodedTokenOffset;
    err->lineLength = encodedWindowLength;
  } else {
    MOZ_ASSERT((std::is_same<Unit, Utf8Unit>::value),
               "should only see UTF-8 here");

    bool simple = utf16WindowLength == encodedWindowLength;
    MOZ_ASSERT(std::all_of(encodedWindow, encodedWindow + encodedWindowLength,
                           IsAscii<Unit>) == simple,
               "equal window lengths in UTF-8 should correspond only to "
               "wholly-ASCII text");

    if (simple) {
      err->tokenOffset = encodedTokenOffset;
      err->lineLength = encodedWindowLength;
    } else {
      sourceUnits.computeWindowOffsetAndLength(
          encodedWindow, encodedTokenOffset, &err->tokenOffset,
          encodedWindowLength, &err->lineLength);
    }
  }

  return true;
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::computeErrorMetadata(
    ErrorMetadata* err, uint32_t offset) {
  if (offset == NoOffset) {
    anyCharsAccess().computeErrorMetadataNoOffset(err);
    return true;
  }

  // This function's return value isn't a success/failure indication: it
  // returns true if this TokenStream's location information could be used,
  // and it returns false when that information can't be used (and so we
  // can't provide a line of context).
  if (!anyCharsAccess().fillExcludingContext(err, offset)) {
    return true;
  }

  // Add a line of context from this TokenStream to help with debugging.
  return internalComputeLineOfContext(err, offset);
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::reportStrictModeError(
    unsigned errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);

  TokenStreamAnyChars& anyChars = anyCharsAccess();
  bool result =
      reportStrictModeErrorNumberVA(nullptr, anyChars.currentToken().pos.begin,
                                    anyChars.strictMode(), errorNumber, &args);

  va_end(args);
  return result;
}

template <typename Unit, class AnyCharsAccess>
void TokenStreamSpecific<Unit, AnyCharsAccess>::reportError(
    unsigned errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);

  TokenStreamAnyChars& anyChars = anyCharsAccess();
  ErrorMetadata metadata;
  if (computeErrorMetadata(&metadata, anyChars.currentToken().pos.begin)) {
    ReportCompileError(anyChars.cx, std::move(metadata), nullptr,
                       JSREPORT_ERROR, errorNumber, args);
  }

  va_end(args);
}

void TokenStreamAnyChars::reportErrorNoOffset(unsigned errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);

  reportErrorNoOffsetVA(errorNumber, args);

  va_end(args);
}

void TokenStreamAnyChars::reportErrorNoOffsetVA(unsigned errorNumber,
                                                va_list args) {
  ErrorMetadata metadata;
  computeErrorMetadataNoOffset(&metadata);

  ReportCompileError(cx, std::move(metadata), nullptr, JSREPORT_ERROR,
                     errorNumber, args);
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::warning(unsigned errorNumber,
                                                        ...) {
  va_list args;
  va_start(args, errorNumber);

  ErrorMetadata metadata;
  bool result =
      computeErrorMetadata(&metadata,
                           anyCharsAccess().currentToken().pos.begin) &&
      anyCharsAccess().compileWarning(std::move(metadata), nullptr,
                                      JSREPORT_WARNING, errorNumber, args);

  va_end(args);
  return result;
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::reportExtraWarningErrorNumberVA(
    UniquePtr<JSErrorNotes> notes, uint32_t offset, unsigned errorNumber,
    va_list* args) {
  TokenStreamAnyChars& anyChars = anyCharsAccess();
  if (!anyChars.options().extraWarningsOption) {
    return true;
  }

  ErrorMetadata metadata;
  if (!computeErrorMetadata(&metadata, offset)) {
    return false;
  }

  return anyChars.compileWarning(std::move(metadata), std::move(notes),
                                 JSREPORT_STRICT | JSREPORT_WARNING,
                                 errorNumber, *args);
}

template <typename Unit, class AnyCharsAccess>
void TokenStreamSpecific<Unit, AnyCharsAccess>::error(unsigned errorNumber,
                                                      ...) {
  va_list args;
  va_start(args, errorNumber);

  ErrorMetadata metadata;
  if (computeErrorMetadata(&metadata, this->sourceUnits.offset())) {
    TokenStreamAnyChars& anyChars = anyCharsAccess();
    ReportCompileError(anyChars.cx, std::move(metadata), nullptr,
                       JSREPORT_ERROR, errorNumber, args);
  }

  va_end(args);
}

template <typename Unit, class AnyCharsAccess>
void TokenStreamSpecific<Unit, AnyCharsAccess>::errorAtVA(uint32_t offset,
                                                          unsigned errorNumber,
                                                          va_list* args) {
  ErrorMetadata metadata;
  if (computeErrorMetadata(&metadata, offset)) {
    TokenStreamAnyChars& anyChars = anyCharsAccess();
    ReportCompileError(anyChars.cx, std::move(metadata), nullptr,
                       JSREPORT_ERROR, errorNumber, *args);
  }
}

template <typename Unit, class AnyCharsAccess>
void TokenStreamSpecific<Unit, AnyCharsAccess>::errorAt(uint32_t offset,
                                                        unsigned errorNumber,
                                                        ...) {
  va_list args;
  va_start(args, errorNumber);

  errorAtVA(offset, errorNumber, &args);

  va_end(args);
}

// We have encountered a '\': check for a Unicode escape sequence after it.
// Return the length of the escape sequence and the encoded code point (by
// value) if we found a Unicode escape sequence, and skip all code units
// involed.  Otherwise, return 0 and don't advance along the buffer.
template <typename Unit, class AnyCharsAccess>
uint32_t GeneralTokenStreamChars<Unit, AnyCharsAccess>::matchUnicodeEscape(
    uint32_t* codePoint) {
  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('\\'));

  int32_t unit = getCodeUnit();
  if (unit != 'u') {
    // NOTE: |unit| may be EOF here.
    ungetCodeUnit(unit);
    MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('\\'));
    return 0;
  }

  char16_t v;
  unit = getCodeUnit();
  if (JS7_ISHEX(unit) && this->sourceUnits.matchHexDigits(3, &v)) {
    *codePoint = (JS7_UNHEX(unit) << 12) | v;
    return 5;
  }

  if (unit == '{') {
    return matchExtendedUnicodeEscape(codePoint);
  }

  // NOTE: |unit| may be EOF here, so this ungets either one or two units.
  ungetCodeUnit(unit);
  ungetCodeUnit('u');
  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('\\'));
  return 0;
}

template <typename Unit, class AnyCharsAccess>
uint32_t
GeneralTokenStreamChars<Unit, AnyCharsAccess>::matchExtendedUnicodeEscape(
    uint32_t* codePoint) {
  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('{'));

  int32_t unit = getCodeUnit();

  // Skip leading zeroes.
  uint32_t leadingZeroes = 0;
  while (unit == '0') {
    leadingZeroes++;
    unit = getCodeUnit();
  }

  size_t i = 0;
  uint32_t code = 0;
  while (JS7_ISHEX(unit) && i < 6) {
    code = (code << 4) | JS7_UNHEX(unit);
    unit = getCodeUnit();
    i++;
  }

  uint32_t gotten =
      2 +                  // 'u{'
      leadingZeroes + i +  // significant hexdigits
      (unit != EOF);       // subtract a get if it didn't contribute to length

  if (unit == '}' && (leadingZeroes > 0 || i > 0) &&
      code <= unicode::NonBMPMax) {
    *codePoint = code;
    return gotten;
  }

  this->sourceUnits.unskipCodeUnits(gotten);
  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('\\'));
  return 0;
}

template <typename Unit, class AnyCharsAccess>
uint32_t
GeneralTokenStreamChars<Unit, AnyCharsAccess>::matchUnicodeEscapeIdStart(
    uint32_t* codePoint) {
  uint32_t length = matchUnicodeEscape(codePoint);
  if (MOZ_LIKELY(length > 0)) {
    if (MOZ_LIKELY(unicode::IsIdentifierStart(*codePoint))) {
      return length;
    }

    this->sourceUnits.unskipCodeUnits(length);
  }

  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('\\'));
  return 0;
}

template <typename Unit, class AnyCharsAccess>
bool GeneralTokenStreamChars<Unit, AnyCharsAccess>::matchUnicodeEscapeIdent(
    uint32_t* codePoint) {
  uint32_t length = matchUnicodeEscape(codePoint);
  if (MOZ_LIKELY(length > 0)) {
    if (MOZ_LIKELY(unicode::IsIdentifierPart(*codePoint))) {
      return true;
    }

    this->sourceUnits.unskipCodeUnits(length);
  }

  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('\\'));
  return false;
}

template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool
TokenStreamSpecific<Unit, AnyCharsAccess>::matchIdentifierStart(
    IdentifierEscapes* sawEscape) {
  int32_t unit = getCodeUnit();
  if (unicode::IsIdentifierStart(char16_t(unit))) {
    *sawEscape = IdentifierEscapes::None;
    return true;
  }

  if (unit == '\\') {
    *sawEscape = IdentifierEscapes::SawUnicodeEscape;

    uint32_t codePoint;
    uint32_t escapeLength = matchUnicodeEscapeIdStart(&codePoint);
    if (escapeLength != 0) {
      return true;
    }

    // We could point "into" a mistyped escape, e.g. for "\u{41H}" we
    // could point at the 'H'.  But we don't do that now, so the code
    // unit after the '\' isn't necessarily bad, so just point at the
    // start of the actually-invalid escape.
    ungetCodeUnit('\\');
    error(JSMSG_BAD_ESCAPE);
    return false;
  }

  *sawEscape = IdentifierEscapes::None;

  // NOTE: |unit| may be EOF here.
  ungetCodeUnit(unit);
  error(JSMSG_MISSING_PRIVATE_NAME);
  return false;
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::getDirectives(
    bool isMultiline, bool shouldWarnDeprecated) {
  // Match directive comments used in debugging, such as "//# sourceURL" and
  // "//# sourceMappingURL". Use of "//@" instead of "//#" is deprecated.
  //
  // To avoid a crashing bug in IE, several JavaScript transpilers wrap single
  // line comments containing a source mapping URL inside a multiline
  // comment. To avoid potentially expensive lookahead and backtracking, we
  // only check for this case if we encounter a '#' code unit.

  bool res = getDisplayURL(isMultiline, shouldWarnDeprecated) &&
             getSourceMappingURL(isMultiline, shouldWarnDeprecated);
  if (!res) {
    badToken();
  }

  return res;
}

MOZ_MUST_USE bool TokenStreamCharsShared::copyCharBufferTo(
    JSContext* cx, UniquePtr<char16_t[], JS::FreePolicy>* destination) {
  size_t length = charBuffer.length();

  *destination = cx->make_pod_array<char16_t>(length + 1);
  if (!*destination) {
    return false;
  }

  std::copy(charBuffer.begin(), charBuffer.end(), destination->get());
  (*destination)[length] = '\0';
  return true;
}

template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool TokenStreamSpecific<Unit, AnyCharsAccess>::getDirective(
    bool isMultiline, bool shouldWarnDeprecated, const char* directive,
    uint8_t directiveLength, const char* errorMsgPragma,
    UniquePtr<char16_t[], JS::FreePolicy>* destination) {
  // Stop if we don't find |directive|.  (Note that |directive| must be
  // ASCII, so there are no tricky encoding issues to consider in matching
  // UTF-8/16-agnostically.)
  if (!this->sourceUnits.matchCodeUnits(directive, directiveLength)) {
    return true;
  }

  if (shouldWarnDeprecated) {
    if (!warning(JSMSG_DEPRECATED_PRAGMA, errorMsgPragma)) {
      return false;
    }
  }

  this->charBuffer.clear();

  do {
    int32_t unit = peekCodeUnit();
    if (unit == EOF) {
      break;
    }

    if (MOZ_LIKELY(isAsciiCodePoint(unit))) {
      if (unicode::IsSpaceOrBOM2(unit)) {
        break;
      }

      consumeKnownCodeUnit(unit);

      // Debugging directives can occur in both single- and multi-line
      // comments. If we're currently inside a multi-line comment, we
      // also must recognize multi-line comment terminators.
      if (isMultiline && unit == '*' && peekCodeUnit() == '/') {
        ungetCodeUnit('*');
        break;
      }

      if (!this->charBuffer.append(unit)) {
        return false;
      }

      continue;
    }

    // This ignores encoding errors: subsequent caller-side code to
    // handle the remaining source text in the comment will do so.
    PeekedCodePoint<Unit> peeked = this->sourceUnits.peekCodePoint();
    if (peeked.isNone() || unicode::IsSpaceOrBOM2(peeked.codePoint())) {
      break;
    }

    MOZ_ASSERT(!IsLineTerminator(peeked.codePoint()),
               "!IsSpaceOrBOM2 must imply !IsLineTerminator or else we'll "
               "fail to maintain line-info/flags for EOL");
    this->sourceUnits.consumeKnownCodePoint(peeked);

    if (!appendCodePointToCharBuffer(peeked.codePoint())) {
      return false;
    }
  } while (true);

  if (this->charBuffer.empty()) {
    // The directive's URL was missing, but comments can contain anything,
    // so it isn't an error.
    return true;
  }

  return copyCharBufferTo(anyCharsAccess().cx, destination);
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::getDisplayURL(
    bool isMultiline, bool shouldWarnDeprecated) {
  // Match comments of the form "//# sourceURL=<url>" or
  // "/\* //# sourceURL=<url> *\/"
  //
  // Note that while these are labeled "sourceURL" in the source text,
  // internally we refer to it as a "displayURL" to distinguish what the
  // developer would like to refer to the source as from the source's actual
  // URL.

  static const char sourceURLDirective[] = " sourceURL=";
  constexpr uint8_t sourceURLDirectiveLength =
      ArrayLength(sourceURLDirective) - 1;
  return getDirective(isMultiline, shouldWarnDeprecated, sourceURLDirective,
                      sourceURLDirectiveLength, "sourceURL",
                      &anyCharsAccess().displayURL_);
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::getSourceMappingURL(
    bool isMultiline, bool shouldWarnDeprecated) {
  // Match comments of the form "//# sourceMappingURL=<url>" or
  // "/\* //# sourceMappingURL=<url> *\/"

  static const char sourceMappingURLDirective[] = " sourceMappingURL=";
  constexpr uint8_t sourceMappingURLDirectiveLength =
      ArrayLength(sourceMappingURLDirective) - 1;
  return getDirective(isMultiline, shouldWarnDeprecated,
                      sourceMappingURLDirective,
                      sourceMappingURLDirectiveLength, "sourceMappingURL",
                      &anyCharsAccess().sourceMapURL_);
}

template <typename Unit, class AnyCharsAccess>
MOZ_ALWAYS_INLINE Token*
GeneralTokenStreamChars<Unit, AnyCharsAccess>::newTokenInternal(
    TokenKind kind, TokenStart start, TokenKind* out) {
  MOZ_ASSERT(kind < TokenKind::Limit);
  MOZ_ASSERT(kind != TokenKind::Eol,
             "TokenKind::Eol should never be used in an actual Token, only "
             "returned by peekTokenSameLine()");

  TokenStreamAnyChars& anyChars = anyCharsAccess();
  anyChars.flags.isDirtyLine = true;

  Token* token = anyChars.allocateToken();

  *out = token->type = kind;
  token->pos = TokenPos(start.offset(), this->sourceUnits.offset());
  MOZ_ASSERT(token->pos.begin <= token->pos.end);

  // NOTE: |token->modifier| and |token->modifierException| are set in
  //       |newToken()| so that optimized, non-debug code won't do any work
  //       to pass a modifier-argument that will never be used.

  return token;
}

template <typename Unit, class AnyCharsAccess>
MOZ_COLD bool GeneralTokenStreamChars<Unit, AnyCharsAccess>::badToken() {
  // We didn't get a token, so don't set |flags.isDirtyLine|.
  anyCharsAccess().flags.hadError = true;

  // Poisoning sourceUnits on error establishes an invariant: once an
  // erroneous token has been seen, sourceUnits will not be consulted again.
  // This is true because the parser will deal with the illegal token by
  // aborting parsing immediately.
  this->sourceUnits.poisonInDebug();

  return false;
};

MOZ_MUST_USE bool TokenStreamCharsShared::appendCodePointToCharBuffer(
    uint32_t codePoint) {
  char16_t units[2];
  unsigned numUnits = 0;
  unicode::UTF16Encode(codePoint, units, &numUnits);

  MOZ_ASSERT(numUnits == 1 || numUnits == 2,
             "UTF-16 code points are only encoded in one or two units");

  if (!charBuffer.append(units[0])) {
    return false;
  }

  if (numUnits == 1) {
    return true;
  }

  return charBuffer.append(units[1]);
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::putIdentInCharBuffer(
    const Unit* identStart) {
  const Unit* const originalAddress = this->sourceUnits.addressOfNextCodeUnit();
  this->sourceUnits.setAddressOfNextCodeUnit(identStart);

  auto restoreNextRawCharAddress = MakeScopeExit([this, originalAddress]() {
    this->sourceUnits.setAddressOfNextCodeUnit(originalAddress);
  });

  this->charBuffer.clear();
  do {
    int32_t unit = getCodeUnit();
    if (unit == EOF) {
      break;
    }

    uint32_t codePoint;
    if (MOZ_LIKELY(isAsciiCodePoint(unit))) {
      if (unicode::IsIdentifierPart(char16_t(unit))) {
        if (!this->charBuffer.append(unit)) {
          return false;
        }

        continue;
      }

      if (unit != '\\' || !matchUnicodeEscapeIdent(&codePoint)) {
        break;
      }
    } else {
      // |restoreNextRawCharAddress| undoes all gets, and this function
      // doesn't update line/column info.
      char32_t cp;
      if (!getNonAsciiCodePointDontNormalize(toUnit(unit), &cp)) {
        return false;
      }

      codePoint = cp;
      if (!unicode::IsIdentifierPart(codePoint)) {
        break;
      }
    }

    if (!appendCodePointToCharBuffer(codePoint)) {
      return false;
    }
  } while (true);

  return true;
}

template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool TokenStreamSpecific<Unit, AnyCharsAccess>::identifierName(
    TokenStart start, const Unit* identStart, IdentifierEscapes escaping,
    Modifier modifier, NameVisibility visibility, TokenKind* out) {
  // Run the bad-token code for every path out of this function except the
  // two success-cases.
  auto noteBadToken = MakeScopeExit([this]() { this->badToken(); });

  // We've already consumed an initial code point in the identifer, to *know*
  // that this is an identifier.  So no need to worry about not consuming any
  // code points in the loop below.
  int32_t unit;
  while (true) {
    unit = peekCodeUnit();
    if (unit == EOF) {
      break;
    }

    if (MOZ_LIKELY(isAsciiCodePoint(unit))) {
      consumeKnownCodeUnit(unit);

      if (MOZ_UNLIKELY(
              !unicode::IsIdentifierPart(static_cast<char16_t>(unit)))) {
        // Handle a Unicode escape -- otherwise it's not part of the
        // identifier.
        uint32_t codePoint;
        if (unit != '\\' || !matchUnicodeEscapeIdent(&codePoint)) {
          ungetCodeUnit(unit);
          break;
        }

        escaping = IdentifierEscapes::SawUnicodeEscape;
      }
    } else {
      // This ignores encoding errors: subsequent caller-side code to
      // handle source text after the IdentifierName will do so.
      PeekedCodePoint<Unit> peeked = this->sourceUnits.peekCodePoint();
      if (peeked.isNone() || !unicode::IsIdentifierPart(peeked.codePoint())) {
        break;
      }

      MOZ_ASSERT(!IsLineTerminator(peeked.codePoint()),
                 "IdentifierPart must guarantee !IsLineTerminator or "
                 "else we'll fail to maintain line-info/flags for EOL");

      this->sourceUnits.consumeKnownCodePoint(peeked);
    }
  }

  JSAtom* atom;
  if (MOZ_UNLIKELY(escaping == IdentifierEscapes::SawUnicodeEscape)) {
    // Identifiers containing Unicode escapes have to be converted into
    // tokenbuf before atomizing.
    if (!putIdentInCharBuffer(identStart)) {
      return false;
    }

    atom = drainCharBufferIntoAtom(anyCharsAccess().cx);
  } else {
    // Escape-free identifiers can be created directly from sourceUnits.
    const Unit* chars = identStart;
    size_t length = this->sourceUnits.addressOfNextCodeUnit() - identStart;

    // Private identifiers start with a '#', and so cannot be reserved words.
    if (visibility == NameVisibility::Public) {
      // Represent reserved words lacking escapes as reserved word tokens.
      if (const ReservedWordInfo* rw = FindReservedWord(chars, length)) {
        noteBadToken.release();
        newSimpleToken(rw->tokentype, start, modifier, out);
        return true;
      }
    }

    atom = atomizeSourceChars(anyCharsAccess().cx, MakeSpan(chars, length));
  }
  if (!atom) {
    return false;
  }

  noteBadToken.release();
  if (visibility == NameVisibility::Private) {
    MOZ_ASSERT(identStart[0] == static_cast<Unit>('#'),
               "Private identifier starts with #");
    newPrivateNameToken(atom->asPropertyName(), start, modifier, out);

    // TODO(khypera): Delete the below once private names are supported.
    errorAt(start.offset(), JSMSG_FIELDS_NOT_SUPPORTED);
    return false;
  } else {
    newNameToken(atom->asPropertyName(), start, modifier, out);
  }
  return true;
}

enum FirstCharKind {
  // A char16_t has the 'OneChar' kind if it, by itself, constitutes a valid
  // token that cannot also be a prefix of a longer token.  E.g. ';' has the
  // OneChar kind, but '+' does not, because '++' and '+=' are valid longer
  // tokens
  // that begin with '+'.
  //
  // The few token kinds satisfying these properties cover roughly 35--45%
  // of the tokens seen in practice.
  //
  // We represent the 'OneChar' kind with any positive value less than
  // TokenKind::Limit.  This representation lets us associate
  // each one-char token char16_t with a TokenKind and thus avoid
  // a subsequent char16_t-to-TokenKind conversion.
  OneChar_Min = 0,
  OneChar_Max = size_t(TokenKind::Limit) - 1,

  Space = size_t(TokenKind::Limit),
  Ident,
  Dec,
  String,
  EOL,
  ZeroDigit,
  Other,

  LastCharKind = Other
};

// OneChar: 40,  41,  44,  58,  59,  63,  91,  93,  123, 125, 126:
//          '(', ')', ',', ':', ';', '?', '[', ']', '{', '}', '~'
// Ident:   36, 65..90, 95, 97..122: '$', 'A'..'Z', '_', 'a'..'z'
// Dot:     46: '.'
// Equals:  61: '='
// String:  34, 39, 96: '"', '\'', '`'
// Dec:     49..57: '1'..'9'
// Plus:    43: '+'
// ZeroDigit:  48: '0'
// Space:   9, 11, 12, 32: '\t', '\v', '\f', ' '
// EOL:     10, 13: '\n', '\r'
//
#define T_COMMA size_t(TokenKind::Comma)
#define T_COLON size_t(TokenKind::Colon)
#define T_BITNOT size_t(TokenKind::BitNot)
#define T_LP size_t(TokenKind::LeftParen)
#define T_RP size_t(TokenKind::RightParen)
#define T_SEMI size_t(TokenKind::Semi)
#define T_HOOK size_t(TokenKind::Hook)
#define T_LB size_t(TokenKind::LeftBracket)
#define T_RB size_t(TokenKind::RightBracket)
#define T_LC size_t(TokenKind::LeftCurly)
#define T_RC size_t(TokenKind::RightCurly)
#define _______ Other
static const uint8_t firstCharKinds[] = {
    // clang-format off
/*         0        1        2        3        4        5        6        7        8        9    */
/*   0+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______,   Space,
/*  10+ */     EOL,   Space,   Space,     EOL, _______, _______, _______, _______, _______, _______,
/*  20+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  30+ */ _______, _______,   Space, _______,  String, _______,   Ident, _______, _______,  String,
/*  40+ */    T_LP,    T_RP, _______, _______, T_COMMA, _______, _______, _______,ZeroDigit,    Dec,
/*  50+ */     Dec,     Dec,     Dec,     Dec,     Dec,     Dec,     Dec,     Dec, T_COLON,  T_SEMI,
/*  60+ */ _______, _______, _______,  T_HOOK, _______,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  70+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  80+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  90+ */   Ident,    T_LB, _______,    T_RB, _______,   Ident,  String,   Ident,   Ident,   Ident,
/* 100+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/* 110+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/* 120+ */   Ident,   Ident,   Ident,    T_LC, _______,    T_RC,T_BITNOT, _______
    // clang-format on
};
#undef T_COMMA
#undef T_COLON
#undef T_BITNOT
#undef T_LP
#undef T_RP
#undef T_SEMI
#undef T_HOOK
#undef T_LB
#undef T_RB
#undef T_LC
#undef T_RC
#undef _______

static_assert(LastCharKind < (1 << (sizeof(firstCharKinds[0]) * 8)),
              "Elements of firstCharKinds[] are too small");

template <>
void SourceUnits<char16_t>::consumeRestOfSingleLineComment() {
  while (MOZ_LIKELY(!atEnd())) {
    char16_t unit = peekCodeUnit();
    if (IsLineTerminator(unit)) {
      return;
    }

    consumeKnownCodeUnit(unit);
  }
}

template <>
void SourceUnits<Utf8Unit>::consumeRestOfSingleLineComment() {
  while (MOZ_LIKELY(!atEnd())) {
    const Utf8Unit unit = peekCodeUnit();
    if (IsSingleUnitLineTerminator(unit)) {
      return;
    }

    if (MOZ_LIKELY(IsAscii(unit))) {
      consumeKnownCodeUnit(unit);
      continue;
    }

    PeekedCodePoint<Utf8Unit> peeked = peekCodePoint();
    if (peeked.isNone()) {
      return;
    }

    char32_t c = peeked.codePoint();
    if (MOZ_UNLIKELY(c == unicode::LINE_SEPARATOR ||
                     c == unicode::PARA_SEPARATOR)) {
      return;
    }

    consumeKnownCodePoint(peeked);
  }
}

template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool TokenStreamSpecific<Unit, AnyCharsAccess>::decimalNumber(
    int32_t unit, TokenStart start, const Unit* numStart, Modifier modifier,
    TokenKind* out) {
  // Run the bad-token code for every path out of this function except the
  // one success-case.
  auto noteBadToken = MakeScopeExit([this]() { this->badToken(); });

  // Consume integral component digits.
  while (IsAsciiDigit(unit)) {
    unit = getCodeUnit();
  }

  // Numbers contain no escapes, so we can read directly from |sourceUnits|.
  double dval;
#ifdef ENABLE_BIGINT
  bool isBigInt = false;
#endif
  DecimalPoint decimalPoint = NoDecimal;
  if (unit != '.' && unit != 'e' && unit != 'E' &&
      IF_BIGINT(unit != 'n', true)) {
    // NOTE: |unit| may be EOF here.
    ungetCodeUnit(unit);

    // Most numbers are pure decimal integers without fractional component
    // or exponential notation.  Handle that with optimized code.
    if (!GetDecimalInteger(anyCharsAccess().cx, numStart,
                           this->sourceUnits.addressOfNextCodeUnit(), &dval)) {
      return false;
    }
  }
#ifdef ENABLE_BIGINT
  else if (unit == 'n') {
    isBigInt = true;
    unit = peekCodeUnit();
  }
#endif
  else {
    // Consume any decimal dot and fractional component.
    if (unit == '.') {
      decimalPoint = HasDecimal;
      do {
        unit = getCodeUnit();
      } while (IsAsciiDigit(unit));
    }

    // Consume any exponential notation.
    if (unit == 'e' || unit == 'E') {
      unit = getCodeUnit();
      if (unit == '+' || unit == '-') {
        unit = getCodeUnit();
      }

      // Exponential notation must contain at least one digit.
      if (!IsAsciiDigit(unit)) {
        ungetCodeUnit(unit);
        error(JSMSG_MISSING_EXPONENT);
        return false;
      }

      // Consume exponential digits.
      do {
        unit = getCodeUnit();
      } while (IsAsciiDigit(unit));
    }

    ungetCodeUnit(unit);

    // "0." and "0e..." numbers parse "." or "e..." here.  Neither range
    // contains a number, so we can't use |FullStringToDouble|.  (Parse
    // failures return 0.0, so we'll still get the right result.)
    if (!StringToDouble(anyCharsAccess().cx, numStart,
                        this->sourceUnits.addressOfNextCodeUnit(), &dval)) {
      return false;
    }
  }

  // Number followed by IdentifierStart is an error.  (This is the only place
  // in ECMAScript where token boundary is inadequate to properly separate
  // two tokens, necessitating this unaesthetic lookahead.)
  if (unit != EOF) {
    if (MOZ_LIKELY(isAsciiCodePoint(unit))) {
      if (unicode::IsIdentifierStart(char16_t(unit))) {
        error(JSMSG_IDSTART_AFTER_NUMBER);
        return false;
      }
    } else {
      // This ignores encoding errors: subsequent caller-side code to
      // handle source text after the number will do so.
      PeekedCodePoint<Unit> peeked = this->sourceUnits.peekCodePoint();
      if (!peeked.isNone() && unicode::IsIdentifierStart(peeked.codePoint())) {
        error(JSMSG_IDSTART_AFTER_NUMBER);
        return false;
      }
    }
  }

  noteBadToken.release();

#ifdef ENABLE_BIGINT
  if (isBigInt) {
    return bigIntLiteral(start, modifier, out);
  }
#endif

  newNumberToken(dval, decimalPoint, start, modifier, out);
  return true;
}

template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool TokenStreamSpecific<Unit, AnyCharsAccess>::regexpLiteral(
    TokenStart start, TokenKind* out) {
  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == Unit('/'));
  this->charBuffer.clear();

  auto ProcessNonAsciiCodePoint = [this](int32_t lead) {
    MOZ_ASSERT(lead != EOF);
    MOZ_ASSERT(!this->isAsciiCodePoint(lead));

    char32_t codePoint;
    if (!this->getNonAsciiCodePointDontNormalize(this->toUnit(lead),
                                                 &codePoint)) {
      return false;
    }

    if (MOZ_UNLIKELY(codePoint == unicode::LINE_SEPARATOR ||
                     codePoint == unicode::PARA_SEPARATOR)) {
      this->sourceUnits.ungetLineOrParagraphSeparator();
      this->reportError(JSMSG_UNTERMINATED_REGEXP);
      return false;
    }

    return this->appendCodePointToCharBuffer(codePoint);
  };

  auto ReportUnterminatedRegExp = [this](int32_t unit) {
    this->ungetCodeUnit(unit);
    this->error(JSMSG_UNTERMINATED_REGEXP);
  };

  bool inCharClass = false;
  do {
    int32_t unit = getCodeUnit();
    if (unit == EOF) {
      ReportUnterminatedRegExp(unit);
      return badToken();
    }

    if (MOZ_UNLIKELY(!isAsciiCodePoint(unit))) {
      if (!ProcessNonAsciiCodePoint(unit)) {
        return badToken();
      }

      continue;
    }

    if (unit == '\\') {
      if (!this->charBuffer.append(unit)) {
        return badToken();
      }

      unit = getCodeUnit();
      if (unit == EOF) {
        ReportUnterminatedRegExp(unit);
        return badToken();
      }

      // Fallthrough only handles ASCII code points, so
      // deal with non-ASCII and skip everything else.
      if (MOZ_UNLIKELY(!isAsciiCodePoint(unit))) {
        if (!ProcessNonAsciiCodePoint(unit)) {
          return badToken();
        }

        continue;
      }
    } else if (unit == '[') {
      inCharClass = true;
    } else if (unit == ']') {
      inCharClass = false;
    } else if (unit == '/' && !inCharClass) {
      // For IE compat, allow unescaped / in char classes.
      break;
    }

    // NOTE: Non-ASCII LineTerminators were handled by
    //       ProcessNonAsciiCodePoint calls above.
    if (unit == '\r' || unit == '\n') {
      ReportUnterminatedRegExp(unit);
      return badToken();
    }

    MOZ_ASSERT(!IsLineTerminator(AssertedCast<char32_t>(unit)));
    if (!this->charBuffer.append(unit)) {
      return badToken();
    }
  } while (true);

  int32_t unit;
  RegExpFlag reflags = NoFlags;
  while (true) {
    RegExpFlag flag;
    unit = getCodeUnit();
    if (unit == 'g') {
      flag = GlobalFlag;
    } else if (unit == 'i') {
      flag = IgnoreCaseFlag;
    } else if (unit == 'm') {
      flag = MultilineFlag;
    } else if (unit == 'y') {
      flag = StickyFlag;
    } else if (unit == 'u') {
      flag = UnicodeFlag;
    } else if (IsAsciiAlpha(unit)) {
      flag = NoFlags;
    } else {
      break;
    }

    if ((reflags & flag) || flag == NoFlags) {
      ungetCodeUnit(unit);
      char buf[2] = {char(unit), '\0'};
      error(JSMSG_BAD_REGEXP_FLAG, buf);
      return badToken();
    }

    reflags = RegExpFlag(reflags | flag);
  }
  ungetCodeUnit(unit);

  newRegExpToken(reflags, start, out);
  return true;
}

#ifdef ENABLE_BIGINT
template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool TokenStreamSpecific<Unit, AnyCharsAccess>::bigIntLiteral(
    TokenStart start, Modifier modifier, TokenKind* out) {
  MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == toUnit('n'));
  MOZ_ASSERT(this->sourceUnits.offset() > start.offset());
  uint32_t length = this->sourceUnits.offset() - start.offset();
  MOZ_ASSERT(length >= 2);
  this->charBuffer.clear();
  mozilla::Range<const Unit> chars(
      this->sourceUnits.codeUnitPtrAt(start.offset()), length);
  for (uint32_t idx = 0; idx < length - 1; idx++) {
    int32_t unit = CodeUnitValue(chars[idx]);
    // Char buffer may start with a 0[bBoOxX] prefix, then follows with
    // binary, octal, decimal, or hex digits.  Already checked by caller, as
    // the "n" indicating bigint comes at the end.
    MOZ_ASSERT(isAsciiCodePoint(unit));
    if (!this->appendCodePointToCharBuffer(unit)) {
      return false;
    }
  }
  newBigIntToken(start, modifier, out);
  return true;
}
#endif

template <typename Unit, class AnyCharsAccess>
MOZ_MUST_USE bool TokenStreamSpecific<Unit, AnyCharsAccess>::getTokenInternal(
    TokenKind* const ttp, const Modifier modifier) {
  // Assume we'll fail: success cases will overwrite this.
#ifdef DEBUG
  *ttp = TokenKind::Limit;
#endif
  MOZ_MAKE_MEM_UNDEFINED(ttp, sizeof(*ttp));

  // Check if in the middle of a template string. Have to get this out of
  // the way first.
  if (MOZ_UNLIKELY(modifier == TemplateTail)) {
    return getStringOrTemplateToken('`', modifier, ttp);
  }

  // This loop runs more than once only when whitespace or comments are
  // encountered.
  do {
    int32_t unit = peekCodeUnit();
    if (MOZ_UNLIKELY(unit == EOF)) {
      MOZ_ASSERT(this->sourceUnits.atEnd());
      anyCharsAccess().flags.isEOF = true;
      TokenStart start(this->sourceUnits, 0);
      newSimpleToken(TokenKind::Eof, start, modifier, ttp);
      return true;
    }

    if (MOZ_UNLIKELY(!isAsciiCodePoint(unit))) {
      // Non-ASCII code points can only be identifiers or whitespace.
      // It would be nice to compute these *after* discarding whitespace,
      // but IN A WORLD where |unicode::IsSpaceOrBOM2| requires consuming
      // a variable number of code points, it's easier to assume it's an
      // identifier and maybe do a little wasted work, than to unget and
      // compute and reget if whitespace.
      TokenStart start(this->sourceUnits, 0);
      const Unit* identStart = this->sourceUnits.addressOfNextCodeUnit();

      PeekedCodePoint<Unit> peeked = this->sourceUnits.peekCodePoint();
      if (peeked.isNone()) {
        int32_t bad;
        MOZ_ALWAYS_FALSE(getCodePoint(&bad));
        return badToken();
      }

      char32_t cp = peeked.codePoint();
      if (unicode::IsSpaceOrBOM2(cp)) {
        this->sourceUnits.consumeKnownCodePoint(peeked);
        if (IsLineTerminator(cp)) {
          if (!updateLineInfoForEOL()) {
            return badToken();
          }

          anyCharsAccess().updateFlagsForEOL();
        }

        continue;
      }

      static_assert(isAsciiCodePoint('$'),
                    "IdentifierStart contains '$', but as "
                    "!IsUnicodeIDStart('$'), ensure that '$' is never "
                    "handled here");
      static_assert(isAsciiCodePoint('_'),
                    "IdentifierStart contains '_', but as "
                    "!IsUnicodeIDStart('_'), ensure that '_' is never "
                    "handled here");

      if (MOZ_LIKELY(unicode::IsUnicodeIDStart(cp))) {
        this->sourceUnits.consumeKnownCodePoint(peeked);
        MOZ_ASSERT(!IsLineTerminator(cp),
                   "IdentifierStart must guarantee !IsLineTerminator "
                   "or else we'll fail to maintain line-info/flags "
                   "for EOL here");

        return identifierName(start, identStart, IdentifierEscapes::None,
                              modifier, NameVisibility::Public, ttp);
      }

      error(JSMSG_ILLEGAL_CHARACTER);
      return badToken();
    }  // !isAsciiCodePoint(unit)

    consumeKnownCodeUnit(unit);

    // Get the token kind, based on the first char.  The ordering of c1kind
    // comparison is based on the frequency of tokens in real code:
    // Parsemark (which represents typical JS code on the web) and the
    // Unreal demo (which represents asm.js code).
    //
    //                  Parsemark   Unreal
    //  OneChar         32.9%       39.7%
    //  Space           25.0%        0.6%
    //  Ident           19.2%       36.4%
    //  Dec              7.2%        5.1%
    //  String           7.9%        0.0%
    //  EOL              1.7%        0.0%
    //  ZeroDigit        0.4%        4.9%
    //  Other            5.7%       13.3%
    //
    // The ordering is based mostly only Parsemark frequencies, with Unreal
    // frequencies used to break close categories (e.g. |Dec| and
    // |String|).  |Other| is biggish, but no other token kind is common
    // enough for it to be worth adding extra values to FirstCharKind.
    FirstCharKind c1kind = FirstCharKind(firstCharKinds[unit]);

    // Look for an unambiguous single-char token.
    //
    if (c1kind <= OneChar_Max) {
      TokenStart start(this->sourceUnits, -1);
      newSimpleToken(TokenKind(c1kind), start, modifier, ttp);
      return true;
    }

    // Skip over non-EOL whitespace chars.
    //
    if (c1kind == Space) {
      continue;
    }

    // Look for an identifier.
    //
    if (c1kind == Ident) {
      TokenStart start(this->sourceUnits, -1);
      return identifierName(
          start, this->sourceUnits.addressOfNextCodeUnit() - 1,
          IdentifierEscapes::None, modifier, NameVisibility::Public, ttp);
    }

    // Look for a decimal number.
    //
    if (c1kind == Dec) {
      TokenStart start(this->sourceUnits, -1);
      const Unit* numStart = this->sourceUnits.addressOfNextCodeUnit() - 1;
      return decimalNumber(unit, start, numStart, modifier, ttp);
    }

    // Look for a string or a template string.
    //
    if (c1kind == String) {
      return getStringOrTemplateToken(static_cast<char>(unit), modifier, ttp);
    }

    // Skip over EOL chars, updating line state along the way.
    //
    if (c1kind == EOL) {
      if (unit == '\r') {
        matchLineTerminator('\n');
      }

      if (!updateLineInfoForEOL()) {
        return badToken();
      }

      anyCharsAccess().updateFlagsForEOL();
      continue;
    }

    // From a '0', look for a hexadecimal, binary, octal, or "noctal" (a
    // number starting with '0' that contains '8' or '9' and is treated as
    // decimal) number.
    //
    if (c1kind == ZeroDigit) {
      TokenStart start(this->sourceUnits, -1);
      int radix;
#ifdef ENABLE_BIGINT
      bool isLegacyOctalOrNoctal = false;
      bool isBigInt = false;
#endif
      const Unit* numStart;
      unit = getCodeUnit();
      if (unit == 'x' || unit == 'X') {
        radix = 16;
        unit = getCodeUnit();
        if (!JS7_ISHEX(unit)) {
          // NOTE: |unit| may be EOF here.
          ungetCodeUnit(unit);
          error(JSMSG_MISSING_HEXDIGITS);
          return badToken();
        }

        // one past the '0x'
        numStart = this->sourceUnits.addressOfNextCodeUnit() - 1;

        while (JS7_ISHEX(unit)) {
          unit = getCodeUnit();
        }
      } else if (unit == 'b' || unit == 'B') {
        radix = 2;
        unit = getCodeUnit();
        if (unit != '0' && unit != '1') {
          // NOTE: |unit| may be EOF here.
          ungetCodeUnit(unit);
          error(JSMSG_MISSING_BINARY_DIGITS);
          return badToken();
        }

        // one past the '0b'
        numStart = this->sourceUnits.addressOfNextCodeUnit() - 1;

        while (unit == '0' || unit == '1') {
          unit = getCodeUnit();
        }
      } else if (unit == 'o' || unit == 'O') {
        radix = 8;
        unit = getCodeUnit();
        if (!JS7_ISOCT(unit)) {
          // NOTE: |unit| may be EOF here.
          ungetCodeUnit(unit);
          error(JSMSG_MISSING_OCTAL_DIGITS);
          return badToken();
        }

        // one past the '0o'
        numStart = this->sourceUnits.addressOfNextCodeUnit() - 1;

        while (JS7_ISOCT(unit)) {
          unit = getCodeUnit();
        }
      } else if (IsAsciiDigit(unit)) {
        radix = 8;
#ifdef ENABLE_BIGINT
        isLegacyOctalOrNoctal = true;
#endif
        // one past the '0'
        numStart = this->sourceUnits.addressOfNextCodeUnit() - 1;

        do {
          // Octal integer literals are not permitted in strict mode
          // code.
          if (!reportStrictModeError(JSMSG_DEPRECATED_OCTAL)) {
            return badToken();
          }

          // Outside strict mode, we permit 08 and 09 as decimal
          // numbers, which makes our behaviour a superset of the
          // ECMA numeric grammar. We might not always be so
          // permissive, so we warn about it.
          if (unit >= '8') {
            if (!warning(JSMSG_BAD_OCTAL, unit == '8' ? "08" : "09")) {
              return badToken();
            }

            // Use the decimal scanner for the rest of the number.
            return decimalNumber(unit, start, numStart, modifier, ttp);
          }

          unit = getCodeUnit();
        } while (IsAsciiDigit(unit));
      } else {
        // '0' not followed by [XxBbOo0-9];  scan as a decimal number.
        numStart = this->sourceUnits.addressOfNextCodeUnit() - 1;

        // NOTE: |unit| may be EOF here.  (This is permitted by case #3
        //       in TokenStream.h docs for this function.)
        return decimalNumber(unit, start, numStart, modifier, ttp);
      }

#ifdef ENABLE_BIGINT
      if (unit == 'n') {
        if (isLegacyOctalOrNoctal) {
          error(JSMSG_BIGINT_INVALID_SYNTAX);
          return badToken();
        }
        isBigInt = true;
        unit = peekCodeUnit();
      } else {
        ungetCodeUnit(unit);
      }
#else
      ungetCodeUnit(unit);
#endif

      // Error if an identifier-start code point appears immediately
      // after the number.  Somewhat surprisingly, if we don't check
      // here, we'll never check at all.
      if (MOZ_LIKELY(isAsciiCodePoint(unit))) {
        if (unicode::IsIdentifierStart(char16_t(unit))) {
          error(JSMSG_IDSTART_AFTER_NUMBER);
          return badToken();
        }
      } else if (MOZ_LIKELY(unit != EOF)) {
        // This ignores encoding errors: subsequent caller-side code to
        // handle source text after the number will do so.
        PeekedCodePoint<Unit> peeked = this->sourceUnits.peekCodePoint();
        if (!peeked.isNone() &&
            unicode::IsIdentifierStart(peeked.codePoint())) {
          error(JSMSG_IDSTART_AFTER_NUMBER);
          return badToken();
        }
      }

#ifdef ENABLE_BIGINT
      if (isBigInt) {
        return bigIntLiteral(start, modifier, ttp);
      }
#endif

      double dval;
      if (!GetFullInteger(anyCharsAccess().cx, numStart,
                          this->sourceUnits.addressOfNextCodeUnit(), radix,
                          &dval)) {
        return badToken();
      }
      newNumberToken(dval, NoDecimal, start, modifier, ttp);
      return true;
    }

    MOZ_ASSERT(c1kind == Other);

    // This handles everything else.  Simple tokens distinguished solely by
    // TokenKind should set |simpleKind| and break, to share simple-token
    // creation code for all such tokens.  All other tokens must be handled
    // by returning (or by continuing from the loop enclosing this).
    //
    TokenStart start(this->sourceUnits, -1);
    TokenKind simpleKind;
#ifdef DEBUG
    simpleKind = TokenKind::Limit;  // sentinel value for code after switch
#endif

    // The block a ways above eliminated all non-ASCII, so cast to the
    // smallest type possible to assist the C++ compiler.
    switch (AssertedCast<uint8_t>(CodeUnitValue(toUnit(unit)))) {
      case '.':
        unit = getCodeUnit();
        if (IsAsciiDigit(unit)) {
          return decimalNumber('.', start,
                               this->sourceUnits.addressOfNextCodeUnit() - 2,
                               modifier, ttp);
        }

        if (unit == '.') {
          if (matchCodeUnit('.')) {
            simpleKind = TokenKind::TripleDot;
            break;
          }
        }

        // NOTE: |unit| may be EOF here.  A stray '.' at EOF would be an
        //       error, but subsequent code will handle it.
        ungetCodeUnit(unit);

        simpleKind = TokenKind::Dot;
        break;

      case '#': {
        TokenStart start(this->sourceUnits, -1);
        const Unit* identStart = this->sourceUnits.addressOfNextCodeUnit() - 1;
        IdentifierEscapes sawEscape;
        if (!matchIdentifierStart(&sawEscape)) {
          return badToken();
        }
        return identifierName(start, identStart, sawEscape, modifier,
                              NameVisibility::Private, ttp);
      }

      case '=':
        if (matchCodeUnit('=')) {
          simpleKind = matchCodeUnit('=') ? TokenKind::StrictEq : TokenKind::Eq;
        } else if (matchCodeUnit('>')) {
          simpleKind = TokenKind::Arrow;
        } else {
          simpleKind = TokenKind::Assign;
        }
        break;

      case '+':
        if (matchCodeUnit('+')) {
          simpleKind = TokenKind::Inc;
        } else {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::AddAssign : TokenKind::Add;
        }
        break;

      case '\\': {
        uint32_t codePoint;
        if (uint32_t escapeLength = matchUnicodeEscapeIdStart(&codePoint)) {
          return identifierName(
              start,
              this->sourceUnits.addressOfNextCodeUnit() - escapeLength - 1,
              IdentifierEscapes::SawUnicodeEscape, modifier,
              NameVisibility::Public, ttp);
        }

        // We could point "into" a mistyped escape, e.g. for "\u{41H}" we
        // could point at the 'H'.  But we don't do that now, so the code
        // unit after the '\' isn't necessarily bad, so just point at the
        // start of the actually-invalid escape.
        ungetCodeUnit('\\');
        error(JSMSG_BAD_ESCAPE);
        return badToken();
      }

      case '|':
        if (matchCodeUnit('|')) {
          simpleKind = TokenKind::Or;
#ifdef ENABLE_PIPELINE_OPERATOR
        } else if (matchCodeUnit('>')) {
          simpleKind = TokenKind::Pipeline;
#endif
        } else {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::BitOrAssign : TokenKind::BitOr;
        }
        break;

      case '^':
        simpleKind =
            matchCodeUnit('=') ? TokenKind::BitXorAssign : TokenKind::BitXor;
        break;

      case '&':
        if (matchCodeUnit('&')) {
          simpleKind = TokenKind::And;
        } else {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::BitAndAssign : TokenKind::BitAnd;
        }
        break;

      case '!':
        if (matchCodeUnit('=')) {
          simpleKind = matchCodeUnit('=') ? TokenKind::StrictNe : TokenKind::Ne;
        } else {
          simpleKind = TokenKind::Not;
        }
        break;

      case '<':
        if (anyCharsAccess().options().allowHTMLComments) {
          // Treat HTML begin-comment as comment-till-end-of-line.
          if (matchCodeUnit('!')) {
            if (matchCodeUnit('-')) {
              if (matchCodeUnit('-')) {
                this->sourceUnits.consumeRestOfSingleLineComment();
                continue;
              }
              ungetCodeUnit('-');
            }
            ungetCodeUnit('!');
          }
        }
        if (matchCodeUnit('<')) {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::LshAssign : TokenKind::Lsh;
        } else {
          simpleKind = matchCodeUnit('=') ? TokenKind::Le : TokenKind::Lt;
        }
        break;

      case '>':
        if (matchCodeUnit('>')) {
          if (matchCodeUnit('>')) {
            simpleKind =
                matchCodeUnit('=') ? TokenKind::UrshAssign : TokenKind::Ursh;
          } else {
            simpleKind =
                matchCodeUnit('=') ? TokenKind::RshAssign : TokenKind::Rsh;
          }
        } else {
          simpleKind = matchCodeUnit('=') ? TokenKind::Ge : TokenKind::Gt;
        }
        break;

      case '*':
        if (matchCodeUnit('*')) {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::PowAssign : TokenKind::Pow;
        } else {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::MulAssign : TokenKind::Mul;
        }
        break;

      case '/':
        // Look for a single-line comment.
        if (matchCodeUnit('/')) {
          unit = getCodeUnit();
          if (unit == '@' || unit == '#') {
            bool shouldWarn = unit == '@';
            if (!getDirectives(false, shouldWarn)) {
              return false;
            }
          } else {
            // NOTE: |unit| may be EOF here.
            ungetCodeUnit(unit);
          }

          this->sourceUnits.consumeRestOfSingleLineComment();
          continue;
        }

        // Look for a multi-line comment.
        if (matchCodeUnit('*')) {
          TokenStreamAnyChars& anyChars = anyCharsAccess();
          unsigned linenoBefore = anyChars.lineno;

          do {
            int32_t unit = getCodeUnit();
            if (unit == EOF) {
              reportError(JSMSG_UNTERMINATED_COMMENT);
              return badToken();
            }

            if (unit == '*' && matchCodeUnit('/')) {
              break;
            }

            if (unit == '@' || unit == '#') {
              bool shouldWarn = unit == '@';
              if (!getDirectives(true, shouldWarn)) {
                return badToken();
              }
            } else if (MOZ_LIKELY(isAsciiCodePoint(unit))) {
              int32_t codePoint;
              if (!getFullAsciiCodePoint(unit, &codePoint)) {
                return badToken();
              }
            } else {
              int32_t codePoint;
              if (!getNonAsciiCodePoint(unit, &codePoint)) {
                return badToken();
              }
            }
          } while (true);

          if (linenoBefore != anyChars.lineno) {
            anyChars.updateFlagsForEOL();
          }

          continue;
        }

        // Look for a regexp.
        if (modifier == Operand) {
          return regexpLiteral(start, ttp);
        }

        simpleKind = matchCodeUnit('=') ? TokenKind::DivAssign : TokenKind::Div;
        break;

      case '%':
        simpleKind = matchCodeUnit('=') ? TokenKind::ModAssign : TokenKind::Mod;
        break;

      case '-':
        if (matchCodeUnit('-')) {
          if (anyCharsAccess().options().allowHTMLComments &&
              !anyCharsAccess().flags.isDirtyLine) {
            if (matchCodeUnit('>')) {
              this->sourceUnits.consumeRestOfSingleLineComment();
              continue;
            }
          }

          simpleKind = TokenKind::Dec;
        } else {
          simpleKind =
              matchCodeUnit('=') ? TokenKind::SubAssign : TokenKind::Sub;
        }
        break;

      default:
        // We consumed a bad ASCII code point/unit.  Put it back so the
        // error location is the bad code point.
        ungetCodeUnit(unit);
        error(JSMSG_ILLEGAL_CHARACTER);
        return badToken();
    }  // switch (AssertedCast<uint8_t>(CodeUnitValue(toUnit(unit))))

    MOZ_ASSERT(simpleKind != TokenKind::Limit,
               "switch-statement should have set |simpleKind| before "
               "breaking");

    newSimpleToken(simpleKind, start, modifier, ttp);
    return true;
  } while (true);
}

template <typename Unit, class AnyCharsAccess>
bool TokenStreamSpecific<Unit, AnyCharsAccess>::getStringOrTemplateToken(
    char untilChar, Modifier modifier, TokenKind* out) {
  MOZ_ASSERT(untilChar == '\'' || untilChar == '"' || untilChar == '`',
             "unexpected string/template literal delimiter");

  bool parsingTemplate = (untilChar == '`');
  bool templateHead = false;

  TokenStart start(this->sourceUnits, -1);
  this->charBuffer.clear();

  // Run the bad-token code for every path out of this function except the
  // one success-case.
  auto noteBadToken = MakeScopeExit([this]() { this->badToken(); });

  auto ReportPrematureEndOfLiteral = [this, untilChar](unsigned errnum) {
    // Unicode separators aren't end-of-line in template or (as of
    // recently) string literals, so this assertion doesn't allow them.
    MOZ_ASSERT(this->sourceUnits.atEnd() ||
                   this->sourceUnits.peekCodeUnit() == Unit('\r') ||
                   this->sourceUnits.peekCodeUnit() == Unit('\n'),
               "must be parked at EOF or EOL to call this function");

    // The various errors reported here include language like "in a ''
    // literal" or similar, with '' being '', "", or `` as appropriate.
    const char delimiters[] = {untilChar, untilChar, '\0'};

    this->error(errnum, delimiters);
    return;
  };

  // We need to detect any of these chars:  " or ', \n (or its
  // equivalents), \\, EOF.  Because we detect EOL sequences here and
  // put them back immediately, we can use getCodeUnit().
  int32_t unit;
  while ((unit = getCodeUnit()) != untilChar) {
    if (unit == EOF) {
      ReportPrematureEndOfLiteral(JSMSG_EOF_BEFORE_END_OF_LITERAL);
      return false;
    }

    // Non-ASCII code points are always directly appended -- even
    // U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR that are
    // ordinarily LineTerminatorSequences.  (They contribute their literal
    // values to template and [as of recently] string literals, but they're
    // line terminators when computing line/column coordinates.)  Handle
    // the non-ASCII case early for readability.
    if (MOZ_UNLIKELY(!isAsciiCodePoint(unit))) {
      char32_t cp;
      if (!getNonAsciiCodePointDontNormalize(toUnit(unit), &cp)) {
        return false;
      }

      if (MOZ_UNLIKELY(cp == unicode::LINE_SEPARATOR ||
                       cp == unicode::PARA_SEPARATOR)) {
        if (!updateLineInfoForEOL()) {
          return false;
        }

        anyCharsAccess().updateFlagsForEOL();
      } else {
        MOZ_ASSERT(!IsLineTerminator(cp));
      }

      if (!appendCodePointToCharBuffer(cp)) {
        return false;
      }

      continue;
    }

    if (unit == '\\') {
      // When parsing templates, we don't immediately report errors for
      // invalid escapes; these are handled by the parser.  We don't
      // append to charBuffer in those cases because it won't be read.
      unit = getCodeUnit();
      if (unit == EOF) {
        ReportPrematureEndOfLiteral(JSMSG_EOF_IN_ESCAPE_IN_LITERAL);
        return false;
      }

      // Non-ASCII |unit| isn't handled by code after this, so dedicate
      // an unlikely special-case to it and then continue.
      if (MOZ_UNLIKELY(!isAsciiCodePoint(unit))) {
        int32_t codePoint;
        if (!getNonAsciiCodePoint(unit, &codePoint)) {
          return false;
        }

        // If we consumed U+2028 LINE SEPARATOR or U+2029 PARAGRAPH
        // SEPARATOR, they'll be normalized to '\n'.  '\' followed by
        // LineContinuation represents no code points, so don't append
        // in this case.
        if (codePoint != '\n') {
          if (!appendCodePointToCharBuffer(AssertedCast<char32_t>(codePoint))) {
            return false;
          }
        }

        continue;
      }

      // The block above eliminated all non-ASCII, so cast to the
      // smallest type possible to assist the C++ compiler.
      switch (AssertedCast<uint8_t>(CodeUnitValue(toUnit(unit)))) {
        case 'b':
          unit = '\b';
          break;
        case 'f':
          unit = '\f';
          break;
        case 'n':
          unit = '\n';
          break;
        case 'r':
          unit = '\r';
          break;
        case 't':
          unit = '\t';
          break;
        case 'v':
          unit = '\v';
          break;

        case '\r':
          matchLineTerminator('\n');
          MOZ_FALLTHROUGH;
        case '\n': {
          // LineContinuation represents no code points.  We're manually
          // consuming a LineTerminatorSequence, so we must manually
          // update line/column info.
          if (!updateLineInfoForEOL()) {
            return false;
          }

          continue;
        }

        // Unicode character specification.
        case 'u': {
          int32_t c2 = getCodeUnit();
          if (c2 == EOF) {
            ReportPrematureEndOfLiteral(JSMSG_EOF_IN_ESCAPE_IN_LITERAL);
            return false;
          }

          // First handle a delimited Unicode escape, e.g. \u{1F4A9}.
          if (c2 == '{') {
            uint32_t start = this->sourceUnits.offset() - 3;
            uint32_t code = 0;
            bool first = true;
            bool valid = true;
            do {
              int32_t u3 = getCodeUnit();
              if (u3 == EOF) {
                if (parsingTemplate) {
                  TokenStreamAnyChars& anyChars = anyCharsAccess();
                  anyChars.setInvalidTemplateEscape(start,
                                                    InvalidEscapeType::Unicode);
                  valid = false;
                  break;
                }
                reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                return false;
              }
              if (u3 == '}') {
                if (first) {
                  if (parsingTemplate) {
                    TokenStreamAnyChars& anyChars = anyCharsAccess();
                    anyChars.setInvalidTemplateEscape(
                        start, InvalidEscapeType::Unicode);
                    valid = false;
                    break;
                  }
                  reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                  return false;
                }
                break;
              }

              // Beware: |u3| may be a non-ASCII code point here; if
              // so it'll pass into this |if|-block.
              if (!JS7_ISHEX(u3)) {
                if (parsingTemplate) {
                  // We put the code unit back so that we read it
                  // on the next pass, which matters if it was
                  // '`' or '\'.
                  ungetCodeUnit(u3);

                  TokenStreamAnyChars& anyChars = anyCharsAccess();
                  anyChars.setInvalidTemplateEscape(start,
                                                    InvalidEscapeType::Unicode);
                  valid = false;
                  break;
                }
                reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                return false;
              }

              code = (code << 4) | JS7_UNHEX(u3);
              if (code > unicode::NonBMPMax) {
                if (parsingTemplate) {
                  TokenStreamAnyChars& anyChars = anyCharsAccess();
                  anyChars.setInvalidTemplateEscape(
                      start + 3, InvalidEscapeType::UnicodeOverflow);
                  valid = false;
                  break;
                }
                reportInvalidEscapeError(start + 3,
                                         InvalidEscapeType::UnicodeOverflow);
                return false;
              }

              first = false;
            } while (true);

            if (!valid) {
              continue;
            }

            MOZ_ASSERT(code <= unicode::NonBMPMax);
            if (!appendCodePointToCharBuffer(code)) {
              return false;
            }

            continue;
          }  // end of delimited Unicode escape handling

          // Otherwise it must be a fixed-length \uXXXX Unicode escape.
          // If it isn't, this is usually an error -- but if this is a
          // template literal, we must defer error reporting because
          // malformed escapes are okay in *tagged* template literals.
          char16_t v;
          if (JS7_ISHEX(c2) && this->sourceUnits.matchHexDigits(3, &v)) {
            unit = (JS7_UNHEX(c2) << 12) | v;
          } else {
            // Beware: |c2| may not be an ASCII code point here!
            ungetCodeUnit(c2);
            uint32_t start = this->sourceUnits.offset() - 2;
            if (parsingTemplate) {
              TokenStreamAnyChars& anyChars = anyCharsAccess();
              anyChars.setInvalidTemplateEscape(start,
                                                InvalidEscapeType::Unicode);
              continue;
            }
            reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
            return false;
          }
          break;
        }  // case 'u'

        // Hexadecimal character specification.
        case 'x': {
          char16_t v;
          if (this->sourceUnits.matchHexDigits(2, &v)) {
            unit = v;
          } else {
            uint32_t start = this->sourceUnits.offset() - 2;
            if (parsingTemplate) {
              TokenStreamAnyChars& anyChars = anyCharsAccess();
              anyChars.setInvalidTemplateEscape(start,
                                                InvalidEscapeType::Hexadecimal);
              continue;
            }
            reportInvalidEscapeError(start, InvalidEscapeType::Hexadecimal);
            return false;
          }
          break;
        }

        default: {
          if (!JS7_ISOCT(unit)) {
            break;
          }

          // Octal character specification.
          int32_t val = JS7_UNOCT(unit);

          unit = peekCodeUnit();
          if (MOZ_UNLIKELY(unit == EOF)) {
            ReportPrematureEndOfLiteral(JSMSG_EOF_IN_ESCAPE_IN_LITERAL);
            return false;
          }

          // Strict mode code allows only \0, then a non-digit.
          if (val != 0 || IsAsciiDigit(unit)) {
            TokenStreamAnyChars& anyChars = anyCharsAccess();
            if (parsingTemplate) {
              anyChars.setInvalidTemplateEscape(this->sourceUnits.offset() - 2,
                                                InvalidEscapeType::Octal);
              continue;
            }
            if (!reportStrictModeError(JSMSG_DEPRECATED_OCTAL)) {
              return false;
            }
            anyChars.flags.sawOctalEscape = true;
          }

          if (JS7_ISOCT(unit)) {
            val = 8 * val + JS7_UNOCT(unit);
            consumeKnownCodeUnit(unit);

            unit = peekCodeUnit();
            if (MOZ_UNLIKELY(unit == EOF)) {
              ReportPrematureEndOfLiteral(JSMSG_EOF_IN_ESCAPE_IN_LITERAL);
              return false;
            }

            if (JS7_ISOCT(unit)) {
              int32_t save = val;
              val = 8 * val + JS7_UNOCT(unit);
              if (val <= 0xFF) {
                consumeKnownCodeUnit(unit);
              } else {
                val = save;
              }
            }
          }

          unit = char16_t(val);
          break;
        }  // default
      }    // switch (AssertedCast<uint8_t>(CodeUnitValue(toUnit(unit))))

      if (!this->charBuffer.append(unit)) {
        return false;
      }

      continue;
    }  // (unit == '\\')

    if (unit == '\r' || unit == '\n') {
      if (!parsingTemplate) {
        // String literals don't allow ASCII line breaks.
        ungetCodeUnit(unit);
        ReportPrematureEndOfLiteral(JSMSG_EOL_BEFORE_END_OF_STRING);
        return false;
      }

      if (unit == '\r') {
        unit = '\n';
        matchLineTerminator('\n');
      }

      if (!updateLineInfoForEOL()) {
        return false;
      }

      anyCharsAccess().updateFlagsForEOL();
    } else if (parsingTemplate && unit == '$' && matchCodeUnit('{')) {
      templateHead = true;
      break;
    }

    if (!this->charBuffer.append(unit)) {
      return false;
    }
  }

  JSAtom* atom = drainCharBufferIntoAtom(anyCharsAccess().cx);
  if (!atom) {
    return false;
  }

  noteBadToken.release();

  MOZ_ASSERT_IF(!parsingTemplate, !templateHead);

  TokenKind kind = !parsingTemplate ? TokenKind::String
                                    : templateHead ? TokenKind::TemplateHead
                                                   : TokenKind::NoSubsTemplate;
  newAtomToken(kind, atom, start, modifier, out);
  return true;
}

const char* TokenKindToDesc(TokenKind tt) {
  switch (tt) {
#define EMIT_CASE(name, desc) \
  case TokenKind::name:       \
    return desc;
    FOR_EACH_TOKEN_KIND(EMIT_CASE)
#undef EMIT_CASE
    case TokenKind::Limit:
      MOZ_ASSERT_UNREACHABLE("TokenKind::Limit should not be passed.");
      break;
  }

  return "<bad TokenKind>";
}

#ifdef DEBUG
const char* TokenKindToString(TokenKind tt) {
  switch (tt) {
#define EMIT_CASE(name, desc) \
  case TokenKind::name:       \
    return "TokenKind::" #name;
    FOR_EACH_TOKEN_KIND(EMIT_CASE)
#undef EMIT_CASE
    case TokenKind::Limit:
      break;
  }

  return "<bad TokenKind>";
}
#endif

template class TokenStreamCharsBase<Utf8Unit>;
template class TokenStreamCharsBase<char16_t>;

template class TokenStreamChars<char16_t, TokenStreamAnyCharsAccess>;
template class TokenStreamSpecific<char16_t, TokenStreamAnyCharsAccess>;

template class TokenStreamChars<
    Utf8Unit, ParserAnyCharsAccess<GeneralParser<FullParseHandler, Utf8Unit>>>;
template class TokenStreamChars<
    Utf8Unit,
    ParserAnyCharsAccess<GeneralParser<SyntaxParseHandler, Utf8Unit>>>;
template class TokenStreamChars<
    char16_t, ParserAnyCharsAccess<GeneralParser<FullParseHandler, char16_t>>>;
template class TokenStreamChars<
    char16_t,
    ParserAnyCharsAccess<GeneralParser<SyntaxParseHandler, char16_t>>>;

template class TokenStreamSpecific<
    Utf8Unit, ParserAnyCharsAccess<GeneralParser<FullParseHandler, Utf8Unit>>>;
template class TokenStreamSpecific<
    Utf8Unit,
    ParserAnyCharsAccess<GeneralParser<SyntaxParseHandler, Utf8Unit>>>;
template class TokenStreamSpecific<
    char16_t, ParserAnyCharsAccess<GeneralParser<FullParseHandler, char16_t>>>;
template class TokenStreamSpecific<
    char16_t,
    ParserAnyCharsAccess<GeneralParser<SyntaxParseHandler, char16_t>>>;

}  // namespace frontend

}  // namespace js

JS_FRIEND_API int js_fgets(char* buf, int size, FILE* file) {
  int n, i, c;
  bool crflag;

  n = size - 1;
  if (n < 0) {
    return -1;
  }

  crflag = false;
  for (i = 0; i < n && (c = fast_getc(file)) != EOF; i++) {
    buf[i] = c;
    if (c == '\n') {  // any \n ends a line
      i++;            // keep the \n; we know there is room for \0
      break;
    }
    if (crflag) {  // \r not followed by \n ends line at the \r
      ungetc(c, file);
      break;  // and overwrite c in buf with \0
    }
    crflag = (c == '\r');
  }

  buf[i] = '\0';
  return i;
}
