// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use alloc::boxed::Box;
    use core::str;
    use icu_locid::extensions::unicode::Key;
    use icu_locid::subtags::{Language, Region, Script};
    use icu_locid::Locale;
    use writeable::Writeable;

    use crate::common::ffi::ICU4XOrdering;

    #[diplomat::opaque]
    /// An ICU4X Locale, capable of representing strings like `"en-US"`.
    #[diplomat::rust_link(icu::locid::Locale, Struct)]
    pub struct ICU4XLocale(pub Locale);

    impl ICU4XLocale {
        /// Construct an [`ICU4XLocale`] from an locale identifier.
        ///
        /// This will run the complete locale parsing algorithm. If code size and
        /// performance are critical and the locale is of a known shape (such as
        /// `aa-BB`) use `create_und`, `set_language`, `set_script`, and `set_region`.
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        #[diplomat::rust_link(icu::locid::Locale::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "from_string")]
        pub fn create_from_string(name: &DiplomatStr) -> Result<Box<ICU4XLocale>, ICU4XError> {
            Ok(Box::new(ICU4XLocale(Locale::try_from_bytes(name)?)))
        }

        /// Construct a default undefined [`ICU4XLocale`] "und".
        #[diplomat::rust_link(icu::locid::Locale::UND, AssociatedConstantInStruct)]
        #[diplomat::attr(all(supports = constructors, supports = fallible_constructors, supports = named_constructors), named_constructor = "und")]
        pub fn create_und() -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(Locale::UND))
        }

        /// Clones the [`ICU4XLocale`].
        #[diplomat::rust_link(icu::locid::Locale, Struct)]
        pub fn clone(&self) -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(self.0.clone()))
        }

        /// Write a string representation of the `LanguageIdentifier` part of
        /// [`ICU4XLocale`] to `write`.
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn basename(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.id.write_to(write)?;
            Ok(())
        }

        /// Write a string representation of the unicode extension to `write`
        #[diplomat::rust_link(icu::locid::Locale::extensions, StructField)]
        pub fn get_unicode_extension(
            &self,
            bytes: &DiplomatStr,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0
                .extensions
                .unicode
                .keywords
                .get(&Key::try_from_bytes(bytes)?)
                .ok_or(ICU4XError::LocaleUndefinedSubtagError)?
                .write_to(write)?;
            Ok(())
        }

        /// Write a string representation of [`ICU4XLocale`] language to `write`
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn language(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.id.language.write_to(write)?;
            Ok(())
        }

        /// Set the language part of the [`ICU4XLocale`].
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        #[diplomat::attr(supports = accessors, setter = "language")]
        pub fn set_language(&mut self, bytes: &DiplomatStr) -> Result<(), ICU4XError> {
            self.0.id.language = if bytes.is_empty() {
                Language::UND
            } else {
                Language::try_from_bytes(bytes)?
            };
            Ok(())
        }

        /// Write a string representation of [`ICU4XLocale`] region to `write`
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn region(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            if let Some(region) = self.0.id.region {
                region.write_to(write)?;
                Ok(())
            } else {
                Err(ICU4XError::LocaleUndefinedSubtagError)
            }
        }

        /// Set the region part of the [`ICU4XLocale`].
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        #[diplomat::attr(supports = accessors, setter = "region")]
        pub fn set_region(&mut self, bytes: &DiplomatStr) -> Result<(), ICU4XError> {
            self.0.id.region = if bytes.is_empty() {
                None
            } else {
                Some(Region::try_from_bytes(bytes)?)
            };
            Ok(())
        }

        /// Write a string representation of [`ICU4XLocale`] script to `write`
        #[diplomat::rust_link(icu::locid::Locale::id, StructField)]
        #[diplomat::attr(supports = accessors, getter)]
        pub fn script(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            if let Some(script) = self.0.id.script {
                script.write_to(write)?;
                Ok(())
            } else {
                Err(ICU4XError::LocaleUndefinedSubtagError)
            }
        }

        /// Set the script part of the [`ICU4XLocale`]. Pass an empty string to remove the script.
        #[diplomat::rust_link(icu::locid::Locale::try_from_bytes, FnInStruct)]
        #[diplomat::attr(supports = accessors, setter = "script")]
        pub fn set_script(&mut self, bytes: &DiplomatStr) -> Result<(), ICU4XError> {
            self.0.id.script = if bytes.is_empty() {
                None
            } else {
                Some(Script::try_from_bytes(bytes)?)
            };
            Ok(())
        }

        /// Best effort locale canonicalizer that doesn't need any data
        ///
        /// Use ICU4XLocaleCanonicalizer for better control and functionality
        #[diplomat::rust_link(icu::locid::Locale::canonicalize, FnInStruct)]
        pub fn canonicalize(
            bytes: &DiplomatStr,
            write: &mut DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            Locale::canonicalize(bytes)?.write_to(write)?;
            Ok(())
        }
        /// Write a string representation of [`ICU4XLocale`] to `write`
        #[diplomat::rust_link(icu::locid::Locale::write_to, FnInStruct)]
        #[diplomat::attr(supports = stringifiers, stringifier)]
        pub fn to_string(
            &self,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.write_to(write)?;
            Ok(())
        }

        #[diplomat::rust_link(icu::locid::Locale::normalizing_eq, FnInStruct)]
        pub fn normalizing_eq(&self, other: &DiplomatStr) -> bool {
            if let Ok(other) = str::from_utf8(other) {
                self.0.normalizing_eq(other)
            } else {
                // invalid UTF8 won't be allowed in locales anyway
                false
            }
        }

        #[diplomat::rust_link(icu::locid::Locale::strict_cmp, FnInStruct)]
        #[diplomat::attr(*, disable)]
        pub fn strict_cmp(&self, other: &DiplomatStr) -> ICU4XOrdering {
            self.0.strict_cmp(other).into()
        }

        #[diplomat::rust_link(icu::locid::Locale::strict_cmp, FnInStruct)]
        #[diplomat::skip_if_ast]
        #[diplomat::attr(dart, rename = "compareToString")]
        pub fn strict_cmp_(&self, other: &DiplomatStr) -> core::cmp::Ordering {
            self.0.strict_cmp(other)
        }

        #[diplomat::rust_link(icu::locid::Locale::total_cmp, FnInStruct)]
        #[diplomat::attr(*, disable)]
        pub fn total_cmp(&self, other: &Self) -> ICU4XOrdering {
            self.0.total_cmp(&other.0).into()
        }

        #[diplomat::rust_link(icu::locid::Locale::total_cmp, FnInStruct)]
        #[diplomat::skip_if_ast]
        #[diplomat::attr(supports = comparators, comparison)]
        pub fn total_cmp_(&self, other: &Self) -> core::cmp::Ordering {
            self.0.total_cmp(&other.0)
        }

        /// Deprecated
        ///
        /// Use `create_from_string("en").
        #[cfg(feature = "provider_test")]
        #[diplomat::attr(supports = constructors, disable)]
        pub fn create_en() -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(icu_locid::locale!("en")))
        }

        /// Deprecated
        ///
        /// Use `create_from_string("bn").
        #[cfg(feature = "provider_test")]
        #[diplomat::attr(supports = constructors, disable)]
        pub fn create_bn() -> Box<ICU4XLocale> {
            Box::new(ICU4XLocale(icu_locid::locale!("bn")))
        }
    }
}

impl ffi::ICU4XLocale {
    pub fn to_datalocale(&self) -> icu_provider::DataLocale {
        (&self.0).into()
    }
}
