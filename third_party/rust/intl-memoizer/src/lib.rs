//! This crate contains a memoizer for internationalization formatters. Often it is
//! expensive (in terms of performance and memory) to construct a formatter, but then
//! relatively cheap to run the format operation.
//!
//! The [`IntlMemoizer`] is the main struct that creates a per-locale [`IntlLangMemoizer`].

use std::cell::RefCell;
use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::hash::Hash;
use std::rc::{Rc, Weak};
use unic_langid::LanguageIdentifier;

pub mod concurrent;

/// The trait that needs to be implemented for each intl formatter that needs to be
/// memoized.
pub trait Memoizable {
    /// Type of the arguments that are used to construct the formatter.
    type Args: 'static + Eq + Hash + Clone;

    /// Type of any errors that can occur during the construction process.
    type Error;

    /// Construct a formatter. This maps the [`Self::Args`] type to the actual constructor
    /// for an intl formatter.
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error>
    where
        Self: std::marker::Sized;
}

/// The [`IntlLangMemoizer`] can memoize multiple constructed internationalization
/// formatters, and their configuration for a single locale. For instance, given "en-US",
/// a memorizer could retain 3 `DateTimeFormat` instances, and a `PluralRules`.
///
/// For memoizing with multiple locales, see [`IntlMemoizer`].
///
/// # Example
///
/// The code example does the following steps:
///
/// 1. Create a static counter
/// 2. Create an `ExampleFormatter`
/// 3. Implement [`Memoizable`] for `ExampleFormatter`.
/// 4. Use `IntlLangMemoizer::with_try_get` to run `ExampleFormatter::format`
/// 5. Demonstrate the memoization using the static counter
///
/// ```
/// use intl_memoizer::{IntlLangMemoizer, Memoizable};
/// use unic_langid::LanguageIdentifier;
///
/// // Create a static counter so that we can demonstrate the side effects of when
/// // the memoizer re-constructs an API.
///
/// static mut INTL_EXAMPLE_CONSTRUCTS: u32 = 0;
/// fn increment_constructs() {
///     unsafe {
///         INTL_EXAMPLE_CONSTRUCTS += 1;
///     }
/// }
///
/// fn get_constructs_count() -> u32 {
///     unsafe { INTL_EXAMPLE_CONSTRUCTS }
/// }
///
/// /// Create an example formatter, that doesn't really do anything useful. In a real
/// /// implementation, this could be a PluralRules or DateTimeFormat struct.
/// struct ExampleFormatter {
///     lang: LanguageIdentifier,
///     /// This is here to show how to initiate the API with an argument.
///     prefix: String,
/// }
///
/// impl ExampleFormatter {
///     /// Perform an example format by printing information about the formatter
///     /// configuration, and the arguments passed into the individual format operation.
///     fn format(&self, example_string: &str) -> String {
///         format!(
///             "{} lang({}) string({})",
///             self.prefix, self.lang, example_string
///         )
///     }
/// }
///
/// /// Multiple classes of structs may be add1ed to the memoizer, with the restriction
/// /// that they must implement the `Memoizable` trait.
/// impl Memoizable for ExampleFormatter {
///     /// The arguments will be passed into the constructor. Here a single `String`
///     /// will be used as a prefix to the formatting operation.
///     type Args = (String,);
///
///     /// If the constructor is fallible, than errors can be described here.
///     type Error = ();
///
///     /// This function wires together the `Args` and `Error` type to construct
///     /// the intl API. In our example, there is
///     fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
///         // Keep track for example purposes that this was constructed.
///         increment_constructs();
///
///         Ok(Self {
///             lang,
///             prefix: args.0,
///         })
///     }
/// }
///
/// // The following demonstrates how these structs are actually used with the memoizer.
///
/// // Construct a new memoizer.
/// let lang = "en-US".parse().expect("Failed to parse.");
/// let memoizer = IntlLangMemoizer::new(lang);
///
/// // These arguments are passed into the constructor for `ExampleFormatter`.
/// let construct_args = (String::from("prefix:"),);
/// let message1 = "The format operation will run";
/// let message2 = "ExampleFormatter will be re-used, when a second format is run";
///
/// // Run `IntlLangMemoizer::with_try_get`. The name of the method means "with" an
/// // intl formatter, "try and get" the result. See the method documentation for
/// // more details.
///
/// let result1 = memoizer
///     .with_try_get::<ExampleFormatter, _, _>(construct_args.clone(), |intl_example| {
///         intl_example.format(message1)
///     });
///
/// // The memoized instance of `ExampleFormatter` will be re-used.
/// let result2 = memoizer
///     .with_try_get::<ExampleFormatter, _, _>(construct_args.clone(), |intl_example| {
///         intl_example.format(message2)
///     });
///
/// assert_eq!(
///     result1.unwrap(),
///     "prefix: lang(en-US) string(The format operation will run)"
/// );
/// assert_eq!(
///     result2.unwrap(),
///     "prefix: lang(en-US) string(ExampleFormatter will be re-used, when a second format is run)"
/// );
/// assert_eq!(
///     get_constructs_count(),
///     1,
///     "The constructor was only run once."
/// );
///
/// let construct_args = (String::from("re-init:"),);
///
/// // Since the constructor args changed, `ExampleFormatter` will be re-constructed.
/// let result1 = memoizer
///     .with_try_get::<ExampleFormatter, _, _>(construct_args.clone(), |intl_example| {
///         intl_example.format(message1)
///     });
///
/// // The memoized instance of `ExampleFormatter` will be re-used.
/// let result2 = memoizer
///     .with_try_get::<ExampleFormatter, _, _>(construct_args.clone(), |intl_example| {
///         intl_example.format(message2)
///     });
///
/// assert_eq!(
///     result1.unwrap(),
///     "re-init: lang(en-US) string(The format operation will run)"
/// );
/// assert_eq!(
///     result2.unwrap(),
///     "re-init: lang(en-US) string(ExampleFormatter will be re-used, when a second format is run)"
/// );
/// assert_eq!(
///     get_constructs_count(),
///     2,
///     "The constructor was invalidated and ran again."
/// );
/// ```
#[derive(Debug)]
pub struct IntlLangMemoizer {
    lang: LanguageIdentifier,
    map: RefCell<type_map::TypeMap>,
}

impl IntlLangMemoizer {
    /// Create a new [`IntlLangMemoizer`] that is unique to a specific
    /// [`LanguageIdentifier`]
    pub fn new(lang: LanguageIdentifier) -> Self {
        Self {
            lang,
            map: RefCell::new(type_map::TypeMap::new()),
        }
    }

    /// `with_try_get` means `with` an internationalization formatter, `try` and `get` a result.
    /// The (potentially expensive) constructor for the formatter (such as `PluralRules` or
    /// `DateTimeFormat`) will be memoized and only constructed once for a given
    /// `construct_args`. After that the format operation can be run multiple times
    /// inexpensively.
    ///
    /// The first generic argument `I` must be provided, but the `R` and `U` will be
    /// deduced by the typing of the `callback` argument that is provided.
    ///
    /// I - The memoizable intl object, for instance a `PluralRules` instance. This
    ///     must implement the Memoizable trait.
    ///
    /// R - The return result from the callback `U`.
    ///
    /// U - The callback function. Takes an instance of `I` as the first parameter and
    ///     returns the R value.
    pub fn with_try_get<I, R, U>(&self, construct_args: I::Args, callback: U) -> Result<R, I::Error>
    where
        Self: Sized,
        I: Memoizable + 'static,
        U: FnOnce(&I) -> R,
    {
        let mut map = self
            .map
            .try_borrow_mut()
            .expect("Cannot use memoizer reentrantly");
        let cache = map
            .entry::<HashMap<I::Args, I>>()
            .or_insert_with(HashMap::new);

        let e = match cache.entry(construct_args.clone()) {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => {
                let val = I::construct(self.lang.clone(), construct_args)?;
                entry.insert(val)
            }
        };
        Ok(callback(e))
    }
}

/// [`IntlMemoizer`] is designed to handle lazily-initialized references to
/// internationalization formatters.
///
/// Constructing a new formatter is often expensive in terms of memory and performance,
/// and the instance is often read-only during its lifetime. The format operations in
/// comparison are relatively cheap.
///
/// Because of this relationship, it can be helpful to memoize the constructors, and
/// re-use them across multiple format operations. This strategy is used where all
/// instances of intl APIs such as `PluralRules`, `DateTimeFormat` etc. are memoized
/// between all `FluentBundle` instances.
///
/// # Example
///
/// For a more complete example of the memoization, see the [`IntlLangMemoizer`] documentation.
/// This example provides a higher-level overview.
///
/// ```
/// # use intl_memoizer::{IntlMemoizer, IntlLangMemoizer, Memoizable};
/// # use unic_langid::LanguageIdentifier;
/// # use std::rc::Rc;
/// #
/// # struct ExampleFormatter {
/// #     lang: LanguageIdentifier,
/// #     prefix: String,
/// # }
/// #
/// # impl ExampleFormatter {
/// #     fn format(&self, example_string: &str) -> String {
/// #         format!(
/// #             "{} lang({}) string({})",
/// #             self.prefix, self.lang, example_string
/// #         )
/// #     }
/// # }
/// #
/// # impl Memoizable for ExampleFormatter {
/// #     type Args = (String,);
/// #     type Error = ();
/// #     fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
/// #         Ok(Self {
/// #             lang,
/// #             prefix: args.0,
/// #         })
/// #     }
/// # }
/// #
/// let mut memoizer = IntlMemoizer::default();
///
/// // The memoziation happens per-locale.
/// let en_us = "en-US".parse().expect("Failed to parse.");
/// let en_us_memoizer: Rc<IntlLangMemoizer> = memoizer.get_for_lang(en_us);
///
/// // These arguments are passed into the constructor for `ExampleFormatter`. The
/// // construct_args will be used for determining the memoization, but the message
/// // can be different and re-use the constructed instance.
/// let construct_args = (String::from("prefix:"),);
/// let message = "The format operation will run";
///
/// // Use the `ExampleFormatter` from the `IntlLangMemoizer` example. It returns a
/// // string that demonstrates the configuration of the formatter. This step will
/// // construct a new formatter if needed, and run the format operation.
/// //
/// // See `IntlLangMemoizer` for more details on this step.
/// let en_us_result = en_us_memoizer
///     .with_try_get::<ExampleFormatter, _, _>(construct_args.clone(), |intl_example| {
///         intl_example.format(message)
///     });
///
/// // The example formatter constructs a string with diagnostic information about
/// // the configuration.
/// assert_eq!(
///     en_us_result.unwrap(),
///     "prefix: lang(en-US) string(The format operation will run)"
/// );
///
/// // The process can be repeated for a new locale.
///
/// let de_de = "de-DE".parse().expect("Failed to parse.");
/// let de_de_memoizer: Rc<IntlLangMemoizer> = memoizer.get_for_lang(de_de);
///
/// let de_de_result = de_de_memoizer
///     .with_try_get::<ExampleFormatter, _, _>(construct_args.clone(), |intl_example| {
///         intl_example.format(message)
///     });
///
/// assert_eq!(
///     de_de_result.unwrap(),
///     "prefix: lang(de-DE) string(The format operation will run)"
/// );
/// ```
#[derive(Default)]
pub struct IntlMemoizer {
    map: HashMap<LanguageIdentifier, Weak<IntlLangMemoizer>>,
}

impl IntlMemoizer {
    /// Get a [`IntlLangMemoizer`] for a given language. If one does not exist for
    /// a locale, it will be constructed and weakly retained. See [`IntlLangMemoizer`]
    /// for more detailed documentation how to use it.
    pub fn get_for_lang(&mut self, lang: LanguageIdentifier) -> Rc<IntlLangMemoizer> {
        match self.map.entry(lang.clone()) {
            Entry::Vacant(empty) => {
                let entry = Rc::new(IntlLangMemoizer::new(lang));
                empty.insert(Rc::downgrade(&entry));
                entry
            }
            Entry::Occupied(mut entry) => {
                if let Some(entry) = entry.get().upgrade() {
                    entry
                } else {
                    let e = Rc::new(IntlLangMemoizer::new(lang));
                    entry.insert(Rc::downgrade(&e));
                    e
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use fluent_langneg::{negotiate_languages, NegotiationStrategy};
    use intl_pluralrules::{PluralCategory, PluralRuleType, PluralRules as IntlPluralRules};
    use std::{sync::Arc, thread};

    struct PluralRules(pub IntlPluralRules);

    impl PluralRules {
        pub fn new(
            lang: LanguageIdentifier,
            pr_type: PluralRuleType,
        ) -> Result<Self, &'static str> {
            let default_lang: LanguageIdentifier = "en".parse().unwrap();
            let pr_lang = negotiate_languages(
                &[lang],
                &IntlPluralRules::get_locales(pr_type),
                Some(&default_lang),
                NegotiationStrategy::Lookup,
            )[0]
            .clone();

            Ok(Self(IntlPluralRules::create(pr_lang, pr_type)?))
        }
    }

    impl Memoizable for PluralRules {
        type Args = (PluralRuleType,);
        type Error = &'static str;
        fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
            Self::new(lang, args.0)
        }
    }

    #[test]
    fn test_single_thread() {
        let lang: LanguageIdentifier = "en".parse().unwrap();

        let mut memoizer = IntlMemoizer::default();
        {
            let en_memoizer = memoizer.get_for_lang(lang.clone());

            let result = en_memoizer
                .with_try_get::<PluralRules, _, _>((PluralRuleType::CARDINAL,), |cb| cb.0.select(5))
                .unwrap();
            assert_eq!(result, Ok(PluralCategory::OTHER));
        }

        {
            let en_memoizer = memoizer.get_for_lang(lang);

            let result = en_memoizer
                .with_try_get::<PluralRules, _, _>((PluralRuleType::CARDINAL,), |cb| cb.0.select(5))
                .unwrap();
            assert_eq!(result, Ok(PluralCategory::OTHER));
        }
    }

    #[test]
    fn test_concurrent() {
        let lang: LanguageIdentifier = "en".parse().unwrap();
        let memoizer = Arc::new(concurrent::IntlLangMemoizer::new(lang));
        let mut threads = vec![];

        // Spawn four threads that all use the PluralRules.
        for _ in 0..4 {
            let memoizer = Arc::clone(&memoizer);
            threads.push(thread::spawn(move || {
                memoizer
                    .with_try_get::<PluralRules, _, _>((PluralRuleType::CARDINAL,), |cb| {
                        cb.0.select(5)
                    })
                    .expect("Failed to get a PluralRules result.")
            }));
        }

        for thread in threads.drain(..) {
            let result = thread.join().expect("Failed to join thread.");
            assert_eq!(result, Ok(PluralCategory::OTHER));
        }
    }
}
