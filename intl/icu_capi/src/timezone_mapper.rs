// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_timezone::{
        TimeZoneBcp47Id, TimeZoneIdMapper, TimeZoneIdMapperWithFastCanonicalization,
    };
    use tinystr::TinyAsciiStr;

    /// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
    /// It also supports normalizing and canonicalizing the IANA strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapper, Struct)]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapper::as_borrowed, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed, Struct, hidden)]
    #[diplomat::rust_link(icu::timezone::NormalizedIana, Struct, hidden)]
    pub struct ICU4XTimeZoneIdMapper(pub TimeZoneIdMapper);

    impl ICU4XTimeZoneIdMapper {
        #[diplomat::rust_link(icu::timezone::TimeZoneIdMapper::new, FnInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors), constructor)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XTimeZoneIdMapper>, ICU4XError> {
            Ok(Box::new(ICU4XTimeZoneIdMapper(call_constructor!(
                TimeZoneIdMapper::new [r => Ok(r)],
                TimeZoneIdMapper::try_new_with_any_provider,
                TimeZoneIdMapper::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed::iana_to_bcp47, FnInStruct)]
        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperBorrowed::iana_bytes_to_bcp47,
            FnInStruct,
            hidden
        )]
        pub fn iana_to_bcp47(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            if let Some(s) = handle.iana_bytes_to_bcp47(value) {
                Ok(s.0.write_to(write)?)
            } else {
                Err(ICU4XError::TimeZoneInvalidIdError)
            }
        }

        #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperBorrowed::normalize_iana, FnInStruct)]
        pub fn normalize_iana(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            // Validate the UTF-8 here because it gets echoed back to the writeable
            let value = core::str::from_utf8(value)?;
            if let Some(s) = handle.normalize_iana(value) {
                Ok(s.0.write_to(write)?)
            } else {
                Err(ICU4XError::TimeZoneInvalidIdError)
            }
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperBorrowed::canonicalize_iana,
            FnInStruct
        )]
        pub fn canonicalize_iana(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            // Validate the UTF-8 here because it gets echoed back to the writeable
            let value = core::str::from_utf8(value)?;
            if let Some(s) = handle.canonicalize_iana(value) {
                Ok(s.0.write_to(write)?)
            } else {
                Err(ICU4XError::TimeZoneInvalidIdError)
            }
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperBorrowed::find_canonical_iana_from_bcp47,
            FnInStruct
        )]
        pub fn find_canonical_iana_from_bcp47(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            let bcp47_id = TimeZoneBcp47Id(
                TinyAsciiStr::from_bytes(value).map_err(|_| ICU4XError::TimeZoneInvalidIdError)?,
            );
            if let Some(s) = handle.find_canonical_iana_from_bcp47(bcp47_id) {
                Ok(s.write_to(write)?)
            } else {
                Err(ICU4XError::TimeZoneInvalidIdError)
            }
        }
    }

    /// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
    ///
    /// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
    /// It also supports normalizing and canonicalizing the IANA strings.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::TimeZoneIdMapperWithFastCanonicalization, Struct)]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalization::as_borrowed,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalization::inner,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed,
        Struct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::inner,
        FnInStruct,
        hidden
    )]
    pub struct ICU4XTimeZoneIdMapperWithFastCanonicalization(
        pub TimeZoneIdMapperWithFastCanonicalization<TimeZoneIdMapper>,
    );

    impl ICU4XTimeZoneIdMapperWithFastCanonicalization {
        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalization::new,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors), constructor)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XTimeZoneIdMapperWithFastCanonicalization>, ICU4XError> {
            Ok(Box::new(ICU4XTimeZoneIdMapperWithFastCanonicalization(
                call_constructor!(
                    TimeZoneIdMapperWithFastCanonicalization::new [r => Ok(r)],
                    TimeZoneIdMapperWithFastCanonicalization::try_new_with_any_provider,
                    TimeZoneIdMapperWithFastCanonicalization::try_new_with_buffer_provider,
                    provider,
                )?,
            )))
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::canonicalize_iana,
            FnInStruct
        )]
        pub fn canonicalize_iana(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            // Validate the UTF-8 here because it gets echoed back to the writeable
            let value = core::str::from_utf8(value)?;
            if let Some(s) = handle.canonicalize_iana(value) {
                Ok(s.0.write_to(write)?)
            } else {
                Err(ICU4XError::TimeZoneInvalidIdError)
            }
        }

        #[diplomat::rust_link(
            icu::timezone::TimeZoneIdMapperWithFastCanonicalizationBorrowed::canonical_iana_from_bcp47,
            FnInStruct
        )]
        pub fn canonical_iana_from_bcp47(
            &self,
            value: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            use writeable::Writeable;
            let handle = self.0.as_borrowed();
            let bcp47_id = TimeZoneBcp47Id(
                TinyAsciiStr::from_bytes(value).map_err(|_| ICU4XError::TimeZoneInvalidIdError)?,
            );
            if let Some(s) = handle.canonical_iana_from_bcp47(bcp47_id) {
                Ok(s.write_to(write)?)
            } else {
                Err(ICU4XError::TimeZoneInvalidIdError)
            }
        }
    }
}
