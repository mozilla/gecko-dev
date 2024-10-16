/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(missing_docs)]

//! Parsing for CSS colors.

use super::{
    color_function::ColorFunction,
    component::{ColorComponent, ColorComponentType},
    AbsoluteColor, ColorFlags, ColorSpace,
};
use crate::{
    parser::{Parse, ParserContext},
    values::{
        generics::calc::CalcUnits,
        specified::{
            angle::Angle as SpecifiedAngle, calc::Leaf as SpecifiedLeaf,
            color::Color as SpecifiedColor,
        },
    },
};
use cssparser::{
    color::{parse_hash_color, PredefinedColorSpace, OPAQUE},
    match_ignore_ascii_case, CowRcStr, Parser, Token,
};
use style_traits::{ParseError, StyleParseErrorKind};

/// Returns true if the relative color syntax pref is enabled.
#[inline]
pub fn rcs_enabled() -> bool {
    static_prefs::pref!("layout.css.relative-color-syntax.enabled")
}

/// Represents a channel keyword inside a color.
#[derive(Clone, Copy, Debug, MallocSizeOf, Parse, PartialEq, PartialOrd, ToCss, ToShmem)]
pub enum ChannelKeyword {
    /// alpha
    Alpha,
    /// a
    A,
    /// b, blackness, blue
    B,
    /// chroma
    C,
    /// green
    G,
    /// hue
    H,
    /// lightness
    L,
    /// red
    R,
    /// saturation
    S,
    /// whiteness
    W,
    /// x
    X,
    /// y
    Y,
    /// z
    Z,
}

/// Return the named color with the given name.
///
/// Matching is case-insensitive in the ASCII range.
/// CSS escaping (if relevant) should be resolved before calling this function.
/// (For example, the value of an `Ident` token is fine.)
#[inline]
pub fn parse_color_keyword(ident: &str) -> Result<SpecifiedColor, ()> {
    Ok(match_ignore_ascii_case! { ident,
        "transparent" => {
            SpecifiedColor::from_absolute_color(AbsoluteColor::srgb_legacy(0u8, 0u8, 0u8, 0.0))
        },
        "currentcolor" => SpecifiedColor::CurrentColor,
        _ => {
            let (r, g, b) = cssparser::color::parse_named_color(ident)?;
            SpecifiedColor::from_absolute_color(AbsoluteColor::srgb_legacy(r, g, b, OPAQUE))
        },
    })
}

/// Parse a CSS color using the specified [`ColorParser`] and return a new color
/// value on success.
pub fn parse_color_with<'i, 't>(
    context: &ParserContext,
    input: &mut Parser<'i, 't>,
) -> Result<SpecifiedColor, ParseError<'i>> {
    let location = input.current_source_location();
    let token = input.next()?;
    match *token {
        Token::Hash(ref value) | Token::IDHash(ref value) => parse_hash_color(value.as_bytes())
            .map(|(r, g, b, a)| {
                SpecifiedColor::from_absolute_color(AbsoluteColor::srgb_legacy(r, g, b, a))
            }),
        Token::Ident(ref value) => parse_color_keyword(value),
        Token::Function(ref name) => {
            let name = name.clone();
            return input.parse_nested_block(|arguments| {
                let color_function = parse_color_function(context, name, arguments)?;
                // TODO(tlouw): A color function can be valid, but not resolvable. This check
                //              assumes that if we can't resolve it, it's invalid.
                // TODO(tlouw): Specified colors should not be resolved here and stored as is.
                if let Ok(resolved) = color_function.resolve_to_absolute() {
                    Ok(SpecifiedColor::from_absolute_color(resolved))
                } else {
                    // We should store the unresolvable value here in the specifed color.
                    Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError))
                }
            });
        },
        _ => Err(()),
    }
    .map_err(|()| location.new_unexpected_token_error(token.clone()))
}

/// Parse one of the color functions: rgba(), lab(), color(), etc.
#[inline]
fn parse_color_function<'i, 't>(
    context: &ParserContext,
    name: CowRcStr<'i>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = parse_origin_color(context, arguments)?;

    let component_parser = ComponentParser {
        context,
        origin_color,
    };

    let color = match_ignore_ascii_case! { &name,
        "rgb" | "rgba" => parse_rgb(&component_parser, arguments),
        "hsl" | "hsla" => parse_hsl(&component_parser, arguments),
        "hwb" => parse_hwb(&component_parser, arguments),
        "lab" => parse_lab_like(&component_parser, arguments, ColorSpace::Lab, ColorFunction::Lab),
        "lch" => parse_lch_like(&component_parser, arguments, ColorSpace::Lch, ColorFunction::Lch),
        "oklab" => parse_lab_like(&component_parser, arguments, ColorSpace::Oklab, ColorFunction::Oklab),
        "oklch" => parse_lch_like(&component_parser, arguments, ColorSpace::Oklch, ColorFunction::Oklch),
        "color" =>parse_color_with_color_space(&component_parser, arguments),
        _ => return Err(arguments.new_unexpected_token_error(Token::Ident(name))),
    }?;

    arguments.expect_exhausted()?;

    Ok(color)
}

/// Parse the relative color syntax "from" syntax `from <color>`.
fn parse_origin_color<'i, 't>(
    context: &ParserContext,
    arguments: &mut Parser<'i, 't>,
) -> Result<Option<AbsoluteColor>, ParseError<'i>> {
    if !rcs_enabled() {
        return Ok(None);
    }

    // Not finding the from keyword is not an error, it just means we don't
    // have an origin color.
    if arguments
        .try_parse(|p| p.expect_ident_matching("from"))
        .is_err()
    {
        return Ok(None);
    }

    let location = arguments.current_source_location();

    // We still fail if we can't parse the origin color.
    let origin_color = SpecifiedColor::parse(context, arguments)?;

    // Right now we only handle absolute colors.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1890972
    let Some(computed) = origin_color.to_computed_color(None) else {
        return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError));
    };

    Ok(Some(computed.resolve_to_absolute(&AbsoluteColor::BLACK)))
}

#[inline]
fn parse_rgb<'i, 't>(
    component_parser: &ComponentParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let component_parser = ComponentParser {
        context: component_parser.context,
        origin_color: component_parser.origin_color.map(|c| {
            let mut c = c.to_color_space(ColorSpace::Srgb);
            c.flags.insert(ColorFlags::IS_LEGACY_SRGB);
            c
        }),
    };

    let maybe_red = component_parser.parse_number_or_percentage(arguments, true)?;

    // If the first component is not "none" and is followed by a comma, then we
    // are parsing the legacy syntax.  Legacy syntax also doesn't support an
    // origin color.
    let is_legacy_syntax = component_parser.origin_color.is_none() &&
        !maybe_red.is_none() &&
        arguments.try_parse(|p| p.expect_comma()).is_ok();

    Ok(if is_legacy_syntax {
        let (green, blue) = if maybe_red.is_percentage() {
            let green = component_parser.parse_percentage(arguments, false)?;
            arguments.expect_comma()?;
            let blue = component_parser.parse_percentage(arguments, false)?;
            (green, blue)
        } else {
            let green = component_parser.parse_number(arguments, false)?;
            arguments.expect_comma()?;
            let blue = component_parser.parse_number(arguments, false)?;
            (green, blue)
        };

        let alpha = component_parser.parse_legacy_alpha(arguments)?;

        ColorFunction::Rgb(maybe_red, green, blue, alpha)
    } else {
        let green = component_parser.parse_number_or_percentage(arguments, true)?;
        let blue = component_parser.parse_number_or_percentage(arguments, true)?;

        let alpha = component_parser.parse_modern_alpha(arguments)?;

        ColorFunction::Rgb(maybe_red, green, blue, alpha)
    })
}

/// Parses hsl syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hsl-notation>
#[inline]
fn parse_hsl<'i, 't>(
    component_parser: &ComponentParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let component_parser = ComponentParser {
        context: component_parser.context,
        origin_color: component_parser
            .origin_color
            .map(|c| c.to_color_space(ColorSpace::Hsl)),
    };

    let hue = component_parser.parse_number_or_angle(arguments, true)?;

    // If the hue is not "none" and is followed by a comma, then we are parsing
    // the legacy syntax. Legacy syntax also doesn't support an origin color.
    let is_legacy_syntax = component_parser.origin_color.is_none() &&
        !hue.is_none() &&
        arguments.try_parse(|p| p.expect_comma()).is_ok();

    let (saturation, lightness, alpha) = if is_legacy_syntax {
        let saturation = component_parser.parse_percentage(arguments, false)?;
        arguments.expect_comma()?;
        let lightness = component_parser.parse_percentage(arguments, false)?;
        let alpha = component_parser.parse_legacy_alpha(arguments)?;
        (saturation, lightness, alpha)
    } else {
        let saturation = component_parser.parse_number_or_percentage(arguments, true)?;
        let lightness = component_parser.parse_number_or_percentage(arguments, true)?;
        let alpha = component_parser.parse_modern_alpha(arguments)?;
        (saturation, lightness, alpha)
    };

    Ok(ColorFunction::Hsl(
        hue,
        saturation,
        lightness,
        alpha,
        component_parser.origin_color.is_none(),
    ))
}

/// Parses hwb syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hbw-notation>
#[inline]
fn parse_hwb<'i, 't>(
    component_parser: &ComponentParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let component_parser = ComponentParser {
        context: component_parser.context,
        origin_color: component_parser
            .origin_color
            .map(|c| c.to_color_space(ColorSpace::Hwb)),
    };

    let hue = component_parser.parse_number_or_angle(arguments, true)?;
    let whiteness = component_parser.parse_number_or_percentage(arguments, true)?;
    let blackness = component_parser.parse_number_or_percentage(arguments, true)?;

    let alpha = component_parser.parse_modern_alpha(arguments)?;

    Ok(ColorFunction::Hwb(
        hue,
        whiteness,
        blackness,
        alpha,
        component_parser.origin_color.is_none(),
    ))
}

type IntoLabFn<Output> = fn(
    l: ColorComponent<NumberOrPercentage>,
    a: ColorComponent<NumberOrPercentage>,
    b: ColorComponent<NumberOrPercentage>,
    alpha: ColorComponent<NumberOrPercentage>,
) -> Output;

#[inline]
fn parse_lab_like<'i, 't>(
    component_parser: &ComponentParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
    color_space: ColorSpace,
    into_color: IntoLabFn<ColorFunction>,
) -> Result<ColorFunction, ParseError<'i>> {
    let component_parser = ComponentParser {
        context: component_parser.context,
        origin_color: component_parser
            .origin_color
            .map(|c| c.to_color_space(color_space)),
    };

    let lightness = component_parser.parse_number_or_percentage(arguments, true)?;
    let a = component_parser.parse_number_or_percentage(arguments, true)?;
    let b = component_parser.parse_number_or_percentage(arguments, true)?;

    let alpha = component_parser.parse_modern_alpha(arguments)?;

    Ok(into_color(lightness, a, b, alpha))
}

type IntoLchFn<Output> = fn(
    l: ColorComponent<NumberOrPercentage>,
    a: ColorComponent<NumberOrPercentage>,
    b: ColorComponent<NumberOrAngle>,
    alpha: ColorComponent<NumberOrPercentage>,
) -> Output;

#[inline]
fn parse_lch_like<'i, 't>(
    component_parser: &ComponentParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
    color_space: ColorSpace,
    into_color: IntoLchFn<ColorFunction>,
) -> Result<ColorFunction, ParseError<'i>> {
    let component_parser = ComponentParser {
        context: component_parser.context,
        origin_color: component_parser
            .origin_color
            .map(|c| c.to_color_space(color_space)),
    };

    let lightness = component_parser.parse_number_or_percentage(arguments, true)?;
    let chroma = component_parser.parse_number_or_percentage(arguments, true)?;
    let hue = component_parser.parse_number_or_angle(arguments, true)?;

    let alpha = component_parser.parse_modern_alpha(arguments)?;

    Ok(into_color(lightness, chroma, hue, alpha))
}

/// Parse the color() function.
#[inline]
fn parse_color_with_color_space<'i, 't>(
    component_parser: &ComponentParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let color_space = PredefinedColorSpace::parse(arguments)?;
    let component_parser = ComponentParser {
        context: component_parser.context,
        origin_color: component_parser.origin_color.map(|c| {
            // If the origin color was in legacy srgb, converting it won't
            // change it to modern syntax. So make sure it's in modern syntax.
            let mut c = c.to_color_space(ColorSpace::from(color_space));
            c.flags.remove(ColorFlags::IS_LEGACY_SRGB);
            c
        }),
    };

    let c1 = component_parser.parse_number_or_percentage(arguments, true)?;
    let c2 = component_parser.parse_number_or_percentage(arguments, true)?;
    let c3 = component_parser.parse_number_or_percentage(arguments, true)?;

    let alpha = component_parser.parse_modern_alpha(arguments)?;

    Ok(ColorFunction::Color(color_space.into(), c1, c2, c3, alpha))
}

/// Either a number or a percentage.
#[derive(Clone, Copy, Debug, MallocSizeOf, PartialEq, ToShmem)]
pub enum NumberOrPercentage {
    /// `<number>`.
    Number(f32),
    /// `<percentage>`
    /// The value as a float, divided by 100 so that the nominal range is 0.0 to 1.0.
    Percentage(f32),
}

impl NumberOrPercentage {
    /// Return the value as a number. Percentages will be adjusted to the range
    /// [0..percent_basis].
    pub fn to_number(&self, percentage_basis: f32) -> f32 {
        match *self {
            Self::Number(value) => value,
            Self::Percentage(unit_value) => unit_value * percentage_basis,
        }
    }
}

impl ColorComponentType for NumberOrPercentage {
    fn from_value(value: f32) -> Self {
        Self::Number(value)
    }

    fn units() -> CalcUnits {
        CalcUnits::PERCENTAGE
    }

    fn try_from_token(token: &Token) -> Result<Self, ()> {
        Ok(match *token {
            Token::Number { value, .. } => Self::Number(value),
            Token::Percentage { unit_value, .. } => Self::Percentage(unit_value),
            _ => {
                return Err(());
            },
        })
    }

    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()> {
        Ok(match *leaf {
            SpecifiedLeaf::Percentage(unit_value) => Self::Percentage(unit_value),
            SpecifiedLeaf::Number(value) => Self::Number(value),
            _ => return Err(()),
        })
    }
}

/// Either an angle or a number.
#[derive(Clone, Copy, Debug, MallocSizeOf, PartialEq, ToShmem)]
pub enum NumberOrAngle {
    /// `<number>`.
    Number(f32),
    /// `<angle>`
    /// The value as a number of degrees.
    Angle(f32),
}

impl NumberOrAngle {
    /// Return the angle in degrees. `NumberOrAngle::Number` is returned as
    /// degrees, because it is the canonical unit.
    pub fn degrees(&self) -> f32 {
        match *self {
            Self::Number(value) => value,
            Self::Angle(degrees) => degrees,
        }
    }
}

impl ColorComponentType for NumberOrAngle {
    fn from_value(value: f32) -> Self {
        Self::Number(value)
    }

    fn units() -> CalcUnits {
        CalcUnits::ANGLE
    }

    fn try_from_token(token: &Token) -> Result<Self, ()> {
        Ok(match *token {
            Token::Number { value, .. } => Self::Number(value),
            Token::Dimension {
                value, ref unit, ..
            } => {
                let degrees =
                    SpecifiedAngle::parse_dimension(value, unit, /* from_calc = */ false)
                        .map(|angle| angle.degrees())?;

                NumberOrAngle::Angle(degrees)
            },
            _ => {
                return Err(());
            },
        })
    }

    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()> {
        Ok(match *leaf {
            SpecifiedLeaf::Angle(angle) => Self::Angle(angle.degrees()),
            SpecifiedLeaf::Number(value) => Self::Number(value),
            _ => return Err(()),
        })
    }
}

/// The raw f32 here is for <number>.
impl ColorComponentType for f32 {
    fn from_value(value: f32) -> Self {
        value
    }

    fn units() -> CalcUnits {
        CalcUnits::empty()
    }

    fn try_from_token(token: &Token) -> Result<Self, ()> {
        if let Token::Number { value, .. } = *token {
            Ok(value)
        } else {
            Err(())
        }
    }

    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()> {
        if let SpecifiedLeaf::Number(value) = *leaf {
            Ok(value)
        } else {
            Err(())
        }
    }
}

/// Used to parse the components of a color.
pub struct ComponentParser<'a, 'b: 'a> {
    /// Parser context used for parsing the colors.
    pub context: &'a ParserContext<'b>,
    /// The origin color that will be used to resolve relative components.
    pub origin_color: Option<AbsoluteColor>,
}

impl<'a, 'b: 'a> ComponentParser<'a, 'b> {
    /// Create a new [ColorParser] with the given context.
    pub fn new(context: &'a ParserContext<'b>) -> Self {
        Self {
            context,
            origin_color: None,
        }
    }

    /// Parse an `<number>` or `<angle>` value.
    fn parse_number_or_angle<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrAngle>, ParseError<'i>> {
        ColorComponent::parse(self.context, input, allow_none, self.origin_color.as_ref())
    }

    /// Parse a `<percentage>` value.
    fn parse_percentage<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        let location = input.current_source_location();

        let value = ColorComponent::<NumberOrPercentage>::parse(
            self.context,
            input,
            allow_none,
            self.origin_color.as_ref(),
        )?;

        if !value.is_percentage() {
            return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError));
        }

        Ok(value)
    }

    /// Parse a `<number>` value.
    fn parse_number<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        let location = input.current_source_location();

        let value = ColorComponent::<NumberOrPercentage>::parse(
            self.context,
            input,
            allow_none,
            self.origin_color.as_ref(),
        )?;

        if !value.is_number() {
            return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError));
        }

        Ok(value)
    }

    /// Parse a `<number>` or `<percentage>` value.
    fn parse_number_or_percentage<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        ColorComponent::parse(self.context, input, allow_none, self.origin_color.as_ref())
    }

    fn parse_legacy_alpha<'i, 't>(
        &self,
        arguments: &mut Parser<'i, 't>,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        if !arguments.is_exhausted() {
            arguments.expect_comma()?;
            self.parse_number_or_percentage(arguments, false)
        } else {
            Ok(ColorComponent::Value(NumberOrPercentage::Number(OPAQUE)))
        }
    }

    fn parse_modern_alpha<'i, 't>(
        &self,
        arguments: &mut Parser<'i, 't>,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        if !arguments.is_exhausted() {
            arguments.expect_delim('/')?;
            self.parse_number_or_percentage(arguments, true)
        } else {
            Ok(ColorComponent::Value(NumberOrPercentage::Number(
                self.origin_color.map(|c| c.alpha).unwrap_or(OPAQUE),
            )))
        }
    }
}

impl ColorComponent<NumberOrPercentage> {
    /// Return true if the value contained inside is/can resolve to a percentage.
    /// Also returns false if the node is invalid somehow.
    fn is_number(&self) -> bool {
        match self {
            Self::None => true,
            Self::Value(value) => matches!(value, NumberOrPercentage::Number { .. }),
            Self::Calc(node) => {
                if let Ok(unit) = node.unit() {
                    unit.is_empty()
                } else {
                    false
                }
            },
        }
    }

    /// Return true if the value contained inside is/can resolve to a percentage.
    /// Also returns false if the node is invalid somehow.
    fn is_percentage(&self) -> bool {
        match self {
            Self::None => true,
            Self::Value(value) => matches!(value, NumberOrPercentage::Percentage { .. }),
            Self::Calc(node) => {
                if let Ok(unit) = node.unit() {
                    unit == CalcUnits::PERCENTAGE
                } else {
                    false
                }
            },
        }
    }
}
