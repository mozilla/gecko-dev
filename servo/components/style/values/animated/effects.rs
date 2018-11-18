/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Animated types for CSS values related to effects.

use crate::values::animated::color::Color;
use crate::values::computed::length::Length;
#[cfg(feature = "gecko")]
use crate::values::computed::url::ComputedUrl;
use crate::values::computed::{Angle, Number};
use crate::values::generics::effects::BoxShadow as GenericBoxShadow;
use crate::values::generics::effects::Filter as GenericFilter;
use crate::values::generics::effects::SimpleShadow as GenericSimpleShadow;
#[cfg(not(feature = "gecko"))]
use crate::values::Impossible;

/// An animated value for a single `box-shadow`.
pub type BoxShadow = GenericBoxShadow<Color, Length, Length, Length>;

/// An animated value for a single `filter`.
#[cfg(feature = "gecko")]
pub type Filter = GenericFilter<Angle, Number, Length, SimpleShadow, ComputedUrl>;

/// An animated value for a single `filter`.
#[cfg(not(feature = "gecko"))]
pub type Filter = GenericFilter<Angle, Number, Length, Impossible, Impossible>;

/// An animated value for the `drop-shadow()` filter.
pub type SimpleShadow = GenericSimpleShadow<Color, Length, Length>;
