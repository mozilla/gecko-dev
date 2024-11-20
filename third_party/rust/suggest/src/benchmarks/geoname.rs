/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    benchmarks::{new_store, BenchmarkWithInput},
    geoname::{Geoname, GeonameType},
    SuggestStore,
};

pub struct GeonameBenchmark {
    args: FetchGeonamesArgs,
    should_match: bool,
}

#[derive(Clone, Debug)]
pub struct FetchGeonamesArgs {
    query: &'static str,
    match_name_prefix: bool,
    geoname_type: Option<GeonameType>,
    filter: Option<Vec<Geoname>>,
}

pub struct IterationInput {
    fetch_args: FetchGeonamesArgs,
    should_match_message: String,
}

impl BenchmarkWithInput for GeonameBenchmark {
    type GlobalInput = SuggestStore;
    type IterationInput = IterationInput;

    fn global_input(&self) -> Self::GlobalInput {
        new_store()
    }

    fn iteration_input(&self) -> Self::IterationInput {
        let fetch_args = self.args.clone();
        // Format the message now so it doesn't take up time in the benchmark.
        let should_match_message = format!("should_match for fetch: {:?}", fetch_args);
        IterationInput {
            fetch_args,
            should_match_message,
        }
    }

    fn benchmarked_code(&self, store: &Self::GlobalInput, i_input: Self::IterationInput) {
        let matches = store
            .fetch_geonames(
                i_input.fetch_args.query,
                i_input.fetch_args.match_name_prefix,
                i_input.fetch_args.geoname_type,
                i_input.fetch_args.filter,
            )
            .unwrap_or_else(|e| panic!("Error fetching geonames: {e}"));

        // Make sure matches were returned or not as expected. Otherwise the
        // benchmark might not be testing what it's intended to test.
        assert_eq!(
            !matches.is_empty(),
            self.should_match,
            "{}",
            i_input.should_match_message,
        );
    }
}

pub fn all_benchmarks() -> Vec<(&'static str, GeonameBenchmark)> {
    let ny_state = Geoname {
        geoname_id: 8,
        name: "New York".to_string(),
        latitude: 43.00035,
        longitude: -75.4999,
        country_code: "US".to_string(),
        admin1_code: "NY".to_string(),
        population: 19274244,
    };

    vec![
        // no matches
        (
            "geoname-fetch-no-match-1",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "nomatch",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),
        (
            "geoname-fetch-no-match-2",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "no match",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),
        (
            "geoname-fetch-no-match-3",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "no match either",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),
        (
            "geoname-fetch-no-match-long",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "this is a very long string that does not match anything in the geonames database but it sure is very long",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),

        // no matches w/ prefix matching
        (
            "geoname-fetch-no-match-1-prefix",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "nomatch",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),
        (
            "geoname-fetch-no-match-2-prefix",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "no match",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),
        (
            "geoname-fetch-no-match-3-prefix",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "no match either",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),
        (
            "geoname-fetch-no-match-long-prefix",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "this is a very long string that does not match anything in the geonames database but it sure is very long",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: false,
            }
        ),

        // abbreviations and airport codes
        (
            "geoname-fetch-abbr-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-abbr-nyc",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "nyc",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-abbr-ca",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ca",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-airport-pdx",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "pdx",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-airport-roc",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "roc",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),

        // abbreviations and airport codes w/ prefix matching
        (
            "geoname-fetch-abbr-prefix-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-abbr-prefix-nyc",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "nyc",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-abbr-prefix-ca",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ca",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-airport-prefix-pdx",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "pdx",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-airport-prefix-roc",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "roc",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),

        // full names
        (
            "geoname-fetch-name-new-york",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "new york",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-name-rochester",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "rochester",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),

        // full names w/ prefix matching
        (
            "geoname-fetch-name-prefix-new-york",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "new york",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-name-prefix-rochester",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "rochester",
                    match_name_prefix: true,
                    geoname_type: None,
                    filter: None,
                },
                should_match: true,
            }
        ),

        // restricting to a geoname type
        (
            "geoname-fetch-type-city-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: false,
                    geoname_type: Some(GeonameType::City),
                    filter: None,
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-type-region-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: false,
                    geoname_type: Some(GeonameType::Region),
                    filter: None,
                },
                should_match: true,
            }
        ),

        // filtering
        (
            "geoname-fetch-filter-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: false,
                    geoname_type: None,
                    filter: Some(vec![ny_state.clone()]),
                },
                should_match: true,
            }
        ),

        // restricting to a geoname type + filtering
        (
            "geoname-fetch-type-filter-city-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: false,
                    geoname_type: Some(GeonameType::City),
                    filter: Some(vec![ny_state.clone()]),
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-type-filter-region-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: false,
                    geoname_type: Some(GeonameType::Region),
                    filter: Some(vec![ny_state.clone()]),
                },
                should_match: true,
            }
        ),

        // restricting to a geoname type + filtering w/ prefix matching
        (
            "geoname-fetch-type-filter-prefix-city-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: true,
                    geoname_type: Some(GeonameType::City),
                    filter: Some(vec![ny_state.clone()]),
                },
                should_match: true,
            }
        ),
        (
            "geoname-fetch-type-filter-prefix-region-ny",
            GeonameBenchmark {
                args: FetchGeonamesArgs {
                    query: "ny",
                    match_name_prefix: true,
                    geoname_type: Some(GeonameType::Region),
                    filter: Some(vec![ny_state.clone()]),
                },
                should_match: true,
            }
        ),
    ]
}
