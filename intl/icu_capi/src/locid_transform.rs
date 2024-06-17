// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_locid_transform::{LocaleCanonicalizer, LocaleExpander, TransformResult};

    use crate::{locale::ffi::ICU4XLocale, provider::ffi::ICU4XDataProvider};

    use crate::errors::ffi::ICU4XError;

    #[diplomat::rust_link(icu::locid_transform::TransformResult, Enum)]
    #[diplomat::enum_convert(TransformResult)]
    pub enum ICU4XTransformResult {
        Modified,
        Unmodified,
    }

    /// A locale canonicalizer.
    #[diplomat::rust_link(icu::locid_transform::LocaleCanonicalizer, Struct)]
    #[diplomat::opaque]
    pub struct ICU4XLocaleCanonicalizer(LocaleCanonicalizer);

    impl ICU4XLocaleCanonicalizer {
        /// Create a new [`ICU4XLocaleCanonicalizer`].
        #[diplomat::rust_link(icu::locid_transform::LocaleCanonicalizer::new, FnInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors), constructor)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleCanonicalizer>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleCanonicalizer(call_constructor!(
                LocaleCanonicalizer::new [r => Ok(r)],
                LocaleCanonicalizer::try_new_with_any_provider,
                LocaleCanonicalizer::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Create a new [`ICU4XLocaleCanonicalizer`] with extended data.
        #[diplomat::rust_link(
            icu::locid_transform::LocaleCanonicalizer::new_with_expander,
            FnInStruct
        )]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "extended")]
        pub fn create_extended(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleCanonicalizer>, ICU4XError> {
            let expander = call_constructor!(
                LocaleExpander::new_extended [r => Ok(r)],
                LocaleExpander::try_new_with_any_provider,
                LocaleExpander::try_new_with_buffer_provider,
                provider,
            )?;
            Ok(Box::new(ICU4XLocaleCanonicalizer(call_constructor!(
                LocaleCanonicalizer::new_with_expander [r => Ok(r)],
                LocaleCanonicalizer::try_new_with_expander_with_any_provider,
                LocaleCanonicalizer::try_new_with_expander_with_buffer_provider,
                provider,
                expander
            )?)))
        }

        #[diplomat::rust_link(icu::locid_transform::LocaleCanonicalizer::canonicalize, FnInStruct)]
        pub fn canonicalize(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.canonicalize(&mut locale.0).into()
        }
    }

    /// A locale expander.
    #[diplomat::rust_link(icu::locid_transform::LocaleExpander, Struct)]
    #[diplomat::opaque]
    pub struct ICU4XLocaleExpander(pub LocaleExpander);

    impl ICU4XLocaleExpander {
        /// Create a new [`ICU4XLocaleExpander`].
        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::new, FnInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors), constructor)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleExpander>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleExpander(call_constructor!(
                LocaleExpander::new [r => Ok(r)],
                LocaleExpander::try_new_with_any_provider,
                LocaleExpander::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        /// Create a new [`ICU4XLocaleExpander`] with extended data.
        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::new_extended, FnInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "extended")]
        pub fn create_extended(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleExpander>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleExpander(call_constructor!(
                LocaleExpander::new_extended [r => Ok(r)],
                LocaleExpander::try_new_with_any_provider,
                LocaleExpander::try_new_with_buffer_provider,
                provider,
            )?)))
        }

        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::maximize, FnInStruct)]
        pub fn maximize(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.maximize(&mut locale.0).into()
        }

        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::minimize, FnInStruct)]
        pub fn minimize(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.minimize(&mut locale.0).into()
        }

        #[diplomat::rust_link(
            icu::locid_transform::LocaleExpander::minimize_favor_script,
            FnInStruct
        )]
        pub fn minimize_favor_script(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.minimize_favor_script(&mut locale.0).into()
        }
    }
}
