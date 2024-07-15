/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    benchmarks::{
        client::{RemoteSettingsBenchmarkClient, RemoteSettingsWarmUpClient},
        BenchmarkWithInput,
    },
    rs::SuggestRecordType,
    store::SuggestStoreInner,
    SuggestIngestionConstraints,
};
use std::sync::atomic::{AtomicU32, Ordering};

static DB_FILE_COUNTER: AtomicU32 = AtomicU32::new(0);

pub struct IngestBenchmark {
    temp_dir: tempfile::TempDir,
    client: RemoteSettingsBenchmarkClient,
    record_type: SuggestRecordType,
    reingest: bool,
}

impl IngestBenchmark {
    pub fn new(record_type: SuggestRecordType, reingest: bool) -> Self {
        let temp_dir = tempfile::tempdir().unwrap();
        let store = SuggestStoreInner::new(
            temp_dir.path().join("warmup.sqlite"),
            RemoteSettingsWarmUpClient::new(),
        );
        store.benchmark_ingest_records_by_type(record_type);
        Self {
            client: RemoteSettingsBenchmarkClient::from(store.into_settings_client()),
            temp_dir,
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
    type Input = InputType;

    fn generate_input(&self) -> Self::Input {
        let data_path = self.temp_dir.path().join(format!(
            "db{}.sqlite",
            DB_FILE_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        let store = SuggestStoreInner::new(data_path, self.client.clone());
        store.ensure_db_initialized();
        if self.reingest {
            store.force_reingest(self.record_type);
        }
        InputType(store)
    }

    fn benchmarked_code(&self, input: Self::Input) {
        let InputType(store) = input;
        store.benchmark_ingest_records_by_type(self.record_type);
    }
}

/// Get IngestBenchmark instances for all record types
pub fn all_benchmarks() -> Vec<(&'static str, IngestBenchmark)> {
    vec![
        (
            "ingest-icon",
            IngestBenchmark::new(SuggestRecordType::Icon, false),
        ),
        (
            "ingest-amp-wikipedia",
            IngestBenchmark::new(SuggestRecordType::AmpWikipedia, false),
        ),
        (
            "ingest-amo",
            IngestBenchmark::new(SuggestRecordType::Amo, false),
        ),
        (
            "ingest-pocket",
            IngestBenchmark::new(SuggestRecordType::Pocket, false),
        ),
        (
            "ingest-yelp",
            IngestBenchmark::new(SuggestRecordType::Yelp, false),
        ),
        (
            "ingest-mdn",
            IngestBenchmark::new(SuggestRecordType::Mdn, false),
        ),
        (
            "ingest-weather",
            IngestBenchmark::new(SuggestRecordType::Weather, false),
        ),
        (
            "ingest-global-config",
            IngestBenchmark::new(SuggestRecordType::GlobalConfig, false),
        ),
        (
            "ingest-amp-mobile",
            IngestBenchmark::new(SuggestRecordType::AmpMobile, false),
        ),
        (
            "ingest-again-icon",
            IngestBenchmark::new(SuggestRecordType::Icon, true),
        ),
        (
            "ingest-again-amp-wikipedia",
            IngestBenchmark::new(SuggestRecordType::AmpWikipedia, true),
        ),
        (
            "ingest-again-amo",
            IngestBenchmark::new(SuggestRecordType::Amo, true),
        ),
        (
            "ingest-again-pocket",
            IngestBenchmark::new(SuggestRecordType::Pocket, true),
        ),
        (
            "ingest-again-yelp",
            IngestBenchmark::new(SuggestRecordType::Yelp, true),
        ),
        (
            "ingest-again-mdn",
            IngestBenchmark::new(SuggestRecordType::Mdn, true),
        ),
        (
            "ingest-again-weather",
            IngestBenchmark::new(SuggestRecordType::Weather, true),
        ),
        (
            "ingest-again-global-config",
            IngestBenchmark::new(SuggestRecordType::GlobalConfig, true),
        ),
        (
            "ingest-again-amp-mobile",
            IngestBenchmark::new(SuggestRecordType::AmpMobile, true),
        ),
    ]
}

pub fn print_debug_ingestion_sizes() {
    viaduct_reqwest::use_reqwest_backend();
    let store = SuggestStoreInner::new(
        "file:debug_ingestion_sizes?mode=memory&cache=shared",
        RemoteSettingsWarmUpClient::new(),
    );
    store
        .ingest(SuggestIngestionConstraints::default())
        .unwrap();
    let table_row_counts = store.table_row_counts();
    let db_size = store.db_size();
    let client = store.into_settings_client();
    let total_attachment_size: usize = client
        .get_records_responses
        .lock()
        .values()
        .flat_map(|records| {
            records.iter().map(|r| match &r.attachment_data {
                Some(d) => d.len(),
                None => 0,
            })
        })
        .sum();

    println!(
        "Total attachment size: {}kb",
        (total_attachment_size + 500) / 1000
    );
    println!("Total database size: {}kb", (db_size + 500) / 1000);
    println!();
    println!("Database table row counts");
    println!("-------------------------");
    for (name, count) in table_row_counts {
        println!("{name:30}: {count}");
    }
}
