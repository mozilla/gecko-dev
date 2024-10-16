/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Output of parsing a color function, e.g. rgb(..), hsl(..), color(..)

use crate::{color::ColorFlags, values::normalize};
use cssparser::color::{clamp_floor_256_f32, OPAQUE};

use super::{
    component::ColorComponent,
    convert::normalize_hue,
    parsing::{NumberOrAngle, NumberOrPercentage},
    AbsoluteColor, ColorSpace,
};

/// Represents a specified color function.
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToShmem)]
pub enum ColorFunction {
    /// <https://drafts.csswg.org/css-color-4/#rgb-functions>
    Rgb(
        ColorComponent<NumberOrPercentage>, // red
        ColorComponent<NumberOrPercentage>, // green
        ColorComponent<NumberOrPercentage>, // blue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hsl-notation>
    Hsl(
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // saturation
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // alpha
        bool,                               // is_legacy_syntax
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hwb-notation>
    Hwb(
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // whiteness
        ColorComponent<NumberOrPercentage>, // blackness
        ColorComponent<NumberOrPercentage>, // alpha
        bool,                               // is_legacy_syntax
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lab(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // a
        ColorComponent<NumberOrPercentage>, // b
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lch(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // chroma
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklab(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // a
        ColorComponent<NumberOrPercentage>, // b
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklch(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // chroma
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#color-function>
    Color(
        ColorSpace,
        ColorComponent<NumberOrPercentage>, // red / x
        ColorComponent<NumberOrPercentage>, // green / y
        ColorComponent<NumberOrPercentage>, // blue / z
        ColorComponent<NumberOrPercentage>, // alpha
    ),
}

impl ColorFunction {
    /// Try to resolve the color function to an [`AbsoluteColor`] that does not
    /// contain any variables (currentcolor, color components, etc.).
    pub fn resolve_to_absolute(&self) -> Result<AbsoluteColor, ()> {
        macro_rules! alpha {
            ($alpha:expr) => {{
                $alpha
                    .resolve(None)?
                    .map(|value| normalize(value.to_number(1.0)).clamp(0.0, OPAQUE))
            }};
        }

        Ok(match self {
            ColorFunction::Rgb(r, g, b, alpha) => {
                #[inline]
                fn resolve(component: &ColorComponent<NumberOrPercentage>) -> Result<u8, ()> {
                    // TODO(tlouw): We need to pass an origin color to resolve.
                    Ok(clamp_floor_256_f32(
                        component
                            .resolve(None)?
                            .map(|value| value.to_number(u8::MAX as f32))
                            .unwrap_or(0.0),
                    ))
                }

                AbsoluteColor::srgb_legacy(
                    resolve(r)?,
                    resolve(g)?,
                    resolve(b)?,
                    alpha!(alpha).unwrap_or(0.0),
                )
            },
            ColorFunction::Hsl(h, s, l, alpha, is_legacy_syntax) => {
                // Percent reference range for S and L: 0% = 0.0, 100% = 100.0
                const LIGHTNESS_RANGE: f32 = 100.0;
                const SATURATION_RANGE: f32 = 100.0;

                let mut result = AbsoluteColor::new(
                    ColorSpace::Hsl,
                    h.resolve(None)?.map(|angle| normalize_hue(angle.degrees())),
                    s.resolve(None)?.map(|s| {
                        if *is_legacy_syntax {
                            s.to_number(SATURATION_RANGE).clamp(0.0, SATURATION_RANGE)
                        } else {
                            s.to_number(SATURATION_RANGE)
                        }
                    }),
                    l.resolve(None)?.map(|l| {
                        if *is_legacy_syntax {
                            l.to_number(LIGHTNESS_RANGE).clamp(0.0, LIGHTNESS_RANGE)
                        } else {
                            l.to_number(LIGHTNESS_RANGE)
                        }
                    }),
                    alpha!(alpha),
                );

                if *is_legacy_syntax {
                    result.flags.insert(ColorFlags::IS_LEGACY_SRGB);
                }

                result
            },
            ColorFunction::Hwb(h, w, b, alpha, is_legacy_syntax) => {
                // Percent reference range for W and B: 0% = 0.0, 100% = 100.0
                const WHITENESS_RANGE: f32 = 100.0;
                const BLACKNESS_RANGE: f32 = 100.0;

                let mut result = AbsoluteColor::new(
                    ColorSpace::Hwb,
                    h.resolve(None)?.map(|angle| normalize_hue(angle.degrees())),
                    w.resolve(None)?.map(|w| {
                        if *is_legacy_syntax {
                            w.to_number(WHITENESS_RANGE).clamp(0.0, WHITENESS_RANGE)
                        } else {
                            w.to_number(WHITENESS_RANGE)
                        }
                    }),
                    b.resolve(None)?.map(|b| {
                        if *is_legacy_syntax {
                            b.to_number(BLACKNESS_RANGE).clamp(0.0, BLACKNESS_RANGE)
                        } else {
                            b.to_number(BLACKNESS_RANGE)
                        }
                    }),
                    alpha!(alpha),
                );

                if *is_legacy_syntax {
                    result.flags.insert(ColorFlags::IS_LEGACY_SRGB);
                }

                result
            },
            ColorFunction::Lab(l, a, b, alpha) => {
                // for L: 0% = 0.0, 100% = 100.0
                // for a and b: -100% = -125, 100% = 125
                const LIGHTNESS_RANGE: f32 = 100.0;
                const A_B_RANGE: f32 = 125.0;

                AbsoluteColor::new(
                    ColorSpace::Lab,
                    l.resolve(None)?.map(|l| l.to_number(LIGHTNESS_RANGE)),
                    a.resolve(None)?.map(|a| a.to_number(A_B_RANGE)),
                    b.resolve(None)?.map(|b| b.to_number(A_B_RANGE)),
                    alpha!(alpha),
                )
            },
            ColorFunction::Lch(l, c, h, alpha) => {
                // for L: 0% = 0.0, 100% = 100.0
                // for C: 0% = 0, 100% = 150
                const LIGHTNESS_RANGE: f32 = 100.0;
                const CHROMA_RANGE: f32 = 150.0;

                AbsoluteColor::new(
                    ColorSpace::Lch,
                    l.resolve(None)?.map(|l| l.to_number(LIGHTNESS_RANGE)),
                    c.resolve(None)?.map(|c| c.to_number(CHROMA_RANGE)),
                    h.resolve(None)?.map(|angle| normalize_hue(angle.degrees())),
                    alpha!(alpha),
                )
            },
            ColorFunction::Oklab(l, a, b, alpha) => {
                // for L: 0% = 0.0, 100% = 1.0
                // for a and b: -100% = -0.4, 100% = 0.4
                const LIGHTNESS_RANGE: f32 = 1.0;
                const A_B_RANGE: f32 = 0.4;

                AbsoluteColor::new(
                    ColorSpace::Oklab,
                    l.resolve(None)?.map(|l| l.to_number(LIGHTNESS_RANGE)),
                    a.resolve(None)?.map(|a| a.to_number(A_B_RANGE)),
                    b.resolve(None)?.map(|b| b.to_number(A_B_RANGE)),
                    alpha!(alpha),
                )
            },
            ColorFunction::Oklch(l, c, h, alpha) => {
                // for L: 0% = 0.0, 100% = 1.0
                // for C: 0% = 0.0 100% = 0.4
                const LIGHTNESS_RANGE: f32 = 1.0;
                const CHROMA_RANGE: f32 = 0.4;

                AbsoluteColor::new(
                    ColorSpace::Oklch,
                    l.resolve(None)?.map(|l| l.to_number(LIGHTNESS_RANGE)),
                    c.resolve(None)?.map(|c| c.to_number(CHROMA_RANGE)),
                    h.resolve(None)?.map(|angle| normalize_hue(angle.degrees())),
                    alpha!(alpha),
                )
            },
            ColorFunction::Color(color_space, r, g, b, alpha) => AbsoluteColor::new(
                (*color_space).into(),
                r.resolve(None)?.map(|c| c.to_number(1.0)),
                g.resolve(None)?.map(|c| c.to_number(1.0)),
                b.resolve(None)?.map(|c| c.to_number(1.0)),
                alpha!(alpha),
            ),
        })
    }
}
