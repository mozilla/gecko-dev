/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

use remote_settings::Attachment;
use serde_json::json;
use serde_json::Value as JsonValue;

use crate::{
    db::SuggestDao,
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

fn record_type_for_str(record_type_str: &str) -> SuggestRecordType {
    for record_type in SuggestRecordType::all() {
        if record_type.as_str() == record_type_str {
            return *record_type;
        }
    }
    panic!("Invalid record type string: {record_type_str}");
}

impl MockRemoteSettingsClient {
    // Consuming Builder API, this is best for constructing the initial client
    pub fn with_record(mut self, record_type: &str, record_id: &str, items: JsonValue) -> Self {
        self.add_record(record_type, record_id, items);
        self
    }

    pub fn with_icon(mut self, icon: MockIcon) -> Self {
        self.add_icon(icon);
        self
    }

    pub fn with_record_but_no_attachment(mut self, record_type: &str, record_id: &str) -> Self {
        self.add_record_but_no_attachment(record_type, record_id);
        self
    }

    pub fn with_inline_record(
        mut self,
        record_type: &str,
        record_id: &str,
        inline_data: JsonValue,
    ) -> Self {
        self.add_inline_record(record_type, record_id, inline_data);
        self
    }

    pub fn with_full_record(
        mut self,
        record_type: &str,
        record_id: &str,
        inline_data: Option<JsonValue>,
        items: Option<JsonValue>,
    ) -> Self {
        self.add_full_record(record_type, record_id, inline_data, items);
        self
    }

    // Non-Consuming Builder API, this is best for updating an existing client

    /// Add a record to the mock data
    ///
    /// A single record typically contains multiple items in the attachment data.  Pass all of them
    /// as the `items` param.
    pub fn add_record(
        &mut self,
        record_type: &str,
        record_id: &str,
        items: JsonValue,
    ) -> &mut Self {
        self.add_full_record(record_type, record_id, None, Some(items))
    }

    /// Add a record for an icon to the mock data
    pub fn add_icon(&mut self, icon: MockIcon) -> &mut Self {
        let icon_id = icon.id;
        let record_id = format!("icon-{icon_id}");
        let location = format!("icon-{icon_id}.png");
        self.records.push(Record {
            id: SuggestRecordId::new(record_id.to_string()),
            last_modified: self.last_modified_timestamp,
            collection: Collection::Quicksuggest,
            attachment: Some(Attachment {
                filename: location.clone(),
                mimetype: icon.mimetype.into(),
                hash: "".into(),
                size: 0,
                location: location.clone(),
            }),
            payload: serde_json::from_value(json!({"type": "icon"})).unwrap(),
        });
        self.attachments
            .insert(location, icon.data.as_bytes().to_vec());
        self
    }

    /// Add a record without attachment data
    pub fn add_record_but_no_attachment(
        &mut self,
        record_type: &str,
        record_id: &str,
    ) -> &mut Self {
        self.add_full_record(record_type, record_id, None, None)
    }

    /// Add a record to the mock data, with data stored inline rather than in an attachment
    ///
    /// Use this for record types like weather where the data it stored in the record itself rather
    /// than in an attachment.
    pub fn add_inline_record(
        &mut self,
        record_type: &str,
        record_id: &str,
        inline_data: JsonValue,
    ) -> &mut Self {
        self.add_full_record(record_type, record_id, Some(inline_data), None)
    }

    /// Add a record with optional extra fields stored inline and attachment
    /// items
    pub fn add_full_record(
        &mut self,
        record_type: &str,
        record_id: &str,
        inline_data: Option<JsonValue>,
        items: Option<JsonValue>,
    ) -> &mut Self {
        let location = format!("{record_type}-{record_id}.json");
        self.records.push(Record {
            id: SuggestRecordId::new(record_id.to_string()),
            collection: record_type_for_str(record_type).collection(),
            last_modified: self.last_modified_timestamp,
            payload: serde_json::from_value(
                json!({
                    "type": record_type,
                })
                .merge(inline_data.unwrap_or(json!({}))),
            )
            .unwrap(),
            attachment: items.as_ref().map(|_| Attachment {
                filename: location.clone(),
                mimetype: "application/json".into(),
                hash: "".into(),
                size: 0,
                location: location.clone(),
            }),
        });
        if let Some(i) = items {
            self.attachments.insert(
                location,
                serde_json::to_vec(&i).expect("error serializing attachment data"),
            );
        }
        self
    }

    // Update API, these use the &mut builder pattern, since they're used with already built
    // clients

    /// Update a record, storing a new payload and bumping the modified time
    pub fn update_record(
        &mut self,
        record_type: &str,
        record_id: &str,
        items: JsonValue,
    ) -> &mut Self {
        let record = self
            .records
            .iter_mut()
            .find(|r| r.id.as_str() == record_id)
            .unwrap_or_else(|| panic!("update_record: {record_id} not found"));
        let attachment_data = self
            .attachments
            .get_mut(
                &record
                    .attachment
                    .as_ref()
                    .expect("update_record: no attachment")
                    .location,
            )
            .unwrap_or_else(|| panic!("update_record: attachment not found for {record_id}"));

        record.last_modified += 1;
        record.payload = serde_json::from_value(json!({"type": record_type})).unwrap();
        *attachment_data = serde_json::to_vec(&items).expect("error serializing attachment data");
        self
    }

    /// Update an icon record, storing a new payload and bumping the modified time
    pub fn update_icon(&mut self, icon: MockIcon) -> &mut Self {
        let icon_id = &icon.id;
        let record_id = format!("icon-{icon_id}");
        let record = self
            .records
            .iter_mut()
            .find(|r| r.id.as_str() == record_id)
            .unwrap_or_else(|| panic!("update_icon: {record_id} not found"));
        let attachment_data = self
            .attachments
            .get_mut(
                &record
                    .attachment
                    .as_ref()
                    .expect("update_icon: no attachment")
                    .location,
            )
            .unwrap_or_else(|| panic!("update_icon: attachment not found for {icon_id}"));

        record.last_modified += 1;
        *attachment_data = icon.data.as_bytes().to_vec();
        self
    }

    /// Delete a record and it's attachment
    pub fn delete_record(&mut self, collection: &str, record_id: &str) -> &mut Self {
        let idx = self
            .records
            .iter()
            .position(|r| r.id.as_str() == record_id && r.collection.name() == collection)
            .unwrap_or_else(|| panic!("delete_record: {collection}:{record_id} not found"));
        let deleted = self.records.remove(idx);
        if let Some(a) = deleted.attachment {
            self.attachments.remove(&a.location);
        }
        self
    }

    pub fn delete_icon(&mut self, icon: MockIcon) -> &mut Self {
        self.delete_record("quicksuggest", &format!("icon-{}", icon.id))
    }
}

pub struct MockIcon {
    pub id: &'static str,
    pub data: &'static str,
    pub mimetype: &'static str,
}

impl Client for MockRemoteSettingsClient {
    fn get_records(&self, collection: Collection, _db: &mut SuggestDao) -> Result<Vec<Record>> {
        Ok(self
            .records
            .iter()
            .filter(|r| collection == r.record_type().collection())
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
