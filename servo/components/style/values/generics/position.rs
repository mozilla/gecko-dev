/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Generic types for CSS handling of specified and computed values of
//! [`position`](https://drafts.csswg.org/css-backgrounds-3/#position)

use std::fmt::Write;

use style_traits::CssWriter;
use style_traits::ToCss;

use crate::values::animated::ToAnimatedZero;
use crate::values::generics::ratio::Ratio;
use crate::values::DashedIdent;

/// A generic type for representing a CSS [position](https://drafts.csswg.org/css-values/#position).
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    MallocSizeOf,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C)]
pub struct GenericPosition<H, V> {
    /// The horizontal component of position.
    pub horizontal: H,
    /// The vertical component of position.
    pub vertical: V,
}

impl<H, V> PositionComponent for Position<H, V>
where
    H: PositionComponent,
    V: PositionComponent,
{
    #[inline]
    fn is_center(&self) -> bool {
        self.horizontal.is_center() && self.vertical.is_center()
    }
}

pub use self::GenericPosition as Position;

impl<H, V> Position<H, V> {
    /// Returns a new position.
    pub fn new(horizontal: H, vertical: V) -> Self {
        Self {
            horizontal,
            vertical,
        }
    }
}

/// Implements a method that checks if the position is centered.
pub trait PositionComponent {
    /// Returns if the position component is 50% or center.
    /// For pixel lengths, it always returns false.
    fn is_center(&self) -> bool;
}

/// A generic type for representing an `Auto | <position>`.
/// This is used by <offset-anchor> for now.
/// https://drafts.fxtf.org/motion-1/#offset-anchor-property
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    MallocSizeOf,
    Parse,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedZero,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericPositionOrAuto<Pos> {
    /// The <position> value.
    Position(Pos),
    /// The keyword `auto`.
    Auto,
}

pub use self::GenericPositionOrAuto as PositionOrAuto;

impl<Pos> PositionOrAuto<Pos> {
    /// Return `auto`.
    #[inline]
    pub fn auto() -> Self {
        PositionOrAuto::Auto
    }

    /// Return true if it is 'auto'.
    #[inline]
    pub fn is_auto(&self) -> bool {
        matches!(self, PositionOrAuto::Auto)
    }
}

/// A generic value for the `z-index` property.
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
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericZIndex<I> {
    /// An integer value.
    Integer(I),
    /// The keyword `auto`.
    Auto,
}

pub use self::GenericZIndex as ZIndex;

impl<Integer> ZIndex<Integer> {
    /// Returns `auto`
    #[inline]
    pub fn auto() -> Self {
        ZIndex::Auto
    }

    /// Returns whether `self` is `auto`.
    #[inline]
    pub fn is_auto(self) -> bool {
        matches!(self, ZIndex::Auto)
    }

    /// Returns the integer value if it is an integer, or `auto`.
    #[inline]
    pub fn integer_or(self, auto: Integer) -> Integer {
        match self {
            ZIndex::Integer(n) => n,
            ZIndex::Auto => auto,
        }
    }
}

/// Ratio or None.
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
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum PreferredRatio<N> {
    /// Without specified ratio
    #[css(skip)]
    None,
    /// With specified ratio
    Ratio(
        #[animation(field_bound)]
        #[css(field_bound)]
        #[distance(field_bound)]
        Ratio<N>,
    ),
}

/// A generic value for the `aspect-ratio` property, the value is `auto || <ratio>`.
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
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C)]
pub struct GenericAspectRatio<N> {
    /// Specifiy auto or not.
    #[animation(constant)]
    #[css(represents_keyword)]
    pub auto: bool,
    /// The preferred aspect-ratio value.
    #[animation(field_bound)]
    #[css(field_bound)]
    #[distance(field_bound)]
    pub ratio: PreferredRatio<N>,
}

pub use self::GenericAspectRatio as AspectRatio;
use crate::values::generics::Optional;

impl<N> AspectRatio<N> {
    /// Returns `auto`
    #[inline]
    pub fn auto() -> Self {
        AspectRatio {
            auto: true,
            ratio: PreferredRatio::None,
        }
    }
}

impl<N> ToAnimatedZero for AspectRatio<N> {
    #[inline]
    fn to_animated_zero(&self) -> Result<Self, ()> {
        Err(())
    }
}

/// Anchor function used by inset properties. This resolves
/// to length at computed time.
///
/// https://drafts.csswg.org/css-anchor-position-1/#funcdef-anchor
#[derive(Clone, Debug, MallocSizeOf, PartialEq, SpecifiedValueInfo, ToShmem, ToComputedValue, ToResolvedValue)]
#[repr(C)]
pub struct GenericAnchorFunction<Percentage, LengthPercentage>
where
    Percentage: ToCss,
    LengthPercentage: ToCss,
{
    /// Anchor name of the element to anchor to.
    /// If omitted, selects the implicit anchor element.
    pub target_element: Optional<DashedIdent>,
    /// Where relative to the target anchor element to position
    /// the anchored element to.
    pub side: AnchorSide<Percentage>,
    /// Value to use in case the anchor function is invalid.
    pub fallback: Optional<LengthPercentage>,
}

impl<Percentage, LengthPercentage> ToCss for GenericAnchorFunction<Percentage, LengthPercentage>
where
    Percentage: ToCss,
    LengthPercentage: ToCss,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> std::fmt::Result
    where
        W: Write,
    {
        dest.write_str("anchor(")?;
        if let Some(t) = self.target_element.as_ref() {
            t.to_css(dest)?;
            dest.write_str(" ")?;
        }
        self.side.to_css(dest)?;
        if let Some(f) = self.fallback.as_ref() {
            // This comma isn't really `derive()`-able, unfortunately.
            dest.write_str(", ")?;
            f.to_css(dest)?;
        }
        dest.write_str(")")
    }
}

/// Keyword values for the anchor positioning function.
#[derive(
    Clone, Copy, Debug, MallocSizeOf, PartialEq, SpecifiedValueInfo, ToCss, ToShmem, Parse, ToComputedValue, ToResolvedValue
)]
#[repr(u8)]
pub enum AnchorSideKeyword {
    /// Inside relative (i.e. Same side) to the inset property it's used in.
    Inside,
    /// Same as above, but outside (i.e. Opposite side).
    Outside,
    /// Top of the anchor element.
    Top,
    /// Left of the anchor element.
    Left,
    /// Right of the anchor element.
    Right,
    /// Bottom of the anchor element.
    Bottom,
    /// Refers to the start side of the anchor element for the same axis of the inset
    /// property it's used in, resolved against the positioned element's containing
    /// block's writing mode.
    Start,
    /// Same as above, but for the end side.
    End,
    /// Same as `start`, resolved against the positioned element's writing mode.
    SelfStart,
    /// Same as above, but for the end side.
    SelfEnd,
    /// Halfway between `start` and `end` sides.
    Center,
}

/// Anchor side for the anchor positioning function.
#[derive(
    Clone, Copy, Debug, MallocSizeOf, PartialEq, Parse, SpecifiedValueInfo, ToCss, ToShmem, ToComputedValue, ToResolvedValue
)]
#[repr(C)]
pub enum AnchorSide<P> {
    /// A keyword value for the anchor side.
    Keyword(AnchorSideKeyword),
    /// Percentage value between the `start` and `end` sides.
    Percentage(P),
}
