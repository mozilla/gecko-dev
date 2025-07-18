/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Classes to iterate over grapheme, word, sentence, or line. */

#include "mozilla/intl/Segmenter.h"

#include "icu4x/GraphemeClusterSegmenter.hpp"
#include "icu4x/LineSegmenter.hpp"
#include "icu4x/SentenceSegmenter.hpp"
#include "icu4x/WordSegmenter.hpp"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/intl/LineBreaker.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/intl/UnicodeProperties.h"
#include "mozilla/StaticPrefs_intl.h"
#include "nsUnicodeProperties.h"
#include "nsCharTraits.h"
#include "nsThreadUtils.h"

#include <mutex>

using namespace icu4x;
using namespace mozilla::unicode;

namespace mozilla::intl {

SegmentIteratorUtf16::SegmentIteratorUtf16(Span<const char16_t> aText)
    : mText(aText) {}

Maybe<uint32_t> SegmentIteratorUtf16::Seek(uint32_t aPos) {
  if (mPos < aPos) {
    mPos = aPos;
  }
  return Next();
}

LineBreakIteratorUtf16::LineBreakIteratorUtf16(Span<const char16_t> aText,
                                               const LineBreakOptions& aOptions)
    : SegmentIteratorUtf16(aText), mOptions(aOptions) {
  if (!StaticPrefs::intl_icu4x_segmenter_enabled()) {
    return;
  }
  mSegmenter = capi::icu4x_LineSegmenter_create_auto_mv1();
  mIterator = capi::icu4x_LineSegmenter_segment_utf16_mv1(
      mSegmenter, {mText.Elements(), mText.Length()});
}

LineBreakIteratorUtf16::~LineBreakIteratorUtf16() {
  if (mIterator) {
    capi::icu4x_LineBreakIteratorUtf16_destroy_mv1(mIterator);
  }
  if (mSegmenter) {
    capi::icu4x_LineSegmenter_destroy_mv1(mSegmenter);
  }
}

Maybe<uint32_t> LineBreakIteratorUtf16::Next() {
  if (mIterator) {
    const int32_t nextPos =
        capi::icu4x_LineBreakIteratorUtf16_next_mv1(mIterator);
    if (nextPos < 0) {
      return Nothing();
    }
    if (!nextPos) {
      return Next();
    }
    mPos = nextPos;
    return Some(mPos);
  }
  const int32_t nextPos =
      LineBreaker::Next(mText.Elements(), mText.Length(), mPos);
  if (nextPos == NS_LINEBREAKER_NEED_MORE_TEXT) {
    return Nothing();
  }
  mPos = nextPos;
  return Some(mPos);
}

Maybe<uint32_t> LineBreakIteratorUtf16::Seek(uint32_t aPos) {
  if (mIterator) {
    if (mPos >= aPos) {
      return Next();
    }

    while (mPos < aPos) {
      const int32_t nextPos =
          capi::icu4x_LineBreakIteratorUtf16_next_mv1(mIterator);
      if (nextPos < 0) {
        return Nothing();
      }
      mPos = static_cast<uint32_t>(nextPos);
    }

    if (aPos < mPos) {
      return Some(mPos);
    }

    return Next();
  }
  return SegmentIteratorUtf16::Seek(aPos);
}

WordBreakIteratorUtf16::WordBreakIteratorUtf16(Span<const char16_t> aText)
    : SegmentIteratorUtf16(aText) {
  if (!StaticPrefs::intl_icu4x_segmenter_enabled()) {
    return;
  }
  mSegmenter = capi::icu4x_WordSegmenter_create_auto_mv1();
  mIterator = capi::icu4x_WordSegmenter_segment_utf16_mv1(
      mSegmenter, {mText.Elements(), mText.Length()});
}

WordBreakIteratorUtf16::~WordBreakIteratorUtf16() {
  if (mIterator) {
    capi::icu4x_WordBreakIteratorUtf16_destroy_mv1(mIterator);
  }
  if (mSegmenter) {
    capi::icu4x_WordSegmenter_destroy_mv1(mSegmenter);
  }
}

void WordBreakIteratorUtf16::Reset(Span<const char16_t> aText) {
  mPos = 0;
  mText = aText;
  if (mIterator) {
    capi::icu4x_WordBreakIteratorUtf16_destroy_mv1(mIterator);
    mIterator = nullptr;
  }
  if (!StaticPrefs::intl_icu4x_segmenter_enabled()) {
    return;
  }
  mIterator = capi::icu4x_WordSegmenter_segment_utf16_mv1(
      mSegmenter, {mText.Elements(), mText.Length()});
}

Maybe<uint32_t> WordBreakIteratorUtf16::Next() {
  if (mIterator) {
    const int32_t nextPos =
        capi::icu4x_WordBreakIteratorUtf16_next_mv1(mIterator);
    if (nextPos < 0) {
      return Nothing();
    }
    if (!nextPos) {
      return Next();
    }
    mPos = nextPos;
    return Some(mPos);
  }
  const int32_t nextPos =
      WordBreaker::Next(mText.Elements(), mText.Length(), mPos);
  if (nextPos == NS_WORDBREAKER_NEED_MORE_TEXT) {
    return Nothing();
  }
  mPos = nextPos;
  return Some(mPos);
}

Maybe<uint32_t> WordBreakIteratorUtf16::Seek(uint32_t aPos) {
  if (mIterator) {
    if (mPos >= aPos) {
      return Next();
    }

    while (mPos < aPos) {
      const int32_t nextPos =
          capi::icu4x_WordBreakIteratorUtf16_next_mv1(mIterator);
      if (nextPos < 0) {
        return Nothing();
      }
      mPos = static_cast<uint32_t>(nextPos);
    }

    if (aPos < mPos) {
      return Some(mPos);
    }

    return Next();
  }
  return SegmentIteratorUtf16::Seek(aPos);
}

capi::GraphemeClusterSegmenter* GraphemeClusterBreakIteratorUtf16::sSegmenter =
    nullptr;

GraphemeClusterBreakIteratorUtf16::GraphemeClusterBreakIteratorUtf16(
    Span<const char16_t> aText)
    : SegmentIteratorUtf16(aText) {
  if (!StaticPrefs::intl_icu4x_segmenter_enabled()) {
    return;
  }
  static std::once_flag sOnce;

  std::call_once(sOnce, [] {
    auto result = capi::icu4x_GraphemeClusterSegmenter_create_mv1();
    sSegmenter = result;

    NS_DispatchToMainThread(
        NS_NewRunnableFunction("GraphemeClusterBreakIteratorUtf16", [] {
          RunOnShutdown([] {
            capi::icu4x_GraphemeClusterSegmenter_destroy_mv1(sSegmenter);
            sSegmenter = nullptr;
          });
        }));
  });

  MOZ_RELEASE_ASSERT(sSegmenter);
  mIterator = capi::icu4x_GraphemeClusterSegmenter_segment_utf16_mv1(
      sSegmenter, {mText.Elements(), mText.Length()});
}

GraphemeClusterBreakIteratorUtf16::~GraphemeClusterBreakIteratorUtf16() {
  if (mIterator) {
    capi::icu4x_GraphemeClusterBreakIteratorUtf16_destroy_mv1(mIterator);
  }
}

enum HSType {
  HST_NONE = U_HST_NOT_APPLICABLE,
  HST_L = U_HST_LEADING_JAMO,
  HST_V = U_HST_VOWEL_JAMO,
  HST_T = U_HST_TRAILING_JAMO,
  HST_LV = U_HST_LV_SYLLABLE,
  HST_LVT = U_HST_LVT_SYLLABLE
};

static HSType GetHangulSyllableType(uint32_t aCh) {
  return HSType(UnicodeProperties::GetIntPropertyValue(
      aCh, UnicodeProperties::IntProperty::HangulSyllableType));
}

Maybe<uint32_t> GraphemeClusterBreakIteratorUtf16::Next() {
  const auto len = mText.Length();
  if (mIterator) {
    const int32_t nextPos =
        capi::icu4x_GraphemeClusterBreakIteratorUtf16_next_mv1(mIterator);
    if (nextPos < 0) {
      return Nothing();
    }
    if (!nextPos) {
      return Next();
    }
    mPos = nextPos;
    return Some(mPos);
  }
  if (mPos >= len) {
    // The iterator has already reached the end.
    return Nothing();
  }

  uint32_t ch = mText[mPos++];

  if (mPos < len && NS_IS_SURROGATE_PAIR(ch, mText[mPos])) {
    ch = SURROGATE_TO_UCS4(ch, mText[mPos++]);
  } else if ((ch & ~0xff) == 0x1100 || (ch >= 0xa960 && ch <= 0xa97f) ||
             (ch >= 0xac00 && ch <= 0xd7ff)) {
    // Handle conjoining Jamo that make Hangul syllables
    HSType hangulState = GetHangulSyllableType(ch);
    while (mPos < len) {
      ch = mText[mPos];
      HSType hangulType = GetHangulSyllableType(ch);
      switch (hangulType) {
        case HST_L:
        case HST_LV:
        case HST_LVT:
          if (hangulState == HST_L) {
            hangulState = hangulType;
            mPos++;
            continue;
          }
          break;
        case HST_V:
          if ((hangulState != HST_NONE) && (hangulState != HST_T) &&
              (hangulState != HST_LVT)) {
            hangulState = hangulType;
            mPos++;
            continue;
          }
          break;
        case HST_T:
          if (hangulState != HST_NONE && hangulState != HST_L) {
            hangulState = hangulType;
            mPos++;
            continue;
          }
          break;
        default:
          break;
      }
      break;
    }
  }

  const uint32_t kVS16 = 0xfe0f;
  const uint32_t kZWJ = 0x200d;
  // UTF-16 surrogate values for Fitzpatrick type modifiers
  const uint32_t kFitzpatrickHigh = 0xD83C;
  const uint32_t kFitzpatrickLowFirst = 0xDFFB;
  const uint32_t kFitzpatrickLowLast = 0xDFFF;

  // Checking the emoji-presentation property of the base character is a bit
  // expensive, so we do it lazily.
  enum class EmojiStatus : uint8_t {
    No,
    Yes,
    Unknown,
  } baseIsEmojiStatus = EmojiStatus::Unknown;

  // Remember the base character and the position of the next, in case we need
  // to evaluate its emoji status.
  uint32_t baseCh = ch;
  uint32_t afterBase = mPos;

  auto isFitzpatrickModifierAt = [&](uint32_t aPos) -> bool {
    return aPos + 1 < len && mText[aPos] == kFitzpatrickHigh &&
           mText[aPos + 1] >= kFitzpatrickLowFirst &&
           mText[aPos + 1] <= kFitzpatrickLowLast;
  };

  auto baseIsEmoji = [&]() -> bool {
    if (baseIsEmojiStatus == EmojiStatus::Unknown) {
      auto basePresentation = GetEmojiPresentation(baseCh);
      baseIsEmojiStatus =
          basePresentation == EmojiDefault ||
                  (basePresentation == TextDefault &&
                   ((afterBase < len && mText[afterBase] == kVS16) ||
                    isFitzpatrickModifierAt(afterBase)))
              ? EmojiStatus::Yes
              : EmojiStatus::No;
    }
    return baseIsEmojiStatus == EmojiStatus::Yes;
  };

  bool prevWasZwj = false;

  while (mPos < len) {
    ch = mText[mPos];
    size_t chLen = 1;

    // Check for surrogate pairs; note that isolated surrogates will just
    // be treated as generic (non-cluster-extending) characters here,
    // which is fine for cluster-iterating purposes
    if (mPos < len - 1 && NS_IS_SURROGATE_PAIR(ch, mText[mPos + 1])) {
      ch = SURROGATE_TO_UCS4(ch, mText[mPos + 1]);
      chLen = 2;
    }

    bool extendCluster =
        IsClusterExtender(ch) ||
        (prevWasZwj && baseIsEmoji() &&
         ((GetEmojiPresentation(ch) == EmojiDefault) ||
          (GetEmojiPresentation(ch) == TextDefault && mPos + chLen < len &&
           mText[mPos + chLen] == kVS16)));
    if (!extendCluster) {
      break;
    }

    prevWasZwj = (ch == kZWJ);
    mPos += chLen;
  }

  MOZ_ASSERT(mPos <= len, "Next() has overshot the string!");
  return Some(mPos);
}

Maybe<uint32_t> GraphemeClusterBreakIteratorUtf16::Seek(uint32_t aPos) {
  if (mIterator) {
    if (mPos >= aPos) {
      return Next();
    }

    while (mPos < aPos) {
      const int32_t nextPos =
          capi::icu4x_GraphemeClusterBreakIteratorUtf16_next_mv1(mIterator);
      if (nextPos < 0) {
        return Nothing();
      }
      mPos = static_cast<uint32_t>(nextPos);
    }

    if (aPos < mPos) {
      return Some(mPos);
    }

    return Next();
  }
  return SegmentIteratorUtf16::Seek(aPos);
}

GraphemeClusterBreakReverseIteratorUtf16::
    GraphemeClusterBreakReverseIteratorUtf16(Span<const char16_t> aText)
    : SegmentIteratorUtf16(aText) {
  mPos = mText.Length();
}

Maybe<uint32_t> GraphemeClusterBreakReverseIteratorUtf16::Next() {
  if (mPos == 0) {
    return Nothing();
  }

  uint32_t ch;
  do {
    ch = mText[--mPos];

    if (mPos > 0 && NS_IS_SURROGATE_PAIR(mText[mPos - 1], ch)) {
      ch = SURROGATE_TO_UCS4(mText[--mPos], ch);
    }

    if (!IsClusterExtender(ch)) {
      break;
    }
  } while (mPos > 0);

  // XXX May need to handle conjoining Jamo

  return Some(mPos);
}

Maybe<uint32_t> GraphemeClusterBreakReverseIteratorUtf16::Seek(uint32_t aPos) {
  if (mPos > aPos) {
    mPos = aPos;
  }
  return Next();
}

SentenceBreakIteratorUtf16::SentenceBreakIteratorUtf16(
    Span<const char16_t> aText)
    : SegmentIteratorUtf16(aText) {
  mSegmenter = capi::icu4x_SentenceSegmenter_create_mv1();
  mIterator = capi::icu4x_SentenceSegmenter_segment_utf16_mv1(
      mSegmenter, {mText.Elements(), mText.Length()});
}

SentenceBreakIteratorUtf16::~SentenceBreakIteratorUtf16() {
  if (mIterator) {
    capi::icu4x_SentenceBreakIteratorUtf16_destroy_mv1(mIterator);
  }
  if (mSegmenter) {
    capi::icu4x_SentenceSegmenter_destroy_mv1(mSegmenter);
  }
}

Maybe<uint32_t> SentenceBreakIteratorUtf16::Seek(uint32_t aPos) {
  if (!mIterator) {
    return Nothing();
  }

  if (mPos >= aPos) {
    return Next();
  }

  while (mPos < aPos) {
    const int32_t nextPos =
        capi::icu4x_SentenceBreakIteratorUtf16_next_mv1(mIterator);
    if (nextPos < 0) {
      return Nothing();
    }
    mPos = static_cast<uint32_t>(nextPos);
  }

  if (aPos < mPos) {
    return Some(mPos);
  }

  return Next();
}

Maybe<uint32_t> SentenceBreakIteratorUtf16::Next() {
  if (!mIterator) {
    return Nothing();
  }

  const int32_t nextPos =
      capi::icu4x_SentenceBreakIteratorUtf16_next_mv1(mIterator);
  if (nextPos < 0) {
    return Nothing();
  }
  if (!nextPos) {
    return Next();
  }
  mPos = nextPos;
  return Some(mPos);
}

Result<UniquePtr<Segmenter>, ICUError> Segmenter::TryCreate(
    Span<const char> aLocale, const SegmenterOptions& aOptions) {
  return MakeUnique<Segmenter>(aLocale, aOptions);
}

UniquePtr<SegmentIteratorUtf16> Segmenter::Segment(
    Span<const char16_t> aText) const {
  switch (mOptions.mGranularity) {
    case SegmenterGranularity::Grapheme:
      return MakeUnique<GraphemeClusterBreakIteratorUtf16>(aText);
    case SegmenterGranularity::Sentence:
      if (StaticPrefs::intl_icu4x_segmenter_enabled()) {
        return MakeUnique<SentenceBreakIteratorUtf16>(aText);
      }
      MOZ_ASSERT_UNREACHABLE("Unimplemented yet!");
      return nullptr;
    case SegmenterGranularity::Word:
      return MakeUnique<WordBreakIteratorUtf16>(aText);
    case SegmenterGranularity::Line:
      return MakeUnique<LineBreakIteratorUtf16>(aText);
  }
  MOZ_ASSERT_UNREACHABLE("All granularities must be handled!");
  return nullptr;
}

}  // namespace mozilla::intl
