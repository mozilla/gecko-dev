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
        KeywordInsertStatement, KeywordsMetrics, KeywordsMetricsUpdater, SuggestDao,
        SuggestionInsertStatement, DEFAULT_SUGGESTION_SCORE,
    },
    geoname::GeonameMatch,
    metrics::MetricsContext,
    provider::SuggestionProvider,
    rs::{Client, Record, SuggestRecordId, SuggestRecordType},
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
}

/// This data is used to service every query handled by the weather provider, so
/// we cache it from the DB.
#[derive(Debug, Default)]
pub struct WeatherCache {
    /// Cached value of the same name from `SuggestProviderConfig::Weather`.
    min_keyword_length: i32,
    /// Cached value of the same name from `SuggestProviderConfig::Weather`.
    score: f64,
    /// Cached weather keywords metrics.
    keywords_metrics: KeywordsMetrics,
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
        // they can't possibly match. We'll stipulate that weather queries will
        // include the following parts at most:
        //
        // * 3 geonames max: city + one admin division like a state + country
        // * 1 weather keyword
        // * 3 spaces between the previous geonames and keyword
        // * 10 extra chars to allow for extra spaces and punctuation
        //
        // This will exclude some valid queries because the logic below allows
        // for multiple weather keywords, and a city may have more than one
        // admin division, but we don't expect many users to type such long
        // queries.
        //
        // There's no point in an analogous min length check since weather
        // suggestions can be matched on city alone and many city names are only
        // a few characters long ("nyc").

        let g_cache = self.geoname_cache();
        let w_cache = self.weather_cache();
        let max_query_len =
            3 * g_cache.keywords_metrics.max_len + w_cache.keywords_metrics.max_len + 10;
        if max_query_len < query.keyword.len() {
            return Ok(vec![]);
        }

        let max_chunk_size = std::cmp::max(
            g_cache.keywords_metrics.max_word_count,
            w_cache.keywords_metrics.max_word_count,
        );

        // Lowercase, strip punctuation, and split the query into words.
        let kw_lower = query.keyword.to_lowercase();
        let words: Vec<_> = kw_lower
            .split_whitespace()
            .flat_map(|w| {
                w.split(|c| !char::is_alphanumeric(c))
                    .filter(|s| !s.is_empty())
            })
            .collect();

        let mut matches =
            // Step 2: Parse the query words into a list of token paths.
            filter_map_chunks::<Token>(&words, max_chunk_size, |chunk, chunk_i, path| {
                // Find all token types that match the chunk.
                let mut all_tokens: Option<Vec<Token>> = None;
                for tt in [
                    TokenType::Geoname,
                    TokenType::WeatherKeyword,
                ] {
                    let mut tokens = self.match_weather_tokens(tt, path, chunk, chunk_i == 0)?;
                    if !tokens.is_empty() {
                        let mut ts = all_tokens.take().unwrap_or_default();
                        ts.append(&mut tokens);
                        all_tokens.replace(ts);
                    }
                }
                // If no tokens were matched, `all_tokens` will be `None`.
                Ok(all_tokens)
            })?
            .into_iter()
            // Step 3: Map each token path to a `TokenPath`, which is just a
            // convenient representation of the path.
            .map(TokenPath::from)
            // Step 4: Filter in paths with the right combination of tokens.
            // Along with step 2, this is the core of the matching logic.
            .filter(|tp| {
                if let Some(cm) = &tp.city_match {
                    // city name typed in full ("new york")
                    (cm.match_type.is_name() && !cm.prefix)
                        // city abbreviation typed in full + another related
                        // geoname typed in full ("ny new york")
                        || (cm.match_type.is_abbreviation()
                            && !cm.prefix
                            && tp.any_other_geoname_typed_in_full)
                        // any kind of city + weather keyword ("ny weather",
                        // "weather new y")
                        || tp.keyword_match
                            .as_ref()
                            .map(|kwm| kwm.is_min_keyword_length).unwrap_or(false)
                } else {
                    // weather keyword by itself ("weather")
                    tp.keyword_match.is_some() && !tp.any_other_geoname_matched
                }
            })
            // Step 5: Map each path to its city, an `Option<Geoname>`. Paths
            // without cities will end up as `None` values.
            .map(|tp| tp.city_match.map(|cm| cm.geoname))
            // Step 6: Dedupe. We'll end up with an `Option<Geoname>` for each
            // unique matching city + one `None` value if any keywords by
            // themselves were matched.
            .collect::<HashSet<_>>()
            .into_iter()
            .collect::<Vec<_>>();

        // Sort the matches so cities with larger populations are first.
        matches.sort_by(|city1, city2| match (&city1, &city2) {
            (Some(_), None) => Ordering::Less,
            (None, Some(_)) => Ordering::Greater,
            (Some(c1), Some(c2)) => c2.population.cmp(&c1.population),
            (None, None) => Ordering::Equal,
        });

        // Finally, map matches to suggestions.
        Ok(matches
            .into_iter()
            .map(|city| Suggestion::Weather {
                city,
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
            TokenType::Geoname => {
                // Fetch matching geonames, and filter them to geonames we've
                // already matched in this path.
                let geonames_in_path: Vec<_> = path
                    .iter()
                    .filter_map(|t| t.geoname_match().map(|gm| &gm.geoname))
                    .collect();
                Ok(self
                    .fetch_geonames(
                        candidate,
                        !is_first_chunk,
                        if geonames_in_path.is_empty() {
                            None
                        } else {
                            Some(geonames_in_path)
                        },
                    )?
                    .into_iter()
                    .map(Token::Geoname)
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
        let mut metrics_updater = KeywordsMetricsUpdater::new();

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
                metrics_updater.update(keyword);
            }
            self.put_provider_config(SuggestionProvider::Weather, &attach.into())?;
        }

        metrics_updater.finish(
            self.conn,
            record_id,
            SuggestRecordType::Weather,
            &mut self.weather_cache,
        )?;

        Ok(())
    }

    fn weather_cache(&self) -> &WeatherCache {
        self.weather_cache.get_or_init(|| {
            let mut cache = WeatherCache {
                keywords_metrics: self
                    .get_keywords_metrics(SuggestRecordType::Weather)
                    .unwrap_or_default(),
                ..WeatherCache::default()
            };

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
    Geoname,
    WeatherKeyword,
}

#[derive(Clone, Debug)]
#[allow(clippy::large_enum_variant)]
enum Token {
    Geoname(GeonameMatch),
    WeatherKeyword(WeatherKeywordMatch),
}

impl Token {
    fn geoname_match(&self) -> Option<&GeonameMatch> {
        match self {
            Self::Geoname(gm) => Some(gm),
            _ => None,
        }
    }
}

#[derive(Clone, Debug, Default, Eq, Hash, PartialEq)]
struct WeatherKeywordMatch {
    keyword: String,
    is_min_keyword_length: bool,
}

#[derive(Default)]
struct TokenPath {
    keyword_match: Option<WeatherKeywordMatch>,
    city_match: Option<GeonameMatch>,
    any_other_geoname_matched: bool,
    any_other_geoname_typed_in_full: bool,
}

impl From<Vec<Token>> for TokenPath {
    fn from(tokens: Vec<Token>) -> Self {
        let mut tp = Self::default();
        for t in tokens {
            match t {
                Token::WeatherKeyword(kwm) => {
                    tp.keyword_match = Some(kwm);
                }
                Token::Geoname(gm) => {
                    if gm.geoname.feature_class == "P" {
                        tp.city_match = Some(gm);
                    } else {
                        tp.any_other_geoname_matched = true;
                        if !gm.prefix {
                            tp.any_other_geoname_typed_in_full = true;
                        }
                    }
                }
            }
        }
        tp
    }
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
                city: Some(g),
                score: 0.24,
            }
        }
    }

    #[test]
    fn weather_provider_config() -> anyhow::Result<()> {
        before_each();
        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            SuggestionProvider::Weather.record(
                "weather-1",
                json!({
                    "min_keyword_length": 3,
                    "keywords": ["ab", "xyz", "weather"],
                    "score": 0.24
                }),
            ),
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
            SuggestionProvider::Weather.record(
                "weather-1",
                json!({
                    // min_keyword_length > 0 means prefixes are allowed.
                    "min_keyword_length": 5,
                    "keywords": ["ab", "xyz", "cdefg", "weather"],
                    "score": 0.24
                }),
            ),
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
            "weatherx",
            "xweather",
            "xweat",
            "weatx",
            "x   weather",
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
            // full keyword ("cdefg") + prefix of another keyword ("xyz")
            "cdefg x",
            "weath",
            "weathe",
            "weather",
            "WeAtHeR",
            "  weather  ",
            // full keyword ("weather") + prefix of another keyword ("xyz")
            "   weather x",
        ];
        for q in matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::weather(q)),
                vec![Suggestion::Weather {
                    score: 0.24,
                    city: None,
                }]
            );
        }

        Ok(())
    }

    #[test]
    fn weather_keywords_prefixes_not_allowed() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            SuggestionProvider::Weather.record(
                "weather-1",
                json!({
                    // min_keyword_length == 0 means prefixes are not allowed.
                    "min_keyword_length": 0,
                    "keywords": ["weather"],
                    "score": 0.24
                }),
            ),
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
                }]
            );
        }

        Ok(())
    }

    #[test]
    fn cities_and_regions() -> anyhow::Result<()> {
        before_each();

        let mut store = geoname::tests::new_test_store();
        store
            .client_mut()
            .add_record(SuggestionProvider::Weather.record(
                "weather-1",
                json!({
                    // Include a keyword that's a prefix of another keyword --
                    // "weather" and "weather near me" -- so that when a test
                    // matches both we can verify only one suggestion is returned,
                    // not two.
                    "keywords": ["ab", "xyz", "weather", "weather near me"],
                    "min_keyword_length": 5,
                    "score": 0.24
                }),
            ));

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
                vec![
                    // A suggestion without a city is returned because the query
                    // also matches a keyword ("weather") + a prefix of another
                    // keyword ("ab").
                    Suggestion::Weather {
                        score: 0.24,
                        city: None,
                    },
                ],
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
                    // Matches should be returned by population descending.
                    geoname::tests::waterloo_on().into(),
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
            (
                "ny ny ny",
                vec![geoname::tests::nyc().into()],
            ),
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
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                ],
            ),
            (
                "waterloo w",
                vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                ],
            ),
            (
                // "w" matches "waco", "waterloo", and "weather"
                "weather w w",
                vec![
                    geoname::tests::waco().into(),
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
                    Suggestion::Weather {
                        score: 0.24,
                        city: None,
                    },
                ],
            ),
            ("weather w water", vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
            ]),
            ("weather w waterloo", vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
            ]),
            ("weather water w", vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
            ]),
            ("weather waterloo water", vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
            ]),
            ("weather water water", vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
            ]),
            ("weather water waterloo", vec![
                    geoname::tests::waterloo_on().into(),
                    geoname::tests::waterloo_ia().into(),
                    geoname::tests::waterloo_al().into(),
            ]),
            ("waterloo foo", vec![]),
            ("waterloo weather foo", vec![]),
            ("foo waterloo", vec![]),
            ("foo waterloo weather", vec![]),
            ("weather waterloo foo", vec![]),
            ("weather foo waterloo", vec![]),
            ("weather water foo", vec![]),
            ("weather foo water", vec![]),
            (
                "waterloo on",
                vec![geoname::tests::waterloo_on().into()],
            ),
            (
                "waterloo ont",
                vec![geoname::tests::waterloo_on().into()],
            ),
            (
                "waterloo ont.",
                vec![geoname::tests::waterloo_on().into()],
            ),
            (
                "waterloo ontario",
                vec![geoname::tests::waterloo_on().into()],
            ),
            (
                "waterloo canada",
                vec![geoname::tests::waterloo_on().into()],
            ),
            (
                "waterloo on canada",
                vec![geoname::tests::waterloo_on().into()],
            ),
            (
                "waterloo on us",
                vec![],
            ),
            (
                "waterloo al canada",
                vec![],
            ),
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
                "liverpool",
                vec![geoname::tests::liverpool_city().into()],
            ),
            (
                "liverpool eng",
                vec![geoname::tests::liverpool_city().into()],
            ),
            (
                "liverpool england",
                vec![geoname::tests::liverpool_city().into()],
            ),
            (
                "liverpool uk",
                vec![geoname::tests::liverpool_city().into()],
            ),
            (
                "liverpool england uk",
                vec![geoname::tests::liverpool_city().into()],
            ),
            (
                geoname::tests::LONG_NAME,
                vec![geoname::tests::long_name_city().into()],
            ),
            (
                "     waterloo iowa",
                vec![geoname::tests::waterloo_ia().into()],
            ),
            (
                "   WaTeRlOo   ",
                vec![
                    geoname::tests::waterloo_on().into(),
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
                .with_record(SuggestionProvider::Weather.record(
                    "weather-0",
                    json!({
                        "min_keyword_length": 3,
                        "score": 0.24,
                        "keywords": [
                            "a b c d ef"
                        ],
                    }),
                ))
                .with_record(SuggestionProvider::Weather.record(
                    "weather-1",
                    json!({
                        "min_keyword_length": 3,
                        "score": 0.24,
                        "keywords": [
                            "abcdefghik lmnopqrst"
                        ],
                    }),
                )),
        );

        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        store.read(|dao| {
            let cache = dao.weather_cache();
            assert_eq!(cache.keywords_metrics.max_len, 20);
            assert_eq!(cache.keywords_metrics.max_word_count, 5);
            Ok(())
        })?;

        // Delete the first record. The metrics should change.
        store
            .client_mut()
            .delete_record(SuggestionProvider::Weather.empty_record("weather-0"));
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        store.read(|dao| {
            let cache = dao.weather_cache();
            assert_eq!(cache.keywords_metrics.max_len, 20);
            assert_eq!(cache.keywords_metrics.max_word_count, 2);
            Ok(())
        })?;

        // Add a new record. The metrics should change again.
        store
            .client_mut()
            .add_record(SuggestionProvider::Weather.record(
                "weather-3",
                json!({
                    "min_keyword_length": 3,
                    "score": 0.24,
                    "keywords": [
                        "abcde fghij klmno"
                    ]
                }),
            ));
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        store.read(|dao| {
            let cache = dao.weather_cache();
            assert_eq!(cache.keywords_metrics.max_len, 20);
            assert_eq!(cache.keywords_metrics.max_word_count, 3);
            Ok(())
        })?;

        Ok(())
    }
}
