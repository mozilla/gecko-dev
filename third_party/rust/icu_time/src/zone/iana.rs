// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Tools for parsing IANA time zone IDs.

use icu_provider::prelude::*;
use zerovec::vecs::{VarZeroSliceIter, ZeroSliceIter};

use crate::{
    provider::iana::{
        IanaNames, IanaToBcp47Map, TimezoneIdentifiersIanaCoreV1,
        TimezoneIdentifiersIanaExtendedV1, NON_REGION_CITY_PREFIX,
    },
    TimeZone,
};

/// A parser for parsing an IANA time zone ID to a [`TimeZone`] type.
///
/// There are approximately 600 IANA identifiers and 450 [`TimeZone`] identifiers.
/// These lists grow very slowly; in a typical year, 2-3 new identifiers are added.
///
/// This means that multiple IANA identifiers map to the same [`TimeZone`]. For example, the
/// following four IANA identifiers all map to the same [`TimeZone`]:
///
/// - `America/Fort_Wayne`
/// - `America/Indiana/Indianapolis`
/// - `America/Indianapolis`
/// - `US/East-Indiana`
///
/// For each [`TimeZone`], there is one "canonical" IANA time zone ID (for the above example, it is
/// `America/Indiana/Indianapolis`). Note that the canonical identifier can change over time.
/// For example, the identifier `Europe/Kiev` was renamed to the newly-added identifier `Europe/Kyiv` in 2022.
///
/// # Examples
///
/// ```
/// use icu::locale::subtags::subtag;
/// use icu::time::zone::IanaParser;
/// use icu::time::TimeZone;
///
/// let parser = IanaParser::new();
///
/// // The IANA zone "Australia/Melbourne" is the BCP-47 zone "aumel":
/// assert_eq!(
///     parser.parse("Australia/Melbourne"),
///     TimeZone(subtag!("aumel"))
/// );
///
/// // Parsing is ASCII-case-insensitive:
/// assert_eq!(
///     parser.parse("australia/melbourne"),
///     TimeZone(subtag!("aumel"))
/// );
///
/// // The IANA zone "Australia/Victoria" is an alias:
/// assert_eq!(
///     parser.parse("Australia/Victoria"),
///     TimeZone(subtag!("aumel"))
/// );
///
/// // The IANA zone "Australia/Boing_Boing" does not exist
/// // (maybe not *yet*), so it produces the special unknown
/// // time zone in order for this operation to be infallible:
/// assert_eq!(parser.parse("Australia/Boing_Boing"), TimeZone::UNKNOWN);
/// ```
#[derive(Debug, Clone)]
pub struct IanaParser {
    data: DataPayload<TimezoneIdentifiersIanaCoreV1>,
    checksum: u64,
}

impl IanaParser {
    /// Creates a new [`IanaParser`] using compiled data.
    ///
    /// See [`IanaParser`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> IanaParserBorrowed<'static> {
        IanaParserBorrowed::new()
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P) -> Result<Self, DataError>
    where
        P: DataProvider<TimezoneIdentifiersIanaCoreV1> + ?Sized,
    {
        let response = provider.load(Default::default())?;
        Ok(Self {
            data: response.payload,
            checksum: response.metadata.checksum.ok_or_else(|| {
                DataError::custom("Missing checksum")
                    .with_req(TimezoneIdentifiersIanaCoreV1::INFO, Default::default())
            })?,
        })
    }

    /// Returns a borrowed version of the parser that can be queried.
    ///
    /// This avoids a small potential indirection cost when querying the parser.
    pub fn as_borrowed(&self) -> IanaParserBorrowed {
        IanaParserBorrowed {
            data: self.data.get(),
            checksum: self.checksum,
        }
    }
}

impl AsRef<IanaParser> for IanaParser {
    #[inline]
    fn as_ref(&self) -> &IanaParser {
        self
    }
}

/// A borrowed wrapper around the time zone ID parser, returned by
/// [`IanaParser::as_borrowed()`]. More efficient to query.
#[derive(Debug, Copy, Clone)]
pub struct IanaParserBorrowed<'a> {
    data: &'a IanaToBcp47Map<'a>,
    checksum: u64,
}

#[cfg(feature = "compiled_data")]
impl Default for IanaParserBorrowed<'static> {
    fn default() -> Self {
        Self::new()
    }
}

impl IanaParserBorrowed<'static> {
    /// Creates a new [`IanaParserBorrowed`] using compiled data.
    ///
    /// See [`IanaParserBorrowed`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        Self {
            data: crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_CORE_V1,
            checksum: crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_CORE_V1_CHECKSUM,
        }
    }

    /// Cheaply converts a [`IanaParserBorrowed<'static>`] into a [`IanaParser`].
    ///
    /// Note: Due to branching and indirection, using [`IanaParser`] might inhibit some
    /// compile-time optimizations that are possible with [`IanaParserBorrowed`].
    pub fn static_to_owned(&self) -> IanaParser {
        IanaParser {
            data: DataPayload::from_static_ref(self.data),
            checksum: self.checksum,
        }
    }
}

impl<'a> IanaParserBorrowed<'a> {
    /// Gets the [`TimeZone`] from a case-insensitive IANA time zone ID.
    ///
    /// Returns [`TimeZone::UNKNOWN`] if the IANA ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_time::zone::iana::IanaParser;
    /// use icu_time::TimeZone;
    ///
    /// let parser = IanaParser::new();
    ///
    /// let result = parser.parse("Asia/CALCUTTA");
    ///
    /// assert_eq!(result.as_str(), "inccu");
    ///
    /// // Unknown IANA time zone ID:
    /// assert_eq!(parser.parse("America/San_Francisco"), TimeZone::UNKNOWN);
    /// ```
    pub fn parse(&self, iana_id: &str) -> TimeZone {
        self.parse_from_utf8(iana_id.as_bytes())
    }

    /// Same as [`Self::parse()`] but works with potentially ill-formed UTF-8.
    pub fn parse_from_utf8(&self, iana_id: &[u8]) -> TimeZone {
        let Some(trie_value) = self.trie_value(iana_id) else {
            return TimeZone::UNKNOWN;
        };
        let Some(tz) = self.data.bcp47_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return TimeZone::UNKNOWN;
        };
        tz
    }

    fn trie_value(&self, iana_id: &[u8]) -> Option<IanaTrieValue> {
        let mut cursor = self.data.map.cursor();
        if !iana_id.contains(&b'/') {
            cursor.step(NON_REGION_CITY_PREFIX);
        }
        for &b in iana_id {
            cursor.step(b);
        }
        cursor.take_value().map(IanaTrieValue)
    }

    /// Returns an iterator over all known time zones.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::subtags::subtag;
    /// use icu::time::zone::IanaParser;
    /// use icu::time::zone::TimeZone;
    /// use std::collections::BTreeSet;
    ///
    /// let parser = IanaParser::new();
    ///
    /// let ids = parser.iter().collect::<BTreeSet<_>>();
    ///
    /// assert!(ids.contains(&TimeZone(subtag!("uaiev"))));
    /// ```
    pub fn iter(&self) -> TimeZoneIter<'a> {
        TimeZoneIter {
            inner: self.data.bcp47_ids.iter(),
        }
    }
}

/// Returned by [`IanaParserBorrowed::iter()`]
#[derive(Debug)]
pub struct TimeZoneIter<'a> {
    inner: ZeroSliceIter<'a, TimeZone>,
}

impl Iterator for TimeZoneIter<'_> {
    type Item = TimeZone;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next()
    }
}

/// A parser that supplements [`IanaParser`] with about 10kB of additional data to support
/// returning canonical and case-normalized IANA time zone IDs.
#[derive(Debug, Clone)]
pub struct IanaParserExtended<I> {
    inner: I,
    data: DataPayload<TimezoneIdentifiersIanaExtendedV1>,
}

impl IanaParserExtended<IanaParser> {
    /// Creates a new [`IanaParserExtended`] using compiled data.
    ///
    /// See [`IanaParserExtended`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> IanaParserExtendedBorrowed<'static> {
        IanaParserExtendedBorrowed::new()
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P) -> Result<Self, DataError>
    where
        P: DataProvider<TimezoneIdentifiersIanaCoreV1>
            + DataProvider<TimezoneIdentifiersIanaExtendedV1>
            + ?Sized,
    {
        let parser = IanaParser::try_new_unstable(provider)?;
        Self::try_new_with_parser_unstable(provider, parser)
    }
}

impl<I> IanaParserExtended<I>
where
    I: AsRef<IanaParser>,
{
    /// Creates a new [`IanaParserExtended`] using compiled data
    /// and a pre-existing [`IanaParser`], which can be borrowed.
    ///
    /// See [`IanaParserExtended`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_with_parser(parser: I) -> Result<Self, DataError> {
        if parser.as_ref().checksum
            != crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_EXTENDED_V1_CHECKSUM
        {
            return Err(
                DataErrorKind::InconsistentData(TimezoneIdentifiersIanaCoreV1::INFO)
                    .with_marker(TimezoneIdentifiersIanaExtendedV1::INFO),
            );
        }
        Ok(Self {
            inner: parser,
            data: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_EXTENDED_V1,
            ),
        })
    }

    icu_provider::gen_buffer_data_constructors!((parser: I) -> error: DataError,
        functions: [
            try_new_with_parser: skip,
            try_new_with_parser_with_buffer_provider,
            try_new_with_parser_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_with_parser_unstable<P>(provider: &P, parser: I) -> Result<Self, DataError>
    where
        P: DataProvider<TimezoneIdentifiersIanaCoreV1>
            + DataProvider<TimezoneIdentifiersIanaExtendedV1>
            + ?Sized,
    {
        let response = provider.load(Default::default())?;
        if Some(parser.as_ref().checksum) != response.metadata.checksum {
            return Err(
                DataErrorKind::InconsistentData(TimezoneIdentifiersIanaCoreV1::INFO)
                    .with_marker(TimezoneIdentifiersIanaExtendedV1::INFO),
            );
        }
        Ok(Self {
            inner: parser,
            data: response.payload,
        })
    }

    /// Returns a borrowed version of the parser that can be queried.
    ///
    /// This avoids a small potential indirection cost when querying the parser.
    pub fn as_borrowed(&self) -> IanaParserExtendedBorrowed {
        IanaParserExtendedBorrowed {
            inner: self.inner.as_ref().as_borrowed(),
            data: self.data.get(),
        }
    }
}

/// A borrowed wrapper around the time zone ID parser, returned by
/// [`IanaParserExtended::as_borrowed()`]. More efficient to query.
#[derive(Debug, Copy, Clone)]
pub struct IanaParserExtendedBorrowed<'a> {
    inner: IanaParserBorrowed<'a>,
    data: &'a IanaNames<'a>,
}

#[cfg(feature = "compiled_data")]
impl Default for IanaParserExtendedBorrowed<'static> {
    fn default() -> Self {
        Self::new()
    }
}

impl IanaParserExtendedBorrowed<'static> {
    /// Creates a new [`IanaParserExtendedBorrowed`] using compiled data.
    ///
    /// See [`IanaParserExtendedBorrowed`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        const _: () = assert!(
            crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_CORE_V1_CHECKSUM
                == crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_EXTENDED_V1_CHECKSUM,
        );
        Self {
            inner: IanaParserBorrowed::new(),
            data: crate::provider::Baked::SINGLETON_TIMEZONE_IDENTIFIERS_IANA_EXTENDED_V1,
        }
    }

    /// Cheaply converts a [`IanaParserExtendedBorrowed<'static>`] into a [`IanaParserExtended`].
    ///
    /// Note: Due to branching and indirection, using [`IanaParserExtended`] might inhibit some
    /// compile-time optimizations that are possible with [`IanaParserExtendedBorrowed`].
    pub fn static_to_owned(&self) -> IanaParserExtended<IanaParser> {
        IanaParserExtended {
            inner: self.inner.static_to_owned(),
            data: DataPayload::from_static_ref(self.data),
        }
    }
}

impl<'a> IanaParserExtendedBorrowed<'a> {
    /// Gets the [`TimeZone`], the canonical IANA ID, and the case-normalized IANA ID from a case-insensitive IANA time zone ID.
    ///
    /// Returns `TimeZone::UNKNOWN` / `"Etc/Unknown"` if the IANA ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_time::zone::iana::IanaParserExtended;
    /// use icu_time::TimeZone;
    ///
    /// let parser = IanaParserExtended::new();
    ///
    /// let r = parser.parse("Asia/CALCUTTA");
    ///
    /// assert_eq!(r.time_zone.as_str(), "inccu");
    /// assert_eq!(r.canonical, "Asia/Kolkata");
    /// assert_eq!(r.normalized, "Asia/Calcutta");
    ///
    /// // Unknown IANA time zone ID:
    /// let r = parser.parse("America/San_Francisco");
    ///
    /// assert_eq!(r.time_zone, TimeZone::UNKNOWN);
    /// assert_eq!(r.canonical, "Etc/Unknown");
    /// assert_eq!(r.normalized, "Etc/Unknown");
    /// ```
    pub fn parse(&self, iana_id: &str) -> TimeZoneAndCanonicalAndNormalized<'a> {
        self.parse_from_utf8(iana_id.as_bytes())
    }

    /// Same as [`Self::parse()`] but works with potentially ill-formed UTF-8.
    pub fn parse_from_utf8(&self, iana_id: &[u8]) -> TimeZoneAndCanonicalAndNormalized<'a> {
        let Some(trie_value) = self.inner.trie_value(iana_id) else {
            return TimeZoneAndCanonicalAndNormalized::UKNONWN;
        };
        let Some(time_zone) = self.inner.data.bcp47_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return TimeZoneAndCanonicalAndNormalized::UKNONWN;
        };
        let Some(canonical) = self.data.normalized_iana_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return TimeZoneAndCanonicalAndNormalized::UKNONWN;
        };
        let normalized = if trie_value.is_canonical() {
            canonical
        } else {
            let Some(Ok(index)) = self.data.normalized_iana_ids.binary_search_in_range_by(
                |a| {
                    a.as_bytes()
                        .iter()
                        .map(u8::to_ascii_lowercase)
                        .cmp(iana_id.iter().map(u8::to_ascii_lowercase))
                },
                self.inner.data.bcp47_ids.len()..self.data.normalized_iana_ids.len(),
            ) else {
                debug_assert!(
                    false,
                    "binary search should succeed if trie lookup succeeds"
                );
                return TimeZoneAndCanonicalAndNormalized::UKNONWN;
            };
            let Some(normalized) = self
                .data
                .normalized_iana_ids
                .get(self.inner.data.bcp47_ids.len() + index)
            else {
                debug_assert!(false, "binary search returns valid index");
                return TimeZoneAndCanonicalAndNormalized::UKNONWN;
            };
            normalized
        };
        TimeZoneAndCanonicalAndNormalized {
            time_zone,
            canonical,
            normalized,
        }
    }

    /// Returns an iterator over all time zones and their canonical IANA identifiers.
    ///
    /// The iterator is sorted by the canonical IANA identifiers.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::subtags::subtag;
    /// use icu::time::zone::iana::IanaParserExtended;
    /// use icu::time::zone::TimeZone;
    /// use std::collections::BTreeSet;
    ///
    /// let parser = IanaParserExtended::new();
    ///
    /// let ids = parser
    ///     .iter()
    ///     .map(|t| (t.time_zone, t.canonical))
    ///     .collect::<BTreeSet<_>>();
    ///
    /// assert!(ids.contains(&(TimeZone(subtag!("uaiev")), "Europe/Kyiv")));
    /// assert!(parser.iter().count() >= 445);
    /// ```
    pub fn iter(&self) -> TimeZoneAndCanonicalIter<'a> {
        TimeZoneAndCanonicalIter(
            self.inner
                .data
                .bcp47_ids
                .iter()
                .zip(self.data.normalized_iana_ids.iter()),
        )
    }

    /// Returns an iterator equivalent to calling [`Self::parse`] on all IANA time zone identifiers.
    ///
    /// The only guarantee w.r.t iteration order is that for a given time zone, the canonical IANA
    /// identifier will come first, and the following non-canonical IANA identifiers will be sorted.
    /// However, the output is not grouped by time zone.
    ///
    /// The current implementation returns all sorted canonical IANA identifiers first, followed by all
    /// sorted non-canonical identifiers, however this is subject to change.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::time::zone::iana::IanaParserExtended;
    /// use icu::time::zone::TimeZone;
    /// use std::collections::BTreeMap;
    /// use icu::locale::subtags::subtag;
    ///
    /// let parser = IanaParserExtended::new();
    ///
    /// let ids = parser.iter_all().enumerate().map(|(a, b)| ((b.time_zone, b.canonical, b.normalized), a)).collect::<std::collections::BTreeMap<_, _>>();
    ///
    /// let kyiv_idx = ids[&(TimeZone(subtag!("uaiev")), "Europe/Kyiv", "Europe/Kyiv")];
    /// let kiev_idx = ids[&(TimeZone(subtag!("uaiev")), "Europe/Kyiv", "Europe/Kiev")];
    /// let uzgh_idx = ids[&(TimeZone(subtag!("uaiev")), "Europe/Kyiv", "Europe/Uzhgorod")];
    /// let zapo_idx = ids[&(TimeZone(subtag!("uaiev")), "Europe/Kyiv", "Europe/Zaporozhye")];
    ///
    /// // The order for a particular time zone is guaranteed
    /// assert!(kyiv_idx < kiev_idx && kiev_idx < uzgh_idx && uzgh_idx < zapo_idx);
    /// // It is not guaranteed that the entries for a particular time zone are consecutive
    /// assert!(kyiv_idx + 1 != kiev_idx);
    ///
    /// assert!(parser.iter_all().count() >= 598);
    /// ```
    pub fn iter_all(&self) -> TimeZoneAndCanonicalAndNormalizedIter<'a> {
        TimeZoneAndCanonicalAndNormalizedIter(0, *self)
    }
}

/// Return value of [`IanaParserBorrowed::iter`].
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub struct TimeZoneAndCanonical<'a> {
    /// The parsed [`TimeZone`]
    pub time_zone: TimeZone,
    /// The canonical IANA ID
    pub canonical: &'a str,
}

/// Return value of [`IanaParserExtendedBorrowed::parse`], [`IanaParserExtendedBorrowed::iter`].
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub struct TimeZoneAndCanonicalAndNormalized<'a> {
    /// The parsed [`TimeZone`]
    pub time_zone: TimeZone,
    /// The canonical IANA ID
    pub canonical: &'a str,
    /// The normalized IANA ID
    pub normalized: &'a str,
}

impl TimeZoneAndCanonicalAndNormalized<'static> {
    const UKNONWN: Self = TimeZoneAndCanonicalAndNormalized {
        time_zone: TimeZone::UNKNOWN,
        canonical: "Etc/Unknown",
        normalized: "Etc/Unknown",
    };
}

/// The iterator returned by [`IanaParserExtendedBorrowed::iter()`]
#[derive(Debug)]
pub struct TimeZoneAndCanonicalIter<'a>(
    core::iter::Zip<ZeroSliceIter<'a, TimeZone>, VarZeroSliceIter<'a, str>>,
);

impl<'a> Iterator for TimeZoneAndCanonicalIter<'a> {
    type Item = TimeZoneAndCanonical<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        let (time_zone, canonical) = self.0.next()?;
        Some(TimeZoneAndCanonical {
            time_zone,
            canonical,
        })
    }
}

/// The iterator returned by [`IanaParserExtendedBorrowed::iter_all()`]
#[derive(Debug)]
pub struct TimeZoneAndCanonicalAndNormalizedIter<'a>(usize, IanaParserExtendedBorrowed<'a>);

impl<'a> Iterator for TimeZoneAndCanonicalAndNormalizedIter<'a> {
    type Item = TimeZoneAndCanonicalAndNormalized<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if let (Some(time_zone), Some(canonical)) = (
            self.1.inner.data.bcp47_ids.get(self.0),
            self.1.data.normalized_iana_ids.get(self.0),
        ) {
            self.0 += 1;
            Some(TimeZoneAndCanonicalAndNormalized {
                time_zone,
                canonical,
                normalized: canonical,
            })
        } else if let Some(normalized) = self.1.data.normalized_iana_ids.get(self.0) {
            let Some(trie_value) = self.1.inner.trie_value(normalized.as_bytes()) else {
                debug_assert!(false, "normalized value should be in trie");
                return None;
            };
            let (Some(time_zone), Some(canonical)) = (
                self.1.inner.data.bcp47_ids.get(trie_value.index()),
                self.1.data.normalized_iana_ids.get(trie_value.index()),
            ) else {
                debug_assert!(false, "index should be in range");
                return None;
            };
            self.0 += 1;
            Some(TimeZoneAndCanonicalAndNormalized {
                time_zone,
                canonical,
                normalized,
            })
        } else {
            None
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq)]
#[repr(transparent)]
struct IanaTrieValue(usize);

impl IanaTrieValue {
    #[inline]
    pub(crate) fn index(self) -> usize {
        self.0 >> 1
    }
    #[inline]
    pub(crate) fn is_canonical(self) -> bool {
        (self.0 & 0x1) != 0
    }
}
