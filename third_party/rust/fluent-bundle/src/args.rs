use std::borrow::Cow;
use std::iter::FromIterator;

use crate::types::FluentValue;

/// Fluent messages can use arguments in order to programmatically add values to a
/// translated string. For instance, in a localized application you may wish to display
/// a user's email count. This could be done with the following message.
///
/// `msg-key = Hello, { $user }. You have { $emailCount } messages.`
///
/// Here `$user` and `$emailCount` are the arguments, which can be filled with values.
///
/// The [`FluentArgs`] struct is the map from the argument name (for example `$user`) to
/// the argument value (for example "John".) The logic to apply these to write these
/// to messages is elsewhere, this struct just stores the value.
///
/// # Example
///
/// ```
/// use fluent_bundle::{FluentArgs, FluentBundle, FluentResource};
///
/// let mut args = FluentArgs::new();
/// args.set("user", "John");
/// args.set("emailCount", 5);
///
/// let res = FluentResource::try_new(r#"
///
/// msg-key = Hello, { $user }. You have { $emailCount } messages.
///
/// "#.to_string())
///     .expect("Failed to parse FTL.");
///
/// let mut bundle = FluentBundle::default();
///
/// // For this example, we'll turn on BiDi support.
/// // Please, be careful when doing it, it's a risky move.
/// bundle.set_use_isolating(false);
///
/// bundle.add_resource(res)
///     .expect("Failed to add a resource.");
///
/// let mut err = vec![];
///
/// let msg = bundle.get_message("msg-key")
///     .expect("Failed to retrieve a message.");
/// let value = msg.value()
///     .expect("Failed to retrieve a value.");
///
/// assert_eq!(
///     bundle.format_pattern(value, Some(&args), &mut err),
///     "Hello, John. You have 5 messages."
/// );
/// ```
#[derive(Debug, Default)]
pub struct FluentArgs<'args>(Vec<(Cow<'args, str>, FluentValue<'args>)>);

impl<'args> FluentArgs<'args> {
    /// Creates a new empty argument map.
    pub fn new() -> Self {
        Self::default()
    }

    /// Pre-allocates capacity for arguments.
    pub fn with_capacity(capacity: usize) -> Self {
        Self(Vec::with_capacity(capacity))
    }

    /// Gets the [`FluentValue`] at the `key` if it exists.
    pub fn get<K>(&self, key: K) -> Option<&FluentValue<'args>>
    where
        K: Into<Cow<'args, str>>,
    {
        let key = key.into();
        if let Ok(idx) = self.0.binary_search_by_key(&&key, |(k, _)| k) {
            Some(&self.0[idx].1)
        } else {
            None
        }
    }

    /// Sets the key value pair.
    pub fn set<K, V>(&mut self, key: K, value: V)
    where
        K: Into<Cow<'args, str>>,
        V: Into<FluentValue<'args>>,
    {
        let key = key.into();
        match self.0.binary_search_by_key(&&key, |(k, _)| k) {
            Ok(idx) => self.0[idx] = (key, value.into()),
            Err(idx) => self.0.insert(idx, (key, value.into())),
        };
    }

    /// Iterate over a tuple of the key an [`FluentValue`].
    pub fn iter(&self) -> impl Iterator<Item = (&str, &FluentValue)> {
        self.0.iter().map(|(k, v)| (k.as_ref(), v))
    }
}

impl<'args, K, V> FromIterator<(K, V)> for FluentArgs<'args>
where
    K: Into<Cow<'args, str>>,
    V: Into<FluentValue<'args>>,
{
    fn from_iter<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = (K, V)>,
    {
        let iter = iter.into_iter();
        let mut args = if let Some(size) = iter.size_hint().1 {
            FluentArgs::with_capacity(size)
        } else {
            FluentArgs::new()
        };

        for (k, v) in iter {
            args.set(k, v);
        }

        args
    }
}

impl<'args> IntoIterator for FluentArgs<'args> {
    type Item = (Cow<'args, str>, FluentValue<'args>);
    type IntoIter = std::vec::IntoIter<Self::Item>;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn replace_existing_arguments() {
        let mut args = FluentArgs::new();

        args.set("name", "John");
        args.set("emailCount", 5);
        assert_eq!(args.0.len(), 2);
        assert_eq!(
            args.get("name"),
            Some(&FluentValue::String(Cow::Borrowed("John")))
        );
        assert_eq!(args.get("emailCount"), Some(&FluentValue::try_number("5")));

        args.set("name", "Jane");
        args.set("emailCount", 7);
        assert_eq!(args.0.len(), 2);
        assert_eq!(
            args.get("name"),
            Some(&FluentValue::String(Cow::Borrowed("Jane")))
        );
        assert_eq!(args.get("emailCount"), Some(&FluentValue::try_number("7")));
    }
}
