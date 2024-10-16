/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Output of parsing a color function, e.g. rgb(..), hsl(..), color(..)

use std::fmt::Write;

use super::{
    component::ColorComponent,
    convert::normalize_hue,
    parsing::{NumberOrAngle, NumberOrPercentage},
    AbsoluteColor, ColorFlags, ColorSpace,
};
use crate::values::{normalize, specified::color::Color as SpecifiedColor};
use cssparser::color::{clamp_floor_256_f32, OPAQUE};

/// Represents a specified color function.
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToShmem)]
pub enum ColorFunction {
    /// <https://drafts.csswg.org/css-color-4/#rgb-functions>
    Rgb(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrPercentage>, // red
        ColorComponent<NumberOrPercentage>, // green
        ColorComponent<NumberOrPercentage>, // blue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hsl-notation>
    Hsl(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // saturation
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hwb-notation>
    Hwb(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // whiteness
        ColorComponent<NumberOrPercentage>, // blackness
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lab(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // a
        ColorComponent<NumberOrPercentage>, // b
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lch(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // chroma
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklab(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // a
        ColorComponent<NumberOrPercentage>, // b
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklch(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // chroma
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#color-function>
    Color(
        Option<SpecifiedColor>,             // origin
        ColorComponent<NumberOrPercentage>, // red / x
        ColorComponent<NumberOrPercentage>, // green / y
        ColorComponent<NumberOrPercentage>, // blue / z
        ColorComponent<NumberOrPercentage>, // alpha
        ColorSpace,
    ),
}

impl ColorFunction {
    /// Return true if the color funciton has an origin color specified.
    pub fn has_origin_color(&self) -> bool {
        match self {
            Self::Rgb(origin_color, ..) |
            Self::Hsl(origin_color, ..) |
            Self::Hwb(origin_color, ..) |
            Self::Lab(origin_color, ..) |
            Self::Lch(origin_color, ..) |
            Self::Oklab(origin_color, ..) |
            Self::Oklch(origin_color, ..) |
            Self::Color(origin_color, ..) => origin_color.is_some(),
        }
    }

    /// Try to resolve the color function to an [`AbsoluteColor`] that does not
    /// contain any variables (currentcolor, color components, etc.).
    pub fn resolve_to_absolute(&self) -> Result<AbsoluteColor, ()> {
        macro_rules! alpha {
            ($alpha:expr, $origin_color:expr) => {{
                $alpha
                    .resolve($origin_color)?
                    .map(|value| normalize(value.to_number(1.0)).clamp(0.0, OPAQUE))
            }};
        }

        macro_rules! resolved_origin_color {
            ($origin_color:expr,$color_space:expr) => {{
                match $origin_color {
                    Some(color) => color
                        .resolve_to_absolute()
                        .map(|color| color.to_color_space($color_space)),
                    None => None,
                }
            }};
        }

        Ok(match self {
            ColorFunction::Rgb(origin_color, r, g, b, alpha) => {
                #[inline]
                fn resolve(
                    component: &ColorComponent<NumberOrPercentage>,
                    origin_color: Option<&AbsoluteColor>,
                ) -> Result<u8, ()> {
                    Ok(clamp_floor_256_f32(
                        component
                            .resolve(origin_color)?
                            .map(|value| value.to_number(u8::MAX as f32))
                            .unwrap_or(0.0),
                    ))
                }

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Srgb);

                AbsoluteColor::srgb_legacy(
                    resolve(r, origin_color.as_ref())?,
                    resolve(g, origin_color.as_ref())?,
                    resolve(b, origin_color.as_ref())?,
                    alpha!(alpha, origin_color.as_ref()).unwrap_or(0.0),
                )
            },
            ColorFunction::Hsl(origin_color, h, s, l, alpha) => {
                // Percent reference range for S and L: 0% = 0.0, 100% = 100.0
                const LIGHTNESS_RANGE: f32 = 100.0;
                const SATURATION_RANGE: f32 = 100.0;

                // If the origin color:
                // - was *NOT* specified, then we stick with the old way of serializing the
                //   value to rgb(..).
                // - was specified, we don't use the rgb(..) syntax, because we should allow the
                //   color to be out of gamut and not clamp.
                let use_rgb_sytax = origin_color.is_none();

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Hsl);

                let mut result = AbsoluteColor::new(
                    ColorSpace::Hsl,
                    h.resolve(origin_color.as_ref())?
                        .map(|angle| normalize_hue(angle.degrees())),
                    s.resolve(origin_color.as_ref())?.map(|s| {
                        if use_rgb_sytax {
                            s.to_number(SATURATION_RANGE).clamp(0.0, SATURATION_RANGE)
                        } else {
                            s.to_number(SATURATION_RANGE)
                        }
                    }),
                    l.resolve(origin_color.as_ref())?.map(|l| {
                        if use_rgb_sytax {
                            l.to_number(LIGHTNESS_RANGE).clamp(0.0, LIGHTNESS_RANGE)
                        } else {
                            l.to_number(LIGHTNESS_RANGE)
                        }
                    }),
                    alpha!(alpha, origin_color.as_ref()),
                );

                if use_rgb_sytax {
                    result.flags.insert(ColorFlags::IS_LEGACY_SRGB);
                }

                result
            },
            ColorFunction::Hwb(origin_color, h, w, b, alpha) => {
                // If the origin color:
                // - was *NOT* specified, then we stick with the old way of serializing the
                //   value to rgb(..).
                // - was specified, we don't use the rgb(..) syntax, because we should allow the
                //   color to be out of gamut and not clamp.
                let use_rgb_sytax = origin_color.is_none();

                // Percent reference range for W and B: 0% = 0.0, 100% = 100.0
                const WHITENESS_RANGE: f32 = 100.0;
                const BLACKNESS_RANGE: f32 = 100.0;

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Hwb);

                let mut result = AbsoluteColor::new(
                    ColorSpace::Hwb,
                    h.resolve(origin_color.as_ref())?
                        .map(|angle| normalize_hue(angle.degrees())),
                    w.resolve(origin_color.as_ref())?.map(|w| {
                        if use_rgb_sytax {
                            w.to_number(WHITENESS_RANGE).clamp(0.0, WHITENESS_RANGE)
                        } else {
                            w.to_number(WHITENESS_RANGE)
                        }
                    }),
                    b.resolve(origin_color.as_ref())?.map(|b| {
                        if use_rgb_sytax {
                            b.to_number(BLACKNESS_RANGE).clamp(0.0, BLACKNESS_RANGE)
                        } else {
                            b.to_number(BLACKNESS_RANGE)
                        }
                    }),
                    alpha!(alpha, origin_color.as_ref()),
                );

                if use_rgb_sytax {
                    result.flags.insert(ColorFlags::IS_LEGACY_SRGB);
                }

                result
            },
            ColorFunction::Lab(origin_color, l, a, b, alpha) => {
                // for L: 0% = 0.0, 100% = 100.0
                // for a and b: -100% = -125, 100% = 125
                const LIGHTNESS_RANGE: f32 = 100.0;
                const A_B_RANGE: f32 = 125.0;

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Lab);

                AbsoluteColor::new(
                    ColorSpace::Lab,
                    l.resolve(origin_color.as_ref())?
                        .map(|l| l.to_number(LIGHTNESS_RANGE)),
                    a.resolve(origin_color.as_ref())?
                        .map(|a| a.to_number(A_B_RANGE)),
                    b.resolve(origin_color.as_ref())?
                        .map(|b| b.to_number(A_B_RANGE)),
                    alpha!(alpha, origin_color.as_ref()),
                )
            },
            ColorFunction::Lch(origin_color, l, c, h, alpha) => {
                // for L: 0% = 0.0, 100% = 100.0
                // for C: 0% = 0, 100% = 150
                const LIGHTNESS_RANGE: f32 = 100.0;
                const CHROMA_RANGE: f32 = 150.0;

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Lch);

                AbsoluteColor::new(
                    ColorSpace::Lch,
                    l.resolve(origin_color.as_ref())?
                        .map(|l| l.to_number(LIGHTNESS_RANGE)),
                    c.resolve(origin_color.as_ref())?
                        .map(|c| c.to_number(CHROMA_RANGE)),
                    h.resolve(origin_color.as_ref())?
                        .map(|angle| normalize_hue(angle.degrees())),
                    alpha!(alpha, origin_color.as_ref()),
                )
            },
            ColorFunction::Oklab(origin_color, l, a, b, alpha) => {
                // for L: 0% = 0.0, 100% = 1.0
                // for a and b: -100% = -0.4, 100% = 0.4
                const LIGHTNESS_RANGE: f32 = 1.0;
                const A_B_RANGE: f32 = 0.4;

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Oklab);

                AbsoluteColor::new(
                    ColorSpace::Oklab,
                    l.resolve(origin_color.as_ref())?
                        .map(|l| l.to_number(LIGHTNESS_RANGE)),
                    a.resolve(origin_color.as_ref())?
                        .map(|a| a.to_number(A_B_RANGE)),
                    b.resolve(origin_color.as_ref())?
                        .map(|b| b.to_number(A_B_RANGE)),
                    alpha!(alpha, origin_color.as_ref()),
                )
            },
            ColorFunction::Oklch(origin_color, l, c, h, alpha) => {
                // for L: 0% = 0.0, 100% = 1.0
                // for C: 0% = 0.0 100% = 0.4
                const LIGHTNESS_RANGE: f32 = 1.0;
                const CHROMA_RANGE: f32 = 0.4;

                let origin_color = resolved_origin_color!(origin_color, ColorSpace::Oklch);

                AbsoluteColor::new(
                    ColorSpace::Oklch,
                    l.resolve(origin_color.as_ref())?
                        .map(|l| l.to_number(LIGHTNESS_RANGE)),
                    c.resolve(origin_color.as_ref())?
                        .map(|c| c.to_number(CHROMA_RANGE)),
                    h.resolve(origin_color.as_ref())?
                        .map(|angle| normalize_hue(angle.degrees())),
                    alpha!(alpha, origin_color.as_ref()),
                )
            },
            ColorFunction::Color(origin_color, r, g, b, alpha, color_space) => {
                let origin_color = resolved_origin_color!(origin_color, *color_space);
                AbsoluteColor::new(
                    (*color_space).into(),
                    r.resolve(origin_color.as_ref())?.map(|c| c.to_number(1.0)),
                    g.resolve(origin_color.as_ref())?.map(|c| c.to_number(1.0)),
                    b.resolve(origin_color.as_ref())?.map(|c| c.to_number(1.0)),
                    alpha!(alpha, origin_color.as_ref()),
                )
            },
        })
    }
}

impl style_traits::ToCss for ColorFunction {
    fn to_css<W>(&self, dest: &mut style_traits::CssWriter<W>) -> std::fmt::Result
    where
        W: std::fmt::Write,
    {
        let (origin_color, alpha) = match self {
            Self::Rgb(origin_color, _, _, _, alpha) => {
                dest.write_str("rgb(")?;
                (origin_color, alpha)
            },
            Self::Hsl(origin_color, _, _, _, alpha) => {
                dest.write_str("hsl(")?;
                (origin_color, alpha)
            },
            Self::Hwb(origin_color, _, _, _, alpha) => {
                dest.write_str("hwb(")?;
                (origin_color, alpha)
            },
            Self::Lab(origin_color, _, _, _, alpha) => {
                dest.write_str("lab(")?;
                (origin_color, alpha)
            },
            Self::Lch(origin_color, _, _, _, alpha) => {
                dest.write_str("lch(")?;
                (origin_color, alpha)
            },
            Self::Oklab(origin_color, _, _, _, alpha) => {
                dest.write_str("oklab(")?;
                (origin_color, alpha)
            },
            Self::Oklch(origin_color, _, _, _, alpha) => {
                dest.write_str("oklch(")?;
                (origin_color, alpha)
            },
            Self::Color(origin_color, _, _, _, alpha, _) => {
                dest.write_str("color(")?;
                (origin_color, alpha)
            },
        };

        if let Some(origin_color) = origin_color {
            dest.write_str("from ")?;
            origin_color.to_css(dest)?;
            dest.write_str(" ")?;
        }

        let is_opaque = if let ColorComponent::Value(value) = *alpha {
            value.to_number(OPAQUE) == OPAQUE
        } else {
            false
        };

        match self {
            Self::Rgb(_, r, g, b, alpha) => {
                r.to_css(dest)?;
                dest.write_str(" ")?;
                g.to_css(dest)?;
                dest.write_str(" ")?;
                b.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Hsl(_, h, s, l, alpha) => {
                h.to_css(dest)?;
                dest.write_str(" ")?;
                s.to_css(dest)?;
                dest.write_str(" ")?;
                l.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Hwb(_, h, w, b, alpha) => {
                h.to_css(dest)?;
                dest.write_str(" ")?;
                w.to_css(dest)?;
                dest.write_str(" ")?;
                b.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Lab(_, l, a, b, alpha) => {
                l.to_css(dest)?;
                dest.write_str(" ")?;
                a.to_css(dest)?;
                dest.write_str(" ")?;
                b.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Lch(_, l, c, h, alpha) => {
                l.to_css(dest)?;
                dest.write_str(" ")?;
                c.to_css(dest)?;
                dest.write_str(" ")?;
                h.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Oklab(_, l, a, b, alpha) => {
                l.to_css(dest)?;
                dest.write_str(" ")?;
                a.to_css(dest)?;
                dest.write_str(" ")?;
                b.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Oklch(_, l, c, h, alpha) => {
                l.to_css(dest)?;
                dest.write_str(" ")?;
                c.to_css(dest)?;
                dest.write_str(" ")?;
                h.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
            Self::Color(_, r, g, b, alpha, color_space) => {
                color_space.to_css(dest)?;
                dest.write_str(" ")?;

                r.to_css(dest)?;
                dest.write_str(" ")?;
                g.to_css(dest)?;
                dest.write_str(" ")?;
                b.to_css(dest)?;

                if !is_opaque {
                    dest.write_str(" / ")?;
                    alpha.to_css(dest)?;
                }
            },
        }

        dest.write_str(")")
    }
}
