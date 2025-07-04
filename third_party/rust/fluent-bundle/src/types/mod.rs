//! `types` module contains types necessary for Fluent runtime
//! value handling.
//! The core struct is [`FluentValue`] which is a type that can be passed
//! to the [`FluentBundle::format_pattern`](crate::bundle::FluentBundle) as an argument, it can be passed
//! to any Fluent Function, and any function may return it.
//!
//! This part of functionality is not fully hashed out yet, since we're waiting
//! for the internationalization APIs to mature, at which point all number
//! formatting operations will be moved out of Fluent.
//!
//! For now, [`FluentValue`] can be a string, a number, or a custom [`FluentType`]
//! which allows users of the library to implement their own types of values,
//! such as dates, or more complex structures needed for their bindings.
mod number;
mod plural;

pub use number::*;
use plural::PluralRules;

use std::any::Any;
use std::borrow::{Borrow, Cow};
use std::fmt;
use std::str::FromStr;

use intl_pluralrules::{PluralCategory, PluralRuleType};

use crate::memoizer::MemoizerKind;
use crate::resolver::Scope;
use crate::resource::FluentResource;

/// Custom types can implement the [`FluentType`] trait in order to generate a string
/// value for use in the message generation process.
pub trait FluentType: fmt::Debug + AnyEq + 'static {
    /// Create a clone of the underlying type.
    fn duplicate(&self) -> Box<dyn FluentType + Send>;

    /// Convert the custom type into a string value, for instance a custom `DateTime`
    /// type could return "Oct. 27, 2022".
    fn as_string(&self, intls: &intl_memoizer::IntlLangMemoizer) -> Cow<'static, str>;

    /// Convert the custom type into a string value, for instance a custom `DateTime`
    /// type could return "Oct. 27, 2022". This operation is provided the threadsafe
    /// [`IntlLangMemoizer`](intl_memoizer::concurrent::IntlLangMemoizer).
    fn as_string_threadsafe(
        &self,
        intls: &intl_memoizer::concurrent::IntlLangMemoizer,
    ) -> Cow<'static, str>;
}

impl PartialEq for dyn FluentType + Send {
    fn eq(&self, other: &Self) -> bool {
        self.equals(other.as_any())
    }
}

pub trait AnyEq: Any + 'static {
    fn equals(&self, other: &dyn Any) -> bool;
    fn as_any(&self) -> &dyn Any;
}

impl<T: Any + PartialEq> AnyEq for T {
    fn equals(&self, other: &dyn Any) -> bool {
        other.downcast_ref::<Self>() == Some(self)
    }
    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// The `FluentValue` enum represents values which can be formatted to a String.
///
/// Those values are either passed as arguments to [`FluentBundle::format_pattern`] or
/// produced by functions, or generated in the process of pattern resolution.
///
/// [`FluentBundle::format_pattern`]: crate::bundle::FluentBundle::format_pattern
#[derive(Debug)]
pub enum FluentValue<'source> {
    String(Cow<'source, str>),
    Number(FluentNumber),
    Custom(Box<dyn FluentType + Send>),
    None,
    Error,
}

impl PartialEq for FluentValue<'_> {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (FluentValue::String(s), FluentValue::String(s2)) => s == s2,
            (FluentValue::Number(s), FluentValue::Number(s2)) => s == s2,
            (FluentValue::Custom(s), FluentValue::Custom(s2)) => s == s2,
            _ => false,
        }
    }
}

impl Clone for FluentValue<'_> {
    fn clone(&self) -> Self {
        match self {
            FluentValue::String(s) => FluentValue::String(s.clone()),
            FluentValue::Number(s) => FluentValue::Number(s.clone()),
            FluentValue::Custom(s) => {
                let new_value: Box<dyn FluentType + Send> = s.duplicate();
                FluentValue::Custom(new_value)
            }
            FluentValue::Error => FluentValue::Error,
            FluentValue::None => FluentValue::None,
        }
    }
}

impl<'source> FluentValue<'source> {
    /// Attempts to parse the string representation of a `value` that supports
    /// [`ToString`] into a [`FluentValue::Number`]. If it fails, it will instead
    /// convert it to a [`FluentValue::String`].
    ///
    /// ```
    /// use fluent_bundle::types::{FluentNumber, FluentNumberOptions, FluentValue};
    ///
    /// // "2" parses into a `FluentNumber`
    /// assert_eq!(
    ///     FluentValue::try_number("2"),
    ///     FluentValue::Number(FluentNumber::new(2.0, FluentNumberOptions::default()))
    /// );
    ///
    /// // Floats can be parsed as well.
    /// assert_eq!(
    ///     FluentValue::try_number("3.141569"),
    ///     FluentValue::Number(FluentNumber::new(
    ///         3.141569,
    ///         FluentNumberOptions {
    ///             minimum_fraction_digits: Some(6),
    ///             ..Default::default()
    ///         }
    ///     ))
    /// );
    ///
    /// // When a value is not a valid number, it falls back to a `FluentValue::String`
    /// assert_eq!(
    ///     FluentValue::try_number("A string"),
    ///     FluentValue::String("A string".into())
    /// );
    /// ```
    pub fn try_number(value: &'source str) -> Self {
        if let Ok(number) = FluentNumber::from_str(value) {
            number.into()
        } else {
            value.into()
        }
    }

    /// Checks to see if two [`FluentValues`](FluentValue) match each other by having the
    /// same type and contents. The special exception is in the case of a string being
    /// compared to a number. Here attempt to check that the plural rule category matches.
    ///
    /// ```
    /// use fluent_bundle::resolver::Scope;
    /// use fluent_bundle::{types::FluentValue, FluentBundle, FluentResource};
    /// use unic_langid::langid;
    ///
    /// let langid_ars = langid!("en");
    /// let bundle: FluentBundle<FluentResource> = FluentBundle::new(vec![langid_ars]);
    /// let scope = Scope::new(&bundle, None, None);
    ///
    /// // Matching examples:
    /// assert!(FluentValue::try_number("2").matches(&FluentValue::try_number("2"), &scope));
    /// assert!(FluentValue::from("fluent").matches(&FluentValue::from("fluent"), &scope));
    /// assert!(
    ///     FluentValue::from("one").matches(&FluentValue::try_number("1"), &scope),
    ///     "Plural rules are matched."
    /// );
    ///
    /// // Non-matching examples:
    /// assert!(!FluentValue::try_number("2").matches(&FluentValue::try_number("3"), &scope));
    /// assert!(!FluentValue::from("fluent").matches(&FluentValue::from("not fluent"), &scope));
    /// assert!(!FluentValue::from("two").matches(&FluentValue::try_number("100"), &scope),);
    /// ```
    pub fn matches<R: Borrow<FluentResource>, M>(
        &self,
        other: &FluentValue,
        scope: &Scope<R, M>,
    ) -> bool
    where
        M: MemoizerKind,
    {
        match (self, other) {
            (FluentValue::String(a), FluentValue::String(b)) => a == b,
            (FluentValue::Number(a), FluentValue::Number(b)) => a == b,
            (FluentValue::String(a), FluentValue::Number(b)) => {
                let cat = match a.as_ref() {
                    "zero" => PluralCategory::ZERO,
                    "one" => PluralCategory::ONE,
                    "two" => PluralCategory::TWO,
                    "few" => PluralCategory::FEW,
                    "many" => PluralCategory::MANY,
                    "other" => PluralCategory::OTHER,
                    _ => return false,
                };
                // This string matches a plural rule keyword. Check if the number
                // matches the plural rule category.
                let r#type = match b.options.r#type {
                    FluentNumberType::Cardinal => PluralRuleType::CARDINAL,
                    FluentNumberType::Ordinal => PluralRuleType::ORDINAL,
                };
                scope
                    .bundle
                    .intls
                    .with_try_get_threadsafe::<PluralRules, _, _>((r#type,), |pr| {
                        pr.0.select(b) == Ok(cat)
                    })
                    .unwrap()
            }
            _ => false,
        }
    }

    /// Write out a string version of the [`FluentValue`] to `W`.
    pub fn write<W, R, M>(&self, w: &mut W, scope: &Scope<R, M>) -> fmt::Result
    where
        W: fmt::Write,
        R: Borrow<FluentResource>,
        M: MemoizerKind,
    {
        if let Some(formatter) = &scope.bundle.formatter {
            if let Some(val) = formatter(self, &scope.bundle.intls) {
                return w.write_str(&val);
            }
        }
        match self {
            FluentValue::String(s) => w.write_str(s),
            FluentValue::Number(n) => w.write_str(&n.as_string()),
            FluentValue::Custom(s) => w.write_str(&scope.bundle.intls.stringify_value(&**s)),
            FluentValue::Error => Ok(()),
            FluentValue::None => Ok(()),
        }
    }

    /// Converts the [`FluentValue`] to a string.
    ///
    /// Clones inner values when owned, borrowed data is not cloned.
    /// Prefer using [`FluentValue::into_string()`] when possible.
    pub fn as_string<R: Borrow<FluentResource>, M>(&self, scope: &Scope<R, M>) -> Cow<'source, str>
    where
        M: MemoizerKind,
    {
        if let Some(formatter) = &scope.bundle.formatter {
            if let Some(val) = formatter(self, &scope.bundle.intls) {
                return val.into();
            }
        }
        match self {
            FluentValue::String(s) => s.clone(),
            FluentValue::Number(n) => n.as_string(),
            FluentValue::Custom(s) => scope.bundle.intls.stringify_value(&**s),
            FluentValue::Error => "".into(),
            FluentValue::None => "".into(),
        }
    }

    /// Converts the [`FluentValue`] to a string.
    ///
    /// Takes self by-value to be able to skip expensive clones.
    /// Prefer this method over [`FluentValue::as_string()`] when possible.
    pub fn into_string<R: Borrow<FluentResource>, M>(self, scope: &Scope<R, M>) -> Cow<'source, str>
    where
        M: MemoizerKind,
    {
        if let Some(formatter) = &scope.bundle.formatter {
            if let Some(val) = formatter(&self, &scope.bundle.intls) {
                return val.into();
            }
        }
        match self {
            FluentValue::String(s) => s,
            FluentValue::Number(n) => n.as_string(),
            FluentValue::Custom(s) => scope.bundle.intls.stringify_value(s.as_ref()),
            FluentValue::Error => "".into(),
            FluentValue::None => "".into(),
        }
    }

    pub fn into_owned<'a>(&self) -> FluentValue<'a> {
        match self {
            FluentValue::String(str) => FluentValue::String(Cow::from(str.to_string())),
            FluentValue::Number(s) => FluentValue::Number(s.clone()),
            FluentValue::Custom(s) => FluentValue::Custom(s.duplicate()),
            FluentValue::Error => FluentValue::Error,
            FluentValue::None => FluentValue::None,
        }
    }
}

impl From<String> for FluentValue<'_> {
    fn from(s: String) -> Self {
        FluentValue::String(s.into())
    }
}

impl<'source> From<&'source String> for FluentValue<'source> {
    fn from(s: &'source String) -> Self {
        FluentValue::String(s.into())
    }
}

impl<'source> From<&'source str> for FluentValue<'source> {
    fn from(s: &'source str) -> Self {
        FluentValue::String(s.into())
    }
}

impl<'source> From<Cow<'source, str>> for FluentValue<'source> {
    fn from(s: Cow<'source, str>) -> Self {
        FluentValue::String(s)
    }
}

impl<'source, T> From<Option<T>> for FluentValue<'source>
where
    T: Into<FluentValue<'source>>,
{
    fn from(v: Option<T>) -> Self {
        match v {
            Some(v) => v.into(),
            None => FluentValue::None,
        }
    }
}
