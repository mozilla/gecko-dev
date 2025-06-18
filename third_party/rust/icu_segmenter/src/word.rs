// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::complex::*;
use crate::indices::{Latin1Indices, Utf16Indices};
use crate::iterator_helpers::derive_usize_iterator_with_type;
use crate::provider::*;
use crate::rule_segmenter::*;
use alloc::string::String;
use alloc::vec;
use alloc::vec::Vec;
use icu_locale_core::LanguageIdentifier;
use icu_provider::prelude::*;
use utf8_iter::Utf8CharIndices;

/// Options to tailor word breaking behavior.
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct WordBreakOptions<'a> {
    /// Content locale for word segmenter
    pub content_locale: Option<&'a LanguageIdentifier>,
    /// Options independent of the locale
    pub invariant_options: WordBreakInvariantOptions,
}

/// Locale-independent options to tailor word breaking behavior
///
/// Currently empty but may grow in the future
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct WordBreakInvariantOptions {}

/// Implements the [`Iterator`] trait over the word boundaries of the given string.
///
/// Lifetimes:
///
/// - `'l` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// The [`Iterator::Item`] is an [`usize`] representing index of a code unit
/// _after_ the boundary (for a boundary at the end of text, this index is the length
/// of the [`str`] or array of code units).
///
/// For examples of use, see [`WordSegmenter`].
#[derive(Debug)]
pub struct WordBreakIterator<'data, 's, Y: RuleBreakType>(RuleBreakIterator<'data, 's, Y>);

derive_usize_iterator_with_type!(WordBreakIterator, 'data);

/// Hide ULE type
pub(crate) mod inner {
    /// The word type tag that is returned by [`WordBreakIterator::word_type()`].
    ///
    /// [`WordBreakIterator::word_type()`]: super::WordBreakIterator::word_type
    #[non_exhaustive]
    #[derive(Copy, Clone, PartialEq, Debug)]
    #[repr(u8)]
    #[zerovec::make_ule(WordTypeULE)]
    pub enum WordType {
        /// No category tag.
        None = 0,
        /// Number category tag.
        Number = 1,
        /// Letter category tag, including CJK.
        Letter = 2,
    }
}

pub use inner::WordType;

impl WordType {
    /// Whether the segment is word-like; word-like segments include numbers, as
    /// well as segments made up of letters (including CJKV ideographs).
    pub fn is_word_like(&self) -> bool {
        self != &WordType::None
    }
}

impl<'data, 's, Y: RuleBreakType> WordBreakIterator<'data, 's, Y> {
    /// Returns the word type of the segment preceding the current boundary.
    #[inline]
    pub fn word_type(&self) -> WordType {
        self.0.word_type()
    }

    /// Returns an iterator over pairs of boundary position and word type.
    pub fn iter_with_word_type(self) -> WordBreakIteratorWithWordType<'data, 's, Y> {
        WordBreakIteratorWithWordType(self)
    }

    /// Returns `true` when the segment preceding the current boundary is word-like,
    /// such as letters, numbers, or CJKV ideographs.
    #[inline]
    pub fn is_word_like(&self) -> bool {
        self.word_type().is_word_like()
    }
}

/// Word break iterator that also returns the word type
// We can use impl Trait here once `use<..>` syntax is available, see https://github.com/rust-lang/rust/issues/61756
#[derive(Debug)]
pub struct WordBreakIteratorWithWordType<'data, 's, Y: RuleBreakType>(
    WordBreakIterator<'data, 's, Y>,
);

impl<Y: RuleBreakType> Iterator for WordBreakIteratorWithWordType<'_, '_, Y> {
    type Item = (usize, WordType);
    fn next(&mut self) -> Option<Self::Item> {
        let ret = self.0.next()?;
        Some((ret, self.0 .0.word_type()))
    }
}

/// Supports loading word break data, and creating word break iterators for different string
/// encodings.
///
/// Most segmentation methods live on [`WordSegmenterBorrowed`], which can be obtained via
/// [`WordSegmenter::new_auto()`] (etc) or [`WordSegmenter::as_borrowed()`].
///
/// # Examples
///
/// Segment a string:
///
/// ```rust
/// use icu::segmenter::{options::WordBreakInvariantOptions, WordSegmenter};
/// let segmenter =
///     WordSegmenter::new_auto(WordBreakInvariantOptions::default());
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_str("Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 5, 6, 11]);
/// ```
///
/// Segment a Latin1 byte string:
///
/// ```rust
/// use icu::segmenter::{options::WordBreakInvariantOptions, WordSegmenter};
/// let segmenter =
///     WordSegmenter::new_auto(WordBreakInvariantOptions::default());
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_latin1(b"Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 5, 6, 11]);
/// ```
///
/// Successive boundaries can be used to retrieve the segments.
/// In particular, the first boundary is always 0, and the last one is the
/// length of the segmented text in code units.
///
/// ```rust
/// # use icu::segmenter::{WordSegmenter, options::WordBreakInvariantOptions};
/// # let segmenter = WordSegmenter::new_auto(WordBreakInvariantOptions::default());
/// use itertools::Itertools;
/// let text = "Mark’d ye his words?";
/// let segments: Vec<&str> = segmenter
///     .segment_str(text)
///     .tuple_windows()
///     .map(|(i, j)| &text[i..j])
///     .collect();
/// assert_eq!(
///     &segments,
///     &["Mark’d", " ", "ye", " ", "his", " ", "words", "?"]
/// );
/// ```
///
/// Not all segments delimited by word boundaries are words; some are interword
/// segments such as spaces and punctuation.
/// The [`WordBreakIterator::word_type()`] of a boundary can be used to
/// classify the preceding segment; [`WordBreakIterator::iter_with_word_type()`]
/// associates each boundary with its status.
/// ```rust
/// # use itertools::Itertools;
/// # use icu::segmenter::WordSegmenter;
/// # use icu::segmenter::options::{WordType, WordBreakInvariantOptions};
/// # let segmenter = WordSegmenter::new_auto(WordBreakInvariantOptions::default());
/// # let text = "Mark’d ye his words?";
/// let words: Vec<&str> = segmenter
///     .segment_str(text)
///     .iter_with_word_type()
///     .tuple_windows()
///     .filter(|(_, (_, segment_type))| segment_type.is_word_like())
///     .map(|((i, _), (j, _))| &text[i..j])
///     .collect();
/// assert_eq!(&words, &["Mark’d", "ye", "his", "words"]);
/// ```
#[derive(Debug)]
pub struct WordSegmenter {
    payload: DataPayload<SegmenterBreakWordV1>,
    complex: ComplexPayloads,
    payload_locale_override: Option<DataPayload<SegmenterBreakWordOverrideV1>>,
}

/// Segments a string into words (borrowed version).
///
/// See [`WordSegmenter`] for examples.
#[derive(Clone, Debug, Copy)]
pub struct WordSegmenterBorrowed<'data> {
    data: &'data RuleBreakData<'data>,
    complex: ComplexPayloadsBorrowed<'data>,
    locale_override: Option<&'data RuleBreakDataOverride<'data>>,
}

impl WordSegmenter {
    /// Constructs a [`WordSegmenter`] with an invariant locale and the best available compiled data for
    /// complex scripts (Chinese, Japanese, Khmer, Lao, Myanmar, and Thai).
    ///
    /// The current behavior, which is subject to change, is to use the LSTM model when available
    /// and the dictionary model for Chinese and Japanese.
    ///
    /// ✨ *Enabled with the `compiled_data` and `auto` Cargo features.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// Behavior with complex scripts:
    ///
    /// ```
    /// use icu::segmenter::{options::WordBreakInvariantOptions, WordSegmenter};
    ///
    /// let th_str = "ทุกสองสัปดาห์";
    /// let ja_str = "こんにちは世界";
    ///
    /// let segmenter =
    ///     WordSegmenter::new_auto(WordBreakInvariantOptions::default());
    ///
    /// let th_bps = segmenter.segment_str(th_str).collect::<Vec<_>>();
    /// let ja_bps = segmenter.segment_str(ja_str).collect::<Vec<_>>();
    ///
    /// assert_eq!(th_bps, [0, 9, 18, 39]);
    /// assert_eq!(ja_bps, [0, 15, 21]);
    /// ```
    #[cfg(feature = "compiled_data")]
    #[cfg(feature = "auto")]
    pub fn new_auto(_options: WordBreakInvariantOptions) -> WordSegmenterBorrowed<'static> {
        WordSegmenterBorrowed {
            data: crate::provider::Baked::SINGLETON_SEGMENTER_BREAK_WORD_V1,
            complex: ComplexPayloadsBorrowed::new_auto(),
            locale_override: None,
        }
    }

    #[cfg(feature = "auto")]
    icu_provider::gen_buffer_data_constructors!(
        (options: WordBreakOptions) -> error: DataError,
        functions: [
            try_new_auto,
            try_new_auto_with_buffer_provider,
            try_new_auto_unstable,
            Self
        ]
    );

    #[cfg(feature = "auto")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new_auto)]
    pub fn try_new_auto_unstable<D>(
        provider: &D,
        options: WordBreakOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakWordV1>
            + DataProvider<SegmenterBreakWordOverrideV1>
            + DataProvider<SegmenterDictionaryAutoV1>
            + DataProvider<SegmenterLstmAutoV1>
            + DataProvider<SegmenterBreakGraphemeClusterV1>
            + ?Sized,
    {
        Ok(Self {
            payload: provider.load(Default::default())?.payload,
            complex: ComplexPayloads::try_new_auto(provider)?,
            payload_locale_override: if let Some(locale) = options.content_locale {
                let locale = DataLocale::from(locale);
                let req = DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    metadata: {
                        let mut metadata = DataRequestMetadata::default();
                        metadata.silent = true;
                        metadata
                    },
                };
                provider
                    .load(req)
                    .allow_identifier_not_found()?
                    .map(|r| r.payload)
            } else {
                None
            },
        })
    }

    /// Constructs a [`WordSegmenter`] with an invariant locale and compiled LSTM data for
    /// complex scripts (Burmese, Khmer, Lao, and Thai).
    ///
    /// The LSTM, or Long Term Short Memory, is a machine learning model. It is smaller than
    /// the full dictionary but more expensive during segmentation (inference).
    ///
    /// Warning: there is not currently an LSTM model for Chinese or Japanese, so the [`WordSegmenter`]
    /// created by this function will have unexpected behavior in spans of those scripts.
    ///
    /// ✨ *Enabled with the `compiled_data` and `lstm` Cargo features.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// Behavior with complex scripts:
    ///
    /// ```
    /// use icu::segmenter::{options::WordBreakInvariantOptions, WordSegmenter};
    ///
    /// let th_str = "ทุกสองสัปดาห์";
    /// let ja_str = "こんにちは世界";
    ///
    /// let segmenter =
    ///     WordSegmenter::new_lstm(WordBreakInvariantOptions::default());
    ///
    /// let th_bps = segmenter.segment_str(th_str).collect::<Vec<_>>();
    /// let ja_bps = segmenter.segment_str(ja_str).collect::<Vec<_>>();
    ///
    /// assert_eq!(th_bps, [0, 9, 18, 39]);
    ///
    /// // Note: We aren't able to find a suitable breakpoint in Chinese/Japanese.
    /// assert_eq!(ja_bps, [0, 21]);
    /// ```
    #[cfg(feature = "compiled_data")]
    #[cfg(feature = "lstm")]
    pub fn new_lstm(_options: WordBreakInvariantOptions) -> WordSegmenterBorrowed<'static> {
        WordSegmenterBorrowed {
            data: crate::provider::Baked::SINGLETON_SEGMENTER_BREAK_WORD_V1,
            complex: ComplexPayloadsBorrowed::new_lstm(),
            locale_override: None,
        }
    }

    #[cfg(feature = "lstm")]
    icu_provider::gen_buffer_data_constructors!(
        (options: WordBreakOptions) -> error: DataError,
        functions: [
            try_new_lstm,
            try_new_lstm_with_buffer_provider,
            try_new_lstm_unstable,
            Self
        ]
    );

    #[cfg(feature = "lstm")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new_lstm)]
    pub fn try_new_lstm_unstable<D>(
        provider: &D,
        options: WordBreakOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakWordV1>
            + DataProvider<SegmenterBreakWordOverrideV1>
            + DataProvider<SegmenterLstmAutoV1>
            + DataProvider<SegmenterBreakGraphemeClusterV1>
            + ?Sized,
    {
        Ok(Self {
            payload: provider.load(Default::default())?.payload,
            complex: ComplexPayloads::try_new_lstm(provider)?,
            payload_locale_override: if let Some(locale) = options.content_locale {
                let locale = DataLocale::from(locale);
                let req = DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    metadata: {
                        let mut metadata = DataRequestMetadata::default();
                        metadata.silent = true;
                        metadata
                    },
                };
                provider
                    .load(req)
                    .allow_identifier_not_found()?
                    .map(|r| r.payload)
            } else {
                None
            },
        })
    }

    /// Construct a [`WordSegmenter`] with an invariant locale and compiled dictionary data for
    /// complex scripts (Chinese, Japanese, Khmer, Lao, Myanmar, and Thai).
    ///
    /// The dictionary model uses a list of words to determine appropriate breakpoints. It is
    /// faster than the LSTM model but requires more data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// Behavior with complex scripts:
    ///
    /// ```
    /// use icu::segmenter::{options::WordBreakInvariantOptions, WordSegmenter};
    ///
    /// let th_str = "ทุกสองสัปดาห์";
    /// let ja_str = "こんにちは世界";
    ///
    /// let segmenter =
    ///     WordSegmenter::new_dictionary(WordBreakInvariantOptions::default());
    ///
    /// let th_bps = segmenter.segment_str(th_str).collect::<Vec<_>>();
    /// let ja_bps = segmenter.segment_str(ja_str).collect::<Vec<_>>();
    ///
    /// assert_eq!(th_bps, [0, 9, 18, 39]);
    /// assert_eq!(ja_bps, [0, 15, 21]);
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn new_dictionary(_options: WordBreakInvariantOptions) -> WordSegmenterBorrowed<'static> {
        WordSegmenterBorrowed {
            data: crate::provider::Baked::SINGLETON_SEGMENTER_BREAK_WORD_V1,
            complex: ComplexPayloadsBorrowed::new_dict(),
            locale_override: None,
        }
    }

    icu_provider::gen_buffer_data_constructors!(
        (options: WordBreakOptions) -> error: DataError,
        functions: [
            try_new_dictionary,
            try_new_dictionary_with_buffer_provider,
            try_new_dictionary_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new_dictionary)]
    pub fn try_new_dictionary_unstable<D>(
        provider: &D,
        options: WordBreakOptions,
    ) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakWordV1>
            + DataProvider<SegmenterBreakWordOverrideV1>
            + DataProvider<SegmenterDictionaryAutoV1>
            + DataProvider<SegmenterDictionaryExtendedV1>
            + DataProvider<SegmenterBreakGraphemeClusterV1>
            + ?Sized,
    {
        Ok(Self {
            payload: provider.load(Default::default())?.payload,
            complex: ComplexPayloads::try_new_dict(provider)?,
            payload_locale_override: if let Some(locale) = options.content_locale {
                let locale = DataLocale::from(locale);
                let req = DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    metadata: {
                        let mut metadata = DataRequestMetadata::default();
                        metadata.silent = true;
                        metadata
                    },
                };
                provider
                    .load(req)
                    .allow_identifier_not_found()?
                    .map(|r| r.payload)
            } else {
                None
            },
        })
    }
    /// Constructs a borrowed version of this type for more efficient querying.
    ///
    /// Most useful methods for segmentation are on this type.
    pub fn as_borrowed(&self) -> WordSegmenterBorrowed<'_> {
        WordSegmenterBorrowed {
            data: self.payload.get(),
            complex: self.complex.as_borrowed(),
            locale_override: self.payload_locale_override.as_ref().map(|p| p.get()),
        }
    }
}

impl<'data> WordSegmenterBorrowed<'data> {
    /// Creates a word break iterator for an `str` (a UTF-8 string).
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_str<'s>(self, input: &'s str) -> WordBreakIterator<'data, 's, Utf8> {
        WordBreakIterator(RuleBreakIterator {
            iter: input.char_indices(),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: Some(self.complex),
            boundary_property: 0,
            locale_override: self.locale_override,
            handle_complex_language: Utf8::word_handle_complex_language,
        })
    }

    /// Creates a word break iterator for a potentially ill-formed UTF8 string
    ///
    /// Invalid characters are treated as REPLACEMENT CHARACTER
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf8<'s>(
        self,
        input: &'s [u8],
    ) -> WordBreakIterator<'data, 's, PotentiallyIllFormedUtf8> {
        WordBreakIterator(RuleBreakIterator {
            iter: Utf8CharIndices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: Some(self.complex),
            boundary_property: 0,
            locale_override: self.locale_override,
            handle_complex_language: PotentiallyIllFormedUtf8::word_handle_complex_language,
        })
    }

    /// Creates a word break iterator for a Latin-1 (8-bit) string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_latin1<'s>(self, input: &'s [u8]) -> WordBreakIterator<'data, 's, Latin1> {
        WordBreakIterator(RuleBreakIterator {
            iter: Latin1Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: Some(self.complex),
            boundary_property: 0,
            locale_override: self.locale_override,
            handle_complex_language: Latin1::word_handle_complex_language,
        })
    }

    /// Creates a word break iterator for a UTF-16 string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf16<'s>(self, input: &'s [u16]) -> WordBreakIterator<'data, 's, Utf16> {
        WordBreakIterator(RuleBreakIterator {
            iter: Utf16Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: Some(self.complex),
            boundary_property: 0,
            locale_override: self.locale_override,
            handle_complex_language: Utf16::word_handle_complex_language,
        })
    }
}

impl WordSegmenterBorrowed<'static> {
    /// Cheaply converts a [`WordSegmenterBorrowed<'static>`] into a [`WordSegmenter`].
    ///
    /// Note: Due to branching and indirection, using [`WordSegmenter`] might inhibit some
    /// compile-time optimizations that are possible with [`WordSegmenterBorrowed`].
    pub fn static_to_owned(self) -> WordSegmenter {
        let payload_locale_override = self.locale_override.map(DataPayload::from_static_ref);
        WordSegmenter {
            payload: DataPayload::from_static_ref(self.data),
            complex: self.complex.static_to_owned(),
            payload_locale_override,
        }
    }
}

/// A trait allowing for [`WordBreakIterator`] to be generalized to multiple string iteration methods.
///
/// This is implemented by ICU4X for several common string types.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
/// </div>
pub trait WordBreakType: crate::private::Sealed + Sized + RuleBreakType {
    #[doc(hidden)]
    fn word_handle_complex_language(
        iterator: &mut RuleBreakIterator<'_, '_, Self>,
        left_codepoint: Self::CharType,
    ) -> Option<usize>;
}

impl WordBreakType for Utf8 {
    fn word_handle_complex_language(
        iter: &mut RuleBreakIterator<'_, '_, Self>,
        left_codepoint: Self::CharType,
    ) -> Option<usize> {
        handle_complex_language_utf8(iter, left_codepoint)
    }
}

impl WordBreakType for PotentiallyIllFormedUtf8 {
    fn word_handle_complex_language(
        iter: &mut RuleBreakIterator<'_, '_, Self>,
        left_codepoint: Self::CharType,
    ) -> Option<usize> {
        handle_complex_language_utf8(iter, left_codepoint)
    }
}

impl WordBreakType for Latin1 {
    fn word_handle_complex_language(
        _iter: &mut RuleBreakIterator<'_, '_, Self>,
        _left_codepoint: Self::CharType,
    ) -> Option<usize> {
        debug_assert!(
            false,
            "latin-1 text should never need complex language handling"
        );
        None
    }
}

/// handle_complex_language impl for UTF8 iterators
fn handle_complex_language_utf8<T>(
    iter: &mut RuleBreakIterator<'_, '_, T>,
    left_codepoint: T::CharType,
) -> Option<usize>
where
    T: RuleBreakType<CharType = char>,
{
    // word segmenter doesn't define break rules for some languages such as Thai.
    let start_iter = iter.iter.clone();
    let start_point = iter.current_pos_data;
    let mut s = String::new();
    s.push(left_codepoint);
    loop {
        debug_assert!(!iter.is_eof());
        s.push(iter.get_current_codepoint()?);
        iter.advance_iter();
        if let Some(current_break_property) = iter.get_current_break_property() {
            if current_break_property != iter.data.complex_property {
                break;
            }
        } else {
            // EOF
            break;
        }
    }

    // Restore iterator to move to head of complex string
    iter.iter = start_iter;
    iter.current_pos_data = start_point;
    #[allow(clippy::unwrap_used)] // iter.complex present for word segmenter
    let breaks = iter.complex.unwrap().complex_language_segment_str(&s);
    iter.result_cache = breaks;
    let first_pos = *iter.result_cache.first()?;
    let mut i = left_codepoint.len_utf8();
    loop {
        if i == first_pos {
            // Re-calculate breaking offset
            iter.result_cache = iter.result_cache.iter().skip(1).map(|r| r - i).collect();
            return iter.get_current_position();
        }
        debug_assert!(
            i < first_pos,
            "we should always arrive at first_pos: near index {:?}",
            iter.get_current_position()
        );
        i += iter.get_current_codepoint().map_or(0, T::char_len);
        iter.advance_iter();
        if iter.is_eof() {
            iter.result_cache.clear();
            return Some(iter.len);
        }
    }
}

impl WordBreakType for Utf16 {
    fn word_handle_complex_language(
        iter: &mut RuleBreakIterator<Self>,
        left_codepoint: Self::CharType,
    ) -> Option<usize> {
        // word segmenter doesn't define break rules for some languages such as Thai.
        let start_iter = iter.iter.clone();
        let start_point = iter.current_pos_data;
        let mut s = vec![left_codepoint as u16];
        loop {
            debug_assert!(!iter.is_eof());
            s.push(iter.get_current_codepoint()? as u16);
            iter.advance_iter();
            if let Some(current_break_property) = iter.get_current_break_property() {
                if current_break_property != iter.data.complex_property {
                    break;
                }
            } else {
                // EOF
                break;
            }
        }

        // Restore iterator to move to head of complex string
        iter.iter = start_iter;
        iter.current_pos_data = start_point;
        #[allow(clippy::unwrap_used)] // iter.complex present for word segmenter
        let breaks = iter.complex.unwrap().complex_language_segment_utf16(&s);
        iter.result_cache = breaks;
        // result_cache vector is utf-16 index that is in BMP.
        let first_pos = *iter.result_cache.first()?;
        let mut i = 1;
        loop {
            if i == first_pos {
                // Re-calculate breaking offset
                iter.result_cache = iter.result_cache.iter().skip(1).map(|r| r - i).collect();
                return iter.get_current_position();
            }
            debug_assert!(
                i < first_pos,
                "we should always arrive at first_pos: near index {:?}",
                iter.get_current_position()
            );
            i += 1;
            iter.advance_iter();
            if iter.is_eof() {
                iter.result_cache.clear();
                return Some(iter.len);
            }
        }
    }
}

#[cfg(all(test, feature = "serde"))]
#[test]
fn empty_string() {
    let segmenter = WordSegmenter::new_auto(WordBreakInvariantOptions::default());
    let breaks: Vec<usize> = segmenter.segment_str("").collect();
    assert_eq!(breaks, [0]);
}
