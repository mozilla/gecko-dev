/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, DeviceIntPoint, DeviceIntRect, DeviceIntSize, DevicePixelScale};
use api::{LayoutPixel, DeviceRect, WorldPixel, RasterRect};
use euclid::{Point2D, Rect, Size2D, TypedPoint2D, TypedRect, TypedSize2D, Vector2D};
use euclid::{TypedTransform2D, TypedTransform3D, TypedVector2D, TypedScale};
use num_traits::Zero;
use plane_split::{Clipper, Polygon};
use std::{i32, f32, fmt, ptr};
use std::borrow::Cow;


// Matches the definition of SK_ScalarNearlyZero in Skia.
const NEARLY_ZERO: f32 = 1.0 / 4096.0;

/// A typesafe helper that separates new value construction from
/// vector growing, allowing LLVM to ideally construct the element in place.
pub struct Allocation<'a, T: 'a> {
    vec: &'a mut Vec<T>,
    index: usize,
}

impl<'a, T> Allocation<'a, T> {
    // writing is safe because alloc() ensured enough capacity
    // and `Allocation` holds a mutable borrow to prevent anyone else
    // from breaking this invariant.
    #[inline(always)]
    pub fn init(self, value: T) -> usize {
        unsafe {
            ptr::write(self.vec.as_mut_ptr().add(self.index), value);
            self.vec.set_len(self.index + 1);
        }
        self.index
    }
}

/// An entry into a vector, similar to `std::collections::hash_map::Entry`.
pub enum VecEntry<'a, T: 'a> {
    Vacant(Allocation<'a, T>),
    Occupied(&'a mut T),
}

impl<'a, T> VecEntry<'a, T> {
    #[inline(always)]
    pub fn set(self, value: T) {
        match self {
            VecEntry::Vacant(alloc) => { alloc.init(value); }
            VecEntry::Occupied(slot) => { *slot = value; }
        }
    }
}

pub trait VecHelper<T> {
    /// Growns the vector by a single entry, returning the allocation.
    fn alloc(&mut self) -> Allocation<T>;
    /// Either returns an existing elemenet, or grows the vector by one.
    /// Doesn't expect indices to be higher than the current length.
    fn entry(&mut self, index: usize) -> VecEntry<T>;
}

impl<T> VecHelper<T> for Vec<T> {
    fn alloc(&mut self) -> Allocation<T> {
        let index = self.len();
        if self.capacity() == index {
            self.reserve(1);
        }
        Allocation {
            vec: self,
            index,
        }
    }

    fn entry(&mut self, index: usize) -> VecEntry<T> {
        if index < self.len() {
            VecEntry::Occupied(unsafe {
                self.get_unchecked_mut(index)
            })
        } else {
            assert_eq!(index, self.len());
            VecEntry::Vacant(self.alloc())
        }
    }
}


// Represents an optimized transform where there is only
// a scale and translation (which are guaranteed to maintain
// an axis align rectangle under transformation). The
// scaling is applied first, followed by the translation.
// TODO(gw): We should try and incorporate F <-> T units here,
//           but it's a bit tricky to do that now with the
//           way the current clip-scroll tree works.
#[derive(Debug, Clone, Copy)]
pub struct ScaleOffset {
    pub scale: Vector2D<f32>,
    pub offset: Vector2D<f32>,
}

impl ScaleOffset {
    pub fn identity() -> Self {
        ScaleOffset {
            scale: Vector2D::new(1.0, 1.0),
            offset: Vector2D::zero(),
        }
    }

    // Construct a ScaleOffset from a transform. Returns
    // None if the matrix is not a pure scale / translation.
    pub fn from_transform<F, T>(
        m: &TypedTransform3D<f32, F, T>,
    ) -> Option<ScaleOffset> {

        // To check that we have a pure scale / translation:
        // Every field must match an identity matrix, except:
        //  - Any value present in tx,ty
        //  - Any non-neg value present in sx,sy (avoid negative for reflection/rotation)

        if m.m11 < 0.0 ||
           m.m12.abs() > NEARLY_ZERO ||
           m.m13.abs() > NEARLY_ZERO ||
           m.m14.abs() > NEARLY_ZERO ||
           m.m21.abs() > NEARLY_ZERO ||
           m.m22 < 0.0 ||
           m.m23.abs() > NEARLY_ZERO ||
           m.m24.abs() > NEARLY_ZERO ||
           m.m31.abs() > NEARLY_ZERO ||
           m.m32.abs() > NEARLY_ZERO ||
           (m.m33 - 1.0).abs() > NEARLY_ZERO ||
           m.m34.abs() > NEARLY_ZERO ||
           m.m43.abs() > NEARLY_ZERO ||
           (m.m44 - 1.0).abs() > NEARLY_ZERO {
            return None;
        }

        Some(ScaleOffset {
            scale: Vector2D::new(m.m11, m.m22),
            offset: Vector2D::new(m.m41, m.m42),
        })
    }

    pub fn inverse(&self) -> Self {
        ScaleOffset {
            scale: Vector2D::new(
                1.0 / self.scale.x,
                1.0 / self.scale.y,
            ),
            offset: Vector2D::new(
                -self.offset.x / self.scale.x,
                -self.offset.y / self.scale.y,
            ),
        }
    }

    pub fn offset(&self, offset: Vector2D<f32>) -> Self {
        self.accumulate(
            &ScaleOffset {
                scale: Vector2D::new(1.0, 1.0),
                offset,
            }
        )
    }

    // Produce a ScaleOffset that includes both self
    // and other. The 'self' ScaleOffset is applied
    // after other.
    pub fn accumulate(&self, other: &ScaleOffset) -> Self {
        ScaleOffset {
            scale: Vector2D::new(
                self.scale.x * other.scale.x,
                self.scale.y * other.scale.y,
            ),
            offset: Vector2D::new(
                self.offset.x + self.scale.x * other.offset.x,
                self.offset.y + self.scale.y * other.offset.y,
            ),
        }
    }

    pub fn map_rect<F, T>(&self, rect: &TypedRect<f32, F>) -> TypedRect<f32, T> {
        TypedRect::new(
            TypedPoint2D::new(
                rect.origin.x * self.scale.x + self.offset.x,
                rect.origin.y * self.scale.y + self.offset.y,
            ),
            TypedSize2D::new(
                rect.size.width * self.scale.x,
                rect.size.height * self.scale.y,
            )
        )
    }

    pub fn unmap_rect<F, T>(&self, rect: &TypedRect<f32, F>) -> TypedRect<f32, T> {
        TypedRect::new(
            TypedPoint2D::new(
                (rect.origin.x - self.offset.x) / self.scale.x,
                (rect.origin.y - self.offset.y) / self.scale.y,
            ),
            TypedSize2D::new(
                rect.size.width / self.scale.x,
                rect.size.height / self.scale.y,
            )
        )
    }

    pub fn to_transform<F, T>(&self) -> TypedTransform3D<f32, F, T> {
        TypedTransform3D::row_major(
            self.scale.x,
            0.0,
            0.0,
            0.0,

            0.0,
            self.scale.y,
            0.0,
            0.0,

            0.0,
            0.0,
            1.0,
            0.0,

            self.offset.x,
            self.offset.y,
            0.0,
            1.0,
        )
    }
}

// TODO: Implement these in euclid!
pub trait MatrixHelpers<Src, Dst> {
    fn preserves_2d_axis_alignment(&self) -> bool;
    fn has_perspective_component(&self) -> bool;
    fn has_2d_inverse(&self) -> bool;
    fn exceeds_2d_scale(&self, limit: f64) -> bool;
    fn inverse_project(&self, target: &TypedPoint2D<f32, Dst>) -> Option<TypedPoint2D<f32, Src>>;
    fn inverse_rect_footprint(&self, rect: &TypedRect<f32, Dst>) -> Option<TypedRect<f32, Src>>;
    fn transform_kind(&self) -> TransformedRectKind;
    fn is_simple_translation(&self) -> bool;
    fn is_simple_2d_translation(&self) -> bool;
}

impl<Src, Dst> MatrixHelpers<Src, Dst> for TypedTransform3D<f32, Src, Dst> {
    // A port of the preserves2dAxisAlignment function in Skia.
    // Defined in the SkMatrix44 class.
    fn preserves_2d_axis_alignment(&self) -> bool {
        if self.m14 != 0.0 || self.m24 != 0.0 {
            return false;
        }

        let mut col0 = 0;
        let mut col1 = 0;
        let mut row0 = 0;
        let mut row1 = 0;

        if self.m11.abs() > NEARLY_ZERO {
            col0 += 1;
            row0 += 1;
        }
        if self.m12.abs() > NEARLY_ZERO {
            col1 += 1;
            row0 += 1;
        }
        if self.m21.abs() > NEARLY_ZERO {
            col0 += 1;
            row1 += 1;
        }
        if self.m22.abs() > NEARLY_ZERO {
            col1 += 1;
            row1 += 1;
        }

        col0 < 2 && col1 < 2 && row0 < 2 && row1 < 2
    }

    fn has_perspective_component(&self) -> bool {
         self.m14.abs() > NEARLY_ZERO ||
         self.m24.abs() > NEARLY_ZERO ||
         self.m34.abs() > NEARLY_ZERO ||
         (self.m44 - 1.0).abs() > NEARLY_ZERO
    }

    fn has_2d_inverse(&self) -> bool {
        self.m11 * self.m22 - self.m12 * self.m21 != 0.0
    }

    // Check if the matrix post-scaling on either the X or Y axes could cause geometry
    // transformed by this matrix to have scaling exceeding the supplied limit.
    fn exceeds_2d_scale(&self, limit: f64) -> bool {
        let limit2 = (limit * limit) as f32;
        self.m11 * self.m11 + self.m12 * self.m12 > limit2 ||
        self.m21 * self.m21 + self.m22 * self.m22 > limit2
    }

    fn inverse_project(&self, target: &TypedPoint2D<f32, Dst>) -> Option<TypedPoint2D<f32, Src>> {
        let m: TypedTransform2D<f32, Src, Dst>;
        m = TypedTransform2D::column_major(
            self.m11 - target.x * self.m14,
            self.m21 - target.x * self.m24,
            self.m41 - target.x * self.m44,
            self.m12 - target.y * self.m14,
            self.m22 - target.y * self.m24,
            self.m42 - target.y * self.m44,
        );
        m.inverse().map(|inv| TypedPoint2D::new(inv.m31, inv.m32))
    }

    fn inverse_rect_footprint(&self, rect: &TypedRect<f32, Dst>) -> Option<TypedRect<f32, Src>> {
        Some(TypedRect::from_points(&[
            self.inverse_project(&rect.origin)?,
            self.inverse_project(&rect.top_right())?,
            self.inverse_project(&rect.bottom_left())?,
            self.inverse_project(&rect.bottom_right())?,
        ]))
    }

    fn transform_kind(&self) -> TransformedRectKind {
        if self.preserves_2d_axis_alignment() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }

    fn is_simple_translation(&self) -> bool {
        if (self.m11 - 1.0).abs() > NEARLY_ZERO ||
            (self.m22 - 1.0).abs() > NEARLY_ZERO ||
            (self.m33 - 1.0).abs() > NEARLY_ZERO {
            return false;
        }

        self.m12.abs() < NEARLY_ZERO && self.m13.abs() < NEARLY_ZERO &&
            self.m14.abs() < NEARLY_ZERO && self.m21.abs() < NEARLY_ZERO &&
            self.m23.abs() < NEARLY_ZERO && self.m24.abs() < NEARLY_ZERO &&
            self.m31.abs() < NEARLY_ZERO && self.m32.abs() < NEARLY_ZERO &&
            self.m34.abs() < NEARLY_ZERO
    }

    fn is_simple_2d_translation(&self) -> bool {
        if !self.is_simple_translation() {
            return false;
        }

        self.m43.abs() < NEARLY_ZERO
    }
}

pub trait RectHelpers<U>
where
    Self: Sized,
{
    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self;
    fn is_well_formed_and_nonempty(&self) -> bool;
}

impl<U> RectHelpers<U> for TypedRect<f32, U> {
    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self {
        TypedRect::new(
            TypedPoint2D::new(x0, y0),
            TypedSize2D::new(x1 - x0, y1 - y0),
        )
    }

    fn is_well_formed_and_nonempty(&self) -> bool {
        self.size.width > 0.0 && self.size.height > 0.0
    }
}

// Don't use `euclid`'s `is_empty` because that has effectively has an "and" in the conditional
// below instead of an "or".
pub fn rect_is_empty<N: PartialEq + Zero, U>(rect: &TypedRect<N, U>) -> bool {
    rect.size.width == Zero::zero() || rect.size.height == Zero::zero()
}

#[allow(dead_code)]
#[inline]
pub fn rect_from_points_f(x0: f32, y0: f32, x1: f32, y1: f32) -> Rect<f32> {
    Rect::new(Point2D::new(x0, y0), Size2D::new(x1 - x0, y1 - y0))
}

pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    (b - a) * t + a
}

#[repr(u32)]
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TransformedRectKind {
    AxisAligned = 0,
    Complex = 1,
}

#[inline(always)]
pub fn pack_as_float(value: u32) -> f32 {
    value as f32 + 0.5
}

#[inline]
fn extract_inner_rect_impl<U>(
    rect: &TypedRect<f32, U>,
    radii: &BorderRadius,
    k: f32,
) -> Option<TypedRect<f32, U>> {
    // `k` defines how much border is taken into account
    // We enforce the offsets to be rounded to pixel boundaries
    // by `ceil`-ing and `floor`-ing them

    let xl = (k * radii.top_left.width.max(radii.bottom_left.width)).ceil();
    let xr = (rect.size.width - k * radii.top_right.width.max(radii.bottom_right.width)).floor();
    let yt = (k * radii.top_left.height.max(radii.top_right.height)).ceil();
    let yb =
        (rect.size.height - k * radii.bottom_left.height.max(radii.bottom_right.height)).floor();

    if xl <= xr && yt <= yb {
        Some(TypedRect::new(
            TypedPoint2D::new(rect.origin.x + xl, rect.origin.y + yt),
            TypedSize2D::new(xr - xl, yb - yt),
        ))
    } else {
        None
    }
}

/// Return an aligned rectangle that is inside the clip region and doesn't intersect
/// any of the bounding rectangles of the rounded corners.
pub fn extract_inner_rect_safe<U>(
    rect: &TypedRect<f32, U>,
    radii: &BorderRadius,
) -> Option<TypedRect<f32, U>> {
    // value of `k==1.0` is used for extraction of the corner rectangles
    // see `SEGMENT_CORNER_*` in `clip_shared.glsl`
    extract_inner_rect_impl(rect, radii, 1.0)
}

#[cfg(test)]
pub mod test {
    use api::{LayoutTransform, LayoutVector3D};
    use super::*;
    use euclid::{Point2D, Angle, Transform3D};
    use std::f32::consts::PI;

    #[test]
    fn inverse_project() {
        let m0 = Transform3D::identity();
        let p0 = Point2D::new(1.0, 2.0);
        // an identical transform doesn't need any inverse projection
        assert_eq!(m0.inverse_project(&p0), Some(p0));
        let m1 = Transform3D::create_rotation(0.0, 1.0, 0.0, Angle::radians(PI / 3.0));
        // rotation by 60 degrees would imply scaling of X component by a factor of 2
        assert_eq!(m1.inverse_project(&p0), Some(Point2D::new(2.0, 2.0)));
    }

    fn validate_convert(xref: &LayoutTransform) {
        let so = ScaleOffset::from_transform(xref).unwrap();
        let xf = so.to_transform();
        assert!(xref.approx_eq(&xf));
    }

    #[test]
    fn scale_offset_convert() {
        let xref = LayoutTransform::create_translation(130.0, 200.0, 0.0);
        validate_convert(&xref);

        let xref = LayoutTransform::create_scale(13.0, 8.0, 1.0);
        validate_convert(&xref);

        let xref = LayoutTransform::create_scale(0.5, 0.5, 1.0)
                        .pre_translate(LayoutVector3D::new(124.0, 38.0, 0.0));
        validate_convert(&xref);

        let xref = LayoutTransform::create_translation(50.0, 240.0, 0.0)
                        .pre_mul(&LayoutTransform::create_scale(30.0, 11.0, 1.0));
        validate_convert(&xref);
    }

    fn validate_inverse(xref: &LayoutTransform) {
        let s0 = ScaleOffset::from_transform(xref).unwrap();
        let s1 = s0.inverse().accumulate(&s0);
        assert!((s1.scale.x - 1.0).abs() < NEARLY_ZERO &&
                (s1.scale.y - 1.0).abs() < NEARLY_ZERO &&
                s1.offset.x.abs() < NEARLY_ZERO &&
                s1.offset.y.abs() < NEARLY_ZERO,
                "{:?}",
                s1);
    }

    #[test]
    fn scale_offset_inverse() {
        let xref = LayoutTransform::create_translation(130.0, 200.0, 0.0);
        validate_inverse(&xref);

        let xref = LayoutTransform::create_scale(13.0, 8.0, 1.0);
        validate_inverse(&xref);

        let xref = LayoutTransform::create_scale(0.5, 0.5, 1.0)
                        .pre_translate(LayoutVector3D::new(124.0, 38.0, 0.0));
        validate_inverse(&xref);

        let xref = LayoutTransform::create_translation(50.0, 240.0, 0.0)
                        .pre_mul(&LayoutTransform::create_scale(30.0, 11.0, 1.0));
        validate_inverse(&xref);
    }

    fn validate_accumulate(x0: &LayoutTransform, x1: &LayoutTransform) {
        let x = x0.pre_mul(x1);

        let s0 = ScaleOffset::from_transform(x0).unwrap();
        let s1 = ScaleOffset::from_transform(x1).unwrap();

        let s = s0.accumulate(&s1).to_transform();

        assert!(x.approx_eq(&s), "{:?}\n{:?}", x, s);
    }

    #[test]
    fn scale_offset_accumulate() {
        let x0 = LayoutTransform::create_translation(130.0, 200.0, 0.0);
        let x1 = LayoutTransform::create_scale(7.0, 3.0, 1.0);

        validate_accumulate(&x0, &x1);
    }
}

pub trait MaxRect {
    fn max_rect() -> Self;
}

impl MaxRect for DeviceIntRect {
    fn max_rect() -> Self {
        DeviceIntRect::new(
            DeviceIntPoint::new(i32::MIN / 2, i32::MIN / 2),
            DeviceIntSize::new(i32::MAX, i32::MAX),
        )
    }
}

impl<U> MaxRect for TypedRect<f32, U> {
    fn max_rect() -> Self {
        // Having an unlimited bounding box is fine up until we try
        // to cast it to `i32`, where we get `-2147483648` for any
        // values larger than or equal to 2^31.
        //
        // Note: clamping to i32::MIN and i32::MAX is not a solution,
        // with explanation left as an exercise for the reader.
        const MAX_COORD: f32 = 1.0e9;

        TypedRect::new(
            TypedPoint2D::new(-MAX_COORD, -MAX_COORD),
            TypedSize2D::new(2.0 * MAX_COORD, 2.0 * MAX_COORD),
        )
    }
}

/// An enum that tries to avoid expensive transformation matrix calculations
/// when possible when dealing with non-perspective axis-aligned transformations.
#[derive(Debug, Clone, Copy)]
pub enum FastTransform<Src, Dst> {
    /// A simple offset, which can be used without doing any matrix math.
    Offset(TypedVector2D<f32, Src>),

    /// A 2D transformation with an inverse.
    Transform {
        transform: TypedTransform3D<f32, Src, Dst>,
        inverse: Option<TypedTransform3D<f32, Dst, Src>>,
        is_2d: bool,
    },
}

impl<Src, Dst> FastTransform<Src, Dst> {
    pub fn identity() -> Self {
        FastTransform::Offset(TypedVector2D::zero())
    }

    pub fn with_vector(offset: TypedVector2D<f32, Src>) -> Self {
        FastTransform::Offset(offset)
    }

    #[inline(always)]
    pub fn with_transform(transform: TypedTransform3D<f32, Src, Dst>) -> Self {
        if transform.is_simple_2d_translation() {
            return FastTransform::Offset(TypedVector2D::new(transform.m41, transform.m42));
        }
        let inverse = transform.inverse();
        let is_2d = transform.is_2d();
        FastTransform::Transform { transform, inverse, is_2d}
    }

    pub fn kind(&self) -> TransformedRectKind {
        match *self {
            FastTransform::Offset(_) => TransformedRectKind::AxisAligned,
            FastTransform::Transform { ref transform, .. } if transform.preserves_2d_axis_alignment() => TransformedRectKind::AxisAligned,
            FastTransform::Transform { .. } => TransformedRectKind::Complex,
        }
    }

    pub fn to_transform(&self) -> Cow<TypedTransform3D<f32, Src, Dst>> {
        match *self {
            FastTransform::Offset(offset) => Cow::Owned(
                TypedTransform3D::create_translation(offset.x, offset.y, 0.0)
            ),
            FastTransform::Transform { ref transform, .. } => Cow::Borrowed(transform),
        }
    }

    pub fn is_invertible(&self) -> bool {
        match *self {
            FastTransform::Offset(..) => true,
            FastTransform::Transform { ref inverse, .. } => inverse.is_some(),
        }
    }

    /// Return true if this is an identity transform
    pub fn is_identity(&self)-> bool {
        match *self {
            FastTransform::Offset(offset) => {
                offset == TypedVector2D::zero()
            }
            FastTransform::Transform { ref transform, .. } => {
                *transform == TypedTransform3D::identity()
            }
        }
    }

    #[inline(always)]
    pub fn pre_mul<NewSrc>(
        &self,
        other: &FastTransform<NewSrc, Src>
    ) -> FastTransform<NewSrc, Dst> {
        match (self, other) {
            (&FastTransform::Offset(ref offset), &FastTransform::Offset(ref other_offset)) => {
                let offset = TypedVector2D::from_untyped(&offset.to_untyped());
                FastTransform::Offset(offset + *other_offset)
            }
            _ => {
                let new_transform = self.to_transform().pre_mul(&other.to_transform());
                FastTransform::with_transform(new_transform)
            }
        }
    }

    #[inline(always)]
    pub fn pre_translate(&self, other_offset: &TypedVector2D<f32, Src>) -> Self {
        match *self {
            FastTransform::Offset(ref offset) =>
                FastTransform::Offset(*offset + *other_offset),
            FastTransform::Transform { transform, .. } =>
                FastTransform::with_transform(transform.pre_translate(other_offset.to_3d()))
        }
    }

    #[inline(always)]
    pub fn is_backface_visible(&self) -> bool {
        match *self {
            FastTransform::Offset(..) => false,
            FastTransform::Transform { inverse: None, .. } => false,
            //TODO: fix this properly by taking "det|M33| * det|M34| > 0"
            // see https://www.w3.org/Bugs/Public/show_bug.cgi?id=23014
            FastTransform::Transform { inverse: Some(ref inverse), .. } => inverse.m33 < 0.0,
        }
    }

    #[inline(always)]
    pub fn transform_point2d(&self, point: &TypedPoint2D<f32, Src>) -> Option<TypedPoint2D<f32, Dst>> {
        match *self {
            FastTransform::Offset(offset) => {
                let new_point = *point + offset;
                Some(TypedPoint2D::from_untyped(&new_point.to_untyped()))
            }
            FastTransform::Transform { ref transform, .. } => transform.transform_point2d(point),
        }
    }

    pub fn unapply(&self, rect: &TypedRect<f32, Dst>) -> Option<TypedRect<f32, Src>> {
        match *self {
            FastTransform::Offset(offset) =>
                Some(TypedRect::from_untyped(&rect.to_untyped().translate(&-offset.to_untyped()))),
            FastTransform::Transform { inverse: Some(ref inverse), is_2d: true, .. }  =>
                inverse.transform_rect(rect),
            FastTransform::Transform { ref transform, is_2d: false, .. } =>
                transform.inverse_rect_footprint(rect),
            FastTransform::Transform { inverse: None, .. }  => None,
        }
    }

    pub fn post_translate(&self, new_offset: TypedVector2D<f32, Dst>) -> Self {
        match *self {
            FastTransform::Offset(offset) => {
                let offset = offset.to_untyped() + new_offset.to_untyped();
                FastTransform::Offset(TypedVector2D::from_untyped(&offset))
            }
            FastTransform::Transform { ref transform, .. } => {
                let transform = transform.post_translate(new_offset.to_3d());
                FastTransform::with_transform(transform)
            }
        }
    }

    #[inline(always)]
    pub fn inverse(&self) -> Option<FastTransform<Dst, Src>> {
        match *self {
            FastTransform::Offset(offset) =>
                Some(FastTransform::Offset(TypedVector2D::new(-offset.x, -offset.y))),
            FastTransform::Transform { transform, inverse: Some(inverse), is_2d, } =>
                Some(FastTransform::Transform {
                    transform: inverse,
                    inverse: Some(transform),
                    is_2d
                }),
            FastTransform::Transform { inverse: None, .. } => None,

        }
    }
}

impl<Src, Dst> From<TypedTransform3D<f32, Src, Dst>> for FastTransform<Src, Dst> {
    fn from(transform: TypedTransform3D<f32, Src, Dst>) -> Self {
        FastTransform::with_transform(transform)
    }
}

impl<Src, Dst> From<TypedVector2D<f32, Src>> for FastTransform<Src, Dst> {
    fn from(vector: TypedVector2D<f32, Src>) -> Self {
        FastTransform::with_vector(vector)
    }
}

pub type LayoutFastTransform = FastTransform<LayoutPixel, LayoutPixel>;
pub type LayoutToWorldFastTransform = FastTransform<LayoutPixel, WorldPixel>;

pub fn project_rect<F, T>(
    transform: &TypedTransform3D<f32, F, T>,
    rect: &TypedRect<f32, F>,
    bounds: &TypedRect<f32, T>,
) -> Option<TypedRect<f32, T>>
 where F: fmt::Debug
{
    let homogens = [
        transform.transform_point2d_homogeneous(&rect.origin),
        transform.transform_point2d_homogeneous(&rect.top_right()),
        transform.transform_point2d_homogeneous(&rect.bottom_left()),
        transform.transform_point2d_homogeneous(&rect.bottom_right()),
    ];

    // Note: we only do the full frustum collision when the polygon approaches the camera plane.
    // Otherwise, it will be clamped to the screen bounds anyway.
    if homogens.iter().any(|h| h.w <= 0.0) {
        let mut clipper = Clipper::new();
        let polygon = Polygon::from_rect(*rect, 1);

        let planes = match Clipper::frustum_planes(
            transform,
            Some(*bounds),
        ) {
            Ok(planes) => planes,
            Err(..) => return None,
        };

        for plane in planes {
            clipper.add(plane);
        }

        let results = clipper.clip(polygon);
        if results.is_empty() {
            return None
        }

        Some(TypedRect::from_points(results
            .into_iter()
            // filter out parts behind the view plane
            .flat_map(|poly| &poly.points)
            .map(|p| {
                let mut homo = transform.transform_point2d_homogeneous(&p.to_2d());
                homo.w = homo.w.max(0.00000001); // avoid infinite values
                homo.to_point2d().unwrap()
            })
        ))
    } else {
        // we just checked for all the points to be in positive hemisphere, so `unwrap` is valid
        Some(TypedRect::from_points(&[
            homogens[0].to_point2d().unwrap(),
            homogens[1].to_point2d().unwrap(),
            homogens[2].to_point2d().unwrap(),
            homogens[3].to_point2d().unwrap(),
        ]))
    }
}

pub fn raster_rect_to_device_pixels(
    rect: RasterRect,
    device_pixel_scale: DevicePixelScale,
) -> DeviceRect {
    let world_rect = rect * TypedScale::new(1.0);
    let device_rect = world_rect * device_pixel_scale;
    device_rect.round_out()
}

/// Run the first callback over all elements in the array. If the callback returns true,
/// the element is removed from the array and moved to a second callback.
///
/// This is a simple implementation waiting for Vec::drain_filter to be stable.
/// When that happens, code like:
///
/// let filter = |op| {
///     match *op {
///         Enum::Foo | Enum::Bar => true,
///         Enum::Baz => false,
///     }
/// };
/// drain_filter(
///     &mut ops,
///     filter,
///     |op| {
///         match op {
///             Enum::Foo => { foo(); }
///             Enum::Bar => { bar(); }
///             Enum::Baz => { unreachable!(); }
///         }
///     },
/// );
///
/// Can be rewritten as:
///
/// let filter = |op| {
///     match *op {
///         Enum::Foo | Enum::Bar => true,
///         Enum::Baz => false,
///     }
/// };
/// for op in ops.drain_filter(filter) {
///     match op {
///         Enum::Foo => { foo(); }
///         Enum::Bar => { bar(); }
///         Enum::Baz => { unreachable!(); }
///     }
/// }
///
/// See https://doc.rust-lang.org/std/vec/struct.Vec.html#method.drain_filter
pub fn drain_filter<T, Filter, Action>(
    vec: &mut Vec<T>,
    mut filter: Filter,
    mut action: Action,
)
where
    Filter: FnMut(&mut T) -> bool,
    Action: FnMut(T)
{
    let mut i = 0;
    while i != vec.len() {
        if filter(&mut vec[i]) {
            action(vec.remove(i));
        } else {
            i += 1;
        }
    }
}

/// Clear a vector for re-use, while retaining the backing memory buffer. May shrink the buffer
/// if it's currently much larger than was actually used.
pub fn recycle_vec<T>(vec: &mut Vec<T>) {
    if vec.capacity() > 2 * vec.len() {
        // Reduce capacity of the buffer if it is a lot larger than it needs to be. This prevents
        // a frame with exceptionally large allocations to cause subsequent frames to retain
        // more memory than they need.
        vec.shrink_to_fit();
    }
    vec.clear();
}
