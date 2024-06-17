// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;

    use icu_calendar::Time;

    use crate::errors::ffi::ICU4XError;

    #[diplomat::opaque]
    /// An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond
    #[diplomat::rust_link(icu::calendar::Time, Struct)]
    pub struct ICU4XTime(pub Time);

    impl ICU4XTime {
        /// Creates a new [`ICU4XTime`] given field values
        #[diplomat::rust_link(icu::calendar::Time::try_new, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Time::new, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors), constructor)]
        pub fn create(
            hour: u8,
            minute: u8,
            second: u8,
            nanosecond: u32,
        ) -> Result<Box<ICU4XTime>, ICU4XError> {
            let hour = hour.try_into()?;
            let minute = minute.try_into()?;
            let second = second.try_into()?;
            let nanosecond = nanosecond.try_into()?;
            let time = Time {
                hour,
                minute,
                second,
                nanosecond,
            };
            Ok(Box::new(ICU4XTime(time)))
        }

        /// Creates a new [`ICU4XTime`] representing midnight (00:00.000).
        #[diplomat::rust_link(icu::calendar::Time::midnight, FnInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "midnight")]
        pub fn create_midnight() -> Result<Box<ICU4XTime>, ICU4XError> {
            let time = Time::midnight();
            Ok(Box::new(ICU4XTime(time)))
        }

        /// Returns the hour in this time
        #[diplomat::rust_link(icu::calendar::Time::hour, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn hour(&self) -> u8 {
            self.0.hour.into()
        }
        /// Returns the minute in this time
        #[diplomat::rust_link(icu::calendar::Time::minute, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn minute(&self) -> u8 {
            self.0.minute.into()
        }
        /// Returns the second in this time
        #[diplomat::rust_link(icu::calendar::Time::second, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn second(&self) -> u8 {
            self.0.second.into()
        }
        /// Returns the nanosecond in this time
        #[diplomat::rust_link(icu::calendar::Time::nanosecond, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn nanosecond(&self) -> u32 {
            self.0.nanosecond.into()
        }
    }
}
