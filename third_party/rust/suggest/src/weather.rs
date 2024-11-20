/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::named_params;
use serde::Deserialize;
use sql_support::ConnExt;

use std::{cmp::Ordering, collections::HashSet};

use crate::{
    config::SuggestProviderConfig,
    db::{
        KeywordInsertStatement, KeywordMetricsInsertStatement, SuggestDao,
        SuggestionInsertStatement, DEFAULT_SUGGESTION_SCORE,
    },
    geoname::{GeonameMatch, GeonameType},
    metrics::MetricsContext,
    provider::SuggestionProvider,
    rs::{Client, Record, SuggestRecordId},
    store::SuggestStoreInner,
    suggestion::Suggestion,
    util::filter_map_chunks,
    Result, SuggestionQuery,
};

#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedWeatherAttachment {
    /// Weather keywords.
    pub keywords: Vec<String>,
    /// Threshold for weather keyword prefix matching when a weather keyword is
    /// the first term in a query. `None` means prefix matching is disabled and
    /// weather keywords must be typed in full when they are first in the query.
    /// This threshold does not apply to city and region names. If there are
    /// multiple weather records, we use the `min_keyword_length` in the most
    /// recently ingested record.
    pub min_keyword_length: Option<i32>,
    /// Score for weather suggestions. If there are multiple weather records, we
    /// use the `score` from the most recently ingested record.
    pub score: Option<f64>,
    /// The max length of all keywords in the attachment. Used for keyword
    /// metrics. We pre-compute this to avoid doing duplicate work on all user's
    /// machines.
    pub max_keyword_length: u32,
    /// The max word count of all keywords in the attachment. Used for keyword
    /// metrics. We pre-compute this to avoid doing duplicate work on all user's
    /// machines.
    pub max_keyword_word_count: u32,
}

/// This data is used to service every query handled by the weather provider, so
/// we cache it from the DB.
#[derive(Debug, Default)]
pub struct WeatherCache {
    /// Cached value of the same name from `SuggestProviderConfig::Weather`.
    min_keyword_length: i32,
    /// Cached value of the same name from `SuggestProviderConfig::Weather`.
    score: f64,
    /// Max length of all weather keywords.
    max_keyword_length: usize,
    /// Max word count across all weather keywords.
    max_keyword_word_count: usize,
}

impl SuggestDao<'_> {
    /// Fetches weather suggestions.
    pub fn fetch_weather_suggestions(&self, query: &SuggestionQuery) -> Result<Vec<Suggestion>> {
        // We'll just stipulate we won't support tiny queries in order to avoid
        // a bunch of work when the user starts typing a query.
        if query.keyword.len() < 3 {
            return Ok(vec![]);
        }

        // The first step in parsing the query is lowercasing and splitting it
        // into words. We want to avoid that work for strings that are so long
        // they can't possibly match. The longest possible weather query is two
        // geonames + one weather keyword + at least two spaces between those
        // three components, say, 10 extra characters total for spaces and
        // punctuation. There's no point in an analogous min length check since
        // weather suggestions can be matched on city alone and many city names
        // are only a few characters long ("nyc").
        let g_cache = self.geoname_cache();
        let w_cache = self.weather_cache();
        let max_query_len = 2 * g_cache.max_name_length + w_cache.max_keyword_length + 10;
        if max_query_len < query.keyword.len() {
            return Ok(vec![]);
        }

        let max_chunk_size =
            std::cmp::max(g_cache.max_name_word_count, w_cache.max_keyword_word_count);

        // Lowercase, strip punctuation, and split the query into words.
        let kw_lower = query.keyword.to_lowercase();
        let words: Vec<_> = kw_lower
            .split_whitespace()
            .flat_map(|w| {
                w.split(|c| !char::is_alphabetic(c))
                    .filter(|s| !s.is_empty())
            })
            .collect();

        let mut matches =
            // Step 2: Parse the query words into a list of token paths.
            filter_map_chunks::<Token>(&words, max_chunk_size, |chunk, chunk_i, path| {
                // Match the chunk to token types that haven't already been matched
                // in this path. `all_tokens` will remain `None` until a token is
                // matched.
                let mut all_tokens: Option<Vec<Token>> = None;
                for tt in [
                    TokenType::City,
                    TokenType::Region,
                    TokenType::WeatherKeyword,
                ] {
                    if !path.iter().any(|t| t.token_type() == tt) {
                        let mut tokens = self.match_weather_tokens(tt, path, chunk, chunk_i == 0)?;
                        if !tokens.is_empty() {
                            let mut ts = all_tokens.take().unwrap_or_default();
                            ts.append(&mut tokens);
                            all_tokens.replace(ts);
                        }
                    }
                }
                // If no tokens were matched, `all_tokens` will be `None`.
                Ok(all_tokens)
            })?
            .into_iter()
            // Step 3: Map each token path to a city-region-keyword tuple (each
            // optional). Paths are vecs, so they're ordered, so we may end up
            // with duplicate tuples after this step. e.g., the paths
            // `[<Waterloo IA>, <IA>]` and `[<IA>, <Waterloo IA>]` map to the
            // same `(<Waterloo IA>, <IA>, None)` tuple.
            .map(|path| {
                path.into_iter()
                    .fold((None, None, None), |mut match_tuple, token| {
                        match token {
                            Token::City(c) => {
                                match_tuple.0 = Some(c);
                            }
                            Token::Region(r) => {
                                match_tuple.1 = Some(r);
                            }
                            Token::WeatherKeyword(kw) => {
                                match_tuple.2 = Some(kw);
                            }
                        }
                        match_tuple
                    })
            })
            // Step 4: Discard tuples that don't have the right combination of
            // tokens or that are otherwise invalid. Along with step 2, this is
            // the core of the matching logic. In general, allow a tuple if it
            // has (a) a city name typed in full or (b) a weather keyword at
            // least as long as the config's min keyword length, since that
            // indicates a weather intent.
            .filter(|(city_match, region_match, kw_match)| {
                match (city_match, region_match, kw_match) {
                    (None, None, Some(_)) => true,
                    (None, _, None) | (None, Some(_), Some(_)) => false,
                    (Some(city), region, kw) => {
                        (city.match_type.is_name() && !city.prefix)
                            // Allow city abbreviations without a weather
                            // keyword but only if the region was typed in full.
                            || (city.match_type.is_abbreviation()
                                && !city.prefix
                                && region.as_ref().map(|r| !r.prefix).unwrap_or(false))
                            || kw.as_ref().map(|k| k.is_min_keyword_length).unwrap_or(false)
                    }
                }
            })
            // Step 5: Map each tuple to a city-region tuple: Convert geoname
            // matches to their `Geoname` values and discard keywords.
            // Discarding keywords is important because we'll collect the tuples
            // in a set in the next step in order to dedupe city-regions.
            .map(|(city, region, _)| {
                (city.map(|c| c.geoname), region.map(|r| r.geoname))
            })
            // Step 6: Dedupe the city-regions by collecting them in a set.
            .collect::<HashSet<_>>()
            .into_iter()
            .collect::<Vec<_>>();

        // Sort the matches so cities with larger populations are first.
        matches.sort_by(
            |(city1, region1), (city2, region2)| match (&city1, &city2) {
                (Some(_), None) => Ordering::Less,
                (None, Some(_)) => Ordering::Greater,
                (Some(c1), Some(c2)) => c2.population.cmp(&c1.population),
                (None, None) => match (&region1, &region2) {
                    (Some(_), None) => Ordering::Less,
                    (None, Some(_)) => Ordering::Greater,
                    (Some(r1), Some(r2)) => r2.population.cmp(&r1.population),
                    (None, None) => Ordering::Equal,
                },
            },
        );

        // Finally, map matches to suggestions.
        Ok(matches
            .iter()
            .map(|(city, _)| Suggestion::Weather {
                city: city.as_ref().map(|c| c.name.clone()),
                region: city.as_ref().map(|c| c.admin1_code.clone()),
                country: city.as_ref().map(|c| c.country_code.clone()),
                latitude: city.as_ref().map(|c| c.latitude),
                longitude: city.as_ref().map(|c| c.longitude),
                score: w_cache.score,
            })
            .collect())
    }

    fn match_weather_tokens(
        &self,
        token_type: TokenType,
        path: &[Token],
        candidate: &str,
        is_first_chunk: bool,
    ) -> Result<Vec<Token>> {
        match token_type {
            TokenType::City => {
                // Fetch matching cities, and filter them to regions we've
                // already matched in this path.
                let regions: Vec<_> = path
                    .iter()
                    .filter_map(|t| t.region().map(|m| &m.geoname))
                    .collect();
                Ok(self
                    .fetch_geonames(
                        candidate,
                        !is_first_chunk,
                        Some(GeonameType::City),
                        if regions.is_empty() {
                            None
                        } else {
                            Some(regions)
                        },
                    )?
                    .into_iter()
                    .map(Token::City)
                    .collect())
            }
            TokenType::Region => {
                // Fetch matching regions, and filter them to cities we've
                // already matched in this patch.
                let cities: Vec<_> = path
                    .iter()
                    .filter_map(|t| t.city().map(|m| &m.geoname))
                    .collect();
                Ok(self
                    .fetch_geonames(
                        candidate,
                        !is_first_chunk,
                        Some(GeonameType::Region),
                        if cities.is_empty() {
                            None
                        } else {
                            Some(cities)
                        },
                    )?
                    .into_iter()
                    .map(Token::Region)
                    .collect())
            }
            TokenType::WeatherKeyword => {
                // Fetch matching keywords. `min_keyword_length == 0` in the
                // config means that the config doesn't allow prefix matching.
                // `min_keyword_length > 0` means that the keyword must be at
                // least that long when there's not already a city name present
                // in the query.
                let len = self.weather_cache().min_keyword_length;
                if is_first_chunk && (candidate.len() as i32) < len {
                    // The candidate is the first term in the query and it's too
                    // short.
                    Ok(vec![])
                } else {
                    // Do arbitrary prefix matching if the candidate isn't the
                    // first term in the query or if the config allows prefix
                    // matching.
                    Ok(self
                        .match_weather_keywords(candidate, !is_first_chunk || len > 0)?
                        .into_iter()
                        .map(|keyword| {
                            Token::WeatherKeyword(WeatherKeywordMatch {
                                keyword,
                                is_min_keyword_length: (len as usize) <= candidate.len(),
                            })
                        })
                        .collect())
                }
            }
        }
    }

    fn match_weather_keywords(&self, candidate: &str, prefix: bool) -> Result<Vec<String>> {
        self.conn.query_rows_and_then_cached(
            r#"
            SELECT
                k.keyword,
                s.score,
                k.keyword != :keyword AS matched_prefix
            FROM
                suggestions s
            JOIN
                keywords k
                ON k.suggestion_id = s.id
            WHERE
                s.provider = :provider
                AND (
                    CASE :prefix WHEN FALSE THEN k.keyword = :keyword
                    ELSE (k.keyword BETWEEN :keyword AND :keyword || X'FFFF') END
                )
             "#,
            named_params! {
                ":prefix": prefix,
                ":keyword": candidate,
                ":provider": SuggestionProvider::Weather
            },
            |row| -> Result<String> { Ok(row.get("keyword")?) },
        )
    }

    /// Inserts weather suggestions data into the database.
    fn insert_weather_data(
        &mut self,
        record_id: &SuggestRecordId,
        attachments: &[DownloadedWeatherAttachment],
    ) -> Result<()> {
        self.scope.err_if_interrupted()?;
        let mut suggestion_insert = SuggestionInsertStatement::new(self.conn)?;
        let mut keyword_insert = KeywordInsertStatement::new(self.conn)?;
        let mut keyword_metrics_insert = KeywordMetricsInsertStatement::new(self.conn)?;
        let mut max_len = 0;
        let mut max_word_count = 0;
        for attach in attachments {
            let suggestion_id = suggestion_insert.execute(
                record_id,
                "",
                "",
                attach.score.unwrap_or(DEFAULT_SUGGESTION_SCORE),
                SuggestionProvider::Weather,
            )?;
            for (i, keyword) in attach.keywords.iter().enumerate() {
                keyword_insert.execute(suggestion_id, keyword, None, i)?;
            }
            self.put_provider_config(SuggestionProvider::Weather, &attach.into())?;
            max_len = std::cmp::max(max_len, attach.max_keyword_length as usize);
            max_word_count = std::cmp::max(max_word_count, attach.max_keyword_word_count as usize);
        }

        // Update keyword metrics.
        keyword_metrics_insert.execute(
            record_id,
            SuggestionProvider::Weather,
            max_len,
            max_word_count,
        )?;

        // We just made some insertions that might invalidate the data in the
        // cache. Clear it so it's repopulated the next time it's accessed.
        self.weather_cache.take();

        Ok(())
    }

    fn weather_cache(&self) -> &WeatherCache {
        self.weather_cache.get_or_init(|| {
            let mut cache = WeatherCache::default();

            // keyword metrics
            if let Ok((len, word_count)) = self.conn.query_row_and_then(
                r#"
                SELECT
                    max(max_length) AS len, max(max_word_count) AS word_count
                FROM
                    keywords_metrics
                WHERE
                    provider = :provider
                "#,
                named_params! {
                    ":provider": SuggestionProvider::Weather
                },
                |row| -> Result<(usize, usize)> { Ok((row.get("len")?, row.get("word_count")?)) },
            ) {
                cache.max_keyword_length = len;
                cache.max_keyword_word_count = word_count;
            }

            // provider config
            if let Ok(Some(SuggestProviderConfig::Weather {
                score,
                min_keyword_length,
            })) = self.get_provider_config(SuggestionProvider::Weather)
            {
                cache.min_keyword_length = min_keyword_length;
                cache.score = score;
            }

            cache
        })
    }
}

impl<S> SuggestStoreInner<S>
where
    S: Client,
{
    /// Inserts a weather record into the database.
    pub fn process_weather_record(
        &self,
        dao: &mut SuggestDao,
        record: &Record,
        context: &mut MetricsContext,
    ) -> Result<()> {
        self.download_attachment(dao, record, context, |dao, record_id, data| {
            dao.insert_weather_data(record_id, data)
        })
    }
}

impl From<&DownloadedWeatherAttachment> for SuggestProviderConfig {
    fn from(a: &DownloadedWeatherAttachment) -> Self {
        Self::Weather {
            score: a.score.unwrap_or(DEFAULT_SUGGESTION_SCORE),
            min_keyword_length: a.min_keyword_length.unwrap_or(0),
        }
    }
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
enum TokenType {
    City,
    Region,
    WeatherKeyword,
}

#[derive(Clone, Debug)]
enum Token {
    City(GeonameMatch),
    Region(GeonameMatch),
    WeatherKeyword(WeatherKeywordMatch),
}

impl Token {
    fn city(&self) -> Option<&GeonameMatch> {
        match self {
            Self::City(g) => Some(g),
            _ => None,
        }
    }

    fn region(&self) -> Option<&GeonameMatch> {
        match self {
            Self::Region(g) => Some(g),
            _ => None,
        }
    }

    fn token_type(&self) -> TokenType {
        match self {
            Self::City(_) => TokenType::City,
            Self::Region(_) => TokenType::Region,
            Self::WeatherKeyword(_) => TokenType::WeatherKeyword,
        }
    }
}

#[derive(Clone, Debug, Default, Eq, Hash, PartialEq)]
struct WeatherKeywordMatch {
    keyword: String,
    is_min_keyword_length: bool,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        geoname, geoname::Geoname, store::tests::TestStore, testing::*, SuggestIngestionConstraints,
    };

    impl From<Geoname> for Suggestion {
        fn from(g: Geoname) -> Self {
            Suggestion::Weather {
                city: Some(g.name),
                region: Some(g.admin1_code),
                country: Some(g.country_code),
                latitude: Some(g.latitude),
                longitude: Some(g.longitude),
                score: 0.24,
            }
        }
    }

    #[test]
    fn weather_provider_config() -> anyhow::Result<()> {
        before_each();
        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "weather",
            "weather-1",
            json!({
                "min_keyword_length": 3,
                "keywords": ["ab", "xyz", "weather"],
                "max_keyword_length": "weather".len(),
                "max_keyword_word_count": 1,
                "score": 0.24
            }),
        ));
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        assert_eq!(
            store.fetch_provider_config(SuggestionProvider::Weather),
            Some(SuggestProviderConfig::Weather {
                score: 0.24,
                min_keyword_length: 3,
            })
        );
        Ok(())
    }

    #[test]
    fn weather_keywords_prefixes_allowed() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "weather",
            "weather-1",
            json!({
                // min_keyword_length > 0 means prefixes are allowed.
                "min_keyword_length": 5,
                "keywords": ["ab", "xyz", "cdefg", "weather"],
                "max_keyword_length": "weather".len(),
                "max_keyword_word_count": 1,
                "score": 0.24
            }),
        ));

        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        let no_matches = [
            // doesn't match any keyword
            "ab123",
            "123ab",
            "xyz12",
            "12xyz",
            "xcdefg",
            "cdefgx",
            "x cdefg",
            "cdefg x",
            "weatherx",
            "xweather",
            "xweat",
            "weatx",
            "x   weather",
            "   weather x",
            "weather foo",
            "foo weather",
            // too short
            "ab",
            "xyz",
            "cdef",
            "we",
            "wea",
            "weat",
        ];
        for q in no_matches {
            assert_eq!(store.fetch_suggestions(SuggestionQuery::weather(q)), vec![]);
        }

        let matches = [
            "cdefg",
            "weath",
            "weathe",
            "weather",
            "WeAtHeR",
            "  weather  ",
        ];
        for q in matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::weather(q)),
                vec![Suggestion::Weather {
                    score: 0.24,
                    city: None,
                    region: None,
                    country: None,
                    latitude: None,
                    longitude: None,
                },]
            );
        }

        Ok(())
    }

    #[test]
    fn weather_keywords_prefixes_not_allowed() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "weather",
            "weather-1",
            json!({
                // min_keyword_length == 0 means prefixes are not allowed.
                "min_keyword_length": 0,
                "keywords": ["weather"],
                "max_keyword_length": "weather".len(),
                "max_keyword_word_count": 1,
                "score": 0.24
            }),
        ));

        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        let no_matches = ["wea", "weat", "weath", "weathe"];
        for q in no_matches {
            assert_eq!(store.fetch_suggestions(SuggestionQuery::weather(q)), vec![]);
        }

        let matches = ["weather", "WeAtHeR", "  weather  "];
        for q in matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::weather(q)),
                vec![Suggestion::Weather {
                    score: 0.24,
                    city: None,
                    region: None,
                    country: None,
                    latitude: None,
                    longitude: None,
                },]
            );
        }

        Ok(())
    }

    #[test]
    fn cities_and_regions() -> anyhow::Result<()> {
        before_each();

        let mut store = geoname::tests::new_test_store();
        store.client_mut().add_record(
            "weather",
            "weather-1",
            json!({
                // Include a keyword that's a prefix of another keyword --
                // "weather" and "weather near me" -- so that when a test
                // matches both we can verify only one suggestion is returned,
                // not two.
                "keywords": ["ab", "xyz", "weather", "weather near me"],
                "min_keyword_length": 5,
                "max_keyword_length": "weather".len(),
                "max_keyword_word_count": 1,
                "score": 0.24
            }),
        );

        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        let tests: &[(&str, Vec<Suggestion>)] = &[
            (
                "act",
                vec![],
            ),
            (
                "act w",
                vec![],
            ),
            (
                "act we",
                vec![],
            ),
            (
                "act wea",
                vec![],
            ),
            (
                "act weat",
                vec![],
            ),
            (
                // `min_keyword_length` = 5, so there should be a match.
                "act weath",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act weathe",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act weather",
                vec![geoname::tests::waco().into()],
            ),
            (
                "weather a",
                // The made-up long-name city starts with A.
                vec![geoname::tests::long_name_city().into()],
            ),
            (
                "weather ac",
                vec![],
            ),
            (
                "weather act",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act t",
                vec![],
            ),
            (
                "act tx",
                vec![],
            ),
            (
                "act tx w",
                vec![],
            ),
            (
                "act tx we",
                vec![],
            ),
            (
                "act tx wea",
                vec![],
            ),
            (
                "act tx weat",
                vec![],
            ),
            (
                // `min_keyword_length` = 5, so there should be a match.
                "act tx weath",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act tx weathe",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act tx weather",
                vec![geoname::tests::waco().into()],
            ),
            (
                "tx a",
                vec![],
            ),
            (
                "tx ac",
                vec![],
            ),
            (
                "tx act",
                vec![],
            ),
            (
                "tx act w",
                vec![],
            ),
            (
                "tx act we",
                vec![],
            ),
            (
                "tx act wea",
                vec![],
            ),
            (
                "tx act weat",
                vec![],
            ),
            (
                // `min_keyword_length` = 5, so there should be a match.
                "tx act weath",
                vec![geoname::tests::waco().into()],
            ),
            (
                "tx act weathe",
                vec![geoname::tests::waco().into()],
            ),
            (
                "tx act weather",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act te",
                vec![],
            ),
            (
                "act tex",
                vec![],
            ),
            (
                "act texa",
                vec![],
            ),
            (
                "act texas",
                vec![],
            ),
            (
                "act texas w",
                vec![],
            ),
            (
                "act texas we",
                vec![],
            ),
            (
                "act texas wea",
                vec![],
            ),
            (
                "act texas weat",
                vec![],
            ),
            (
                // `min_keyword_length` = 5, so there should be a match.
                "act texas weath",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act texas weathe",
                vec![geoname::tests::waco().into()],
            ),
            (
                "act texas weather",
                vec![geoname::tests::waco().into()],
            ),
            (
                "texas a",
                vec![],
            ),
            (
                "texas ac",
                vec![],
            ),
            (
                "texas act",
                vec![],
            ),
            (
                "texas act w",
                vec![],
            ),
            (
                "texas act we",
                vec![],
            ),
            (
                "texas act wea",
                vec![],
            ),
            (
                "texas act weat",
                vec![],
            ),
            (
                // `min_keyword_length` = 5, so there should be a match.
                "texas act weath",
                vec![geoname::tests::waco().into()],
            ),
            (
                "texas act weathe",
                vec![geoname::tests::waco().into()],
            ),
            (
                "texas act weather",
                vec![geoname::tests::waco().into()],
            ),
            (
                "ia w",
                vec![],
            ),
            (
                "ia wa",
                vec![],
            ),
            (
                "ia wat",
                vec![],
            ),
            (
                "ia wate",
                vec![],
            ),
            (
                "ia water",
                vec![],
            ),
            (
                "ia waterl",
                vec![],
            ),
            (
                "ia waterlo",
                vec![],
            ),
            (
                "waterloo",
                vec![
                    // Waterloo, IA should be first since its population is
                    // larger than Waterloo, AL.
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                ],
            ),
            (
                "waterloo i",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo ia",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo io",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo iow",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo iowa",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "ia waterloo",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo al",
                vec![geoname::tests::waterloo_al().into()],
            ),
            (
                "al waterloo",
                vec![geoname::tests::waterloo_al().into()],
            ),
            ("waterloo ia al", vec![]),
            ("waterloo ny", vec![]),
            (
                "ia",
                vec![],
            ),
            (
                "iowa",
                vec![],
            ),
            (
                "al",
                vec![],
            ),
            (
                "alabama",
                vec![],
            ),
            (
                "new york",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york new york",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny ny",
                vec![geoname::tests::nyc().into()],
            ),
            ("ny ny ny", vec![]),
            (
                "ny n",
                vec![],
            ),
            (
                "ny ne",
                vec![],
            ),
            (
                "ny new",
                vec![],
            ),
            (
                "ny new ",
                vec![],
            ),
            (
                "ny new y",
                vec![],
            ),
            (
                "ny new yo",
                vec![],
            ),
            (
                "ny new yor",
                vec![],
            ),
            (
                "ny new york",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny w",
                vec![],
            ),
            (
                "ny we",
                vec![],
            ),
            (
                "ny wea",
                vec![],
            ),
            (
                "ny weat",
                vec![],
            ),
            (
                // `min_keyword_length` = 5, so there should be a match.
                "ny weath",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny weathe",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather ny ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny weather ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny ny weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "rochester ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "ny rochester",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester weather ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester ny weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather ny rochester",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "ny weather rochester",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "ny rochester weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather new york",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new weather york",
                vec![],
            ),
            (
                "new york weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather new york new york",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york weather new york",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york new york weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather water",
                vec![
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                ],
            ),
            (
                "waterloo w",
                vec![
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                ],
            ),
            ("weather w w", vec![]),
            ("weather w water", vec![]),
            ("weather w waterloo", vec![]),
            ("weather water w", vec![]),
            ("weather waterloo water", vec![]),
            ("weather water water", vec![]),
            ("weather water waterloo", vec![]),
            ("waterloo foo", vec![]),
            ("waterloo weather foo", vec![]),
            ("foo waterloo", vec![]),
            ("foo waterloo weather", vec![]),
            (
                "ny",
                vec![],
            ),
            (
                "nyc",
                vec![],
            ),
            (
                "roc",
                vec![],
            ),
            (
                "nyc ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "ny nyc",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "roc ny",
                vec![],
            ),
            (
                "ny roc",
                vec![],
            ),
            (
                "nyc weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather nyc",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "roc weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather roc",
                vec![geoname::tests::rochester().into()],
            ),
            (
                geoname::tests::LONG_NAME,
                vec![geoname::tests::long_name_city().into()],
            ),
            (
                "   WaTeRlOo   ",
                vec![
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                ],
            ),
            (
                "     waterloo ia",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo     ia",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "waterloo ia     ",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "  waterloo   ia    ",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "     new york weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new     york weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york     weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york weather     ",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "rochester,",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester ,",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester , ",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester,ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester, ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester ,ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester , ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester,",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester, ",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester , ",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester,ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester, ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester ,ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "weather rochester , ny",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester,weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester, weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester ,weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester , weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester,ny weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester, ny weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester ,ny weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "rochester , ny weather",
                vec![geoname::tests::rochester().into()],
            ),
            (
                "new york,",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york ,",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york , ",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york,ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york, ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york ,ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york , ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather new york,ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather new york, ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather new york ,ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "weather new york , ny",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york,weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york, weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york ,weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york , weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york,ny weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york, ny weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york ,ny weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                "new york , ny weather",
                vec![geoname::tests::nyc().into()],
            ),
            (
                &format!("{} weather", geoname::tests::LONG_NAME),
                vec![geoname::tests::long_name_city().into()],
            ),
            (
                &format!("weather {}", geoname::tests::LONG_NAME),
                vec![geoname::tests::long_name_city().into()],
            ),
            (
                &format!("{} and some other words that don't match anything but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("and some other words that don't match anything {} but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("and some other words that don't match anything but that is neither here nor there {}", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("weather {} and some other words that don't match anything but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("{} weather and some other words that don't match anything but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("{} and some other words that don't match anything weather but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("{} and some other words that don't match anything but that is neither here nor there weather", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("weather and some other words that don't match anything {} but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("weather and some other words that don't match anything but that is neither here nor there {}", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("and some other words that don't match anything weather {} but that is neither here nor there", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("and some other words that don't match anything but that is neither here nor there weather {}", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("{} weather and then this also doesn't match anything down here", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("{} and then this also doesn't match anything down here weather", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("and then this also doesn't match anything down here {} weather", geoname::tests::LONG_NAME),
                vec![]
            ),
            (
                &format!("and then this also doesn't match anything down here weather {}", geoname::tests::LONG_NAME),
                vec![]
            ),
        ];

        for (query, expected_suggestions) in tests {
            assert_eq!(
                &store.fetch_suggestions(SuggestionQuery::weather(query)),
                expected_suggestions,
                "Query: {:?}",
                query
            );
        }

        Ok(())
    }

    #[test]
    fn keywords_metrics() -> anyhow::Result<()> {
        before_each();

        // Add a couple of records with different metrics. We're just testing
        // metrics so the other values don't matter.
        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "weather",
                    "weather-0",
                    json!({
                        "max_keyword_length": 10,
                        "max_keyword_word_count": 5,
                        "min_keyword_length": 3,
                        "score": 0.24,
                        "keywords": []
                    }),
                )
                .with_record(
                    "weather",
                    "weather-1",
                    json!({
                        "max_keyword_length": 20,
                        "max_keyword_word_count": 2,
                        "min_keyword_length": 3,
                        "score": 0.24,
                        "keywords": []
                    }),
                ),
        );

        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        store.read(|dao| {
            let cache = dao.weather_cache();
            assert_eq!(cache.max_keyword_length, 20);
            assert_eq!(cache.max_keyword_word_count, 5);
            Ok(())
        })?;

        // Delete the first record. The metrics should change.
        store
            .client_mut()
            .delete_record("quicksuggest", "weather-0");
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        store.read(|dao| {
            let cache = dao.weather_cache();
            assert_eq!(cache.max_keyword_length, 20);
            assert_eq!(cache.max_keyword_word_count, 2);
            Ok(())
        })?;

        // Add a new record. The metrics should change again.
        store.client_mut().add_record(
            "weather",
            "weather-3",
            json!({
                "max_keyword_length": 15,
                "max_keyword_word_count": 3,
                "min_keyword_length": 3,
                "score": 0.24,
                "keywords": []
            }),
        );
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        store.read(|dao| {
            let cache = dao.weather_cache();
            assert_eq!(cache.max_keyword_length, 20);
            assert_eq!(cache.max_keyword_word_count, 3);
            Ok(())
        })?;

        Ok(())
    }
}
