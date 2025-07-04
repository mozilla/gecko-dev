use crate::types::FluentType;
use intl_memoizer::Memoizable;
use unic_langid::LanguageIdentifier;

/// This trait contains thread-safe methods which extend [`intl_memoizer::IntlLangMemoizer`].
/// It is used as the generic bound in this crate when a memoizer is needed.
pub trait MemoizerKind: 'static {
    fn new(lang: LanguageIdentifier) -> Self
    where
        Self: Sized;

    /// A threadsafe variant of `with_try_get` from [`intl_memoizer::IntlLangMemoizer`].
    /// The generics enforce that `Self` and its arguments are actually threadsafe.
    ///
    /// `I` - The [Memoizable](intl_memoizer::Memoizable) internationalization formatter.
    ///
    /// `R` - The result from the format operation.
    ///
    /// `U` - The callback that accepts the instance of the intl formatter, and generates
    ///       some kind of results `R`.
    fn with_try_get_threadsafe<I, R, U>(&self, args: I::Args, callback: U) -> Result<R, I::Error>
    where
        Self: Sized,
        I: Memoizable + Send + Sync + 'static,
        I::Args: Send + Sync + 'static,
        U: FnOnce(&I) -> R;

    /// Wires up the `as_string` or `as_string_threadsafe` variants for [`FluentType`].
    fn stringify_value(&self, value: &dyn FluentType) -> std::borrow::Cow<'static, str>;
}
