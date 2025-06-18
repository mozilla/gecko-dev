// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::RoundingIncrement;
use fixed_decimal::Sign;
use fixed_decimal::SignDisplay;

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use fixed_decimal::{DoublePrecision, FixedDecimal};
    use writeable::Writeable;

    use crate::errors::ffi::ICU4XError;

    #[diplomat::opaque]
    #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
    pub struct ICU4XFixedDecimal(pub FixedDecimal);

    /// The sign of a FixedDecimal, as shown in formatting.
    #[diplomat::rust_link(fixed_decimal::Sign, Enum)]
    pub enum ICU4XFixedDecimalSign {
        /// No sign (implicitly positive, e.g., 1729).
        None,
        /// A negative sign, e.g., -1729.
        Negative,
        /// An explicit positive sign, e.g., +1729.
        Positive,
    }

    /// ECMA-402 compatible sign display preference.
    #[diplomat::rust_link(fixed_decimal::SignDisplay, Enum)]
    pub enum ICU4XFixedDecimalSignDisplay {
        Auto,
        Never,
        Always,
        ExceptZero,
        Negative,
    }

    // TODO: Rename to `ICU4XFixedDecimalRoundingIncrement` for 2.0
    /// Increment used in a rounding operation.
    #[diplomat::rust_link(fixed_decimal::RoundingIncrement, Enum)]
    pub enum ICU4XRoundingIncrement {
        MultiplesOf1,
        MultiplesOf2,
        MultiplesOf5,
        MultiplesOf25,
    }

    impl ICU4XFixedDecimal {
        /// Construct an [`ICU4XFixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_i32")]
        pub fn create_from_i32(v: i32) -> Box<ICU4XFixedDecimal> {
            Box::new(ICU4XFixedDecimal(FixedDecimal::from(v)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_u32")]
        pub fn create_from_u32(v: u32) -> Box<ICU4XFixedDecimal> {
            Box::new(ICU4XFixedDecimal(FixedDecimal::from(v)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, rename = "from_int")]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_i64")]
        pub fn create_from_i64(v: i64) -> Box<ICU4XFixedDecimal> {
            Box::new(ICU4XFixedDecimal(FixedDecimal::from(v)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an integer.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal, Struct)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_u64")]
        pub fn create_from_u64(v: u64) -> Box<ICU4XFixedDecimal> {
            Box::new(ICU4XFixedDecimal(FixedDecimal::from(v)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an integer-valued float
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(dart, disable)]
        #[diplomat::attr(dart, rename = "from_double_with_integer_precision")]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_f64_with_integer_precision")]
        pub fn create_from_f64_with_integer_precision(
            f: f64,
        ) -> Result<Box<ICU4XFixedDecimal>, ICU4XError> {
            let precision = DoublePrecision::Integer;
            Ok(Box::new(ICU4XFixedDecimal(FixedDecimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an float, with a given power of 10 for the lower magnitude
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(dart, rename = "from_double_with_lower_magnitude")]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_f64_with_lower_magnitude")]
        pub fn create_from_f64_with_lower_magnitude(
            f: f64,
            magnitude: i16,
        ) -> Result<Box<ICU4XFixedDecimal>, ICU4XError> {
            let precision = DoublePrecision::Magnitude(magnitude);
            Ok(Box::new(ICU4XFixedDecimal(FixedDecimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an float, for a given number of significant digits
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(dart, rename = "from_double_with_significant_digits")]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_f64_with_significant_digits")]
        pub fn create_from_f64_with_significant_digits(
            f: f64,
            digits: u8,
        ) -> Result<Box<ICU4XFixedDecimal>, ICU4XError> {
            let precision = DoublePrecision::SignificantDigits(digits);
            Ok(Box::new(ICU4XFixedDecimal(FixedDecimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from an float, with enough digits to recover
        /// the original floating point in IEEE 754 without needing trailing zeros
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::try_from_f64, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FloatPrecision, Enum)]
        #[diplomat::rust_link(fixed_decimal::DoublePrecision, Enum, hidden)]
        #[diplomat::attr(dart, rename = "from_double_with_double_precision")]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_f64_with_floating_precision")]
        pub fn create_from_f64_with_floating_precision(
            f: f64,
        ) -> Result<Box<ICU4XFixedDecimal>, ICU4XError> {
            let precision = DoublePrecision::Floating;
            Ok(Box::new(ICU4XFixedDecimal(FixedDecimal::try_from_f64(
                f, precision,
            )?)))
        }

        /// Construct an [`ICU4XFixedDecimal`] from a string.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::from_str, FnInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_string")]
        pub fn create_from_string(v: &DiplomatStr) -> Result<Box<ICU4XFixedDecimal>, ICU4XError> {
            Ok(Box::new(ICU4XFixedDecimal(FixedDecimal::try_from(v)?)))
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::digit_at, FnInStruct)]
        pub fn digit_at(&self, magnitude: i16) -> u8 {
            self.0.digit_at(magnitude)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::magnitude_range, FnInStruct)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn magnitude_start(&self) -> i16 {
            *self.0.magnitude_range().start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::magnitude_range, FnInStruct)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn magnitude_end(&self) -> i16 {
            *self.0.magnitude_range().end()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::nonzero_magnitude_start, FnInStruct)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn nonzero_magnitude_start(&self) -> i16 {
            self.0.nonzero_magnitude_start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::nonzero_magnitude_end, FnInStruct)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn nonzero_magnitude_end(&self) -> i16 {
            self.0.nonzero_magnitude_end()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::is_zero, FnInStruct)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn is_zero(&self) -> bool {
            self.0.is_zero()
        }

        /// Multiply the [`ICU4XFixedDecimal`] by a given power of ten.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::multiply_pow10, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::multiplied_pow10, FnInStruct, hidden)]
        pub fn multiply_pow10(&mut self, power: i16) {
            self.0.multiply_pow10(power)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::sign, FnInStruct)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn sign(&self) -> ICU4XFixedDecimalSign {
            self.0.sign().into()
        }

        /// Set the sign of the [`ICU4XFixedDecimal`].
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::set_sign, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_sign, FnInStruct, hidden)]
        #[diplomat::attr(supports = accessors, setter = "sign")]
        pub fn set_sign(&mut self, sign: ICU4XFixedDecimalSign) {
            self.0.set_sign(sign.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::apply_sign_display, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_sign_display, FnInStruct, hidden)]
        pub fn apply_sign_display(&mut self, sign_display: ICU4XFixedDecimalSignDisplay) {
            self.0.apply_sign_display(sign_display.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trim_start, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trimmed_start, FnInStruct, hidden)]
        pub fn trim_start(&mut self) {
            self.0.trim_start()
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trim_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trimmed_end, FnInStruct, hidden)]
        pub fn trim_end(&mut self) {
            self.0.trim_end()
        }

        /// Zero-pad the [`ICU4XFixedDecimal`] on the left to a particular position
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::pad_start, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::padded_start, FnInStruct, hidden)]
        pub fn pad_start(&mut self, position: i16) {
            self.0.pad_start(position)
        }

        /// Zero-pad the [`ICU4XFixedDecimal`] on the right to a particular position
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::pad_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::padded_end, FnInStruct, hidden)]
        pub fn pad_end(&mut self, position: i16) {
            self.0.pad_end(position)
        }

        /// Truncate the [`ICU4XFixedDecimal`] on the left to a particular position, deleting digits if necessary. This is useful for, e.g. abbreviating years
        /// ("2022" -> "22")
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::set_max_position, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::with_max_position, FnInStruct, hidden)]
        pub fn set_max_position(&mut self, position: i16) {
            self.0.set_max_position(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trunc, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trunced, FnInStruct, hidden)]
        pub fn trunc(&mut self, position: i16) {
            self.0.trunc(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::trunc_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::trunced_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn trunc_to_increment(&mut self, position: i16, increment: ICU4XRoundingIncrement) {
            self.0.trunc_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_trunc, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_trunced, FnInStruct, hidden)]
        pub fn half_trunc(&mut self, position: i16) {
            self.0.half_trunc(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_trunc_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::half_trunced_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn half_trunc_to_increment(
            &mut self,
            position: i16,
            increment: ICU4XRoundingIncrement,
        ) {
            self.0.half_trunc_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::expand, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::expanded, FnInStruct, hidden)]
        pub fn expand(&mut self, position: i16) {
            self.0.expand(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::expand_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::expanded_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn expand_to_increment(&mut self, position: i16, increment: ICU4XRoundingIncrement) {
            self.0.expand_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_expand, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_expanded, FnInStruct, hidden)]
        pub fn half_expand(&mut self, position: i16) {
            self.0.half_expand(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_expand_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::half_expanded_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn half_expand_to_increment(
            &mut self,
            position: i16,
            increment: ICU4XRoundingIncrement,
        ) {
            self.0.half_expand_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::ceil, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::ceiled, FnInStruct, hidden)]
        pub fn ceil(&mut self, position: i16) {
            self.0.ceil(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::ceil_to_increment, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::ceiled_to_increment, FnInStruct, hidden)]
        pub fn ceil_to_increment(&mut self, position: i16, increment: ICU4XRoundingIncrement) {
            self.0.ceil_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_ceil, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_ceiled, FnInStruct, hidden)]
        pub fn half_ceil(&mut self, position: i16) {
            self.0.half_ceil(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_ceil_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::half_ceiled_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn half_ceil_to_increment(&mut self, position: i16, increment: ICU4XRoundingIncrement) {
            self.0.half_ceil_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::floor, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::floored, FnInStruct, hidden)]
        pub fn floor(&mut self, position: i16) {
            self.0.floor(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::floor_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::floored_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn floor_to_increment(&mut self, position: i16, increment: ICU4XRoundingIncrement) {
            self.0.floor_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_floor, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_floored, FnInStruct, hidden)]
        pub fn half_floor(&mut self, position: i16) {
            self.0.half_floor(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_floor_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::half_floored_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn half_floor_to_increment(
            &mut self,
            position: i16,
            increment: ICU4XRoundingIncrement,
        ) {
            self.0.half_floor_to_increment(position, increment.into())
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_even, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_evened, FnInStruct, hidden)]
        pub fn half_even(&mut self, position: i16) {
            self.0.half_even(position)
        }

        #[diplomat::rust_link(fixed_decimal::FixedDecimal::half_even_to_increment, FnInStruct)]
        #[diplomat::rust_link(
            fixed_decimal::FixedDecimal::half_evened_to_increment,
            FnInStruct,
            hidden
        )]
        pub fn half_even_to_increment(&mut self, position: i16, increment: ICU4XRoundingIncrement) {
            self.0.half_even_to_increment(position, increment.into())
        }

        /// Concatenates `other` to the end of `self`.
        ///
        /// If successful, `other` will be set to 0 and a successful status is returned.
        ///
        /// If not successful, `other` will be unchanged and an error is returned.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::concatenate_end, FnInStruct)]
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::concatenated_end, FnInStruct, hidden)]
        pub fn concatenate_end(&mut self, other: &mut ICU4XFixedDecimal) -> Result<(), ()> {
            let x = core::mem::take(&mut other.0);
            self.0.concatenate_end(x).map_err(|y| {
                other.0 = y;
            })
        }

        /// Format the [`ICU4XFixedDecimal`] as a string.
        #[diplomat::rust_link(fixed_decimal::FixedDecimal::write_to, FnInStruct)]
        #[diplomat::attr(supports = stringifiers, stringifier)]
        pub fn to_string(&self, to: &mut diplomat_runtime::DiplomatWriteable) {
            let _ = self.0.write_to(to);
        }
    }
}

impl From<ffi::ICU4XFixedDecimalSign> for Sign {
    fn from(other: ffi::ICU4XFixedDecimalSign) -> Self {
        match other {
            ffi::ICU4XFixedDecimalSign::None => Self::None,
            ffi::ICU4XFixedDecimalSign::Negative => Self::Negative,
            ffi::ICU4XFixedDecimalSign::Positive => Self::Positive,
        }
    }
}

impl From<Sign> for ffi::ICU4XFixedDecimalSign {
    fn from(other: Sign) -> Self {
        match other {
            Sign::None => Self::None,
            Sign::Negative => Self::Negative,
            Sign::Positive => Self::Positive,
        }
    }
}

impl From<ffi::ICU4XFixedDecimalSignDisplay> for SignDisplay {
    fn from(other: ffi::ICU4XFixedDecimalSignDisplay) -> Self {
        match other {
            ffi::ICU4XFixedDecimalSignDisplay::Auto => Self::Auto,
            ffi::ICU4XFixedDecimalSignDisplay::Never => Self::Never,
            ffi::ICU4XFixedDecimalSignDisplay::Always => Self::Always,
            ffi::ICU4XFixedDecimalSignDisplay::ExceptZero => Self::ExceptZero,
            ffi::ICU4XFixedDecimalSignDisplay::Negative => Self::Negative,
        }
    }
}

impl From<ffi::ICU4XRoundingIncrement> for RoundingIncrement {
    fn from(value: ffi::ICU4XRoundingIncrement) -> Self {
        match value {
            ffi::ICU4XRoundingIncrement::MultiplesOf1 => RoundingIncrement::MultiplesOf1,
            ffi::ICU4XRoundingIncrement::MultiplesOf2 => RoundingIncrement::MultiplesOf2,
            ffi::ICU4XRoundingIncrement::MultiplesOf5 => RoundingIncrement::MultiplesOf5,
            ffi::ICU4XRoundingIncrement::MultiplesOf25 => RoundingIncrement::MultiplesOf25,
        }
    }
}
