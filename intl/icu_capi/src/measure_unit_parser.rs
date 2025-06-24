// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    #[cfg(feature = "buffer_provider")]
    use crate::unstable::errors::ffi::DataError;
    #[cfg(feature = "buffer_provider")]
    use crate::unstable::provider::ffi::DataProvider;

    #[diplomat::opaque]
    /// An ICU4X Measurement Unit object which represents a single unit of measurement
    /// such as `meter`, `second`, `kilometer-per-hour`, `square-meter`, etc.
    ///
    /// You can create an instance of this object using [`MeasureUnitParser`] by calling the `parse` method.
    #[diplomat::rust_link(icu::experimental::measure::measureunit::MeasureUnit, Struct)]
    pub struct MeasureUnit(pub icu_experimental::measure::measureunit::MeasureUnit);

    #[diplomat::opaque]
    /// An ICU4X Measure Unit Parser object, capable of parsing the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`MeasureUnit`].
    #[diplomat::rust_link(icu::experimental::measure::parser::MeasureUnitParser, Struct)]
    pub struct MeasureUnitParser(pub icu_experimental::measure::parser::MeasureUnitParser);

    impl MeasureUnitParser {
        /// Construct a new [`MeasureUnitParser`] instance using compiled data.
        #[diplomat::rust_link(
            icu::experimental::measure::parser::MeasureUnitParser::new,
            FnInStruct
        )]
        #[diplomat::attr(auto, constructor)]
        #[cfg(feature = "compiled_data")]
        pub fn create() -> Box<MeasureUnitParser> {
            Box::new(MeasureUnitParser(
                icu_experimental::measure::parser::MeasureUnitParser::default(),
            ))
        }
        /// Construct a new [`MeasureUnitParser`] instance using a particular data source.
        #[diplomat::rust_link(
            icu::experimental::measure::parser::MeasureUnitParser::new,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor = "with_provider")]
        #[cfg(feature = "buffer_provider")]
        pub fn create_with_provider(
            provider: &DataProvider,
        ) -> Result<Box<MeasureUnitParser>, DataError> {
            Ok(Box::new(MeasureUnitParser(
                icu_experimental::measure::parser::MeasureUnitParser::try_new_with_buffer_provider(
                    provider.get()?,
                )?,
            )))
        }

        #[diplomat::rust_link(
            icu::experimental::measure::parser::MeasureUnitParser::parse,
            FnInStruct
        )]
        pub fn parse(&self, unit_id: &DiplomatStr) -> Option<Box<MeasureUnit>> {
            self.0
                .try_from_utf8(unit_id)
                .ok()
                .map(MeasureUnit)
                .map(Box::new)
        }
    }
}
