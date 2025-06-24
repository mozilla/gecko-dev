// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::vec::Vec;
use icu_provider::prelude::*;

use crate::indices::{Latin1Indices, Utf16Indices};
use crate::iterator_helpers::derive_usize_iterator_with_type;
use crate::provider::*;
use crate::rule_segmenter::*;
use utf8_iter::Utf8CharIndices;

/// Implements the [`Iterator`] trait over the grapheme cluster boundaries of the given string.
///
/// Lifetimes:
///
/// - `'data` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// The [`Iterator::Item`] is an [`usize`] representing index of a code unit
/// _after_ the boundary (for a boundary at the end of text, this index is the length
/// of the [`str`] or array of code units).
///
/// For examples of use, see [`GraphemeClusterSegmenter`].
#[derive(Debug)]
pub struct GraphemeClusterBreakIterator<'data, 's, Y: RuleBreakType>(
    RuleBreakIterator<'data, 's, Y>,
);

derive_usize_iterator_with_type!(GraphemeClusterBreakIterator, 'data);

/// Segments a string into grapheme clusters.
///
/// Supports loading grapheme cluster break data, and creating grapheme cluster break iterators for
/// different string encodings.
///
/// Most segmentation methods live on [`GraphemeClusterSegmenterBorrowed`], which can be obtained via
/// [`GraphemeClusterSegmenter::new()`] or [`GraphemeClusterSegmenter::as_borrowed()`].
///
/// # Examples
///
/// Segment a string:
///
/// ```rust
/// use icu::segmenter::GraphemeClusterSegmenter;
/// let segmenter = GraphemeClusterSegmenter::new();
///
/// let breakpoints: Vec<usize> = segmenter.segment_str("Hello 🗺").collect();
/// // World Map (U+1F5FA) is encoded in four bytes in UTF-8.
/// assert_eq!(&breakpoints, &[0, 1, 2, 3, 4, 5, 6, 10]);
/// ```
///
/// Segment a Latin1 byte string:
///
/// ```rust
/// use icu::segmenter::GraphemeClusterSegmenter;
/// let segmenter = GraphemeClusterSegmenter::new();
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_latin1(b"Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]);
/// ```
///
/// Successive boundaries can be used to retrieve the grapheme clusters.
/// In particular, the first boundary is always 0, and the last one is the
/// length of the segmented text in code units.
///
/// ```rust
/// # use icu::segmenter::GraphemeClusterSegmenter;
/// # let segmenter =
/// #     GraphemeClusterSegmenter::new();
/// use itertools::Itertools;
/// let text = "मांजर";
/// let grapheme_clusters: Vec<&str> = segmenter
///     .segment_str(text)
///     .tuple_windows()
///     .map(|(i, j)| &text[i..j])
///     .collect();
/// assert_eq!(&grapheme_clusters, &["मां", "ज", "र"]);
/// ```
///
/// This segmenter applies all rules provided to the constructor.
/// Thus, if the data supplied by the provider comprises all
/// [grapheme cluster boundary rules][Rules] from Unicode Standard Annex #29,
/// _Unicode Text Segmentation_, which is the case of default data
/// (both test data and data produced by `icu_provider_source`), the `segment_*`
/// functions return extended grapheme cluster boundaries, as opposed to
/// legacy grapheme cluster boundaries.  See [_Section 3, Grapheme Cluster
/// Boundaries_][GC], and [_Table 1a, Sample Grapheme Clusters_][Sample_GC],
/// in Unicode Standard Annex #29, _Unicode Text Segmentation_.
///
/// [Rules]: https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundary_Rules
/// [GC]: https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
/// [Sample_GC]: https://www.unicode.org/reports/tr29/#Table_Sample_Grapheme_Clusters
///
/// ```rust
/// use icu::segmenter::GraphemeClusterSegmenter;
/// let segmenter =
///     GraphemeClusterSegmenter::new();
///
/// // நி (TAMIL LETTER NA, TAMIL VOWEL SIGN I) is an extended grapheme cluster,
/// // but not a legacy grapheme cluster.
/// let ni = "நி";
/// let egc_boundaries: Vec<usize> = segmenter.segment_str(ni).collect();
/// assert_eq!(&egc_boundaries, &[0, ni.len()]);
/// ```
#[derive(Debug)]
pub struct GraphemeClusterSegmenter {
    payload: DataPayload<SegmenterBreakGraphemeClusterV1>,
}

/// Segments a string into grapheme clusters (borrowed version).
///
/// See [`GraphemeClusterSegmenter`] for examples.
#[derive(Clone, Debug, Copy)]
pub struct GraphemeClusterSegmenterBorrowed<'data> {
    data: &'data RuleBreakData<'data>,
}

impl GraphemeClusterSegmenter {
    /// Constructs a [`GraphemeClusterSegmenterBorrowed`] with an invariant locale from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)] // Deliberate choice, see #5554
    pub const fn new() -> GraphemeClusterSegmenterBorrowed<'static> {
        GraphemeClusterSegmenterBorrowed {
            data: crate::provider::Baked::SINGLETON_SEGMENTER_BREAK_GRAPHEME_CLUSTER_V1,
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakGraphemeClusterV1> + ?Sized,
    {
        let payload = provider.load(Default::default())?.payload;
        Ok(Self { payload })
    }

    /// Constructs a borrowed version of this type for more efficient querying.
    ///
    /// Most useful methods for segmentation are on this type.
    pub fn as_borrowed(&self) -> GraphemeClusterSegmenterBorrowed<'_> {
        GraphemeClusterSegmenterBorrowed {
            data: self.payload.get(),
        }
    }
}

impl<'data> GraphemeClusterSegmenterBorrowed<'data> {
    /// Creates a grapheme cluster break iterator for an `str` (a UTF-8 string).
    pub fn segment_str<'s>(self, input: &'s str) -> GraphemeClusterBreakIterator<'data, 's, Utf8> {
        GraphemeClusterBreakIterator(RuleBreakIterator {
            iter: input.char_indices(),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: None,
            boundary_property: 0,
            locale_override: None,
            handle_complex_language: empty_handle_complex_language,
        })
    }
    /// Creates a grapheme cluster break iterator for a potentially ill-formed UTF8 string
    ///
    /// Invalid characters are treated as REPLACEMENT CHARACTER
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf8<'s>(
        self,
        input: &'s [u8],
    ) -> GraphemeClusterBreakIterator<'data, 's, PotentiallyIllFormedUtf8> {
        GraphemeClusterBreakIterator(RuleBreakIterator {
            iter: Utf8CharIndices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: None,
            boundary_property: 0,
            locale_override: None,
            handle_complex_language: empty_handle_complex_language,
        })
    }
    /// Creates a grapheme cluster break iterator for a Latin-1 (8-bit) string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_latin1<'s>(
        self,
        input: &'s [u8],
    ) -> GraphemeClusterBreakIterator<'data, 's, Latin1> {
        GraphemeClusterBreakIterator(RuleBreakIterator {
            iter: Latin1Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: None,
            boundary_property: 0,
            locale_override: None,
            handle_complex_language: empty_handle_complex_language,
        })
    }

    /// Creates a grapheme cluster break iterator for a UTF-16 string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf16<'s>(
        self,
        input: &'s [u16],
    ) -> GraphemeClusterBreakIterator<'data, 's, Utf16> {
        GraphemeClusterBreakIterator(RuleBreakIterator {
            iter: Utf16Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.data,
            complex: None,
            boundary_property: 0,
            locale_override: None,
            handle_complex_language: empty_handle_complex_language,
        })
    }
}
impl GraphemeClusterSegmenterBorrowed<'static> {
    /// Cheaply converts a [`GraphemeClusterSegmenterBorrowed<'static>`] into a [`GraphemeClusterSegmenter`].
    ///
    /// Note: Due to branching and indirection, using [`GraphemeClusterSegmenter`] might inhibit some
    /// compile-time optimizations that are possible with [`GraphemeClusterSegmenterBorrowed`].
    pub const fn static_to_owned(self) -> GraphemeClusterSegmenter {
        GraphemeClusterSegmenter {
            payload: DataPayload::from_static_ref(self.data),
        }
    }
}

#[test]
fn empty_string() {
    let segmenter = GraphemeClusterSegmenter::new();
    let breaks: Vec<usize> = segmenter.segment_str("").collect();
    assert_eq!(breaks, [0]);
}

#[test]
fn emoji_flags() {
    // https://github.com/unicode-org/icu4x/issues/4780
    let segmenter = GraphemeClusterSegmenter::new();
    let breaks: Vec<usize> = segmenter.segment_str("🇺🇸🏴󠁧󠁢󠁥󠁮󠁧󠁿").collect();
    assert_eq!(breaks, [0, 8, 36]);
}
