/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Computed types for text properties.

#[cfg(feature = "servo")]
use crate::properties::StyleBuilder;
use crate::values::computed::length::LengthPercentage;
use crate::values::generics::text::{
    GenericInitialLetter, GenericTextDecorationLength, GenericTextIndent,
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

/// A struct that represents the _used_ value of the text-decoration property.
///
/// FIXME(emilio): This is done at style resolution time, though probably should
/// be done at layout time, otherwise we need to account for display: contents
/// and similar stuff when we implement it.
///
/// FIXME(emilio): Also, should be just a bitfield instead of three bytes.
#[derive(Clone, Copy, Debug, Default, MallocSizeOf, PartialEq, ToResolvedValue)]
pub struct TextDecorationsInEffect {
    /// Whether an underline is in effect.
    pub underline: bool,
    /// Whether an overline decoration is in effect.
    pub overline: bool,
    /// Whether a line-through style is in effect.
    pub line_through: bool,
}

impl TextDecorationsInEffect {
    /// Computes the text-decorations in effect for a given style.
    #[cfg(feature = "servo")]
    pub fn from_style(style: &StyleBuilder) -> Self {
        // Start with no declarations if this is an atomic inline-level box;
        // otherwise, start with the declarations in effect and add in the text
        // decorations that this block specifies.
        let mut result = if style.get_box().clone_display().is_atomic_inline_level() {
            Self::default()
        } else {
            style
                .get_parent_inherited_text()
                .text_decorations_in_effect
                .clone()
        };

        let line = style.get_text().clone_text_decoration_line();

        result.underline |= line.contains(TextDecorationLine::UNDERLINE);
        result.overline |= line.contains(TextDecorationLine::OVERLINE);
        result.line_through |= line.contains(TextDecorationLine::LINE_THROUGH);

        result
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
