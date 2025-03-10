/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use core::clone::Clone;

use crate::{RemoteSettingsRecord, Result};
use rc_crypto::contentsignature;
use serde_json::{json, Value};

/// Remove `deleted` and `attachment` fields if they are null.
fn select_record_fields(value: &Value) -> Value {
    match value {
        Value::Object(map) => Value::Object(
            map.iter()
                .filter(|(key, v)| !(*key == "deleted" || (*key == "attachment" && v.is_null())))
                .map(|(key, v)| (key.clone(), v.clone()))
                .collect(),
        ),
        _ => value.clone(), // Return the value as-is if it's not an object
    }
}

/// Serialize collection data into canonical JSON. This must match the server implementation.
fn serialize_data(timestamp: u64, records: &[RemoteSettingsRecord]) -> Result<Vec<u8>> {
    let mut sorted_records = records.to_vec();
    sorted_records.sort_by_cached_key(|r| r.id.clone());
    let serialized = canonical_json::to_string(&json!({
        "data": sorted_records.into_iter().map(|r| select_record_fields(&json!(r))).collect::<Vec<Value>>(),
        "last_modified": timestamp.to_string()
    }))?;
    let data = format!("Content-Signature:\x00{}", serialized);
    Ok(data.as_bytes().to_vec())
}

/// Verify that the timestamp and records match the signature in the metadata.
pub fn verify_signature(
    timestamp: u64,
    records: &[RemoteSettingsRecord],
    signature: &[u8],
    cert_chain_bytes: &[u8],
    epoch_seconds: u64,
    expected_root_hash: &str,
    expected_leaf_cname: &str,
) -> Result<()> {
    let message = serialize_data(timestamp, records)?;
    // Check that certificate chain is valid at specific date time, and
    // that signature matches the input message.
    contentsignature::verify(
        &message,
        signature,
        cert_chain_bytes,
        epoch_seconds,
        expected_root_hash,
        expected_leaf_cname,
    )?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::serialize_data;
    use crate::{Attachment, RemoteSettingsRecord};
    use serde_json::json;

    #[test]
    fn test_records_canonicaljson_serialization() {
        let bytes = serialize_data(
            1337,
            &vec![RemoteSettingsRecord {
                last_modified: 42,
                id: "bonjour".into(),
                deleted: false,
                attachment: None,
                fields: json!({"foo": "bar"}).as_object().unwrap().clone(),
            }],
        )
        .unwrap();
        let s = String::from_utf8(bytes).unwrap();
        assert_eq!(s, "Content-Signature:\u{0}{\"data\":[{\"id\":\"bonjour\",\"last_modified\":42,\"foo\":\"bar\"}],\"last_modified\":\"1337\"}");
    }

    #[test]
    fn test_records_canonicaljson_serialization_with_attachment() {
        let bytes = serialize_data(
            1337,
            &vec![RemoteSettingsRecord {
                last_modified: 42,
                id: "bonjour".into(),
                deleted: true,
                attachment: Some(Attachment {
                    filename: "pix.jpg".into(),
                    mimetype: "image/jpeg".into(),
                    location: "folder/file.jpg".into(),
                    hash: "aabbcc".into(),
                    size: 1234567,
                }),
                fields: json!({}).as_object().unwrap().clone(),
            }],
        )
        .unwrap();
        let s = String::from_utf8(bytes).unwrap();
        assert_eq!(s, "Content-Signature:\0{\"data\":[{\"id\":\"bonjour\",\"last_modified\":42,\"attachment\":{\"filename\":\"pix.jpg\",\"mimetype\":\"image/jpeg\",\"location\":\"folder/file.jpg\",\"hash\":\"aabbcc\",\"size\":1234567}}],\"last_modified\":\"1337\"}");
    }
}
