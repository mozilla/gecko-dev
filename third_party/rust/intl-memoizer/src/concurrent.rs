//! Contains thread-safe variants.
use super::*;
use std::sync::Mutex;

/// A thread-safe version of the [`intl_memoizer::IntlLangMemoizer`](super::IntlLangMemoizer).
/// See the single-thread version for more documentation.
#[derive(Debug)]
pub struct IntlLangMemoizer {
    lang: LanguageIdentifier,
    map: Mutex<type_map::concurrent::TypeMap>,
}

impl IntlLangMemoizer {
    /// Create a new [`IntlLangMemoizer`] that is unique to a specific [`LanguageIdentifier`]
    pub fn new(lang: LanguageIdentifier) -> Self {
        Self {
            lang,
            map: Mutex::new(type_map::concurrent::TypeMap::new()),
        }
    }

    /// Lazily initialize and run a formatter. See
    /// [`intl_memoizer::IntlLangMemoizer::with_try_get`](crate::IntlLangMemoizer::with_try_get)
    /// for documentation.
    pub fn with_try_get<I, R, U>(&self, args: I::Args, cb: U) -> Result<R, I::Error>
    where
        Self: Sized,
        I: Memoizable + Sync + Send + 'static,
        I::Args: Send + Sync + 'static,
        U: FnOnce(&I) -> R,
    {
        let mut map = self.map.lock().unwrap();
        let cache = map
            .entry::<HashMap<I::Args, I>>()
            .or_insert_with(HashMap::new);

        let e = match cache.entry(args.clone()) {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => {
                let val = I::construct(self.lang.clone(), args)?;
                entry.insert(val)
            }
        };
        Ok(cb(e))
    }
}
