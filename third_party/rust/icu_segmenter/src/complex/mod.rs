// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::*;
use crate::{GraphemeClusterSegmenter, GraphemeClusterSegmenterBorrowed};
use alloc::vec::Vec;
use icu_provider::prelude::*;

mod dictionary;
use dictionary::*;
mod language;
use language::*;
#[cfg(feature = "lstm")]
mod lstm;
#[cfg(feature = "lstm")]
use lstm::*;

#[derive(Debug, Clone)]
#[allow(clippy::large_enum_variant)]
enum DictOrLstm {
    Dict(DataPayload<UCharDictionaryBreakDataV1>),
    #[cfg(feature = "lstm")]
    Lstm(DataPayload<SegmenterLstmAutoV1>),
}

#[derive(Debug, Clone, Copy)]
enum DictOrLstmBorrowed<'data> {
    Dict(&'data UCharDictionaryBreakData<'data>),
    #[cfg(feature = "lstm")]
    Lstm(&'data LstmData<'data>),
}

fn borrow_dictor(dict_or: &DictOrLstm) -> DictOrLstmBorrowed<'_> {
    match dict_or {
        DictOrLstm::Dict(dict) => DictOrLstmBorrowed::Dict(dict.get()),
        #[cfg(feature = "lstm")]
        DictOrLstm::Lstm(lstm) => DictOrLstmBorrowed::Lstm(lstm.get()),
    }
}

fn fromstatic_dictor(dict_or: DictOrLstmBorrowed<'static>) -> DictOrLstm {
    match dict_or {
        DictOrLstmBorrowed::Dict(dict) => DictOrLstm::Dict(DataPayload::from_static_ref(dict)),
        #[cfg(feature = "lstm")]
        DictOrLstmBorrowed::Lstm(lstm) => DictOrLstm::Lstm(DataPayload::from_static_ref(lstm)),
    }
}

#[derive(Debug)]
pub(crate) struct ComplexPayloads {
    grapheme: GraphemeClusterSegmenter,
    my: Option<DictOrLstm>,
    km: Option<DictOrLstm>,
    lo: Option<DictOrLstm>,
    th: Option<DictOrLstm>,
    ja: Option<DataPayload<UCharDictionaryBreakDataV1>>,
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct ComplexPayloadsBorrowed<'data> {
    grapheme: GraphemeClusterSegmenterBorrowed<'data>,
    my: Option<DictOrLstmBorrowed<'data>>,
    km: Option<DictOrLstmBorrowed<'data>>,
    lo: Option<DictOrLstmBorrowed<'data>>,
    th: Option<DictOrLstmBorrowed<'data>>,
    ja: Option<&'data UCharDictionaryBreakData<'data>>,
}

#[cfg(feature = "lstm")]
const MY_LSTM: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("Burmese_");
#[cfg(feature = "lstm")]
const KM_LSTM: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("Khmer_");
#[cfg(feature = "lstm")]
const LO_LSTM: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("Lao_");
#[cfg(feature = "lstm")]
const TH_LSTM: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("Thai_");

const MY_DICT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("burmesedict");
const KM_DICT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("khmerdict");
const LO_DICT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("laodict");
const TH_DICT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("thaidict");
const CJ_DICT: &DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("cjdict");

impl<'data> ComplexPayloadsBorrowed<'data> {
    fn select(&self, language: Language) -> Option<DictOrLstmBorrowed<'data>> {
        const ERR: DataError = DataError::custom("No segmentation model for language");
        match language {
            Language::Burmese => self.my.or_else(|| {
                ERR.with_display_context("my");
                None
            }),
            Language::Khmer => self.km.or_else(|| {
                ERR.with_display_context("km");
                None
            }),
            Language::Lao => self.lo.or_else(|| {
                ERR.with_display_context("lo");
                None
            }),
            Language::Thai => self.th.or_else(|| {
                ERR.with_display_context("th");
                None
            }),
            Language::ChineseOrJapanese => self.ja.map(DictOrLstmBorrowed::Dict).or_else(|| {
                ERR.with_display_context("ja");
                None
            }),
            Language::Unknown => None,
        }
    }
    pub(crate) fn complex_language_segment_str(&self, input: &str) -> Vec<usize> {
        let mut result = Vec::new();
        let mut offset = 0;
        for (slice, lang) in LanguageIterator::new(input) {
            match self.select(lang) {
                Some(DictOrLstmBorrowed::Dict(dict)) => {
                    let seg = DictionarySegmenter::new(dict, self.grapheme);
                    result.extend(seg.segment_str(slice).map(|n| offset + n));
                }
                #[cfg(feature = "lstm")]
                Some(DictOrLstmBorrowed::Lstm(lstm)) => {
                    let seg = LstmSegmenter::new(lstm, self.grapheme);
                    result.extend(seg.segment_str(slice).map(|n| offset + n));
                }
                None => {
                    result.push(offset + slice.len());
                }
            }
            offset += slice.len();
        }
        result
    }
    /// Return UTF-16 segment offset array using dictionary or lstm segmenter.
    pub(crate) fn complex_language_segment_utf16(&self, input: &[u16]) -> Vec<usize> {
        let mut result = Vec::new();
        let mut offset = 0;
        for (slice, lang) in LanguageIteratorUtf16::new(input) {
            match self.select(lang) {
                Some(DictOrLstmBorrowed::Dict(dict)) => {
                    let seg = DictionarySegmenter::new(dict, self.grapheme);
                    result.extend(seg.segment_utf16(slice).map(|n| offset + n));
                }
                #[cfg(feature = "lstm")]
                Some(DictOrLstmBorrowed::Lstm(lstm)) => {
                    let seg = LstmSegmenter::new(lstm, self.grapheme);
                    result.extend(seg.segment_utf16(slice).map(|n| offset + n));
                }
                None => {
                    result.push(offset + slice.len());
                }
            }
            offset += slice.len();
        }
        result
    }
}
impl ComplexPayloadsBorrowed<'static> {
    #[cfg(feature = "lstm")]
    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_lstm() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: GraphemeClusterSegmenter::new(),
            my: try_load_static::<SegmenterLstmAutoV1, _>(&crate::provider::Baked, MY_LSTM)
                .unwrap()
                .map(DictOrLstmBorrowed::Lstm),
            km: try_load_static::<SegmenterLstmAutoV1, _>(&crate::provider::Baked, KM_LSTM)
                .unwrap()
                .map(DictOrLstmBorrowed::Lstm),
            lo: try_load_static::<SegmenterLstmAutoV1, _>(&crate::provider::Baked, LO_LSTM)
                .unwrap()
                .map(DictOrLstmBorrowed::Lstm),
            th: try_load_static::<SegmenterLstmAutoV1, _>(&crate::provider::Baked, TH_LSTM)
                .unwrap()
                .map(DictOrLstmBorrowed::Lstm),
            ja: None,
        }
    }
    #[cfg(feature = "auto")]
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::unwrap_used)]
    pub(crate) fn new_auto() -> Self {
        let mut this = Self::new_lstm();
        this.ja = try_load_static::<SegmenterDictionaryAutoV1, _>(&crate::provider::Baked, CJ_DICT)
            .unwrap();
        this
    }
    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_dict() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: GraphemeClusterSegmenter::new(),
            my: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                MY_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            km: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                KM_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            lo: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                LO_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            th: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                TH_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            ja: try_load_static::<SegmenterDictionaryAutoV1, _>(&crate::provider::Baked, CJ_DICT)
                .unwrap(),
        }
    }

    #[cfg(feature = "compiled_data")]
    pub(crate) fn new_southeast_asian() -> Self {
        #[allow(clippy::unwrap_used)]
        // try_load is infallible if the provider only returns `MissingLocale`.
        Self {
            grapheme: GraphemeClusterSegmenter::new(),
            my: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                MY_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            km: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                KM_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            lo: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                LO_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            th: try_load_static::<SegmenterDictionaryExtendedV1, _>(
                &crate::provider::Baked,
                TH_DICT,
            )
            .unwrap()
            .map(DictOrLstmBorrowed::Dict),
            ja: None,
        }
    }

    pub(crate) fn static_to_owned(self) -> ComplexPayloads {
        ComplexPayloads {
            grapheme: self.grapheme.static_to_owned(),
            my: self.my.map(fromstatic_dictor),
            km: self.km.map(fromstatic_dictor),
            lo: self.lo.map(fromstatic_dictor),
            th: self.th.map(fromstatic_dictor),
            ja: self.ja.map(DataPayload::from_static_ref),
        }
    }
}

impl ComplexPayloads {
    pub(crate) fn as_borrowed(&self) -> ComplexPayloadsBorrowed<'_> {
        ComplexPayloadsBorrowed {
            grapheme: self.grapheme.as_borrowed(),
            my: self.my.as_ref().map(borrow_dictor),
            km: self.km.as_ref().map(borrow_dictor),
            lo: self.lo.as_ref().map(borrow_dictor),
            th: self.th.as_ref().map(borrow_dictor),
            ja: self.ja.as_ref().map(|p| p.get()),
        }
    }

    #[cfg(feature = "lstm")]
    pub(crate) fn try_new_lstm<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakGraphemeClusterV1>
            + DataProvider<SegmenterLstmAutoV1>
            + ?Sized,
    {
        Ok(Self {
            grapheme: GraphemeClusterSegmenter::try_new_unstable(provider)?,
            my: try_load::<SegmenterLstmAutoV1, D>(provider, MY_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            km: try_load::<SegmenterLstmAutoV1, D>(provider, KM_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            lo: try_load::<SegmenterLstmAutoV1, D>(provider, LO_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            th: try_load::<SegmenterLstmAutoV1, D>(provider, TH_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            ja: None,
        })
    }

    pub(crate) fn try_new_dict<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakGraphemeClusterV1>
            + DataProvider<SegmenterDictionaryExtendedV1>
            + DataProvider<SegmenterDictionaryAutoV1>
            + ?Sized,
    {
        Ok(Self {
            grapheme: GraphemeClusterSegmenter::try_new_unstable(provider)?,
            my: try_load::<SegmenterDictionaryExtendedV1, D>(provider, MY_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            km: try_load::<SegmenterDictionaryExtendedV1, D>(provider, KM_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            lo: try_load::<SegmenterDictionaryExtendedV1, D>(provider, LO_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            th: try_load::<SegmenterDictionaryExtendedV1, D>(provider, TH_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            ja: try_load::<SegmenterDictionaryAutoV1, D>(provider, CJ_DICT)?.map(DataPayload::cast),
        })
    }

    #[cfg(feature = "auto")] // Use by WordSegmenter with "auto" enabled.
    pub(crate) fn try_new_auto<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterBreakGraphemeClusterV1>
            + DataProvider<SegmenterLstmAutoV1>
            + DataProvider<SegmenterDictionaryAutoV1>
            + ?Sized,
    {
        Ok(Self {
            grapheme: GraphemeClusterSegmenter::try_new_unstable(provider)?,
            my: try_load::<SegmenterLstmAutoV1, D>(provider, MY_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            km: try_load::<SegmenterLstmAutoV1, D>(provider, KM_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            lo: try_load::<SegmenterLstmAutoV1, D>(provider, LO_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            th: try_load::<SegmenterLstmAutoV1, D>(provider, TH_LSTM)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Lstm),
            ja: try_load::<SegmenterDictionaryAutoV1, D>(provider, CJ_DICT)?.map(DataPayload::cast),
        })
    }

    pub(crate) fn try_new_southeast_asian<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<SegmenterDictionaryExtendedV1>
            + DataProvider<SegmenterBreakGraphemeClusterV1>
            + ?Sized,
    {
        Ok(Self {
            grapheme: GraphemeClusterSegmenter::try_new_unstable(provider)?,
            my: try_load::<SegmenterDictionaryExtendedV1, _>(provider, MY_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            km: try_load::<SegmenterDictionaryExtendedV1, _>(provider, KM_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            lo: try_load::<SegmenterDictionaryExtendedV1, _>(provider, LO_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            th: try_load::<SegmenterDictionaryExtendedV1, _>(provider, TH_DICT)?
                .map(DataPayload::cast)
                .map(DictOrLstm::Dict),
            ja: None,
        })
    }
}
fn try_load<M: DataMarker, P: DataProvider<M> + ?Sized>(
    provider: &P,
    model: &'static DataMarkerAttributes,
) -> Result<Option<DataPayload<M>>, DataError> {
    provider
        .load(DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes(model),
            metadata: {
                let mut m = DataRequestMetadata::default();
                m.silent = true;
                m.attributes_prefix_match = true;
                m
            },
        })
        .allow_identifier_not_found()
        .map(|r| r.map(|r| r.payload))
}

#[cfg(feature = "compiled_data")]
fn try_load_static<M: DataMarker, P: DataProvider<M> + ?Sized>(
    provider: &P,
    model: &'static DataMarkerAttributes,
) -> Result<Option<&'static <M::DataStruct as yoke::Yokeable<'static>>::Output>, DataError> {
    provider
        .load(DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes(model),
            metadata: {
                let mut m = DataRequestMetadata::default();
                m.silent = true;
                m.attributes_prefix_match = true;
                m
            },
        })
        .allow_identifier_not_found()
        .map(|r| r.and_then(|r| r.payload.get_static()))
}

#[cfg(test)]
#[cfg(feature = "serde")]
mod tests {
    use super::*;

    #[test]
    fn thai_word_break() {
        const TEST_STR: &str = "ภาษาไทยภาษาไทย";
        let utf16: Vec<u16> = TEST_STR.encode_utf16().collect();

        let lstm = ComplexPayloadsBorrowed::new_lstm();
        let dict = ComplexPayloadsBorrowed::new_dict();

        assert_eq!(
            lstm.complex_language_segment_str(TEST_STR),
            [12, 21, 33, 42]
        );
        assert_eq!(lstm.complex_language_segment_utf16(&utf16), [4, 7, 11, 14]);

        assert_eq!(
            dict.complex_language_segment_str(TEST_STR),
            [12, 21, 33, 42]
        );
        assert_eq!(dict.complex_language_segment_utf16(&utf16), [4, 7, 11, 14]);
    }
}
