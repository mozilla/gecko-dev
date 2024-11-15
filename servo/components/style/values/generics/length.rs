/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Generic types for CSS values related to length.

use crate::parser::{Parse, ParserContext};
use crate::values::generics::Optional;
use crate::values::DashedIdent;
#[cfg(feature = "gecko")]
use crate::Zero;
use cssparser::Parser;
use std::fmt::Write;
use style_traits::ParseError;
use style_traits::StyleParseErrorKind;
use style_traits::ToCss;
use style_traits::{CssWriter, SpecifiedValueInfo};

/// A `<length-percentage> | auto` value.
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    MallocSizeOf,
    PartialEq,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[repr(C, u8)]
pub enum GenericLengthPercentageOrAuto<LengthPercent> {
    LengthPercentage(LengthPercent),
    Auto,
}

pub use self::GenericLengthPercentageOrAuto as LengthPercentageOrAuto;

impl<LengthPercentage> LengthPercentageOrAuto<LengthPercentage> {
    /// `auto` value.
    #[inline]
    pub fn auto() -> Self {
        LengthPercentageOrAuto::Auto
    }

    /// Whether this is the `auto` value.
    #[inline]
    pub fn is_auto(&self) -> bool {
        matches!(*self, LengthPercentageOrAuto::Auto)
    }

    /// A helper function to parse this with quirks or not and so forth.
    pub fn parse_with<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
        parser: impl FnOnce(
            &ParserContext,
            &mut Parser<'i, 't>,
        ) -> Result<LengthPercentage, ParseError<'i>>,
    ) -> Result<Self, ParseError<'i>> {
        if input.try_parse(|i| i.expect_ident_matching("auto")).is_ok() {
            return Ok(LengthPercentageOrAuto::Auto);
        }

        Ok(LengthPercentageOrAuto::LengthPercentage(parser(
            context, input,
        )?))
    }
}

impl<LengthPercentage> LengthPercentageOrAuto<LengthPercentage>
where
    LengthPercentage: Clone,
{
    /// Resolves `auto` values by calling `f`.
    #[inline]
    pub fn auto_is(&self, f: impl FnOnce() -> LengthPercentage) -> LengthPercentage {
        match self {
            LengthPercentageOrAuto::LengthPercentage(length) => length.clone(),
            LengthPercentageOrAuto::Auto => f(),
        }
    }

    /// Returns the non-`auto` value, if any.
    #[inline]
    pub fn non_auto(&self) -> Option<LengthPercentage> {
        match self {
            LengthPercentageOrAuto::LengthPercentage(length) => Some(length.clone()),
            LengthPercentageOrAuto::Auto => None,
        }
    }

    /// Maps the length of this value.
    pub fn map<T>(&self, f: impl FnOnce(LengthPercentage) -> T) -> LengthPercentageOrAuto<T> {
        match self {
            LengthPercentageOrAuto::LengthPercentage(l) => {
                LengthPercentageOrAuto::LengthPercentage(f(l.clone()))
            },
            LengthPercentageOrAuto::Auto => LengthPercentageOrAuto::Auto,
        }
    }
}

impl<LengthPercentage: Zero> Zero for LengthPercentageOrAuto<LengthPercentage> {
    fn zero() -> Self {
        LengthPercentageOrAuto::LengthPercentage(Zero::zero())
    }

    fn is_zero(&self) -> bool {
        match *self {
            LengthPercentageOrAuto::LengthPercentage(ref l) => l.is_zero(),
            LengthPercentageOrAuto::Auto => false,
        }
    }
}

impl<LengthPercentage: Parse> Parse for LengthPercentageOrAuto<LengthPercentage> {
    fn parse<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self, ParseError<'i>> {
        Self::parse_with(context, input, LengthPercentage::parse)
    }
}

/// A generic value for the `width`, `height`, `min-width`, or `min-height` property.
///
/// Unlike `max-width` or `max-height` properties, a Size can be `auto`,
/// and cannot be `none`.
///
/// Note that it only accepts non-negative values.
#[allow(missing_docs)]
#[derive(
    Animate,
    ComputeSquaredDistance,
    Clone,
    Debug,
    MallocSizeOf,
    PartialEq,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericSize<LengthPercent> {
    LengthPercentage(LengthPercent),
    Auto,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    MaxContent,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    MinContent,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    FitContent,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    MozAvailable,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    WebkitFillAvailable,
    #[animation(error)]
    Stretch,
    #[animation(error)]
    #[css(function = "fit-content")]
    FitContentFunction(LengthPercent),
    AnchorSizeFunction(
        #[animation(field_bound)]
        #[distance(field_bound)]
        Box<GenericAnchorSizeFunction<LengthPercent>>
    ),
}

impl<LengthPercent> SpecifiedValueInfo for GenericSize<LengthPercent>
where
LengthPercent: SpecifiedValueInfo
{
    fn collect_completion_keywords(f: style_traits::KeywordsCollectFn) {
        LengthPercent::collect_completion_keywords(f);
        f(&["auto", "stretch", "fit-content"]);
        if cfg!(feature = "gecko") {
            f(&["max-content", "min-content", "-moz-available", "-webkit-fill-available"]);
        }
        if static_prefs::pref!("layout.css.anchor-positioning.enabled") {
            f(&["anchor-size"]);
        }
    }
}

pub use self::GenericSize as Size;

impl<LengthPercentage> Size<LengthPercentage> {
    /// `auto` value.
    #[inline]
    pub fn auto() -> Self {
        Size::Auto
    }

    /// Returns whether we're the auto value.
    #[inline]
    pub fn is_auto(&self) -> bool {
        matches!(*self, Size::Auto)
    }
}

/// A generic value for the `max-width` or `max-height` property.
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    MallocSizeOf,
    PartialEq,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericMaxSize<LengthPercent> {
    LengthPercentage(LengthPercent),
    None,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    MaxContent,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    MinContent,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    FitContent,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    MozAvailable,
    #[cfg(feature = "gecko")]
    #[animation(error)]
    WebkitFillAvailable,
    #[animation(error)]
    Stretch,
    #[animation(error)]
    #[css(function = "fit-content")]
    FitContentFunction(LengthPercent),
    AnchorSizeFunction(
        #[animation(field_bound)]
        #[distance(field_bound)]
        Box<GenericAnchorSizeFunction<LengthPercent>>
    ),
}

impl<LP> SpecifiedValueInfo for GenericMaxSize<LP>
where
    LP: SpecifiedValueInfo
{
    fn collect_completion_keywords(f: style_traits::KeywordsCollectFn) {
        LP::collect_completion_keywords(f);
        f(&["none", "stretch", "fit-content"]);
        if cfg!(feature = "gecko") {
            f(&["max-content", "min-content", "-moz-available", "-webkit-fill-available"]);
        }
        if static_prefs::pref!("layout.css.anchor-positioning.enabled") {
            f(&["anchor-size"]);
        }
    }
}

pub use self::GenericMaxSize as MaxSize;

impl<LengthPercentage> MaxSize<LengthPercentage> {
    /// `none` value.
    #[inline]
    pub fn none() -> Self {
        MaxSize::None
    }
}

/// A generic `<length>` | `<number>` value for the `tab-size` property.
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    MallocSizeOf,
    Parse,
    PartialEq,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericLengthOrNumber<L, N> {
    /// A number.
    ///
    /// NOTE: Numbers need to be before lengths, in order to parse them
    /// first, since `0` should be a number, not the `0px` length.
    Number(N),
    /// A length.
    Length(L),
}

pub use self::GenericLengthOrNumber as LengthOrNumber;

impl<L, N: Zero> Zero for LengthOrNumber<L, N> {
    fn zero() -> Self {
        LengthOrNumber::Number(Zero::zero())
    }

    fn is_zero(&self) -> bool {
        match *self {
            LengthOrNumber::Number(ref n) => n.is_zero(),
            LengthOrNumber::Length(..) => false,
        }
    }
}

/// A generic `<length-percentage>` | normal` value.
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    MallocSizeOf,
    Parse,
    PartialEq,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
#[allow(missing_docs)]
pub enum GenericLengthPercentageOrNormal<LengthPercent> {
    LengthPercentage(LengthPercent),
    Normal,
}

pub use self::GenericLengthPercentageOrNormal as LengthPercentageOrNormal;

impl<LengthPercent> LengthPercentageOrNormal<LengthPercent> {
    /// Returns the normal value.
    #[inline]
    pub fn normal() -> Self {
        LengthPercentageOrNormal::Normal
    }
}

/// Anchor size function used by sizing, margin and inset properties.
/// This resolves to the size of the anchor at computed time.
///
/// https://drafts.csswg.org/css-anchor-position-1/#funcdef-anchor-size
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    MallocSizeOf,
    PartialEq,
    SpecifiedValueInfo,
    ToShmem,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToResolvedValue,
    Serialize,
    Deserialize,
)]
#[repr(C)]
pub struct GenericAnchorSizeFunction<LengthPercentage> {
    /// Anchor name of the element to anchor to.
    /// If omitted (i.e. empty), selects the implicit anchor element.
    #[animation(constant)]
    pub target_element: DashedIdent,
    /// Size of the positioned element, expressed in that of the anchor element.
    /// If omitted, defaults to the axis of the property the function is used in.
    pub size: AnchorSizeKeyword,
    /// Value to use in case the anchor function is invalid.
    pub fallback: Optional<LengthPercentage>,
}

impl<LengthPercentage> ToCss for GenericAnchorSizeFunction<LengthPercentage>
where
    LengthPercentage: ToCss,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> std::fmt::Result
    where
        W: Write,
    {
        dest.write_str("anchor-size(")?;
        let mut previous_entry_printed = false;
        if !self.target_element.is_empty() {
            previous_entry_printed = true;
            self.target_element.to_css(dest)?;
        }
        if self.size != AnchorSizeKeyword::None {
            if previous_entry_printed {
                dest.write_str(" ")?;
            }
            previous_entry_printed = true;
            self.size.to_css(dest)?;
        }
        if let Some(f) = self.fallback.as_ref() {
            if previous_entry_printed {
                dest.write_str(", ")?;
            }
            f.to_css(dest)?;
        }
        dest.write_str(")")
    }
}

impl<LengthPercentage> Parse for GenericAnchorSizeFunction<LengthPercentage>
where
    LengthPercentage: Parse,
{
    fn parse<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self, ParseError<'i>> {
        if !static_prefs::pref!("layout.css.anchor-positioning.enabled") {
            return Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError));
        }
        input.expect_function_matching("anchor-size")?;
        Self::parse_inner(
            context,
            input,
            |i| LengthPercentage::parse(context, i)
        )
    }
}

impl<LengthPercentage> GenericAnchorSizeFunction<LengthPercentage>
{
    /// Parse the inner part of `anchor-size()`, after the parser has consumed "anchor-size(".
    pub fn parse_inner<'i, 't, F>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
        f: F,
    ) -> Result<Self, ParseError<'i>>
    where
        F: FnOnce(&mut Parser<'i, '_>) -> Result<LengthPercentage, ParseError<'i>>,
    {
        input.parse_nested_block(|i| {
            let mut target_element = i
                .try_parse(|i| DashedIdent::parse(context, i))
                .unwrap_or(DashedIdent::empty());
            let size = i.try_parse(AnchorSizeKeyword::parse).unwrap_or(AnchorSizeKeyword::None);
            if target_element.is_empty() {
                target_element = i
                    .try_parse(|i| DashedIdent::parse(context, i))
                    .unwrap_or(DashedIdent::empty());
            }
            let previous_parsed = !target_element.is_empty() || size != AnchorSizeKeyword::None;
            let fallback = i
                .try_parse(|i| {
                    if previous_parsed {
                        i.expect_comma()?;
                    }
                    f(i)
                })
                .ok();
            Ok(GenericAnchorSizeFunction {
                target_element,
                size: size.into(),
                fallback: fallback.into(),
            })
        })
    }
}

/// Keyword values for the anchor size function.
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    MallocSizeOf,
    PartialEq,
    Parse,
    SpecifiedValueInfo,
    ToCss,
    ToShmem,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToResolvedValue,
    Serialize,
    Deserialize,
)]
#[repr(u8)]
pub enum AnchorSizeKeyword {
    /// Magic value for nothing.
    #[css(skip)]
    None,
    /// Width of the anchor element.
    Width,
    /// Height of the anchor element.
    Height,
    /// Block size of the anchor element.
    Block,
    /// Inline size of the anchor element.
    Inline,
    /// Same as `Block`, resolved against the positioned element's writing mode.
    SelfBlock,
    /// Same as `Inline`, resolved against the positioned element's writing mode.
    SelfInline,
}

/// Specified type for `margin` properties, which allows
/// the use of the `anchor-size()` function.
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    MallocSizeOf,
    PartialEq,
    ToCss,
    ToShmem,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToResolvedValue,
)]
#[repr(C)]
pub enum GenericMargin<LP> {
    /// A `<length-percentage>` value.
    LengthPercentage(LP),
    /// An `auto` value.
    Auto,
    /// Margin size defined by the anchor element.
    ///
    /// https://drafts.csswg.org/css-anchor-position-1/#funcdef-anchor-size
    AnchorSizeFunction(
        #[animation(field_bound)]
        #[distance(field_bound)]
        Box<GenericAnchorSizeFunction<LP>>,
    ),
}

impl<LP> SpecifiedValueInfo for GenericMargin<LP>
where
    LP: SpecifiedValueInfo,
{
    fn collect_completion_keywords(f: style_traits::KeywordsCollectFn) {
        LP::collect_completion_keywords(f);
        f(&["auto"]);
        if static_prefs::pref!("layout.css.anchor-positioning.enabled") {
            f(&["anchor-size"]);
        }
    }
}

impl<LP> Zero for GenericMargin<LP>
where
    LP: Zero,
{
    fn is_zero(&self) -> bool {
        match self {
            Self::LengthPercentage(l) => l.is_zero(),
            Self::Auto | Self::AnchorSizeFunction(_) => false,
        }
    }

    fn zero() -> Self {
        Self::LengthPercentage(LP::zero())
    }
}
