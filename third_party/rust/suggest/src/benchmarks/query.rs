/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    benchmarks::{new_store, BenchmarkWithInput},
    SuggestStore, SuggestionProvider, SuggestionQuery,
};

pub struct QueryBenchmark {
    provider: SuggestionProvider,
    query: &'static str,
    should_match: bool,
}

pub struct IterationInput {
    query: SuggestionQuery,
    should_match_message: String,
}

impl BenchmarkWithInput for QueryBenchmark {
    type GlobalInput = SuggestStore;
    type IterationInput = IterationInput;

    fn global_input(&self) -> Self::GlobalInput {
        new_store()
    }

    fn iteration_input(&self) -> Self::IterationInput {
        let query = SuggestionQuery {
            providers: vec![self.provider],
            keyword: self.query.to_string(),
            ..SuggestionQuery::default()
        };
        // Format the message now so it doesn't take up time in the benchmark.
        let should_match_message = format!("should_match for query: {:?}", query);
        IterationInput {
            query,
            should_match_message,
        }
    }

    fn benchmarked_code(&self, store: &Self::GlobalInput, i_input: Self::IterationInput) {
        let suggestions = store
            .query(i_input.query)
            .unwrap_or_else(|e| panic!("Error querying store: {e}"));

        // Make sure matches were returned or not as expected. Otherwise the
        // benchmark might not be testing what it's intended to test.
        assert_eq!(
            !suggestions.is_empty(),
            self.should_match,
            "{}",
            i_input.should_match_message,
        );
    }
}

pub fn all_benchmarks() -> Vec<(&'static str, QueryBenchmark)> {
    vec![
        // Fakespot queries, these attempt to perform prefix matches with various character
        // lengths.
        //
        // The query code will only do a prefix match if the total input length is > 3 chars.
        // Therefore, to test shorter prefixes we use 2-term queries.
        (
            "query-fakespot-hand-s",
            QueryBenchmark {
                provider: SuggestionProvider::Fakespot,
                query: "hand s",
                should_match: true,
            }
        ),
        (
            "query-fakespot-hand-sa",
            QueryBenchmark {
                provider: SuggestionProvider::Fakespot,
                query: "hand sa",
                should_match: true,
            }
        ),
        (
            "query-fakespot-hand-san",
            QueryBenchmark {
                provider: SuggestionProvider::Fakespot,
                query: "hand san",
                should_match: true,
            }
        ),
        (
            "query-fakespot-sani",
            QueryBenchmark {
                provider: SuggestionProvider::Fakespot,
                query: "sani",
                should_match: true,
            }
        ),
        (
            "query-fakespot-sanit",
            QueryBenchmark {
                provider: SuggestionProvider::Fakespot,
                query: "sanit",
                should_match: true,
            }
        ),
        (
            "query-fakespot-saniti",
            QueryBenchmark {
                provider: SuggestionProvider::Fakespot,
                query: "saniti",
                should_match: false,
            },
        ),

        // weather: no matches
        (
            "query-weather-no-match-1",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "nomatch",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-2",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "no match",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-3",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "no match either",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-long-1",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "city1 city2 state1 state2 keyword1 keyword2 keyword3",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-long-2",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "this does not match anything especially not a weather suggestion but nevertheless it is a very long query which as previously mentioned doesn't match anything at all",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-keyword-prefix",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "wea",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-city-abbr",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "ny",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-airport-code",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "pdx",
                should_match: false,
            },
        ),
        (
            "query-weather-no-match-airport-code-region",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "pdx or",
                should_match: false,
            },
        ),

        // weather: keyword only
        (
            "query-weather-keyword-only",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather",
                should_match: true,
            },
        ),

        // weather: city only
        (
            "query-weather-city-only",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "new york",
                should_match: true,
            },
        ),

        // weather: city + region
        (
            "query-weather-city-region-los-angeles-c",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "los angeles c",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-los-angeles-ca",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "los angeles ca",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-la-ca",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "la ca",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-ny-ny",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "ny ny",
                should_match: true,
            },
        ),

        // weather: keyword + city
        (
            "query-weather-keyword-city-n",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather n",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-ne",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather ne",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-new",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather new",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-new-york",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather new york",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-ny",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather ny",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-pdx",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather pdx",
                should_match: true,
            },
        ),

        // weather: keyword + city + region
        (
            "query-weather-keyword-city-region-los-angeles-c",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather los angeles c",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-region-los-angeles-ca",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather los angeles ca",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-region-la-ca",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather la ca",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-region-ny-ny",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather ny ny",
                should_match: true,
            },
        ),
        (
            "query-weather-keyword-city-region-pdx-or",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "weather pdx or",
                should_match: true,
            },
        ),

        // weather: city + keyword
        (
            "query-weather-city-keyword-new-york-w",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "new york w",
                should_match: true,
            },
        ),
        (
            "query-weather-city-keyword-new-york-we",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "new york we",
                should_match: true,
            },
        ),
        (
            "query-weather-city-keyword-new-york-wea",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "new york wea",
                should_match: true,
            },
        ),
        (
            "query-weather-city-keyword-new-york-weather",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "new york weather",
                should_match: true,
            },
        ),
        (
            "query-weather-city-keyword-ny-w",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "ny w",
                should_match: true,
            },
        ),
        (
            "query-weather-city-keyword-ny-weather",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "ny weather",
                should_match: true,
            },
        ),

        // weather: city + region + keyword
        (
            "query-weather-city-region-keyword-los-angeles-w",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "los angeles ca w",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-keyword-los-angeles-we",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "los angeles ca we",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-keyword-los-angeles-wea",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "los angeles ca wea",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-keyword-los-angeles-weather",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "los angeles ca weather",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-keyword-la-ca-weather",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "la ca weather",
                should_match: true,
            },
        ),
        (
            "query-weather-city-region-keyword-ny-ny-weather",
            QueryBenchmark {
                provider: SuggestionProvider::Weather,
                query: "ny ny weather",
                should_match: true,
            },
        ),
    ]
}
