/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Computed types for text properties.

use crate::values::computed::length::LengthPercentage;
use crate::values::generics::NumberOrAuto;
use crate::values::generics::text::{
    GenericHyphenateLimitChars, GenericInitialLetter, GenericTextDecorationLength, GenericTextIndent,
};
use crate::values::specified::text as specified;
use crate::values::specified::text::{TextEmphasisFillMode, TextEmphasisShapeKeyword};
use crate::values::{CSSFloat, CSSInteger};
use crate::Zero;
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

pub use crate::values::specified::text::{
    HyphenateCharacter, LineBreak, MozControlCharacterVisibility, OverflowWrap, RubyPosition,
    TextAlignLast, TextDecorationLine, TextDecorationSkipInk, TextEmphasisPosition, TextJustify,
    TextOverflow, TextTransform, TextUnderlinePosition, WordBreak,
};

/// A computed value for the `initial-letter` property.
pub type InitialLetter = GenericInitialLetter<CSSFloat, CSSInteger>;

/// Implements type for `text-decoration-thickness` property.
pub type TextDecorationLength = GenericTextDecorationLength<LengthPercentage>;

/// The computed value of `text-align`.
pub type TextAlign = specified::TextAlignKeyword;

/// The computed value of `text-indent`.
pub type TextIndent = GenericTextIndent<LengthPercentage>;

/// A computed value for the `hyphenate-character` property.
pub type HyphenateLimitChars = GenericHyphenateLimitChars<CSSInteger>;

impl HyphenateLimitChars {
    /// Return the `auto` value, which has all three component values as `auto`.
    #[inline]
    pub fn auto() -> Self {
        Self {
            total_word_length: NumberOrAuto::Auto,
            pre_hyphen_length: NumberOrAuto::Auto,
            post_hyphen_length: NumberOrAuto::Auto,
        }
    }
}

/// A computed value for the `letter-spacing` property.
#[repr(transparent)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    MallocSizeOf,
    PartialEq,
    ToAnimatedValue,
    ToAnimatedZero,
    ToResolvedValue,
)]
pub struct GenericLetterSpacing<L>(pub L);
/// This is generic just to make the #[derive()] code do the right thing for lengths.
pub type LetterSpacing = GenericLetterSpacing<LengthPercentage>;

impl LetterSpacing {
    /// Return the `normal` computed value, which is just zero.
    #[inline]
    pub fn normal() -> Self {
        Self(LengthPercentage::zero())
    }
}

impl ToCss for LetterSpacing {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        // https://drafts.csswg.org/css-text/#propdef-letter-spacing
        //
        // For legacy reasons, a computed letter-spacing of zero yields a
        // resolved value (getComputedStyle() return value) of normal.
        if self.0.is_zero() {
            return dest.write_str("normal");
        }
        self.0.to_css(dest)
    }
}

/// A computed value for the `word-spacing` property.
pub type WordSpacing = LengthPercentage;

impl WordSpacing {
    /// Return the `normal` computed value, which is just zero.
    #[inline]
    pub fn normal() -> Self {
        LengthPercentage::zero()
    }
}

/// Computed value for the text-emphasis-style property
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToCss, ToResolvedValue)]
#[allow(missing_docs)]
#[repr(C, u8)]
pub enum TextEmphasisStyle {
    /// [ <fill> || <shape> ]
    Keyword {
        #[css(skip_if = "TextEmphasisFillMode::is_filled")]
        fill: TextEmphasisFillMode,
        shape: TextEmphasisShapeKeyword,
    },
    /// `none`
    None,
    /// `<string>` (of which only the first grapheme cluster will be used).
    String(crate::OwnedStr),
}
