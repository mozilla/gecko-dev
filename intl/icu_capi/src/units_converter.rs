// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::{errors::ffi::ICU4XError, provider::ffi::ICU4XDataProvider};
    use alloc::boxed::Box;
    use diplomat_runtime::DiplomatStr;
    use icu_experimental::units::converter::UnitsConverter;
    use icu_experimental::units::converter_factory::ConverterFactory;
    use icu_experimental::units::measureunit::MeasureUnit;
    use icu_experimental::units::measureunit::MeasureUnitParser;

    #[diplomat::opaque]
    /// An ICU4X Units Converter Factory object, capable of creating converters a [`ICU4XUnitsConverter`]
    /// for converting between two [`ICU4XMeasureUnit`]s.
    /// Also, it can parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`ICU4XMeasureUnit`].
    #[diplomat::rust_link(icu::experimental::units::converter_factory::ConverterFactory, Struct)]
    pub struct ICU4XUnitsConverterFactory(pub ConverterFactory);

    impl ICU4XUnitsConverterFactory {
        /// Construct a new [`ICU4XUnitsConverterFactory`] instance.
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::new,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors), constructor)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XUnitsConverterFactory>, ICU4XError> {
            Ok(Box::new(ICU4XUnitsConverterFactory(call_constructor!(
                ConverterFactory::new [r => Ok(r)],
                ConverterFactory::try_new_with_any_provider,
                ConverterFactory::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Creates a new [`ICU4XUnitsConverter`] from the input and output [`ICU4XMeasureUnit`]s.
        /// Returns nothing if the conversion between the two units is not possible.
        /// For example, conversion between `meter` and `second` is not possible.
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::converter,
            FnInStruct
        )]
        pub fn converter(
            &self,
            from: &ICU4XMeasureUnit,
            to: &ICU4XMeasureUnit,
        ) -> Option<Box<ICU4XUnitsConverter>> {
            let converter: Option<UnitsConverter<f64>> = self.0.converter(&from.0, &to.0);
            Some(ICU4XUnitsConverter(converter?).into())
        }

        /// Creates a parser to parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`ICU4XMeasureUnit`].
        #[diplomat::rust_link(
            icu::experimental::units::converter_factory::ConverterFactory::parser,
            FnInStruct
        )]
        pub fn parser<'a>(&'a self) -> Box<ICU4XMeasureUnitParser<'a>> {
            ICU4XMeasureUnitParser(self.0.parser()).into()
        }
    }

    #[diplomat::opaque]
    /// An ICU4X Measurement Unit parser object which is capable of parsing the CLDR unit identifier
    /// (e.g. `meter-per-square-second`) and get the [`ICU4XMeasureUnit`].
    #[diplomat::rust_link(icu::experimental::units::measureunit::MeasureUnitParser, Struct)]
    pub struct ICU4XMeasureUnitParser<'a>(pub MeasureUnitParser<'a>);

    impl<'a> ICU4XMeasureUnitParser<'a> {
        /// Parses the CLDR unit identifier (e.g. `meter-per-square-second`) and returns the corresponding [`ICU4XMeasureUnit`].
        /// Returns an error if the unit identifier is not valid.
        #[diplomat::rust_link(
            icu::experimental::units::measureunit::MeasureUnitParser::parse,
            FnInStruct
        )]
        pub fn parse(&self, unit_id: &DiplomatStr) -> Result<Box<ICU4XMeasureUnit>, ICU4XError> {
            Ok(Box::new(ICU4XMeasureUnit(self.0.try_from_bytes(unit_id)?)))
        }
    }

    #[diplomat::opaque]
    /// An ICU4X Measurement Unit object which represents a single unit of measurement
    /// such as `meter`, `second`, `kilometer-per-hour`, `square-meter`, etc.
    ///
    /// You can create an instance of this object using [`ICU4XMeasureUnitParser`] by calling the `parse_measure_unit` method.
    #[diplomat::rust_link(icu::experimental::units::measureunit::MeasureUnit, Struct)]
    pub struct ICU4XMeasureUnit(pub MeasureUnit);

    #[diplomat::opaque]
    /// An ICU4X Units Converter object, capable of converting between two [`ICU4XMeasureUnit`]s.
    ///
    /// You can create an instance of this object using [`ICU4XUnitsConverterFactory`] by calling the `converter` method.
    #[diplomat::rust_link(icu::experimental::units::converter::UnitsConverter, Struct)]
    pub struct ICU4XUnitsConverter(pub UnitsConverter<f64>);
    impl ICU4XUnitsConverter {
        /// Converts the input value in float from the input unit to the output unit (that have been used to create this converter).
        /// NOTE:
        ///   The conversion using floating-point operations is not as accurate as the conversion using ratios.
        #[diplomat::rust_link(
            icu::experimental::units::converter::UnitsConverter::convert,
            FnInStruct
        )]
        #[diplomat::attr(dart, rename = "convert_double")]
        pub fn convert_f64(&self, value: f64) -> f64 {
            self.0.convert(&value)
        }

        /// Clones the current [`ICU4XUnitsConverter`] object.
        #[diplomat::rust_link(
            icu::experimental::units::converter::UnitsConverter::clone,
            FnInStruct
        )]
        pub fn clone(&self) -> Box<Self> {
            Box::new(ICU4XUnitsConverter(self.0.clone()))
        }
    }
}
