/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/// GeoNames support. GeoNames is an open-source geographical database of place
/// names worldwide, including cities, regions, and countries [1]. Notably it's
/// used by MaxMind's databases [2]. We use GeoNames to detect city and region
/// names and to map cities to regions.
///
/// [1]: https://www.geonames.org/
/// [2]: https://www.maxmind.com/en/geoip-databases
use rusqlite::{named_params, Connection};
use serde::Deserialize;
use sql_support::ConnExt;
use std::hash::{Hash, Hasher};

use crate::{
    db::SuggestDao,
    error::RusqliteResultExt,
    metrics::MetricsContext,
    rs::{deserialize_f64_or_default, Client, Record, SuggestRecordId},
    store::SuggestStoreInner,
    Result,
};

/// The type of a geoname.
#[derive(Clone, Debug, Eq, Hash, PartialEq, uniffi::Enum)]
pub enum GeonameType {
    City,
    Region,
}

/// A single geographic place.
///
/// This corresponds to a single row in the main "geoname" table described in
/// the GeoNames documentation [1]. We exclude fields we don't need.
///
/// [1]: https://download.geonames.org/export/dump/readme.txt
#[derive(Clone, Debug, uniffi::Record)]
pub struct Geoname {
    /// The `geonameid` straight from the geoname table.
    pub geoname_id: i64,
    /// This is pretty much the place's canonical name. Usually there will be a
    /// row in the alternates table with the same name, but not always. When
    /// there is such a row, it doesn't always have `is_preferred_name` set, and
    /// in fact fact there may be another row with a different name with
    /// `is_preferred_name` set.
    pub name: String,
    /// Latitude in decimal degrees.
    pub latitude: f64,
    /// Longitude in decimal degrees.
    pub longitude: f64,
    /// ISO-3166 two-letter uppercase country code, e.g., "US".
    pub country_code: String,
    /// The top-level administrative region for the place within its country,
    /// like a state or province. For the U.S., the two-letter uppercase state
    /// abbreviation.
    pub admin1_code: String,
    /// Population size.
    pub population: u64,
}

impl Geoname {
    /// Whether `self` and `other` have the same region and country. If one is a
    /// city and the other is a region, this will return `true` if the city is
    /// located in the region.
    pub fn has_same_region(&self, other: &Self) -> bool {
        self.admin1_code == other.admin1_code && self.country_code == other.country_code
    }
}

impl PartialEq for Geoname {
    fn eq(&self, other: &Geoname) -> bool {
        self.geoname_id == other.geoname_id
    }
}

impl Eq for Geoname {}

impl Hash for Geoname {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.geoname_id.hash(state)
    }
}

/// A fetched geoname with info on how it was matched.
#[derive(Clone, Debug, Eq, PartialEq, uniffi::Record)]
pub struct GeonameMatch {
    /// The geoname that was matched.
    pub geoname: Geoname,
    /// The type of name that was matched.
    pub match_type: GeonameMatchType,
    /// Whether the name was matched by prefix.
    pub prefix: bool,
}

#[derive(Clone, Debug, Eq, PartialEq, uniffi::Enum)]
pub enum GeonameMatchType {
    /// For U.S. states, abbreviations are the usual two-letter codes ("CA").
    Abbreviation,
    AirportCode,
    /// This includes any names that aren't abbreviations or airport codes.
    Name,
}

impl GeonameMatchType {
    pub fn is_abbreviation(&self) -> bool {
        matches!(self, GeonameMatchType::Abbreviation)
    }

    pub fn is_name(&self) -> bool {
        matches!(self, GeonameMatchType::Name)
    }
}

/// This data is used to service every query handled by the weather provider and
/// potentially other providers, so we cache it from the DB.
#[derive(Debug, Default)]
pub struct GeonameCache {
    /// Max length of all geoname names.
    pub max_name_length: usize,
    /// Max word count across all geoname names.
    pub max_name_word_count: usize,
}

#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedGeonameAttachment {
    /// The max length of all names in the attachment. Used for name metrics. We
    /// pre-compute this to avoid doing duplicate work on all user's machines.
    pub max_alternate_name_length: u32,
    /// The max word count across all names in the attachment. Used for name
    /// metrics. We pre-compute this to avoid doing duplicate work on all user's
    /// machines.
    pub max_alternate_name_word_count: u32,
    pub geonames: Vec<DownloadedGeoname>,
}

/// This corresponds to a single row in the main "geoname" table described in
/// the GeoNames documentation [1] except where noted. It represents a single
/// place. We exclude fields we don't need.
///
/// [1] https://download.geonames.org/export/dump/readme.txt
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedGeoname {
    /// The `geonameid` straight from the geoname table.
    pub id: i64,
    /// NOTE: For ease of implementation, this name should always also be
    /// included as a lowercased alternate name even if the original GeoNames
    /// data doesn't include it as an alternate.
    pub name: String,
    /// "P" - Populated place like a city or village.
    /// "A" - Administrative division like a country, state, or region.
    pub feature_class: String,
    /// "ADM1" - Primary administrative division like a U.S. state.
    pub feature_code: String,
    /// ISO-3166 two-letter uppercase country code, e.g., "US".
    pub country_code: String,
    /// For the U.S., the two-letter uppercase state abbreviation.
    pub admin1_code: String,
    /// This can be helpful for resolving name conflicts. If two geonames have
    /// the same name, we might prefer the one with the larger population.
    pub population: u64,
    /// Latitude in decimal degrees. Expected to be a string in the RS data.
    #[serde(deserialize_with = "deserialize_f64_or_default")]
    pub latitude: f64,
    /// Longitude in decimal degrees. Expected to be a string in the RS data.
    #[serde(deserialize_with = "deserialize_f64_or_default")]
    pub longitude: f64,
    /// List of names that the place is known by. Despite the word "alternate",
    /// this often includes the place's proper name. This list is pulled from
    /// the "alternate names" table described in the GeoNames documentation and
    /// included here inline.
    ///
    /// NOTE: For ease of implementation, this list should always include a
    /// lowercase version of `name` even if the original GeoNames record doesn't
    /// include it as an alternate.
    ///
    /// Version 1 of this field was a `Vec<String>`.
    pub alternate_names_2: Vec<DownloadedGeonameAlternate>,
}

#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedGeonameAlternate {
    /// Lowercase alternate name.
    name: String,
    /// The value of the `iso_language` field for the alternate. This will be
    /// `None` for the alternate we artificially create for the `name` in the
    /// corresponding geoname record.
    iso_language: Option<String>,
}

impl SuggestDao<'_> {
    /// Fetches geonames that have at least one name matching the `query`
    /// string.
    ///
    /// `match_name_prefix` determines whether prefix matching is performed on
    /// names that aren't abbreviations and airport codes. When `true`, names
    /// that start with `query` will match. When false, names that equal `query`
    /// will match. Prefix matching is never performed on abbreviations and
    /// airport codes because we don't currently have a use case for that.
    ///
    /// `geoname_type` restricts returned geonames to the specified type. `None`
    /// restricts geonames to cities and regions. There's no way to return
    /// geonames of other types, but we shouldn't ingest other types to begin
    /// with.
    ///
    /// `filter` restricts returned geonames to certain cities or regions.
    /// Cities can be restricted to certain regions by including the regions in
    /// `filter`, and regions can be restricted to those containing certain
    /// cities by including the cities in `filter`. This is especially useful
    /// since city and region names are not unique. `filter` is disjunctive: If
    /// any item in `filter` matches a geoname, the geoname will be filtered in.
    /// If `filter` is empty, all geonames will be filtered out.
    ///
    /// The returned matches will include all matching types for a geoname, one
    /// match per type per geoname. For example, if the query matches both a
    /// geoname's name and abbreviation, two matches for that geoname will be
    /// returned: one with a `match_type` of `GeonameMatchType::Name` and one
    /// with a `match_type` of `GeonameMatchType::Abbreviation`. `prefix` is set
    /// according to whether the query matched a prefix of the given type.
    pub fn fetch_geonames(
        &self,
        query: &str,
        match_name_prefix: bool,
        geoname_type: Option<GeonameType>,
        filter: Option<Vec<&Geoname>>,
    ) -> Result<Vec<GeonameMatch>> {
        let city_pred = "(g.feature_class = 'P')";
        let region_pred = "(g.feature_class = 'A' AND g.feature_code = 'ADM1')";
        let type_pred = match geoname_type {
            None => format!("({} OR {})", city_pred, region_pred),
            Some(GeonameType::City) => city_pred.to_string(),
            Some(GeonameType::Region) => region_pred.to_string(),
        };
        Ok(self
            .conn
            .query_rows_and_then_cached(
                &format!(
                    r#"
                    SELECT
                        g.id,
                        g.name,
                        g.latitude,
                        g.longitude,
                        g.feature_class,
                        g.country_code,
                        g.admin1_code,
                        g.population,
                        a.name != :name AS prefix,
                        (SELECT CASE
                             -- abbreviation
                             WHEN a.iso_language = 'abbr' THEN 1
                             -- airport code
                             WHEN a.iso_language IN ('iata', 'icao', 'faac') THEN 2
                             -- name
                             ELSE 3
                             END
                        ) AS match_type
                    FROM
                        geonames g
                    JOIN
                        geonames_alternates a ON g.id = a.geoname_id
                    WHERE
                        {}
                        AND CASE :prefix
                            WHEN FALSE THEN a.name = :name
                            ELSE (a.name = :name OR (
                                (a.name BETWEEN :name AND :name || X'FFFF')
                                AND match_type = 3
                            ))
                            END
                    GROUP BY
                        g.id, match_type
                    ORDER BY
                        g.feature_class = 'P' DESC, g.population DESC, g.id ASC, a.iso_language ASC
                    "#,
                    type_pred
                ),
                named_params! {
                    ":name": query.to_lowercase(),
                    ":prefix": match_name_prefix,
                },
                |row| -> Result<Option<GeonameMatch>> {
                    let g_match = GeonameMatch {
                        geoname: Geoname {
                            geoname_id: row.get("id")?,
                            name: row.get("name")?,
                            latitude: row.get("latitude")?,
                            longitude: row.get("longitude")?,
                            country_code: row.get("country_code")?,
                            admin1_code: row.get("admin1_code")?,
                            population: row.get("population")?,
                        },
                        prefix: row.get("prefix")?,
                        match_type: match row.get::<_, i32>("match_type")? {
                            1 => GeonameMatchType::Abbreviation,
                            2 => GeonameMatchType::AirportCode,
                            _ => GeonameMatchType::Name,
                        },
                    };
                    if let Some(geonames) = &filter {
                        geonames
                            .iter()
                            .find(|g| g.has_same_region(&g_match.geoname))
                            .map(|_| Ok(Some(g_match)))
                            .unwrap_or(Ok(None))
                    } else {
                        Ok(Some(g_match))
                    }
                },
            )?
            .into_iter()
            .flatten()
            .collect())
    }

    /// Inserts GeoNames data into the database.
    fn insert_geonames(
        &mut self,
        record_id: &SuggestRecordId,
        attachments: &[DownloadedGeonameAttachment],
    ) -> Result<()> {
        self.scope.err_if_interrupted()?;
        let mut geoname_insert = GeonameInsertStatement::new(self.conn)?;
        let mut alt_insert = GeonameAlternateInsertStatement::new(self.conn)?;
        let mut metrics_insert = GeonameMetricsInsertStatement::new(self.conn)?;
        let mut max_len = 0;
        let mut max_word_count = 0;
        for attach in attachments {
            for geoname in &attach.geonames {
                geoname_insert.execute(record_id, geoname)?;
                for alt in &geoname.alternate_names_2 {
                    alt_insert.execute(alt, geoname.id)?;
                }
            }
            max_len = std::cmp::max(max_len, attach.max_alternate_name_length as usize);
            max_word_count = std::cmp::max(
                max_word_count,
                attach.max_alternate_name_word_count as usize,
            );
        }

        // Update geoname metrics.
        metrics_insert.execute(record_id, max_len, max_word_count)?;

        // We just made some insertions that might invalidate the data in the
        // cache. Clear it so it's repopulated the next time it's accessed.
        self.geoname_cache.take();

        Ok(())
    }

    pub fn geoname_cache(&self) -> &GeonameCache {
        self.geoname_cache.get_or_init(|| {
            self.conn
                .query_row_and_then(
                    r#"
                    SELECT
                        max(max_name_length) AS len, max(max_name_word_count) AS word_count
                    FROM
                        geonames_metrics
                    "#,
                    [],
                    |row| -> Result<GeonameCache> {
                        Ok(GeonameCache {
                            max_name_length: row.get("len")?,
                            max_name_word_count: row.get("word_count")?,
                        })
                    },
                )
                .unwrap_or_default()
        })
    }
}

impl<S> SuggestStoreInner<S>
where
    S: Client,
{
    /// Inserts a GeoNames record into the database.
    pub fn process_geoname_record(
        &self,
        dao: &mut SuggestDao,
        record: &Record,
        context: &mut MetricsContext,
    ) -> Result<()> {
        self.download_attachment(dao, record, context, |dao, record_id, data| {
            dao.insert_geonames(record_id, data)
        })
    }
}

struct GeonameInsertStatement<'conn>(rusqlite::Statement<'conn>);

impl<'conn> GeonameInsertStatement<'conn> {
    fn new(conn: &'conn Connection) -> Result<Self> {
        Ok(Self(conn.prepare(
            "INSERT INTO geonames(
                 id,
                 record_id,
                 name,
                 latitude,
                 longitude,
                 feature_class,
                 feature_code,
                 country_code,
                 admin1_code,
                 population
             )
             VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
             ",
        )?))
    }

    fn execute(&mut self, record_id: &SuggestRecordId, g: &DownloadedGeoname) -> Result<()> {
        self.0
            .execute((
                &g.id,
                record_id.as_str(),
                &g.name,
                &g.latitude,
                &g.longitude,
                &g.feature_class,
                &g.feature_code,
                &g.country_code,
                &g.admin1_code,
                &g.population,
            ))
            .with_context("geoname insert")?;
        Ok(())
    }
}

struct GeonameAlternateInsertStatement<'conn>(rusqlite::Statement<'conn>);

impl<'conn> GeonameAlternateInsertStatement<'conn> {
    fn new(conn: &'conn Connection) -> Result<Self> {
        Ok(Self(conn.prepare(
            "INSERT INTO geonames_alternates(
                 name,
                 geoname_id,
                 iso_language
             )
             VALUES(?, ?, ?)
             ",
        )?))
    }

    fn execute(&mut self, a: &DownloadedGeonameAlternate, geoname_id: i64) -> Result<()> {
        self.0
            .execute((&a.name, geoname_id, &a.iso_language))
            .with_context("geoname alternate insert")?;
        Ok(())
    }
}

struct GeonameMetricsInsertStatement<'conn>(rusqlite::Statement<'conn>);

impl<'conn> GeonameMetricsInsertStatement<'conn> {
    pub(crate) fn new(conn: &'conn Connection) -> Result<Self> {
        Ok(Self(conn.prepare(
            "INSERT INTO geonames_metrics(
                 record_id,
                 max_name_length,
                 max_name_word_count
             )
             VALUES(?, ?, ?)
             ",
        )?))
    }

    pub(crate) fn execute(
        &mut self,
        record_id: &SuggestRecordId,
        max_len: usize,
        max_word_count: usize,
    ) -> Result<()> {
        self.0
            .execute((record_id.as_str(), max_len, max_word_count))
            .with_context("geoname metrics insert")?;
        Ok(())
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::{
        provider::SuggestionProvider, store::tests::TestStore, testing::*,
        SuggestIngestionConstraints,
    };

    pub(crate) const LONG_NAME: &str = "aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll mmm nnn ooo ppp qqq rrr sss ttt uuu vvv www x yyy zzz";

    pub(crate) fn new_test_store() -> TestStore {
        TestStore::new(MockRemoteSettingsClient::default().with_record(
            "geonames",
            "geonames-0",
            json!({
                "max_alternate_name_length": LONG_NAME.len(),
                "max_alternate_name_word_count": LONG_NAME.split_whitespace().collect::<Vec<_>>().len(),
                "geonames": [
                    // Waterloo, AL
                    {
                        "id": 1,
                        "name": "Waterloo",
                        "latitude": "34.91814",
                        "longitude": "-88.0642",
                        "feature_class": "P",
                        "feature_code": "PPL",
                        "country_code": "US",
                        "admin1_code": "AL",
                        "population": 200,
                        "alternate_names": ["waterloo"],
                        "alternate_names_2": [
                            { "name": "waterloo" },
                        ],
                    },
                    // AL
                    {
                        "id": 2,
                        "name": "Alabama",
                        "latitude": "32.75041",
                        "longitude": "-86.75026",
                        "feature_class": "A",
                        "feature_code": "ADM1",
                        "country_code": "US",
                        "admin1_code": "AL",
                        "population": 4530315,
                        "alternate_names": ["al", "alabama"],
                        "alternate_names_2": [
                            { "name": "alabama" },
                            { "name": "al", "iso_language": "abbr" },
                        ],
                    },
                    // Waterloo, IA
                    {
                        "id": 3,
                        "name": "Waterloo",
                        "latitude": "42.49276",
                        "longitude": "-92.34296",
                        "feature_class": "P",
                        "feature_code": "PPLA2",
                        "country_code": "US",
                        "admin1_code": "IA",
                        "population": 68460,
                        "alternate_names": ["waterloo"],
                        "alternate_names_2": [
                            { "name": "waterloo" },
                        ],
                    },
                    // IA
                    {
                        "id": 4,
                        "name": "Iowa",
                        "latitude": "42.00027",
                        "longitude": "-93.50049",
                        "feature_class": "A",
                        "feature_code": "ADM1",
                        "country_code": "US",
                        "admin1_code": "IA",
                        "population": 2955010,
                        "alternate_names": ["ia", "iowa"],
                        "alternate_names_2": [
                            { "name": "iowa" },
                            { "name": "ia", "iso_language": "abbr" },
                        ],
                    },
                    // Waterloo (Lake, not a city or region)
                    {
                        "id": 5,
                        "name": "waterloo lake",
                        "latitude": "31.25044",
                        "longitude": "-99.25061",
                        "feature_class": "H",
                        "feature_code": "LK",
                        "country_code": "US",
                        "admin1_code": "TX",
                        "population": 0,
                        "alternate_names_2": [
                            { "name": "waterloo lake" },
                            { "name": "waterloo", "iso_language": "en" },
                        ],
                    },
                    // New York City
                    {
                        "id": 6,
                        "name": "New York City",
                        "latitude": "40.71427",
                        "longitude": "-74.00597",
                        "feature_class": "P",
                        "feature_code": "PPL",
                        "country_code": "US",
                        "admin1_code": "NY",
                        "population": 8804190,
                        "alternate_names_2": [
                            { "name": "new york city" },
                            { "name": "new york", "iso_language": "en" },
                            { "name": "nyc", "iso_language": "abbr" },
                            { "name": "ny", "iso_language": "abbr" },
                        ],
                    },
                    // Rochester, NY
                    {
                        "id": 7,
                        "name": "Rochester",
                        "latitude": "43.15478",
                        "longitude": "-77.61556",
                        "feature_class": "P",
                        "feature_code": "PPLA2",
                        "country_code": "US",
                        "admin1_code": "NY",
                        "population": 209802,
                        "alternate_names_2": [
                            { "name": "rochester" },
                            { "name": "roc", "iso_language": "iata" },
                        ],
                    },
                    // NY state
                    {
                        "id": 8,
                        "name": "New York",
                        "latitude": "43.00035",
                        "longitude": "-75.4999",
                        "feature_class": "A",
                        "feature_code": "ADM1",
                        "country_code": "US",
                        "admin1_code": "NY",
                        "population": 19274244,
                        "alternate_names_2": [
                            { "name": "new york" },
                            { "name": "ny", "iso_language": "abbr" },
                        ],
                    },
                    // Waco, TX: Has a surprising IATA airport code that's a
                    // common English word and not a prefix of the city name
                    {
                        "id": 9,
                        "name": "Waco",
                        "latitude": "31.54933",
                        "longitude": "-97.14667",
                        "feature_class": "P",
                        "feature_code": "PPLA2",
                        "country_code": "US",
                        "admin1_code": "TX",
                        "population": 132356,
                        "alternate_names_2": [
                            { "name": "waco" },
                            { "name": "act", "iso_language": "iata" },
                        ],
                    },
                    // TX
                    {
                        "id": 10,
                        "name": "Texas",
                        "latitude": "31.25044",
                        "longitude": "-99.25061",
                        "feature_class": "A",
                        "feature_code": "ADM1",
                        "country_code": "US",
                        "admin1_code": "TX",
                        "population": 22875689,
                        "alternate_names_2": [
                            { "name": "texas" },
                            { "name": "tx", "iso_language": "abbr" },
                        ],
                    },
                    // Made-up city with a long name
                    {
                        "id": 999,
                        "name": "Long Name",
                        "latitude": "38.06084",
                        "longitude": "-97.92977",
                        "feature_class": "P",
                        "feature_code": "PPLA2",
                        "country_code": "US",
                        "admin1_code": "NY",
                        "population": 2,
                        "alternate_names_2": [
                            { "name": "long name" },
                            { "name": LONG_NAME, "iso_language": "en" },
                        ],
                    },
                ],
            }),
        ))
    }

    pub(crate) fn waterloo_al() -> Geoname {
        Geoname {
            geoname_id: 1,
            name: "Waterloo".to_string(),
            latitude: 34.91814,
            longitude: -88.0642,
            country_code: "US".to_string(),
            admin1_code: "AL".to_string(),
            population: 200,
        }
    }

    pub(crate) fn waterloo_ia() -> Geoname {
        Geoname {
            geoname_id: 3,
            name: "Waterloo".to_string(),
            latitude: 42.49276,
            longitude: -92.34296,
            country_code: "US".to_string(),
            admin1_code: "IA".to_string(),
            population: 68460,
        }
    }

    pub(crate) fn nyc() -> Geoname {
        Geoname {
            geoname_id: 6,
            name: "New York City".to_string(),
            latitude: 40.71427,
            longitude: -74.00597,
            country_code: "US".to_string(),
            admin1_code: "NY".to_string(),
            population: 8804190,
        }
    }

    pub(crate) fn rochester() -> Geoname {
        Geoname {
            geoname_id: 7,
            name: "Rochester".to_string(),
            latitude: 43.15478,
            longitude: -77.61556,
            country_code: "US".to_string(),
            admin1_code: "NY".to_string(),
            population: 209802,
        }
    }

    pub(crate) fn waco() -> Geoname {
        Geoname {
            geoname_id: 9,
            name: "Waco".to_string(),
            latitude: 31.54933,
            longitude: -97.14667,
            country_code: "US".to_string(),
            admin1_code: "TX".to_string(),
            population: 132356,
        }
    }

    pub(crate) fn long_name_city() -> Geoname {
        Geoname {
            geoname_id: 999,
            name: "Long Name".to_string(),
            latitude: 38.06084,
            longitude: -97.92977,
            country_code: "US".to_string(),
            admin1_code: "NY".to_string(),
            population: 2,
        }
    }

    pub(crate) fn al() -> Geoname {
        Geoname {
            geoname_id: 2,
            name: "Alabama".to_string(),
            latitude: 32.75041,
            longitude: -86.75026,
            country_code: "US".to_string(),
            admin1_code: "AL".to_string(),
            population: 4530315,
        }
    }

    pub(crate) fn ia() -> Geoname {
        Geoname {
            geoname_id: 4,
            name: "Iowa".to_string(),
            latitude: 42.00027,
            longitude: -93.50049,
            country_code: "US".to_string(),
            admin1_code: "IA".to_string(),
            population: 2955010,
        }
    }

    pub(crate) fn ny_state() -> Geoname {
        Geoname {
            geoname_id: 8,
            name: "New York".to_string(),
            latitude: 43.00035,
            longitude: -75.4999,
            country_code: "US".to_string(),
            admin1_code: "NY".to_string(),
            population: 19274244,
        }
    }

    #[test]
    fn geonames() -> anyhow::Result<()> {
        before_each();

        let store = new_test_store();

        // Ingest weather to also ingest geonames.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        #[derive(Debug)]
        struct Test {
            query: &'static str,
            match_name_prefix: bool,
            geoname_type: Option<GeonameType>,
            filter: Option<Vec<Geoname>>,
            expected: Vec<GeonameMatch>,
        }

        let tests = [
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "ia",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![waterloo_ia(), waterloo_al()]),
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![waterloo_ia()]),
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![waterloo_al()]),
                expected: vec![],
            },
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: Some(GeonameType::City),
                filter: None,
                expected: vec![],
            },
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: Some(GeonameType::Region),
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "iowa",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Name,
                    prefix: false,
                }],
            },
            Test {
                query: "al",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: al(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            // "al" is both a name prefix and an abbreviation.
            Test {
                query: "al",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: al(),
                        match_type: GeonameMatchType::Name,
                        prefix: true,
                    },
                    GeonameMatch {
                        geoname: al(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "waterloo",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![ia()]),
                expected: vec![GeonameMatch {
                    geoname: waterloo_ia(),
                    match_type: GeonameMatchType::Name,
                    prefix: false,
                }],
            },
            Test {
                query: "waterloo",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![al()]),
                expected: vec![GeonameMatch {
                    geoname: waterloo_al(),
                    match_type: GeonameMatchType::Name,
                    prefix: false,
                }],
            },
            Test {
                query: "waterloo",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![ny_state()]),
                expected: vec![],
            },
            Test {
                query: "waterloo",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                // Waterloo, IA should be first since it has a larger
                // population.
                expected: vec![
                    GeonameMatch {
                        geoname: waterloo_ia(),
                        match_type: GeonameMatchType::Name,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: waterloo_al(),
                        match_type: GeonameMatchType::Name,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "water",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: waterloo_ia(),
                        match_type: GeonameMatchType::Name,
                        prefix: true,
                    },
                    GeonameMatch {
                        geoname: waterloo_al(),
                        match_type: GeonameMatchType::Name,
                        prefix: true,
                    },
                ],
            },
            Test {
                query: "water",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "ny",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                // NYC should be first since cities are ordered before regions.
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "ny",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "ny",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![nyc()]),
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "ny",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![ny_state()]),
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "ny",
                match_name_prefix: false,
                geoname_type: Some(GeonameType::City),
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: nyc(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "ny",
                match_name_prefix: false,
                geoname_type: Some(GeonameType::Region),
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ny_state(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            Test {
                query: "NeW YoRk",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Name,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Name,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "NY",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "new",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "new",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Name,
                        prefix: true,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Name,
                        prefix: true,
                    },
                ],
            },
            Test {
                query: "new york foo",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "new york foo",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "new foo",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "foo new york",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "foo new york",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "foo new",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![],
            },
            Test {
                query: "roc",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: rochester(),
                    match_type: GeonameMatchType::AirportCode,
                    prefix: false,
                }],
            },
            // "roc" is both a name prefix and an airport code.
            Test {
                query: "roc",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: rochester(),
                        match_type: GeonameMatchType::Name,
                        prefix: true,
                    },
                    GeonameMatch {
                        geoname: rochester(),
                        match_type: GeonameMatchType::AirportCode,
                        prefix: false,
                    },
                ],
            },
            Test {
                query: "long name",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: long_name_city(),
                    match_type: GeonameMatchType::Name,
                    prefix: false,
                }],
            },
            Test {
                query: LONG_NAME,
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: long_name_city(),
                    match_type: GeonameMatchType::Name,
                    prefix: false,
                }],
            },
        ];

        store.read(|dao| {
            for t in tests {
                let gs = t.filter.clone().unwrap_or_default();
                let gs_refs: Vec<_> = gs.iter().collect();
                let filters = if gs_refs.is_empty() {
                    None
                } else {
                    Some(gs_refs)
                };
                assert_eq!(
                    dao.fetch_geonames(
                        t.query,
                        t.match_name_prefix,
                        t.geoname_type.clone(),
                        filters
                    )?,
                    t.expected,
                    "Test: {:?}",
                    t
                );
            }
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn geonames_metrics() -> anyhow::Result<()> {
        before_each();

        // Add a couple of records with different metrics. We're just testing
        // metrics so the other values don't matter.
        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "geonames",
                    "geonames-0",
                    json!({
                        "max_alternate_name_length": 10,
                        "max_alternate_name_word_count": 5,
                        "geonames": []
                    }),
                )
                .with_record(
                    "geonames",
                    "geonames-1",
                    json!({
                        "max_alternate_name_length": 20,
                        "max_alternate_name_word_count": 2,
                        "geonames": []
                    }),
                ),
        );

        // Ingest weather to also ingest geonames.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        store.read(|dao| {
            let cache = dao.geoname_cache();
            assert_eq!(cache.max_name_length, 20);
            assert_eq!(cache.max_name_word_count, 5);
            Ok(())
        })?;

        // Delete the first record. The metrics should change.
        store
            .client_mut()
            .delete_record("quicksuggest", "geonames-0");
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        store.read(|dao| {
            let cache = dao.geoname_cache();
            assert_eq!(cache.max_name_length, 20);
            assert_eq!(cache.max_name_word_count, 2);
            Ok(())
        })?;

        // Add a new record. The metrics should change again.
        store.client_mut().add_record(
            "geonames",
            "geonames-3",
            json!({
                "max_alternate_name_length": 15,
                "max_alternate_name_word_count": 3,
                "geonames": []
            }),
        );
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });
        store.read(|dao| {
            let cache = dao.geoname_cache();
            assert_eq!(cache.max_name_length, 20);
            assert_eq!(cache.max_name_word_count, 3);
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn geonames_deleted_record() -> anyhow::Result<()> {
        before_each();

        // Create the store with the test data and ingest.
        let mut store = new_test_store();
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        // Make sure we have a match.
        store.read(|dao| {
            assert_eq!(
                dao.fetch_geonames("waterloo", false, None, None)?,
                vec![
                    GeonameMatch {
                        geoname: waterloo_ia(),
                        match_type: GeonameMatchType::Name,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: waterloo_al(),
                        match_type: GeonameMatchType::Name,
                        prefix: false,
                    },
                ],
            );
            Ok(())
        })?;

        // Delete the record.
        store
            .client_mut()
            .delete_record("quicksuggest", "geonames-0");
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        // The same query shouldn't match anymore and the tables should be
        // empty.
        store.read(|dao| {
            assert_eq!(dao.fetch_geonames("waterloo", false, None, None)?, vec![],);

            let g_ids = dao.conn.query_rows_and_then(
                "SELECT id FROM geonames",
                [],
                |row| -> Result<i64> { Ok(row.get("id")?) },
            )?;
            assert_eq!(g_ids, Vec::<i64>::new());

            let alt_g_ids = dao.conn.query_rows_and_then(
                "SELECT geoname_id FROM geonames_alternates",
                [],
                |row| -> Result<i64> { Ok(row.get("geoname_id")?) },
            )?;
            assert_eq!(alt_g_ids, Vec::<i64>::new());

            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn geonames_store_api() -> anyhow::Result<()> {
        before_each();

        let store = new_test_store();

        // Ingest weather to also ingest geonames.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Weather]),
            ..SuggestIngestionConstraints::all_providers()
        });

        #[derive(Debug)]
        struct Test {
            query: &'static str,
            match_name_prefix: bool,
            geoname_type: Option<GeonameType>,
            filter: Option<Vec<Geoname>>,
            expected: Vec<GeonameMatch>,
        }

        // This only tests a few different calls to exercise all the fetch
        // options. Comprehensive fetch cases are in the main `geonames` test.
        let tests = [
            // simple fetch with no options
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: None,
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            // filter
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: None,
                filter: Some(vec![waterloo_ia(), waterloo_al()]),
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            // geoname type: city
            Test {
                query: "ia",
                match_name_prefix: false,
                geoname_type: Some(GeonameType::Region),
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: ia(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            // geoname type: region
            Test {
                query: "ny",
                match_name_prefix: false,
                geoname_type: Some(GeonameType::City),
                filter: None,
                expected: vec![GeonameMatch {
                    geoname: nyc(),
                    match_type: GeonameMatchType::Abbreviation,
                    prefix: false,
                }],
            },
            // prefix matching
            Test {
                query: "ny",
                match_name_prefix: true,
                geoname_type: None,
                filter: None,
                expected: vec![
                    GeonameMatch {
                        geoname: nyc(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                    GeonameMatch {
                        geoname: ny_state(),
                        match_type: GeonameMatchType::Abbreviation,
                        prefix: false,
                    },
                ],
            },
        ];

        for t in tests {
            assert_eq!(
                store.fetch_geonames(
                    t.query,
                    t.match_name_prefix,
                    t.geoname_type.clone(),
                    t.filter.clone()
                ),
                t.expected,
                "Test: {:?}",
                t
            );
        }

        Ok(())
    }
}
