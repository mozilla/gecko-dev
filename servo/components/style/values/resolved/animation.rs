/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Resolved animation values.

use super::{Context, ToResolvedValue};

use crate::values::computed::time::Time;
use crate::values::computed::AnimationDuration;

impl ToResolvedValue for AnimationDuration {
    type ResolvedValue = Self;

    fn to_resolved_value(self, context: &Context) -> Self::ResolvedValue {
        match self {
            // For backwards-compatibility with Level 1, when the computed value of
            // animation-timeline is auto (i.e. only one list value, and that value being auto),
            // the resolved value of auto for animation-duration is 0s whenever its used value
            // would also be 0s.
            // https://drafts.csswg.org/css-animations-2/#animation-duration
            Self::Auto if context.style.get_ui().has_initial_animation_timeline() => {
                Self::Time(Time::from_seconds(0.0f32))
            },
            _ => self,
        }
    }

    #[inline]
    fn from_resolved_value(value: Self::ResolvedValue) -> Self {
        value
    }
}
