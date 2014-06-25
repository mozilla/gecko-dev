/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextRage_h_
#define mozilla_TextRage_h_

#include <stdint.h>

#include "nsAutoPtr.h"
#include "nsColor.h"
#include "nsStyleConsts.h"
#include "nsTArray.h"

namespace mozilla {

/******************************************************************************
 * mozilla::TextRangeStyle
 ******************************************************************************/

struct TextRangeStyle
{
  enum
  {
    LINESTYLE_NONE   = NS_STYLE_TEXT_DECORATION_STYLE_NONE,
    LINESTYLE_SOLID  = NS_STYLE_TEXT_DECORATION_STYLE_SOLID,
    LINESTYLE_DOTTED = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED,
    LINESTYLE_DASHED = NS_STYLE_TEXT_DECORATION_STYLE_DASHED,
    LINESTYLE_DOUBLE = NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE,
    LINESTYLE_WAVY   = NS_STYLE_TEXT_DECORATION_STYLE_WAVY
  };

  enum
  {
    DEFINED_NONE             = 0x00,
    DEFINED_LINESTYLE        = 0x01,
    DEFINED_FOREGROUND_COLOR = 0x02,
    DEFINED_BACKGROUND_COLOR = 0x04,
    DEFINED_UNDERLINE_COLOR  = 0x08
  };

  // Initialize all members, because TextRange instances may be compared by
  // memcomp.
  TextRangeStyle()
  {
    Clear();
  }

  void Clear()
  {
    mDefinedStyles = DEFINED_NONE;
    mLineStyle = LINESTYLE_NONE;
    mIsBoldLine = false;
    mForegroundColor = mBackgroundColor = mUnderlineColor = NS_RGBA(0, 0, 0, 0);
  }

  bool IsDefined() const { return mDefinedStyles != DEFINED_NONE; }

  bool IsLineStyleDefined() const
  {
    return (mDefinedStyles & DEFINED_LINESTYLE) != 0;
  }

  bool IsForegroundColorDefined() const
  {
    return (mDefinedStyles & DEFINED_FOREGROUND_COLOR) != 0;
  }

  bool IsBackgroundColorDefined() const
  {
    return (mDefinedStyles & DEFINED_BACKGROUND_COLOR) != 0;
  }

  bool IsUnderlineColorDefined() const
  {
    return (mDefinedStyles & DEFINED_UNDERLINE_COLOR) != 0;
  }

  bool IsNoChangeStyle() const
  {
    return !IsForegroundColorDefined() && !IsBackgroundColorDefined() &&
           IsLineStyleDefined() && mLineStyle == LINESTYLE_NONE;
  }

  bool Equals(const TextRangeStyle& aOther)
  {
    if (mDefinedStyles != aOther.mDefinedStyles)
      return false;
    if (IsLineStyleDefined() && (mLineStyle != aOther.mLineStyle ||
                                 !mIsBoldLine != !aOther.mIsBoldLine))
      return false;
    if (IsForegroundColorDefined() &&
        (mForegroundColor != aOther.mForegroundColor))
      return false;
    if (IsBackgroundColorDefined() &&
        (mBackgroundColor != aOther.mBackgroundColor))
      return false;
    if (IsUnderlineColorDefined() &&
        (mUnderlineColor != aOther.mUnderlineColor))
      return false;
    return true;
  }

  bool operator !=(const TextRangeStyle &aOther)
  {
    return !Equals(aOther);
  }

  bool operator ==(const TextRangeStyle &aOther)
  {
    return Equals(aOther);
  }

  uint8_t mDefinedStyles;
  uint8_t mLineStyle;        // DEFINED_LINESTYLE

  bool mIsBoldLine;  // DEFINED_LINESTYLE

  nscolor mForegroundColor;  // DEFINED_FOREGROUND_COLOR
  nscolor mBackgroundColor;  // DEFINED_BACKGROUND_COLOR
  nscolor mUnderlineColor;   // DEFINED_UNDERLINE_COLOR
};

/******************************************************************************
 * mozilla::TextRange
 ******************************************************************************/

#define NS_TEXTRANGE_CARETPOSITION         0x01
#define NS_TEXTRANGE_RAWINPUT              0x02
#define NS_TEXTRANGE_SELECTEDRAWTEXT       0x03
#define NS_TEXTRANGE_CONVERTEDTEXT         0x04
#define NS_TEXTRANGE_SELECTEDCONVERTEDTEXT 0x05

struct TextRange
{
  TextRange() :
    mStartOffset(0), mEndOffset(0), mRangeType(0)
  {
  }

  uint32_t mStartOffset;
  // XXX Storing end offset makes the initializing code very complicated.
  //     We should replace it with mLength.
  uint32_t mEndOffset;
  uint32_t mRangeType;

  TextRangeStyle mRangeStyle;

  uint32_t Length() const { return mEndOffset - mStartOffset; }

  bool IsClause() const
  {
    MOZ_ASSERT(mRangeType >= NS_TEXTRANGE_CARETPOSITION &&
                 mRangeType <= NS_TEXTRANGE_SELECTEDCONVERTEDTEXT,
               "Invalid range type");
    return mRangeType != NS_TEXTRANGE_CARETPOSITION;
  }
};

/******************************************************************************
 * mozilla::TextRangeArray
 ******************************************************************************/
class TextRangeArray MOZ_FINAL : public nsAutoTArray<TextRange, 10>
{
  ~TextRangeArray() {}

  NS_INLINE_DECL_REFCOUNTING(TextRangeArray)

public:
  bool IsComposing() const
  {
    for (uint32_t i = 0; i < Length(); ++i) {
      if (ElementAt(i).IsClause()) {
        return true;
      }
    }
    return false;
  }

  // Returns target clase offset.  If there are selected clauses, this returns
  // the first selected clause offset.  Otherwise, 0.
  uint32_t TargetClauseOffset() const
  {
    for (uint32_t i = 0; i < Length(); ++i) {
      const TextRange& range = ElementAt(i);
      if (range.mRangeType == NS_TEXTRANGE_SELECTEDRAWTEXT ||
          range.mRangeType == NS_TEXTRANGE_SELECTEDCONVERTEDTEXT) {
        return range.mStartOffset;
      }
    }
    return 0;
  }
};

} // namespace mozilla

#endif // mozilla_TextRage_h_
