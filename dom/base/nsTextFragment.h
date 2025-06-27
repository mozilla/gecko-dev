/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class which represents a fragment of text (eg inside a text
 * node); if only codepoints below 256 are used, the text is stored as
 * a char*; otherwise the text is stored as a char16_t*
 */

#ifndef nsTextFragment_h___
#define nsTextFragment_h___

#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "mozilla/MemoryReporting.h"

#include "nsCharTraits.h"
#include "nsString.h"
#include "mozilla/StringBuffer.h"
#include "nsReadableUtils.h"
#include "nsISupportsImpl.h"

// XXX should this normalize the code to keep a \u0000 at the end?

// XXX nsTextFragmentPool?

/**
 * A fragment of text. If mIs2b is 1 then the m2b pointer is valid
 * otherwise the m1b pointer is valid. If m1b is used then each byte
 * of data represents a single ucs2 character with the high byte being
 * zero.
 *
 * This class does not have a virtual destructor therefore it is not
 * meant to be subclassed.
 */
class nsTextFragment final {
 private:
  constexpr static unsigned char kFormFeed = '\f';
  constexpr static unsigned char kNewLine = '\n';
  constexpr static unsigned char kCarriageReturn = '\r';
  constexpr static unsigned char kTab = '\t';
  constexpr static unsigned char kSpace = ' ';
  constexpr static unsigned char kNBSP = 0xA0;

 public:
  static nsresult Init();
  static void Shutdown();

  /**
   * Default constructor. Initialize the fragment to be empty.
   */
  nsTextFragment() : m1b(nullptr), mAllBits(0) {
    MOZ_COUNT_CTOR(nsTextFragment);
    NS_ASSERTION(sizeof(FragmentBits) == 4, "Bad field packing!");
  }

  ~nsTextFragment();

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument fragment, or to "" if unable to allocate enough memory.
   */
  nsTextFragment& operator=(const nsTextFragment& aOther);

  /**
   * Return true if this fragment is represented by char16_t data
   */
  bool Is2b() const { return mState.mIs2b; }

  /**
   * Return true if this fragment contains Bidi text
   * For performance reasons this flag is only set if explicitely requested (by
   * setting the aUpdateBidi argument on SetTo or Append to true).
   */
  bool IsBidi() const { return mState.mIsBidi; }

  /**
   * Get a pointer to constant char16_t data.
   */
  const char16_t* Get2b() const {
    MOZ_ASSERT(Is2b(), "not 2b text");
    return static_cast<char16_t*>(m2b->Data());
  }

  /**
   * Get a pointer to constant char data.
   * NOTE: nsTextFragment treat the 1b buffer as an array of unsigned chars.
   * Therefore, Get1b() is not good one for looking for a character between 0x80
   * - 0xFF in the buffer.
   */
  const char* Get1b() const {
    NS_ASSERTION(!Is2b(), "not 1b text");
    return (const char*)m1b;
  }
  /**
   * Get a pointer to constant unsigned char data.
   */
  const unsigned char* GetUnsigned1b() const {
    NS_ASSERTION(!Is2b(), "not 1b text");
    return (const unsigned char*)m1b;
  }

  /**
   * Get the length of the fragment. The length is the number of logical
   * characters, not the number of bytes to store the characters.
   */
  uint32_t GetLength() const { return mState.mLength; }

#define NS_MAX_TEXT_FRAGMENT_LENGTH (static_cast<uint32_t>(0x1FFFFFFF))

  bool CanGrowBy(size_t n) const {
    return n < (1 << 29) && mState.mLength + n < (1 << 29);
  }

  /**
   * Change the contents of this fragment to be a copy of the given
   * buffer. If aUpdateBidi is true, contents of the fragment will be scanned,
   * and mState.mIsBidi will be turned on if it includes any Bidi characters.
   * If aForce2b is true, aBuffer will be stored as char16_t as is.  Then,
   * you can access the value faster but may waste memory if all characters
   * are less than U+0100.
   */
  bool SetTo(const char16_t* aBuffer, uint32_t aLength, bool aUpdateBidi,
             bool aForce2b);

  bool SetTo(const nsString& aString, bool aUpdateBidi, bool aForce2b) {
    if (MOZ_UNLIKELY(aString.Length() > NS_MAX_TEXT_FRAGMENT_LENGTH)) {
      return false;
    }
    ReleaseText();
    if (aForce2b && !aUpdateBidi) {
      if (mozilla::StringBuffer* buffer = aString.GetStringBuffer()) {
        NS_ADDREF(m2b = buffer);
        mState.mInHeap = true;
        mState.mIs2b = true;
        mState.mLength = aString.Length();
        return true;
      }
    }

    return SetTo(aString.get(), aString.Length(), aUpdateBidi, aForce2b);
  }

  /**
   * Append aData to the end of this fragment. If aUpdateBidi is true, contents
   * of the fragment will be scanned, and mState.mIsBidi will be turned on if
   * it includes any Bidi characters.
   * If aForce2b is true, the string will be stored as char16_t as is.  Then,
   * you can access the value faster but may waste memory if all characters
   * are less than U+0100.
   */
  bool Append(const char16_t* aBuffer, uint32_t aLength, bool aUpdateBidi,
              bool aForce2b);

  /**
   * Append the contents of this string fragment to aString
   */
  void AppendTo(nsAString& aString) const {
    if (!AppendTo(aString, mozilla::fallible)) {
      aString.AllocFailed(aString.Length() + GetLength());
    }
  }

  /**
   * Append the contents of this string fragment to aString
   * @return false if an out of memory condition is detected, true otherwise
   */
  [[nodiscard]] bool AppendTo(nsAString& aString,
                              const mozilla::fallible_t& aFallible) const {
    if (mState.mIs2b) {
      if (aString.IsEmpty()) {
        aString.Assign(m2b, mState.mLength);
        return true;
      }
      return aString.Append(Get2b(), mState.mLength, aFallible);
    }
    return AppendASCIItoUTF16(Substring(m1b, mState.mLength), aString,
                              aFallible);
  }

  /**
   * Append a substring of the contents of this string fragment to aString.
   * @param aOffset where to start the substring in this text fragment
   * @param aLength the length of the substring
   */
  void AppendTo(nsAString& aString, uint32_t aOffset, uint32_t aLength) const {
    if (!AppendTo(aString, aOffset, aLength, mozilla::fallible)) {
      aString.AllocFailed(aString.Length() + aLength);
    }
  }

  /**
   * Append a substring of the contents of this string fragment to aString.
   * @param aString the string in which to append
   * @param aOffset where to start the substring in this text fragment
   * @param aLength the length of the substring
   * @return false if an out of memory condition is detected, true otherwise
   */
  [[nodiscard]] bool AppendTo(nsAString& aString, uint32_t aOffset,
                              uint32_t aLength,
                              const mozilla::fallible_t& aFallible) const {
    if (mState.mIs2b) {
      bool ok = aString.Append(Get2b() + aOffset, aLength, aFallible);
      if (!ok) {
        return false;
      }

      return true;
    } else {
      return AppendASCIItoUTF16(Substring(m1b + aOffset, aLength), aString,
                                aFallible);
    }
  }

  /**
   * Make a copy of the fragments contents starting at offset for
   * count characters. The offset and count will be adjusted to
   * lie within the fragments data. The fragments data is converted if
   * necessary.
   */
  void CopyTo(char16_t* aDest, uint32_t aOffset, uint32_t aCount);

  /**
   * Return the character in the text-fragment at the given
   * index. This always returns a char16_t.
   */
  [[nodiscard]] char16_t CharAt(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mState.mLength, "bad index");
    return mState.mIs2b ? Get2b()[aIndex]
                        : static_cast<unsigned char>(m1b[aIndex]);
  }
  [[nodiscard]] char16_t SafeCharAt(uint32_t aIndex) const {
    return MOZ_LIKELY(mState.mLength < aIndex) ? CharAt(aIndex)
                                               : static_cast<char16_t>(0);
  }

  /**
   * Return the first char, but if you're not sure whether this is empty, you
   * should use GetFirstChar() instead.
   */
  [[nodiscard]] char16_t FirstChar() const {
    MOZ_ASSERT(mState.mLength);
    return CharAt(0u);
  }
  [[nodiscard]] char16_t SafeFirstChar() const {
    return MOZ_LIKELY(mState.mLength) ? FirstChar() : static_cast<char16_t>(0);
  }
  /**
   * Return the last char, but if you're not sure whether this is empty, you
   * should use GetLastChar() instead.
   */
  [[nodiscard]] char16_t LastChar() const {
    MOZ_ASSERT(mState.mLength);
    return CharAt(mState.mLength - 1);
  }
  [[nodiscard]] char16_t SafeLastChar() const {
    return MOZ_LIKELY(mState.mLength) ? LastChar() : static_cast<char16_t>(0);
  }

  /**
   * IsHighSurrogateFollowedByLowSurrogateAt() returns true if character at
   * aIndex is high surrogate and it's followed by low surrogate.
   */
  inline bool IsHighSurrogateFollowedByLowSurrogateAt(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mState.mLength);
    if (!mState.mIs2b || aIndex + 1 >= mState.mLength) {
      return false;
    }
    return NS_IS_SURROGATE_PAIR(Get2b()[aIndex], Get2b()[aIndex + 1]);
  }

  /**
   * IsLowSurrogateFollowingHighSurrogateAt() returns true if character at
   * aIndex is low surrogate and it follows high surrogate.
   */
  inline bool IsLowSurrogateFollowingHighSurrogateAt(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mState.mLength);
    if (!mState.mIs2b || !aIndex) {
      return false;
    }
    return NS_IS_SURROGATE_PAIR(Get2b()[aIndex - 1], Get2b()[aIndex]);
  }

  /**
   * ScalarValueAt() returns a Unicode scalar value at aIndex.  If the character
   * at aIndex is a high surrogate followed by low surrogate, returns character
   * code for the pair.  If the index is low surrogate, or a high surrogate but
   * not in a pair, returns 0.
   */
  inline char32_t ScalarValueAt(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mState.mLength);
    if (!mState.mIs2b) {
      return static_cast<unsigned char>(m1b[aIndex]);
    }
    char16_t ch = Get2b()[aIndex];
    if (!IS_SURROGATE(ch)) {
      return ch;
    }
    if (aIndex + 1 < mState.mLength && NS_IS_HIGH_SURROGATE(ch)) {
      char16_t nextCh = Get2b()[aIndex + 1];
      if (NS_IS_LOW_SURROGATE(nextCh)) {
        return SURROGATE_TO_UCS4(ch, nextCh);
      }
    }
    return 0;
  }

  void SetBidi(bool aBidi) { mState.mIsBidi = aBidi; }

  struct FragmentBits {
    // uint32_t to ensure that the values are unsigned, because we
    // want 0/1, not 0/-1!
    // Making these bool causes Windows to not actually pack them,
    // which causes crashes because we assume this structure is no more than
    // 32 bits!
    uint32_t mInHeap : 1;
    uint32_t mIs2b : 1;
    uint32_t mIsBidi : 1;
    // Note that when you change the bits of mLength, you also need to change
    // NS_MAX_TEXT_FRAGMENT_LENGTH.
    uint32_t mLength : 29;
  };

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  /**
   * Check whether the text in this fragment is the same as the text in the
   * other fragment.
   */
  [[nodiscard]] bool TextEquals(const nsTextFragment& aOther) const;

  // FYI: FragmentBits::mLength is only 29 bits.  Therefore, UINT32_MAX won't
  // be valid offset in the data.
  constexpr static uint32_t kNotFound = UINT32_MAX;

  [[nodiscard]] uint32_t FindChar(char16_t aChar, uint32_t aOffset = 0) const {
    if (aOffset >= GetLength()) {
      return kNotFound;
    }
    if (Is2b()) {
      const char16_t* end = Get2b() + GetLength();
      for (const char16_t* ch = Get2b() + aOffset; ch != end; ch++) {
        if (*ch == aChar) {
          return ch - Get2b();
        }
      }
      return kNotFound;
    }
    if (aChar > 0xFF) {
      return kNotFound;
    }
    const unsigned char* end = GetUnsigned1b() + GetLength();
    for (const unsigned char* ch = GetUnsigned1b() + aOffset; ch != end; ch++) {
      if (*ch == aChar) {
        return ch - GetUnsigned1b();
      }
    }
    return kNotFound;
  }

  [[nodiscard]] uint32_t RFindChar(char16_t aChar,
                                   uint32_t aOffset = UINT32_MAX) const {
    const uint32_t length = GetLength();
    if (!length) {
      return kNotFound;
    }
    aOffset = std::min(length - 1u, aOffset);
    if (Is2b()) {
      const char16_t* end = Get2b() - 1;
      for (const char16_t* ch = Get2b() + aOffset; ch != end; ch--) {
        if (*ch == aChar) {
          return ch - Get2b();
        }
      }
      return kNotFound;
    }
    if (aChar > 0xFF) {
      return kNotFound;
    }
    const unsigned char* end = GetUnsigned1b() - 1;
    for (const unsigned char* ch = GetUnsigned1b() + aOffset; ch != end; ch--) {
      if (*ch == aChar) {
        return ch - GetUnsigned1b();
      }
    }
    return kNotFound;
  }

  enum class WhitespaceOption {
    // If set, new lines (\n, U+000A) are treated as significant.
    NewLineIsSignificant,
    // If set, NBSPs (&nbsp;, U+00A0) are treated as collapsible whitespaces.
    // This option is useful to scan previous or next visible character from
    // middle of a whitespace sequence because our editor makes consecutive
    // whitespaces visible with converting collapsible whitespaces to pairs of
    // &nbsp; and ASCII whitespace.
    TreatNBSPAsCollapsible,
    // If set, form feeds (\f, U+000C) are treated as significant.
    // Be aware, form feed is defined as a whitespace by the HTML spec, but is
    // not defined as so by the CSS spec. Therefore, it won't be rendered if it
    // appears in a whitespace sequence surrounded by block boundaries like
    // `data:text/html,%0C<div>%0Cabc%0C</div>%0C`. However, it'll be rendered
    // as a character if it appears if surrounded by visible contents like
    // `data:text/html,<div>abc %0C def</div>`.
    FormFeedIsSignificant,
  };
  using WhitespaceOptions = mozilla::EnumSet<WhitespaceOption>;

 private:
  // Helper class to check whether the character is a non-whitespace or not.
  // This avoids to call EnumSet<WhitespaceOption>::contains() a lot.
  class MOZ_STACK_CLASS AutoWhitespaceChecker final {
   public:
    explicit AutoWhitespaceChecker(const WhitespaceOptions& aOptions)
        : mNBSPIsSignificant(
              !aOptions.contains(WhitespaceOption::TreatNBSPAsCollapsible)),
          mFormFeedIsSignificant(
              aOptions.contains(WhitespaceOption::FormFeedIsSignificant)),
          mNewLineIsSignificant(
              aOptions.contains(WhitespaceOption::NewLineIsSignificant)) {}

    [[nodiscard]] bool IsNonWhitespace(char16_t aChar) const {
      switch (aChar) {
        case kNBSP:
          return mNBSPIsSignificant;
        case kFormFeed:
          return mFormFeedIsSignificant;
        case kNewLine:
          return mNewLineIsSignificant;
        case kSpace:
        case kTab:
        case kCarriageReturn:
          return false;
        default:
          return true;
      }
    }

   private:
    const bool mNBSPIsSignificant;
    const bool mFormFeedIsSignificant;
    const bool mNewLineIsSignificant;
  };

 public:
  /**
   * Return the first non-whitespace character index.
   *
   * @param aOptions Set options to change which character should not be treated
   * as a whitespace.
   * @param aOffset Start offset, so, the result will equal or greater than
   * aOffset if a char found.
   */
  [[nodiscard]] uint32_t FindNonWhitespaceChar(
      const WhitespaceOptions& aOptions = {}, uint32_t aOffset = 0) const {
    if (aOffset >= GetLength()) {
      return kNotFound;
    }
    const AutoWhitespaceChecker checker(aOptions);
    if (Is2b()) {
      const char16_t* end = Get2b() + GetLength();
      for (const char16_t* ch = Get2b() + aOffset; ch != end; ch++) {
        if (checker.IsNonWhitespace(*ch)) {
          return ch - Get2b();
        }
      }
      return kNotFound;
    }
    const unsigned char* end = GetUnsigned1b() + GetLength();
    for (const unsigned char* ch = GetUnsigned1b() + aOffset; ch != end; ch++) {
      if (checker.IsNonWhitespace(*ch)) {
        return ch - GetUnsigned1b();
      }
    }
    return kNotFound;
  }

  /**
   * Return the last non-whitespace character index.
   *
   * @param aOptions Set options to change which character should not be
   * treated as a whitespace.
   * @param aOffset Start offset, so, the result will equal or greater than
   * aOffset if a char found.
   */
  [[nodiscard]] uint32_t RFindNonWhitespaceChar(
      const WhitespaceOptions& aOptions = {},
      uint32_t aOffset = UINT32_MAX) const {
    const uint32_t length = GetLength();
    if (!length) {
      return kNotFound;
    }
    const AutoWhitespaceChecker checker(aOptions);
    aOffset = std::min(length - 1u, aOffset);
    if (Is2b()) {
      const char16_t* end = Get2b() - 1;
      for (const char16_t* ch = Get2b() + aOffset; ch != end; ch--) {
        if (checker.IsNonWhitespace(*ch)) {
          return ch - Get2b();
        }
      }
      return kNotFound;
    }
    const unsigned char* end = GetUnsigned1b() - 1;
    for (const unsigned char* ch = GetUnsigned1b() + aOffset; ch != end; ch--) {
      if (checker.IsNonWhitespace(*ch)) {
        return ch - GetUnsigned1b();
      }
    }
    return kNotFound;
  }

  /**
   * Return first different char offset in this fragment after
   * aOffsetInFragment. For example, if we have "abcdefg", aStr is "bXYe" and
   * aOffsetInFragment is 1, scan from "b" and return the offset of "c",
   * i.e., 2.
   *
   * Note that this is currently not usable to compare us with longer string.
   */
  [[nodiscard]] uint32_t FindFirstDifferentCharOffset(
      const nsAString& aStr, uint32_t aOffsetInFragment = 0u) const {
    return FindFirstDifferentCharOffsetInternal(aStr, aOffsetInFragment);
  }
  [[nodiscard]] uint32_t FindFirstDifferentCharOffset(
      const nsACString& aStr, uint32_t aOffsetInFragment = 0u) const {
    return FindFirstDifferentCharOffsetInternal(aStr, aOffsetInFragment);
  }

  /**
   * Return first different char offset in this fragment before
   * aOffsetInFragment (from backward scanning point of view).
   * For example, if we have "abcdef", aStr is "bXYe" and aOffsetInFragment is
   * 5, scan from "e" and return the offset of "d" (vs. "Y") in this fragment,
   * i.e., 3.  In other words, aOffsetInFragment should be the next offset of
   * you start to scan. I.e., at least 1 and at most the length of this.  So,
   * if you want to compare with start of this, you should specify
   * aStr.Length(), and if you want to compare with end of this, you should
   * specify GetLength() result of this (or just omit it).
   *
   * Note that this is currently not usable to compare us with longer string.
   */
  [[nodiscard]] uint32_t RFindFirstDifferentCharOffset(
      const nsAString& aStr, uint32_t aOffsetInFragment = UINT32_MAX) const {
    return RFindFirstDifferentCharOffsetInternal(aStr, aOffsetInFragment);
  }
  [[nodiscard]] uint32_t RFindFirstDifferentCharOffset(
      const nsACString& aStr, uint32_t aOffsetInFragment = UINT32_MAX) const {
    return RFindFirstDifferentCharOffsetInternal(aStr, aOffsetInFragment);
  }

 private:
  void ReleaseText();

  /**
   * Scan the contents of the fragment and turn on mState.mIsBidi if it
   * includes any Bidi characters.
   */
  void UpdateBidiFlag(const char16_t* aBuffer, uint32_t aLength);

  union {
    mozilla::StringBuffer* m2b;
    // FIXME: m1b is actually treated as const unsigned char* since the array
    // may contain characters between 0x80 - 0xFF.  So, copying the value to
    // char16_t might depend on how the compiler to treat the values.
    const char* m1b;  // This is const since it can point to shared data
  };

  union {
    uint32_t mAllBits;
    FragmentBits mState;
  };

  /**
   * See the explanation of FindFirstDifferentCharOffset() for the detail.
   *
   * This should not be directly exposed as a public method because it will
   * cause instantiating the method with various derived classes of nsAString
   * and nsACString.
   */
  template <typename nsAXString>
  [[nodiscard]] uint32_t FindFirstDifferentCharOffsetInternal(
      const nsAXString& aStr, uint32_t aOffsetInFragment) const {
    static_assert(std::is_same_v<nsAXString, nsAString> ||
                  std::is_same_v<nsAXString, nsACString>);
    MOZ_ASSERT(!aStr.IsEmpty());
    const uint32_t length = GetLength();
    MOZ_ASSERT(aOffsetInFragment <= length);
    if (NS_WARN_IF(aStr.IsEmpty()) || NS_WARN_IF(length <= aOffsetInFragment) ||
        NS_WARN_IF(length - aOffsetInFragment < aStr.Length())) {
      return kNotFound;
    }
    if (Is2b()) {
      const auto* ch = aStr.BeginReading();
      // At the first char of the scan range.
      const char16_t* ourCh = Get2b() + aOffsetInFragment;
      const auto* const end = aStr.EndReading();
      const char16_t* const ourEnd = Get2b() + length;
      for (; ch != end && ourCh != ourEnd; ch++, ourCh++) {
        if (*ch != *ourCh) {
          return ourCh - Get2b();
        }
      }
      return kNotFound;
    }
    const auto* ch = aStr.BeginReading();
    // At the first char of the scan range.
    const char* ourCh = Get1b() + aOffsetInFragment;
    const auto* const end = aStr.EndReading();
    const char* ourEnd = Get1b() + length;
    for (; ch != end && ourCh != ourEnd; ch++, ourCh++) {
      if (*ch != *ourCh) {
        return ourCh - Get1b();
      }
    }
    return kNotFound;
  }

  /**
   * See the explanation of RFindFirstDifferentCharOffset() for the detail.
   *
   * This should not be directly exposed as a public method because it will
   * cause instantiating the method with various derived classes of nsAString
   * and nsACString.
   */
  template <typename nsAXString>
  [[nodiscard]] uint32_t RFindFirstDifferentCharOffsetInternal(
      const nsAXString& aStr, uint32_t aOffsetInFragment) const {
    static_assert(std::is_same_v<nsAXString, nsAString> ||
                  std::is_same_v<nsAXString, nsACString>);
    MOZ_ASSERT(!aStr.IsEmpty());
    const uint32_t length = GetLength();
    MOZ_ASSERT(aOffsetInFragment <= length);
    aOffsetInFragment = std::min(length, aOffsetInFragment);
    if (NS_WARN_IF(aStr.IsEmpty()) || NS_WARN_IF(!aOffsetInFragment) ||
        NS_WARN_IF(aOffsetInFragment < aStr.Length())) {
      return kNotFound;
    }
    if (Is2b()) {
      const auto* ch = aStr.EndReading() - 1;
      // At the last char of the scan range
      const char16_t* ourCh = Get2b() + aOffsetInFragment - 1;
      const auto* const end = aStr.BeginReading() - 1;
      const char16_t* const ourEnd = Get2b() - 1;
      for (; ch != end && ourCh != ourEnd; ch--, ourCh--) {
        if (*ch != *ourCh) {
          return ourCh - Get2b();
        }
      }
      return kNotFound;
    }
    const auto* ch = aStr.EndReading() - 1;
    // At the last char of the scan range
    const char* ourCh = Get1b() + aOffsetInFragment - 1;
    const auto* const end = aStr.BeginReading() - 1;
    const char* const ourEnd = Get1b() - 1;
    for (; ch != end && ourCh != ourEnd; ch--, ourCh--) {
      if (*ch != *ourCh) {
        return ourCh - Get1b();
      }
    }
    return kNotFound;
  }
};

#endif /* nsTextFragment_h___ */
