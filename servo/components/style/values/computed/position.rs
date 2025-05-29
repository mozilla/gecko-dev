/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! CSS handling for the computed value of
//! [`position`][position] values.
//!
//! [position]: https://drafts.csswg.org/css-backgrounds-3/#position

use crate::values::computed::{Integer, LengthPercentage, NonNegativeNumber, Percentage};
use crate::values::generics::position::{AnchorSideKeyword, GenericAnchorFunction, GenericAnchorSide};
use crate::values::generics::position::Position as GenericPosition;
use crate::values::generics::position::PositionComponent as GenericPositionComponent;
use crate::values::generics::position::PositionOrAuto as GenericPositionOrAuto;
use crate::values::generics::position::ZIndex as GenericZIndex;
use crate::values::generics::position::{AspectRatio as GenericAspectRatio, GenericInset};
pub use crate::values::specified::position::{
    AnchorName, AnchorScope, DashedIdentAndOrTryTactic, PositionAnchor, PositionArea,
    PositionAreaKeyword, PositionTryFallbacks, PositionTryOrder, PositionVisibility,
};
pub use crate::values::specified::position::{GridAutoFlow, GridTemplateAreas, MasonryAutoFlow};
use crate::Zero;
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

/// The computed value of a CSS `<position>`
pub type Position = GenericPosition<HorizontalPosition, VerticalPosition>;

/// The computed value of an `auto | <position>`
pub type PositionOrAuto = GenericPositionOrAuto<Position>;

/// The computed value of a CSS horizontal position.
pub type HorizontalPosition = LengthPercentage;

/// The computed value of a CSS vertical position.
pub type VerticalPosition = LengthPercentage;

/// The computed value of anchor side.
pub type AnchorSide = GenericAnchorSide<Percentage>;

impl AnchorSide {
    /// Break down given anchor side into its equivalent keyword and percentage.
    pub fn keyword_and_percentage(&self) -> (AnchorSideKeyword, Percentage) {
        match self {
            Self::Percentage(p) => (AnchorSideKeyword::Start, *p),
            Self::Keyword(k) => if matches!(k, AnchorSideKeyword::Center) {
                (AnchorSideKeyword::Start, Percentage(0.5))
            } else {
                (*k, Percentage::hundred())
            },
        }
    }
}

/// The computed value of an `anchor()` function.
pub type AnchorFunction = GenericAnchorFunction<Percentage, LengthPercentage>;

#[cfg(feature="gecko")]
use crate::{
    gecko_bindings::structs::AnchorPosResolutionParams,
    logical_geometry::PhysicalSide,
    values::{DashedIdent, computed::Length},
};

impl AnchorFunction {
    /// Resolve the anchor function with the given resolver. Returns `Err()` if no anchor is found.
    #[cfg(feature="gecko")]
    pub fn resolve(
        anchor_name: &DashedIdent,
        anchor_side: &AnchorSide,
        prop_side: PhysicalSide,
        params: &AnchorPosResolutionParams,
    ) -> Result<Length, ()> {
        use crate::gecko_bindings::structs::Gecko_GetAnchorPosOffset;

        let (keyword, percentage) = anchor_side.keyword_and_percentage();
        let mut offset = Length::zero();
        let valid = unsafe {
            Gecko_GetAnchorPosOffset(
                params,
                anchor_name.0.as_ptr(),
                prop_side as u8,
                keyword as u8,
                percentage.0,
                &mut offset,
            )
        };

        if !valid {
            return Err(());
        }

        Ok(offset)
    }
}

/// A computed type for `inset` properties.
pub type Inset = GenericInset<Percentage, LengthPercentage>;

impl Position {
    /// `50% 50%`
    #[inline]
    pub fn center() -> Self {
        Self::new(
            LengthPercentage::new_percent(Percentage(0.5)),
            LengthPercentage::new_percent(Percentage(0.5)),
        )
    }

    /// `0% 0%`
    #[inline]
    pub fn zero() -> Self {
        Self::new(LengthPercentage::zero(), LengthPercentage::zero())
    }
}

impl ToCss for Position {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        self.horizontal.to_css(dest)?;
        dest.write_char(' ')?;
        self.vertical.to_css(dest)
    }
}

impl GenericPositionComponent for LengthPercentage {
    fn is_center(&self) -> bool {
        match self.to_percentage() {
            Some(Percentage(per)) => per == 0.5,
            _ => false,
        }
    }
}

/// A computed value for the `z-index` property.
pub type ZIndex = GenericZIndex<Integer>;

/// A computed value for the `aspect-ratio` property.
pub type AspectRatio = GenericAspectRatio<NonNegativeNumber>;
