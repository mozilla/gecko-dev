/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::types::ToSqlOutput;
use rusqlite::{named_params, Result as RusqliteResult, ToSql};
use sql_support::ConnExt;
use url::form_urlencoded;

use crate::{
    db::SuggestDao,
    provider::SuggestionProvider,
    rs::{DownloadedYelpSuggestion, SuggestRecordId},
    suggestion::Suggestion,
    Result, SuggestionQuery,
};

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
#[repr(u8)]
enum Modifier {
    Pre = 0,
    Post = 1,
    Yelp = 2,
    LocationSign = 3,
}

impl ToSql for Modifier {
    fn to_sql(&self) -> RusqliteResult<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(*self as u8))
    }
}

#[derive(Eq, PartialEq)]
enum FindFrom {
    First,
    Last,
}

/// This module assumes like following query.
/// "Yelp-modifier? Pre-modifier? Subject Post-modifier? (Location-modifier | Location-sign Location?)? Yelp-modifier?"
/// For example, the query below is valid.
/// "Yelp (Yelp-modifier) Best(Pre-modifier) Ramen(Subject) Delivery(Post-modifier) In(Location-sign) Tokyo(Location)"
/// Also, as everything except Subject is optional, "Ramen" will be also valid query.
/// However, "Best Best Ramen" and "Ramen Best" is out of the above appearance order rule,
/// parsing will be failed. Also, every words except Location needs to be registered in DB.
/// Please refer to the query test in store.rs for all of combination.
/// Currently, the maximum query length is determined while referring to having word lengths in DB
/// and location names.
/// max subject: 50 + pre-modifier: 10 + post-modifier: 10 + location-sign: 7 + location: 50 = 127 = 150.
const MAX_QUERY_LENGTH: usize = 150;

/// The max number of words consisting the modifier. To improve the SQL performance by matching with
/// "keyword=:modifier" (please see is_modifier()), define this how many words we should check.
const MAX_MODIFIER_WORDS_NUMBER: usize = 2;

/// At least this many characters must be typed for a subject to be matched.
const SUBJECT_PREFIX_MATCH_THRESHOLD: usize = 2;

impl SuggestDao<'_> {
    /// Inserts the suggestions for Yelp attachment into the database.
    pub(crate) fn insert_yelp_suggestions(
        &mut self,
        record_id: &SuggestRecordId,
        suggestion: &DownloadedYelpSuggestion,
    ) -> Result<()> {
        for keyword in &suggestion.subjects {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_subjects(record_id, keyword) VALUES(:record_id, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.pre_modifiers {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::Pre,
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.post_modifiers {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::Post,
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.yelp_modifiers {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::Yelp,
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.location_signs {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::LocationSign,
                    ":keyword": keyword,
                },
            )?;
        }

        self.scope.err_if_interrupted()?;
        self.conn.execute_cached(
            "INSERT INTO yelp_custom_details(record_id, icon_id, score) VALUES(:record_id, :icon_id, :score)",
            named_params! {
                ":record_id": record_id.as_str(),
                ":icon_id": suggestion.icon_id,
                ":score": suggestion.score,
            },
        )?;

        Ok(())
    }

    /// Fetch Yelp suggestion from given user's query.
    pub(crate) fn fetch_yelp_suggestions(
        &self,
        query: &SuggestionQuery,
    ) -> Result<Vec<Suggestion>> {
        if !query.providers.contains(&SuggestionProvider::Yelp) {
            return Ok(vec![]);
        }

        if query.keyword.len() > MAX_QUERY_LENGTH {
            return Ok(vec![]);
        }

        let query_vec: Vec<_> = query.keyword.split_whitespace().collect();
        let mut query_words: &[&str] = &query_vec;

        let pre_yelp_modifier_tuple =
            self.find_modifier(query_words, Modifier::Yelp, FindFrom::First)?;
        if let Some((_, n)) = pre_yelp_modifier_tuple {
            query_words = &query_words[n..];
        }

        let pre_modifier_tuple = self.find_modifier(query_words, Modifier::Pre, FindFrom::First)?;
        if let Some((_, n)) = pre_modifier_tuple {
            query_words = &query_words[n..];
        }

        let Some(subject_tuple) = self.find_subject(query_words)? else {
            return Ok(vec![]);
        };
        query_words = &query_words[subject_tuple.2..];

        let post_modifier_tuple =
            self.find_modifier(query_words, Modifier::Post, FindFrom::First)?;
        if let Some((_, n)) = post_modifier_tuple {
            query_words = &query_words[n..];
        }

        let location_sign_tuple =
            self.find_modifier(query_words, Modifier::LocationSign, FindFrom::First)?;
        if let Some((_, n)) = location_sign_tuple {
            query_words = &query_words[n..];
        }

        let post_yelp_modifier_tuple =
            self.find_modifier(query_words, Modifier::Yelp, FindFrom::Last)?;
        if let Some((_, n)) = post_yelp_modifier_tuple {
            query_words = &query_words[0..query_words.len() - n];
        }

        let location = if query_words.is_empty() {
            None
        } else {
            Some(query_words.join(" "))
        };

        let (icon, icon_mimetype, score) = self.fetch_custom_details()?;
        let builder = SuggestionBuilder {
            subject: &subject_tuple.0,
            subject_exact_match: subject_tuple.1,
            pre_modifier: pre_modifier_tuple.map(|(words, _)| words.to_string()),
            post_modifier: post_modifier_tuple.map(|(words, _)| words.to_string()),
            location_sign: location_sign_tuple.map(|(words, _)| words.to_string()),
            location,
            icon,
            icon_mimetype,
            score,
        };
        Ok(vec![builder.into()])
    }

    /// Find the modifier for given query and modifier type.
    /// Find from last word, if set FindFrom::Last to find_from.
    /// It returns Option<tuple> as follows:
    /// (
    ///   String: The keyword in DB (but the case is inherited by query).
    ///   usize: Number of words in query_words that match the keyword.
    ///          Maximum number is MAX_MODIFIER_WORDS_NUMBER.
    /// )
    fn find_modifier(
        &self,
        query_words: &[&str],
        modifier_type: Modifier,
        find_from: FindFrom,
    ) -> Result<Option<(String, usize)>> {
        if query_words.is_empty() {
            return Ok(None);
        }

        for n in (1..=std::cmp::min(MAX_MODIFIER_WORDS_NUMBER, query_words.len())).rev() {
            let candidate_chunk = match find_from {
                FindFrom::First => query_words.chunks(n).next(),
                FindFrom::Last => query_words.rchunks(n).next(),
            };
            let candidate = candidate_chunk
                .map(|chunk| chunk.join(" "))
                .unwrap_or_default();
            if let Some(keyword_lowercase) = self.conn.try_query_one::<String, _>(
                if n == query_words.len() {
                    "
                    SELECT keyword FROM yelp_modifiers
                    WHERE type = :type AND keyword BETWEEN :word AND :word || x'FFFF'
                    LIMIT 1
                    "
                } else {
                    "
                    SELECT keyword FROM yelp_modifiers
                    WHERE type = :type AND keyword = :word
                    LIMIT 1
                    "
                },
                named_params! {
                    ":type": modifier_type,
                    ":word": candidate.to_lowercase(),
                },
                true,
            )? {
                // Preserve the query as the user typed it including its case.
                let keyword = format!("{}{}", candidate, &keyword_lowercase[candidate.len()..]);
                return Ok(Some((keyword, n)));
            }
        }

        Ok(None)
    }

    /// Find the subject for given query.
    /// It returns Option<tuple> as follows:
    /// (
    ///   String: The keyword in DB (but the case is inherited by query).
    ///   bool: Whether or not the keyword is exact match.
    ///   usize: Number of words in query_words that match the keyword.
    /// )
    fn find_subject(&self, query_words: &[&str]) -> Result<Option<(String, bool, usize)>> {
        if query_words.is_empty() {
            return Ok(None);
        }

        let query_string = query_words.join(" ");

        // This checks if keyword is a substring of the query.
        if let Some(keyword_lowercase) = self.conn.try_query_one::<String, _>(
            "SELECT keyword
             FROM yelp_subjects
             WHERE :query BETWEEN keyword AND keyword || x'FFFF'
             ORDER BY LENGTH(keyword) ASC, keyword ASC
             LIMIT 1",
            named_params! {
                ":query": query_string.to_lowercase(),
            },
            true,
        )? {
            // Preserve the query as the user typed it including its case.
            let keyword = &query_string[0..keyword_lowercase.len()];
            let count = keyword.split_whitespace().count();
            return Ok(Some((keyword.to_string(), true, count)));
        };

        if query_string.len() < SUBJECT_PREFIX_MATCH_THRESHOLD {
            return Ok(None);
        }

        // Oppositely, this checks if the query is a substring of keyword.
        if let Some(keyword_lowercase) = self.conn.try_query_one::<String, _>(
            "SELECT keyword
             FROM yelp_subjects
             WHERE keyword BETWEEN :query AND :query || x'FFFF'
             ORDER BY LENGTH(keyword) ASC, keyword ASC
             LIMIT 1",
            named_params! {
                ":query": query_string.to_lowercase(),
            },
            true,
        )? {
            // Preserve the query as the user typed it including its case.
            let keyword = format!(
                "{}{}",
                query_string,
                &keyword_lowercase[query_string.len()..]
            );
            let count = keyword.split_whitespace().count();
            return Ok(Some((keyword, false, count)));
        };

        Ok(None)
    }

    /// Fetch the custom details for Yelp suggestions.
    /// It returns the location tuple as follows:
    /// (
    ///   Option<Vec<u8>>: Icon data. If not found, returns None.
    ///   Option<String>: Mimetype of the icon data. If not found, returns None.
    ///   f64: Reflects score field in the yelp_custom_details table.
    /// )
    ///
    /// Note that there should be only one record in `yelp_custom_details`
    /// as all the Yelp assets are stored in the attachment of a single record
    /// on Remote Settings. The following query will perform a table scan against
    /// `yelp_custom_details` followed by an index search against `icons`,
    /// which should be fine since there is only one record in the first table.
    fn fetch_custom_details(&self) -> Result<(Option<Vec<u8>>, Option<String>, f64)> {
        let result = self.conn.query_row_and_then_cachable(
            r#"
            SELECT
              i.data, i.mimetype, y.score
            FROM
              yelp_custom_details y
            LEFT JOIN
              icons i
              ON y.icon_id = i.id
            LIMIT
              1
            "#,
            (),
            |row| -> Result<_> {
                Ok((
                    row.get::<_, Option<Vec<u8>>>(0)?,
                    row.get::<_, Option<String>>(1)?,
                    row.get::<_, f64>(2)?,
                ))
            },
            true,
        )?;

        Ok(result)
    }
}

struct SuggestionBuilder<'a> {
    subject: &'a str,
    subject_exact_match: bool,
    pre_modifier: Option<String>,
    post_modifier: Option<String>,
    location_sign: Option<String>,
    location: Option<String>,
    icon: Option<Vec<u8>>,
    icon_mimetype: Option<String>,
    score: f64,
}

impl<'a> From<SuggestionBuilder<'a>> for Suggestion {
    fn from(builder: SuggestionBuilder<'a>) -> Suggestion {
        let description = [
            builder.pre_modifier.as_deref(),
            Some(builder.subject),
            builder.post_modifier.as_deref(),
        ]
        .iter()
        .flatten()
        .copied()
        .collect::<Vec<_>>()
        .join(" ");

        // https://www.yelp.com/search?find_desc={description}&find_loc={location}
        let mut url = String::from("https://www.yelp.com/search?");
        let mut parameters = form_urlencoded::Serializer::new(String::new());
        parameters.append_pair("find_desc", &description);
        if let Some(location) = &builder.location {
            parameters.append_pair("find_loc", location);
        }
        url.push_str(&parameters.finish());

        let title = [
            builder.pre_modifier.as_deref(),
            Some(builder.subject),
            builder.post_modifier.as_deref(),
            builder.location_sign.as_deref(),
            builder.location.as_deref(),
        ]
        .iter()
        .flatten()
        .copied()
        .collect::<Vec<_>>()
        .join(" ");

        Suggestion::Yelp {
            url,
            title,
            icon: builder.icon,
            icon_mimetype: builder.icon_mimetype,
            score: builder.score,
            has_location_sign: builder.location_sign.is_some(),
            subject_exact_match: builder.subject_exact_match,
            location_param: "find_loc".to_string(),
        }
    }
}
