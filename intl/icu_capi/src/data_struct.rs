// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "icu_decimal")]

use alloc::borrow::Cow;

#[diplomat::bridge]
pub mod ffi {

    #[cfg(feature = "icu_decimal")]
    use crate::errors::ffi::ICU4XError;
    use alloc::boxed::Box;
    use icu_provider::AnyPayload;
    #[cfg(feature = "icu_decimal")]
    use icu_provider::DataPayload;

    #[diplomat::opaque]
    /// A generic data struct to be used by ICU4X
    ///
    /// This can be used to construct a StructDataProvider.
    #[diplomat::attr(*, disable)]
    pub struct ICU4XDataStruct(#[allow(dead_code)] AnyPayload);

    impl ICU4XDataStruct {
        /// Construct a new DecimalSymbolsV1 data struct.
        ///
        /// Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according
        /// to the WHATWG Encoding Standard.
        #[diplomat::rust_link(icu::decimal::provider::DecimalSymbolsV1, Struct)]
        #[allow(clippy::too_many_arguments)]
        pub fn create_decimal_symbols_v1(
            plus_sign_prefix: &DiplomatStr,
            plus_sign_suffix: &DiplomatStr,
            minus_sign_prefix: &DiplomatStr,
            minus_sign_suffix: &DiplomatStr,
            decimal_separator: &DiplomatStr,
            grouping_separator: &DiplomatStr,
            primary_group_size: u8,
            secondary_group_size: u8,
            min_group_size: u8,
            digits: &[DiplomatChar],
        ) -> Result<Box<ICU4XDataStruct>, ICU4XError> {
            use super::str_to_cow;
            use icu_decimal::provider::{
                AffixesV1, DecimalSymbolsV1, DecimalSymbolsV1Marker, GroupingSizesV1,
            };
            let digits = if digits.len() == 10 {
                let mut new_digits = ['\0'; 10];
                for (old, new) in digits.iter().zip(new_digits.iter_mut()) {
                    *new = char::from_u32(*old).ok_or(ICU4XError::DataStructValidityError)?;
                }
                new_digits
            } else {
                return Err(ICU4XError::DataStructValidityError);
            };
            let plus_sign_affixes = AffixesV1 {
                prefix: str_to_cow(plus_sign_prefix),
                suffix: str_to_cow(plus_sign_suffix),
            };
            let minus_sign_affixes = AffixesV1 {
                prefix: str_to_cow(minus_sign_prefix),
                suffix: str_to_cow(minus_sign_suffix),
            };
            let grouping_sizes = GroupingSizesV1 {
                primary: primary_group_size,
                secondary: secondary_group_size,
                min_grouping: min_group_size,
            };

            let symbols = DecimalSymbolsV1 {
                plus_sign_affixes,
                minus_sign_affixes,
                decimal_separator: str_to_cow(decimal_separator),
                grouping_separator: str_to_cow(grouping_separator),
                grouping_sizes,
                digits,
            };

            let payload: DataPayload<DecimalSymbolsV1Marker> = DataPayload::from_owned(symbols);
            Ok(Box::new(ICU4XDataStruct(payload.wrap_into_any_payload())))
        }
    }
}

fn str_to_cow(s: &diplomat_runtime::DiplomatStr) -> Cow<'static, str> {
    if s.is_empty() {
        Cow::default()
    } else {
        Cow::Owned(alloc::string::String::from_utf8_lossy(s).into_owned())
    }
}
