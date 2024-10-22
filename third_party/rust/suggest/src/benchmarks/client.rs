/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

use crate::{db::SuggestDao, error::Error, rs, Result};

/// Remotes settings client for benchmarking
///
/// This fetches all data in `new`, then implements [rs::Client] by returning the local data.
/// Construct this one before the benchmark is run, then clone it as input for the benchmark.  This
/// ensures that network time does not count towards the benchmark time.
#[derive(Clone, Default)]
pub struct RemoteSettingsBenchmarkClient {
    records: Vec<rs::Record>,
    attachments: HashMap<String, Vec<u8>>,
}

impl RemoteSettingsBenchmarkClient {
    pub fn new() -> Result<Self> {
        let mut new_benchmark_client = Self::default();
        new_benchmark_client.fetch_data_with_client(
            remote_settings::RemoteSettings::new(remote_settings::RemoteSettingsConfig {
                server: None,
                bucket_name: None,
                collection_name: "quicksuggest".to_owned(),
                server_url: None,
            })?,
            rs::Collection::Quicksuggest,
        )?;
        new_benchmark_client.fetch_data_with_client(
            remote_settings::RemoteSettings::new(remote_settings::RemoteSettingsConfig {
                server: None,
                bucket_name: None,
                collection_name: "fakespot-suggest-products".to_owned(),
                server_url: None,
            })?,
            rs::Collection::Fakespot,
        )?;
        Ok(new_benchmark_client)
    }

    fn fetch_data_with_client(
        &mut self,
        client: remote_settings::RemoteSettings,
        collection: rs::Collection,
    ) -> Result<()> {
        let response = client.get_records()?;
        for r in &response.records {
            if let Some(a) = &r.attachment {
                self.attachments
                    .insert(a.location.clone(), client.get_attachment(&a.location)?);
            }
        }
        self.records.extend(
            response
                .records
                .into_iter()
                .filter_map(|r| rs::Record::new(r, collection).ok()),
        );
        Ok(())
    }

    pub fn attachment_size_by_record_type(&self) -> Vec<(rs::SuggestRecordType, usize)> {
        let mut sizes = HashMap::<rs::SuggestRecordType, usize>::new();
        for record in self.records.iter() {
            let record_type = rs::SuggestRecordType::from(&record.payload);
            if let Some(a) = &record.attachment {
                if let Some(attachment) = self.attachments.get(&a.location) {
                    sizes
                        .entry(record_type)
                        .and_modify(|size| *size += attachment.len())
                        .or_insert(attachment.len());
                }
            }
        }
        let mut sizes_vec: Vec<_> = sizes.into_iter().collect();
        sizes_vec.sort_by_key(|(_, size)| *size);
        sizes_vec.reverse();
        sizes_vec
    }

    pub fn total_attachment_size(&self) -> usize {
        self.attachments.values().map(|a| a.len()).sum()
    }
}

impl rs::Client for RemoteSettingsBenchmarkClient {
    fn get_records(
        &self,
        collection: rs::Collection,
        _db: &mut SuggestDao,
    ) -> Result<Vec<rs::Record>> {
        Ok(self
            .records
            .iter()
            .filter(|r| r.collection == collection)
            .cloned()
            .collect())
    }

    fn download_attachment(&self, record: &rs::Record) -> Result<Vec<u8>> {
        match &record.attachment {
            Some(a) => match self.attachments.get(&a.location) {
                Some(data) => Ok(data.clone()),
                None => Err(Error::MissingAttachment(record.id.to_string())),
            },
            None => Err(Error::MissingAttachment(record.id.to_string())),
        }
    }
}
