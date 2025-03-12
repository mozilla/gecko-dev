/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

use remote_settings::Attachment;
use serde_json::json;
use serde_json::Value as JsonValue;

use crate::{
    error::Error,
    rs::{Client, Collection, Record, SuggestRecordId, SuggestRecordType},
    testing::JsonExt,
    Result,
};

/// Mock remote settings client
///
/// MockRemoteSettingsClient uses the builder pattern for its API: most methods input `self` and
/// return a modified version of it.
pub struct MockRemoteSettingsClient {
    pub records: Vec<Record>,
    pub attachments: HashMap<String, Vec<u8>>,
    pub last_modified_timestamp: u64,
}

impl Default for MockRemoteSettingsClient {
    fn default() -> Self {
        Self {
            records: Vec::new(),
            attachments: HashMap::new(),
            last_modified_timestamp: 100,
        }
    }
}

impl MockRemoteSettingsClient {
    // Consuming Builder API, this is best for constructing the initial client
    pub fn with_record(mut self, record: MockRecord) -> Self {
        self.add_record(record);
        self
    }

    // Non-Consuming Builder API, this is best for updating an existing client

    /// Add a record to the mock data
    pub fn add_record(&mut self, mock_record: MockRecord) -> &mut Self {
        self.insert_attachment(&mock_record);
        self.records.push(self.record_from_mock(mock_record));
        self
    }

    // Update API, these use the &mut builder pattern, since they're used with already built
    // clients

    /// Update a record, storing a new payload and bumping the modified time
    pub fn update_record(&mut self, mock_record: MockRecord) -> &mut Self {
        let index = self
            .records
            .iter()
            .position(|r| mock_record.matches_record(r))
            .unwrap_or_else(|| panic!("update_record: {} not found", mock_record.qualified_id()));

        self.insert_attachment(&mock_record);

        let mut record = self.record_from_mock(mock_record);
        record.last_modified += 1;
        self.records.splice(index..=index, std::iter::once(record));

        self
    }

    /// Delete a record and its attachment
    pub fn delete_record(&mut self, mock_record: MockRecord) -> &mut Self {
        let index = self
            .records
            .iter()
            .position(|r| mock_record.matches_record(r))
            .unwrap_or_else(|| panic!("delete_record: {} not found", mock_record.qualified_id()));
        self.records.remove(index);
        self.attachments.remove(&mock_record.qualified_id());
        self
    }

    pub fn insert_attachment(&mut self, mock_record: &MockRecord) {
        if let Some(bytes) = mock_record.attachment.as_ref().map(|a| match a {
            MockAttachment::Json(items) => serde_json::to_vec(&items).unwrap_or_else(|_| {
                panic!(
                    "error serializing attachment data: {}",
                    mock_record.qualified_id()
                )
            }),
            MockAttachment::Icon(icon) => icon.data.as_bytes().to_vec(),
        }) {
            self.attachments.insert(mock_record.qualified_id(), bytes);
        }
    }

    fn record_from_mock(&self, mock_record: MockRecord) -> Record {
        let mut record: Record = mock_record.into();
        record.last_modified = self.last_modified_timestamp;
        record
    }
}

impl Client for MockRemoteSettingsClient {
    fn get_records(&self, collection: Collection) -> Result<Vec<Record>> {
        Ok(self
            .records
            .iter()
            .filter(|r| collection == r.collection)
            .cloned()
            .collect())
    }

    fn download_attachment(&self, record: &Record) -> Result<Vec<u8>> {
        match &record.attachment {
            None => Err(Error::MissingAttachment(record.id.to_string())),
            Some(a) => Ok(self
                .attachments
                .get(&a.location)
                .expect("Attachment not in hash map")
                .clone()),
        }
    }
}

pub struct MockRecord {
    pub collection: Collection,
    pub record_type: SuggestRecordType,
    pub id: String,
    pub inline_data: Option<JsonValue>,
    pub attachment: Option<MockAttachment>,
}

impl MockRecord {
    pub fn qualified_id(&self) -> String {
        format!("{}:{}", self.collection.name(), self.id)
    }

    fn matches_record(&self, record: &Record) -> bool {
        self.collection == record.collection && self.id.as_str() == record.id.as_str()
    }
}

impl From<MockRecord> for Record {
    fn from(mock_record: MockRecord) -> Self {
        let attachment = mock_record.attachment.as_ref().map(|a| match a {
            MockAttachment::Json(_) => Attachment {
                filename: mock_record.id.to_string(),
                location: mock_record.qualified_id(),
                mimetype: "application/json".into(),
                hash: "".into(),
                size: 0,
            },
            MockAttachment::Icon(icon) => Attachment {
                filename: mock_record.id.to_string(),
                location: mock_record.qualified_id(),
                mimetype: icon.mimetype.to_string(),
                hash: "".into(),
                size: 0,
            },
        });

        Self {
            id: SuggestRecordId::new(mock_record.id),
            collection: mock_record.collection,
            last_modified: 0,
            payload: serde_json::from_value(
                json!({
                    "type": mock_record.record_type.as_str(),
                })
                .merge(mock_record.inline_data.unwrap_or(json!({}))),
            )
            .unwrap(),
            attachment,
        }
    }
}

pub enum MockAttachment {
    Json(JsonValue),
    Icon(MockIcon),
}

pub struct MockIcon {
    pub id: &'static str,
    pub data: &'static str,
    pub mimetype: &'static str,
}
