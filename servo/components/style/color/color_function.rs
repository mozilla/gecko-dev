/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Output of parsing a color function, e.g. rgb(..), hsl(..), color(..)

use std::fmt::Write;

use super::{
    component::ColorComponent,
    convert::normalize_hue,
    parsing::{NumberOrAngleComponent, NumberOrPercentageComponent},
    AbsoluteColor, ColorFlags, ColorSpace,
};
use crate::values::{
    computed::color::Color as ComputedColor, generics::Optional, normalize,
    specified::color::Color as SpecifiedColor,
};
use cssparser::color::{clamp_floor_256_f32, OPAQUE};

/// Represents a specified color function.
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToAnimatedValue, ToShmem)]
#[repr(u8)]
pub enum ColorFunction<OriginColor> {
    /// <https://drafts.csswg.org/css-color-4/#rgb-functions>
    Rgb(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrPercentageComponent>, // red
        ColorComponent<NumberOrPercentageComponent>, // green
        ColorComponent<NumberOrPercentageComponent>, // blue
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hsl-notation>
    Hsl(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrAngleComponent>,      // hue
        ColorComponent<NumberOrPercentageComponent>, // saturation
        ColorComponent<NumberOrPercentageComponent>, // lightness
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hwb-notation>
    Hwb(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrAngleComponent>,      // hue
        ColorComponent<NumberOrPercentageComponent>, // whiteness
        ColorComponent<NumberOrPercentageComponent>, // blackness
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lab(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrPercentageComponent>, // lightness
        ColorComponent<NumberOrPercentageComponent>, // a
        ColorComponent<NumberOrPercentageComponent>, // b
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lch(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrPercentageComponent>, // lightness
        ColorComponent<NumberOrPercentageComponent>, // chroma
        ColorComponent<NumberOrAngleComponent>,      // hue
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklab(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrPercentageComponent>, // lightness
        ColorComponent<NumberOrPercentageComponent>, // a
        ColorComponent<NumberOrPercentageComponent>, // b
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklch(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrPercentageComponent>, // lightness
        ColorComponent<NumberOrPercentageComponent>, // chroma
        ColorComponent<NumberOrAngleComponent>,      // hue
        ColorComponent<NumberOrPercentageComponent>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#color-function>
    Color(
        Optional<OriginColor>,                       // origin
        ColorComponent<NumberOrPercentageComponent>, // red / x
        ColorComponent<NumberOrPercentageComponent>, // green / y
        ColorComponent<NumberOrPercentageComponent>, // blue / z
        ColorComponent<NumberOrPercentageComponent>, // alpha
        ColorSpace,
    ),
}

impl ColorFunction<AbsoluteColor> {
    /// Try to resolve into a valid absolute color.
    pub fn resolve_to_absolute(&self) -> Result<AbsoluteColor, ()> {
        macro_rules! alpha {
            ($alpha:expr, $origin_color:expr) => {{
                $alpha
                    .resolve($origin_color)?
                    .map(|value| normalize(value.to_number(1.0)).clamp(0.0, OPAQUE))
            }};
        }

        Ok(match self {
            ColorFunction::Rgb(origin_color, r, g, b, alpha) => {
                // Use `color(srgb ...)` to serialize `rgb(...)` if an origin color is available;
                // this is the only reason for now.
                let use_color_syntax = origin_color.is_some();

                if use_color_syntax {
                    let origin_color = origin_color.as_ref().map(|origin| {
                        let origin = origin.to_color_space(ColorSpace::Srgb);
                        // Because rgb(..) syntax have components in range [0..255), we have to
                        // map them.
                        // NOTE: The IS_LEGACY_SRGB flag is not added back to the color, because
                        //       we're going to return the modern color(srgb ..) syntax.
                        AbsoluteColor::new(
                            ColorSpace::Srgb,
                            origin.c0().map(|v| v * 255.0),
                            origin.c1().map(|v| v * 255.0),
                            origin.c2().map(|v| v * 255.0),
                            origin.alpha(),
                        )
                    });

                    // We have to map all the components back to [0..1) range after all the
                    // calculations.
                    AbsoluteColor::new(
                        ColorSpace::Srgb,
                        r.resolve(origin_color.as_ref())?
                            .map(|c| c.to_number(255.0) / 255.0),
                        g.resolve(origin_color.as_ref())?
                            .map(|c| c.to_number(255.0) / 255.0),
                        b.resolve(origin_color.as_ref())?
                            .map(|c| c.to_number(255.0) / 255.0),
                        alpha!(alpha, origin_color.as_ref()),
                    )
                } else {
                    #[inline]
                    fn resolve(
                        component: &ColorComponent<NumberOrPercentageComponent>,
                        origin_color: Option<&AbsoluteColor>,
                    ) -> Result<u8, ()> {
                        Ok(clamp_floor_256_f32(
                            component
                                .resolve(origin_color)?
                                .map_or(0.0, |value| value.to_number(u8::MAX as f32)),
                        ))
                    }

                    let origin_color = origin_color.as_ref().map(|o| o.into_srgb_legacy());

                    AbsoluteColor::srgb_legacy(
                        resolve(r, origin_color.as_ref())?,
                        resolve(g, origin_color.as_ref())?,
                        resolve(b, origin_color.as_ref())?,
                        alpha!(alpha, origin_color.as_ref()).unwrap_or(0.0),
                    )
                }
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

                let origin_color = origin_color
                    .as_ref()
                    .map(|o| o.to_color_space(ColorSpace::Hsl));

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

                let origin_color = origin_color
                    .as_ref()
                    .map(|o| o.to_color_space(ColorSpace::Hwb));

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

                let origin_color = origin_color
                    .as_ref()
                    .map(|o| o.to_color_space(ColorSpace::Lab));

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

                let origin_color = origin_color
                    .as_ref()
                    .map(|o| o.to_color_space(ColorSpace::Lch));

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

                let origin_color = origin_color
                    .as_ref()
                    .map(|o| o.to_color_space(ColorSpace::Oklab));

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

                let origin_color = origin_color
                    .as_ref()
                    .map(|o| o.to_color_space(ColorSpace::Oklch));

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
                let origin_color = origin_color.as_ref().map(|o| {
                    let mut result = o.to_color_space(*color_space);

                    // If the origin color was a `rgb(..)` function, we should
                    // make sure it doesn't have the legacy flag any more so
                    // that it is recognized as a `color(srgb ..)` function.
                    result.flags.set(ColorFlags::IS_LEGACY_SRGB, false);

                    result
                });

                AbsoluteColor::new(
                    *color_space,
                    r.resolve(origin_color.as_ref())?.map(|c| c.to_number(1.0)),
                    g.resolve(origin_color.as_ref())?.map(|c| c.to_number(1.0)),
                    b.resolve(origin_color.as_ref())?.map(|c| c.to_number(1.0)),
                    alpha!(alpha, origin_color.as_ref()),
                )
            },
        })
    }
}

impl ColorFunction<SpecifiedColor> {
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
        // Map the color function to one with an absolute origin color.
        let resolvable = self.map_origin_color(|o| o.resolve_to_absolute());
        resolvable.resolve_to_absolute()
    }
}

impl<Color> ColorFunction<Color> {
    /// Map the origin color to another type.  Return None from `f` if the conversion fails.
    pub fn map_origin_color<U>(&self, f: impl FnOnce(&Color) -> Option<U>) -> ColorFunction<U> {
        macro_rules! map {
            ($f:ident, $o:expr, $c0:expr, $c1:expr, $c2:expr, $alpha:expr) => {{
                ColorFunction::$f(
                    $o.as_ref().and_then(f).into(),
                    $c0.clone(),
                    $c1.clone(),
                    $c2.clone(),
                    $alpha.clone(),
                )
            }};
        }
        match self {
            ColorFunction::Rgb(o, c0, c1, c2, alpha) => map!(Rgb, o, c0, c1, c2, alpha),
            ColorFunction::Hsl(o, c0, c1, c2, alpha) => map!(Hsl, o, c0, c1, c2, alpha),
            ColorFunction::Hwb(o, c0, c1, c2, alpha) => map!(Hwb, o, c0, c1, c2, alpha),
            ColorFunction::Lab(o, c0, c1, c2, alpha) => map!(Lab, o, c0, c1, c2, alpha),
            ColorFunction::Lch(o, c0, c1, c2, alpha) => map!(Lch, o, c0, c1, c2, alpha),
            ColorFunction::Oklab(o, c0, c1, c2, alpha) => map!(Oklab, o, c0, c1, c2, alpha),
            ColorFunction::Oklch(o, c0, c1, c2, alpha) => map!(Oklch, o, c0, c1, c2, alpha),
            ColorFunction::Color(o, c0, c1, c2, alpha, color_space) => ColorFunction::Color(
                o.as_ref().and_then(f).into(),
                c0.clone(),
                c1.clone(),
                c2.clone(),
                alpha.clone(),
                color_space.clone(),
            ),
        }
    }
}

impl ColorFunction<ComputedColor> {
    /// Resolve a computed color function to an absolute computed color.
    pub fn resolve_to_absolute(&self, current_color: &AbsoluteColor) -> AbsoluteColor {
        // Map the color function to one with an absolute origin color.
        let resolvable = self.map_origin_color(|o| Some(o.resolve_to_absolute(current_color)));
        match resolvable.resolve_to_absolute() {
            Ok(color) => color,
            Err(..) => {
                debug_assert!(
                    false,
                    "the color could not be resolved even with a currentcolor specified?"
                );
                AbsoluteColor::TRANSPARENT_BLACK
            },
        }
    }
}

impl<C: style_traits::ToCss> style_traits::ToCss for ColorFunction<C> {
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

        if let Optional::Some(origin_color) = origin_color {
            dest.write_str("from ")?;
            origin_color.to_css(dest)?;
            dest.write_str(" ")?;
        }

        let is_opaque = if let ColorComponent::Value(value) = *alpha {
            value.to_number(OPAQUE) == OPAQUE
        } else {
            false
        };

        macro_rules! serialize_alpha {
            ($alpha_component:expr) => {{
                if !is_opaque && !matches!($alpha_component, ColorComponent::AlphaOmitted) {
                    dest.write_str(" / ")?;
                    $alpha_component.to_css(dest)?;
                }
            }};
        }

        macro_rules! serialize_components {
            ($c0:expr, $c1:expr, $c2:expr) => {{
                debug_assert!(!matches!($c0, ColorComponent::AlphaOmitted));
                debug_assert!(!matches!($c1, ColorComponent::AlphaOmitted));
                debug_assert!(!matches!($c2, ColorComponent::AlphaOmitted));

                $c0.to_css(dest)?;
                dest.write_str(" ")?;
                $c1.to_css(dest)?;
                dest.write_str(" ")?;
                $c2.to_css(dest)?;
            }};
        }

        match self {
            Self::Rgb(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Hsl(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Hwb(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Lab(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Lch(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Oklab(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Oklch(_, c0, c1, c2, alpha) => {
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
            Self::Color(_, c0, c1, c2, alpha, color_space) => {
                color_space.to_css(dest)?;
                dest.write_str(" ")?;
                serialize_components!(c0, c1, c2);
                serialize_alpha!(alpha);
            },
        }

        dest.write_str(")")
    }
}
