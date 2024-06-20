// @generated
/// Implement `DataProvider<WeekDataV1Marker>` on the given struct using the data
/// hardcoded in this file. This allows the struct to be used with
/// `icu`'s `_unstable` constructors.
#[doc(hidden)]
#[macro_export]
macro_rules! __impl_datetime_week_data_v1 {
    ($ provider : ty) => {
        #[clippy::msrv = "1.67"]
        const _: () = <$provider>::MUST_USE_MAKE_PROVIDER_MACRO;
        #[clippy::msrv = "1.67"]
        impl icu_provider::DataProvider<icu::calendar::provider::WeekDataV1Marker> for $provider {
            fn load(&self, req: icu_provider::DataRequest) -> Result<icu_provider::DataResponse<icu::calendar::provider::WeekDataV1Marker>, icu_provider::DataError> {
                static UND_MV: <icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable = icu::calendar::provider::WeekDataV1 { first_weekday: icu::calendar::types::IsoWeekday::Friday, min_week_days: 1u8 };
                static UND: <icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable = icu::calendar::provider::WeekDataV1 { first_weekday: icu::calendar::types::IsoWeekday::Monday, min_week_days: 1u8 };
                static UND_AD: <icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable = icu::calendar::provider::WeekDataV1 { first_weekday: icu::calendar::types::IsoWeekday::Monday, min_week_days: 4u8 };
                static UND_AE: <icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable = icu::calendar::provider::WeekDataV1 { first_weekday: icu::calendar::types::IsoWeekday::Saturday, min_week_days: 1u8 };
                static UND_AG: <icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable = icu::calendar::provider::WeekDataV1 { first_weekday: icu::calendar::types::IsoWeekday::Sunday, min_week_days: 1u8 };
                static UND_PT: <icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable = icu::calendar::provider::WeekDataV1 { first_weekday: icu::calendar::types::IsoWeekday::Sunday, min_week_days: 4u8 };
                static VALUES: [&<icu::calendar::provider::WeekDataV1Marker as icu_provider::DataMarker>::Yokeable; 115usize] = [&UND, &UND_AD, &UND_AE, &UND_AE, &UND_AG, &UND_AD, &UND_AG, &UND_AD, &UND_AD, &UND_AG, &UND_AD, &UND_AD, &UND_AE, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AD, &UND_AG, &UND_AD, &UND_AD, &UND_AE, &UND_AD, &UND_AG, &UND_AG, &UND_AE, &UND_AD, &UND_AE, &UND_AD, &UND_AG, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AD, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AD, &UND_AG, &UND_AD, &UND_AG, &UND_AD, &UND_AG, &UND_AE, &UND_AE, &UND_AD, &UND_AD, &UND_AD, &UND_AG, &UND_AE, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AE, &UND_AG, &UND_AD, &UND_AD, &UND_AD, &UND_AE, &UND_AD, &UND_AG, &UND_AG, &UND_AG, &UND_AD, &UND_AG, &UND_MV, &UND_AG, &UND_AG, &UND_AG, &UND_AD, &UND_AD, &UND_AG, &UND_AE, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AD, &UND_AG, &UND_PT, &UND_AG, &UND_AE, &UND_AD, &UND_AD, &UND_AG, &UND_AE, &UND_AD, &UND_AG, &UND_AD, &UND_AD, &UND_AD, &UND_AG, &UND_AE, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AD, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AG, &UND_AG];
                static KEYS: [&str; 115usize] = ["und", "und-AD", "und-AE", "und-AF", "und-AG", "und-AN", "und-AS", "und-AT", "und-AX", "und-BD", "und-BE", "und-BG", "und-BH", "und-BR", "und-BS", "und-BT", "und-BW", "und-BZ", "und-CA", "und-CH", "und-CO", "und-CZ", "und-DE", "und-DJ", "und-DK", "und-DM", "und-DO", "und-DZ", "und-EE", "und-EG", "und-ES", "und-ET", "und-FI", "und-FJ", "und-FO", "und-FR", "und-GB", "und-GF", "und-GG", "und-GI", "und-GP", "und-GR", "und-GT", "und-GU", "und-HK", "und-HN", "und-HU", "und-ID", "und-IE", "und-IL", "und-IM", "und-IN", "und-IQ", "und-IR", "und-IS", "und-IT", "und-JE", "und-JM", "und-JO", "und-JP", "und-KE", "und-KH", "und-KR", "und-KW", "und-LA", "und-LI", "und-LT", "und-LU", "und-LY", "und-MC", "und-MH", "und-MM", "und-MO", "und-MQ", "und-MT", "und-MV", "und-MX", "und-MZ", "und-NI", "und-NL", "und-NO", "und-NP", "und-OM", "und-PA", "und-PE", "und-PH", "und-PK", "und-PL", "und-PR", "und-PT", "und-PY", "und-QA", "und-RE", "und-RU", "und-SA", "und-SD", "und-SE", "und-SG", "und-SJ", "und-SK", "und-SM", "und-SV", "und-SY", "und-TH", "und-TT", "und-TW", "und-UM", "und-US", "und-VA", "und-VE", "und-VI", "und-WS", "und-YE", "und-ZA", "und-ZW"];
                let mut metadata = icu_provider::DataResponseMetadata::default();
                let payload = if let Ok(payload) = KEYS.binary_search_by(|k| req.locale.strict_cmp(k.as_bytes()).reverse()).map(|i| *unsafe { VALUES.get_unchecked(i) }) {
                    payload
                } else {
                    const FALLBACKER: icu::locid_transform::fallback::LocaleFallbackerWithConfig<'static> = icu::locid_transform::fallback::LocaleFallbacker::new().for_config(<icu::calendar::provider::WeekDataV1Marker as icu_provider::KeyedDataMarker>::KEY.fallback_config());
                    let mut fallback_iterator = FALLBACKER.fallback_for(req.locale.clone());
                    loop {
                        if let Ok(payload) = KEYS.binary_search_by(|k| fallback_iterator.get().strict_cmp(k.as_bytes()).reverse()).map(|i| *unsafe { VALUES.get_unchecked(i) }) {
                            metadata.locale = Some(fallback_iterator.take());
                            break payload;
                        }
                        fallback_iterator.step();
                    }
                };
                Ok(icu_provider::DataResponse { payload: Some(icu_provider::DataPayload::from_static_ref(payload)), metadata })
            }
        }
    };
}
