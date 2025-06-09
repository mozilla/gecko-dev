/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::OnceLock;

use crate::{
    benchmarks::{client::RemoteSettingsBenchmarkClient, unique_db_filename, BenchmarkWithInput},
    provider::SuggestionProvider,
    rs::{Collection, SuggestRecordType},
    store::SuggestStoreInner,
    SuggestIngestionConstraints,
};

pub struct IngestBenchmark {
    temp_dir: tempfile::TempDir,
    client: RemoteSettingsBenchmarkClient,
    collection: Collection,
    record_type: SuggestRecordType,
    reingest: bool,
}

/// Get a benchmark client to use for the tests
///
/// Uses OnceLock to ensure we only construct it once.
fn get_benchmark_client() -> RemoteSettingsBenchmarkClient {
    static CELL: OnceLock<RemoteSettingsBenchmarkClient> = OnceLock::new();
    CELL.get_or_init(|| {
        RemoteSettingsBenchmarkClient::new()
            .unwrap_or_else(|e| panic!("Error creating benchmark client {e}"))
    })
    .clone()
}

impl IngestBenchmark {
    pub fn new(provider: SuggestionProvider, reingest: bool) -> Self {
        Self::new_with_record_type(
            provider.primary_collection(),
            provider.primary_record_type(),
            reingest,
        )
    }

    pub fn new_with_record_type(
        collection: Collection,
        record_type: SuggestRecordType,
        reingest: bool,
    ) -> Self {
        let temp_dir = tempfile::tempdir().unwrap();
        Self {
            client: get_benchmark_client(),
            temp_dir,
            collection,
            record_type,
            reingest,
        }
    }
}

// The input for each benchmark is `SuggestStoreInner` with a fresh database.
//
// This is wrapped in a newtype so that it can be exposed in the public trait
pub struct InputType(SuggestStoreInner<RemoteSettingsBenchmarkClient>);

impl BenchmarkWithInput for IngestBenchmark {
    type GlobalInput = ();
    type IterationInput = InputType;

    fn global_input(&self) -> Self::GlobalInput {}

    fn iteration_input(&self) -> Self::IterationInput {
        let data_path = self.temp_dir.path().join(unique_db_filename());
        let store = SuggestStoreInner::new(data_path, vec![], self.client.clone());
        store.ensure_db_initialized();
        if self.reingest {
            store.ingest_records_by_type(self.collection, self.record_type);
            store.force_reingest();
        }
        InputType(store)
    }

    fn benchmarked_code(&self, _: &Self::GlobalInput, input: Self::IterationInput) {
        let InputType(store) = input;
        store.ingest_records_by_type(self.collection, self.record_type);
    }
}

/// Get IngestBenchmark instances for all record types
pub fn all_benchmarks() -> Vec<(&'static str, IngestBenchmark)> {
    vec![
        (
            "ingest-icon-amp",
            IngestBenchmark::new_with_record_type(Collection::Amp, SuggestRecordType::Icon, false),
        ),
        (
            "ingest-again-icon-amp",
            IngestBenchmark::new_with_record_type(Collection::Amp, SuggestRecordType::Icon, true),
        ),
        (
            "ingest-icon-other",
            IngestBenchmark::new_with_record_type(
                Collection::Other,
                SuggestRecordType::Icon,
                false,
            ),
        ),
        (
            "ingest-again-icon-other",
            IngestBenchmark::new_with_record_type(Collection::Other, SuggestRecordType::Icon, true),
        ),
        (
            "ingest-amp",
            IngestBenchmark::new(SuggestionProvider::Amp, false),
        ),
        (
            "ingest-again-amp",
            IngestBenchmark::new(SuggestionProvider::Amp, true),
        ),
        (
            "ingest-wikipedia",
            IngestBenchmark::new(SuggestionProvider::Wikipedia, false),
        ),
        (
            "ingest-again-wikipedia",
            IngestBenchmark::new(SuggestionProvider::Wikipedia, true),
        ),
        (
            "ingest-amo",
            IngestBenchmark::new(SuggestionProvider::Amo, false),
        ),
        (
            "ingest-again-amo",
            IngestBenchmark::new(SuggestionProvider::Amo, true),
        ),
        (
            "ingest-yelp",
            IngestBenchmark::new(SuggestionProvider::Yelp, false),
        ),
        (
            "ingest-again-yelp",
            IngestBenchmark::new(SuggestionProvider::Yelp, true),
        ),
        (
            "ingest-mdn",
            IngestBenchmark::new(SuggestionProvider::Mdn, false),
        ),
        (
            "ingest-again-mdn",
            IngestBenchmark::new(SuggestionProvider::Mdn, true),
        ),
        (
            "ingest-weather",
            IngestBenchmark::new(SuggestionProvider::Weather, false),
        ),
        (
            "ingest-again-weather",
            IngestBenchmark::new(SuggestionProvider::Weather, true),
        ),
        (
            "ingest-global-config",
            IngestBenchmark::new_with_record_type(
                Collection::Other,
                SuggestRecordType::GlobalConfig,
                false,
            ),
        ),
        (
            "ingest-again-global-config",
            IngestBenchmark::new_with_record_type(
                Collection::Other,
                SuggestRecordType::GlobalConfig,
                true,
            ),
        ),
        (
            "ingest-fakespot",
            IngestBenchmark::new(SuggestionProvider::Fakespot, false),
        ),
        (
            "ingest-again-fakespot",
            IngestBenchmark::new(SuggestionProvider::Fakespot, true),
        ),
    ]
}

pub fn print_debug_ingestion_sizes() {
    viaduct_reqwest::use_reqwest_backend();
    let store = SuggestStoreInner::new(
        "file:debug_ingestion_sizes?mode=memory&cache=shared",
        vec![],
        RemoteSettingsBenchmarkClient::new().unwrap(),
    );
    store
        .ingest(SuggestIngestionConstraints {
            // Uncomment to measure the size for a specific provider
            // providers: Some(vec![crate::SuggestionProvider::Fakespot]),
            ..SuggestIngestionConstraints::default()
        })
        .unwrap();
    let table_row_counts = store.table_row_counts();
    let db_size = store.db_size();
    let client = store.into_settings_client();
    println!("Attachment sizes");
    println!("-------------------------");
    let attachment_sizes = client.attachment_size_by_record_type();
    let total_attachment_size: usize = attachment_sizes.iter().map(|(_, size)| size).sum();
    for (record_type, size) in attachment_sizes {
        println!("{:30} {}kb", record_type.as_str(), (size + 500) / 1000)
    }
    println!();
    println!(
        "Total attachment size: {}kb",
        (total_attachment_size + 500) / 1000
    );

    println!("Database table row counts");
    println!("-------------------------");
    for (name, count) in table_row_counts {
        println!("{name:30} {count}");
    }
    println!();
    println!("Total database size: {}kb", (db_size + 500) / 1000);
}
