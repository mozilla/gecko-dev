/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    benchmarks::{unique_db_filename, BenchmarkWithInput},
    SuggestIngestionConstraints, SuggestStore, SuggestionProvider, SuggestionQuery,
};

pub struct QueryBenchmark {
    store: SuggestStore,
    provider: SuggestionProvider,
    query: &'static str,
}

impl QueryBenchmark {
    pub fn new(provider: SuggestionProvider, query: &'static str) -> Self {
        let temp_dir = tempfile::tempdir().unwrap();
        let data_path = temp_dir.path().join(unique_db_filename());
        let store =
            SuggestStore::new(&data_path.to_string_lossy(), None).expect("Error building store");
        store
            .ingest(SuggestIngestionConstraints::all_providers())
            .expect("Error during ingestion");
        Self {
            store,
            provider,
            query,
        }
    }
}

// The input for each benchmark a query to pass to the store
pub struct InputType(SuggestionQuery);

impl BenchmarkWithInput for QueryBenchmark {
    type Input = InputType;

    fn generate_input(&self) -> Self::Input {
        InputType(SuggestionQuery {
            providers: vec![self.provider],
            keyword: self.query.to_string(),
            ..SuggestionQuery::default()
        })
    }

    fn benchmarked_code(&self, input: Self::Input) {
        let InputType(query) = input;
        self.store
            .query(query)
            .unwrap_or_else(|e| panic!("Error querying store: {e}"));
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
            QueryBenchmark::new(SuggestionProvider::Fakespot, "hand s"),
        ),
        (
            "query-fakespot-hand-sa",
            QueryBenchmark::new(SuggestionProvider::Fakespot, "hand sa"),
        ),
        (
            "query-fakespot-hand-san",
            QueryBenchmark::new(SuggestionProvider::Fakespot, "hand san"),
        ),
        (
            "query-fakespot-sani",
            QueryBenchmark::new(SuggestionProvider::Fakespot, "sani"),
        ),
        (
            "query-fakespot-sanit",
            QueryBenchmark::new(SuggestionProvider::Fakespot, "sanit"),
        ),
        (
            "query-fakespot-saniti",
            QueryBenchmark::new(SuggestionProvider::Fakespot, "saniti"),
        ),
    ]
}
