/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

use serde_json::Value as JsonValue;

use crate::{rs, Result};

/// Remotes settings client for benchmarking
///
/// This fetches all data in `new`, then implements [rs::Client] by returning the local data.
/// Construct this one before the benchmark is run, then clone it as input for the benchmark.  This
/// ensures that network time does not count towards the benchmark time.
#[derive(Clone, Default)]
pub struct RemoteSettingsBenchmarkClient {
    pub records: Vec<remote_settings::RemoteSettingsRecord>,
    pub attachments: HashMap<String, Vec<u8>>,
}

impl RemoteSettingsBenchmarkClient {
    pub fn new() -> Result<Self> {
        let mut new_benchmark_client = Self::default();
        new_benchmark_client.fetch_data_with_client(remote_settings::Client::new(
            remote_settings::RemoteSettingsConfig {
                server: None,
                bucket_name: None,
                collection_name: "quicksuggest".to_owned(),
                server_url: None,
            },
        )?)?;
        new_benchmark_client.fetch_data_with_client(remote_settings::Client::new(
            remote_settings::RemoteSettingsConfig {
                server: None,
                bucket_name: None,
                collection_name: "fakespot-suggest-products".to_owned(),
                server_url: None,
            },
        )?)?;
        Ok(new_benchmark_client)
    }

    fn fetch_data_with_client(&mut self, client: remote_settings::Client) -> Result<()> {
        let response = client.get_records()?;
        for r in &response.records {
            if let Some(a) = &r.attachment {
                self.attachments
                    .insert(a.location.clone(), client.get_attachment(&a.location)?);
            }
        }
        self.records.extend(response.records);
        Ok(())
    }
}

impl rs::Client for RemoteSettingsBenchmarkClient {
    fn get_records(&self, request: rs::RecordRequest) -> Result<Vec<rs::Record>> {
        self.records
            .iter()
            .filter(|r| {
                r.fields.get("type").and_then(JsonValue::as_str)
                    == Some(request.record_type.as_str())
            })
            .filter(|r| match request.last_modified {
                None => true,
                Some(last_modified) => r.last_modified > last_modified,
            })
            .map(|record| {
                let attachment_data = record
                    .attachment
                    .as_ref()
                    .and_then(|a| self.attachments.get(&a.location).cloned());
                Ok(rs::Record::new(record.clone(), attachment_data))
            })
            .collect()
    }
}
