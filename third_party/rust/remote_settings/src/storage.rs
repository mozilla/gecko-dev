/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{Attachment, RemoteSettingsRecord, Result};
use camino::Utf8PathBuf;
use rusqlite::{params, Connection, OptionalExtension, Transaction};
use serde_json;
use sha2::{Digest, Sha256};

/// Internal storage type
///
/// This will store downloaded records/attachments in a SQLite database.  Nothing is implemented
/// yet other than the initial API.
///
/// Most methods input a `collection_url` parameter, is a URL that includes the remote settings
/// server, bucket, and collection. If the `collection_url` for a get method does not match the one
/// for a set method, then this means the application has switched their remote settings config and
/// [Storage] should pretend like nothing is stored in the database.
///
/// The reason for this is the [crate::RemoteSettingsService::update_config] method.  If a consumer
/// passes a new server or bucket to `update_config`, we don't want to be using cached data from
/// the previous config.
pub struct Storage {
    conn: Connection,
}

impl Storage {
    pub fn new(path: Utf8PathBuf) -> Result<Self> {
        let conn = Connection::open(path)?;
        let storage = Self { conn };
        storage.initialize_database()?;

        Ok(storage)
    }

    // Create the different tables for records and attachements for every new sqlite path
    fn initialize_database(&self) -> Result<()> {
        self.conn.execute_batch(
            "
        CREATE TABLE IF NOT EXISTS records (
            id TEXT PRIMARY KEY,
            collection_url TEXT NOT NULL,
            data BLOB NOT NULL
        );
       CREATE TABLE IF NOT EXISTS attachments (
            id TEXT PRIMARY KEY,
            collection_url TEXT NOT NULL,
            data BLOB NOT NULL
        );
        CREATE TABLE IF NOT EXISTS collection_metadata (
            collection_url TEXT PRIMARY KEY,
            last_modified INTEGER,
            fetched BOOLEAN
        );
        ",
        )?;
        Ok(())
    }

    /// Get the last modified timestamp for the stored records
    ///
    /// Returns None if no records are stored or if `collection_url` does not match the
    /// last `collection_url` passed to `set_records` / `merge_records`
    pub fn get_last_modified_timestamp(&self, collection_url: &str) -> Result<Option<u64>> {
        let mut stmt = self
            .conn
            .prepare("SELECT last_modified FROM collection_metadata WHERE collection_url = ?")?;
        let result: Option<u64> = stmt
            .query_row((collection_url,), |row| row.get(0))
            .optional()?;
        Ok(result)
    }

    /// Get cached records for this collection
    ///
    /// Returns None if no records are stored or if `collection_url` does not match the `collection_url` passed
    /// to `set_records`.
    pub fn get_records(
        &mut self,
        collection_url: &str,
    ) -> Result<Option<Vec<RemoteSettingsRecord>>> {
        let tx = self.conn.transaction()?;

        let fetched: Option<bool> = tx
            .prepare("SELECT fetched FROM collection_metadata WHERE collection_url = ?")?
            .query_row(params![collection_url], |row| row.get(0))
            .optional()?;

        let result = match fetched {
            Some(true) => {
                // If fetched before, get the records from the records table
                let records: Vec<RemoteSettingsRecord> = tx
                    .prepare("SELECT data FROM records WHERE collection_url = ?")?
                    .query_map(params![collection_url], |row| row.get::<_, Vec<u8>>(0))?
                    .map(|data| serde_json::from_slice(&data.unwrap()).unwrap())
                    .collect();

                Ok(Some(records))
            }
            _ => Ok(None),
        };

        tx.commit()?;
        result
    }

    /// Get cached attachment data
    ///
    /// This returns the last attachment data sent to [Self::set_attachment].
    ///
    /// Returns None if no attachment data is stored or if `collection_url` does not match the `collection_url`
    /// passed to `set_attachment`.
    pub fn get_attachment(
        &self,
        collection_url: &str,
        metadata: Attachment,
    ) -> Result<Option<Vec<u8>>> {
        let mut stmt = self
            .conn
            .prepare("SELECT data FROM attachments WHERE id = ? AND collection_url = ?")?;

        if let Some(data) = stmt
            .query_row((metadata.location, collection_url), |row| {
                row.get::<_, Vec<u8>>(0)
            })
            .optional()?
        {
            // Return None if data doesn't match expected metadata
            if data.len() as u64 != metadata.size {
                return Ok(None);
            }
            let hash = format!("{:x}", Sha256::digest(&data));
            if hash != metadata.hash {
                return Ok(None);
            }
            Ok(Some(data))
        } else {
            Ok(None)
        }
    }

    /// Set the list of records stored in the database, clearing out any previously stored records
    pub fn set_records(
        &mut self,
        collection_url: &str,
        records: &[RemoteSettingsRecord],
    ) -> Result<()> {
        let tx = self.conn.transaction()?;

        tx.execute("DELETE FROM records", [])?;
        tx.execute("DELETE FROM collection_metadata", [])?;
        let max_last_modified = Self::update_record_rows(&tx, collection_url, records)?;
        Self::update_collection_metadata(&tx, collection_url, max_last_modified)?;
        tx.commit()?;
        Ok(())
    }

    /// Merge new records with records stored in the database
    ///
    /// Records with `deleted=false` will be inserted into the DB, replacing any previously stored
    /// records with the same ID. Records with `deleted=true` will be removed.
    pub fn merge_records(
        &mut self,
        collection_url: &str,
        records: &[RemoteSettingsRecord],
    ) -> Result<()> {
        let tx = self.conn.transaction()?;

        // Delete ALL existing records and metadata for with different collection_urls.
        //
        // This way, if a user (probably QA) switches the remote settings server in the middle of a
        // browser sessions, we'll delete the stale data from the previous server.
        tx.execute(
            "DELETE FROM records where collection_url <> ?",
            [collection_url],
        )?;
        tx.execute(
            "DELETE FROM collection_metadata where collection_url <> ?",
            [collection_url],
        )?;
        let max_last_modified = Self::update_record_rows(&tx, collection_url, records)?;
        Self::update_collection_metadata(&tx, collection_url, max_last_modified)?;
        tx.commit()?;
        Ok(())
    }

    /// Insert/remove/update rows in the records table based on a records list
    ///
    /// Returns the max last modified record from the list
    fn update_record_rows(
        tx: &Transaction<'_>,
        collection_url: &str,
        records: &[RemoteSettingsRecord],
    ) -> Result<u64> {
        // Find the max last_modified time while inserting records
        let mut max_last_modified = 0;
        {
            let mut insert_stmt = tx.prepare(
                "INSERT OR REPLACE INTO records (id, collection_url, data) VALUES (?, ?, ?)",
            )?;
            let mut delete_stmt = tx.prepare("DELETE FROM records WHERE id=?")?;
            for record in records {
                if record.deleted {
                    delete_stmt.execute(params![&record.id])?;
                } else {
                    max_last_modified = max_last_modified.max(record.last_modified);
                    let data = serde_json::to_vec(&record)?;
                    insert_stmt.execute(params![record.id, collection_url, data])?;
                }
            }
        }
        Ok(max_last_modified)
    }

    /// Update the collection metadata after setting/merging records
    fn update_collection_metadata(
        tx: &Transaction<'_>,
        collection_url: &str,
        last_modified: u64,
    ) -> Result<()> {
        // Update the metadata
        let fetched = true;
        tx.execute(
            "INSERT OR REPLACE INTO collection_metadata (collection_url, last_modified, fetched) VALUES (?, ?, ?)",
            (collection_url, last_modified, fetched),
        )?;
        Ok(())
    }

    /// Set the attachment data stored in the database, clearing out any previously stored data
    pub fn set_attachment(
        &mut self,
        collection_url: &str,
        location: &str,
        attachment: &[u8],
    ) -> Result<()> {
        let tx = self.conn.transaction()?;

        // Delete ALL existing attachments for every collection_url
        tx.execute(
            "DELETE FROM attachments WHERE collection_url != ?",
            params![collection_url],
        )?;

        tx.execute(
            "INSERT OR REPLACE INTO ATTACHMENTS \
            (id, collection_url, data) \
            VALUES (?, ?, ?)",
            params![location, collection_url, attachment,],
        )?;

        tx.commit()?;

        Ok(())
    }

    /// Empty out all cached values and start from scratch.  This is called when
    /// RemoteSettingsService::update_config() is called, since that could change the remote
    /// settings server which would invalidate all cached data.
    pub fn empty(&mut self) -> Result<()> {
        let tx = self.conn.transaction()?;
        tx.execute("DELETE FROM records", [])?;
        tx.execute("DELETE FROM attachments", [])?;
        tx.execute("DELETE FROM collection_metadata", [])?;
        tx.commit()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::Storage;
    use crate::{Attachment, RemoteSettingsRecord, Result, RsJsonObject};
    use sha2::{Digest, Sha256};

    #[test]
    fn test_storage_set_and_get_records() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";
        let records = vec![
            RemoteSettingsRecord {
                id: "1".to_string(),
                last_modified: 100,
                deleted: false,
                attachment: None,
                fields: serde_json::json!({"key": "value1"})
                    .as_object()
                    .unwrap()
                    .clone(),
            },
            RemoteSettingsRecord {
                id: "2".to_string(),
                last_modified: 200,
                deleted: false,
                attachment: None,
                fields: serde_json::json!({"key": "value2"})
                    .as_object()
                    .unwrap()
                    .clone(),
            },
        ];

        // Set records
        storage.set_records(collection_url, &records)?;

        // Get records
        let fetched_records = storage.get_records(collection_url)?;
        assert!(fetched_records.is_some());
        let fetched_records = fetched_records.unwrap();
        assert_eq!(fetched_records.len(), 2);
        assert_eq!(fetched_records, records);

        assert_eq!(fetched_records[0].fields["key"], "value1");

        // Get last modified timestamp
        let last_modified = storage.get_last_modified_timestamp(collection_url)?;
        assert_eq!(last_modified, Some(200));

        Ok(())
    }

    #[test]
    fn test_storage_get_records_none() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";

        // Get records when none are set
        let fetched_records = storage.get_records(collection_url)?;
        assert!(fetched_records.is_none());

        // Get last modified timestamp when no records
        let last_modified = storage.get_last_modified_timestamp(collection_url)?;
        assert!(last_modified.is_none());

        Ok(())
    }

    #[test]
    fn test_storage_get_records_empty() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";

        // Set empty records
        storage.set_records(collection_url, &Vec::<RemoteSettingsRecord>::default())?;

        // Get records
        let fetched_records = storage.get_records(collection_url)?;
        assert_eq!(fetched_records, Some(Vec::new()));

        // Get last modified timestamp when no records
        let last_modified = storage.get_last_modified_timestamp(collection_url)?;
        assert_eq!(last_modified, Some(0));

        Ok(())
    }

    #[test]
    fn test_storage_set_and_get_attachment() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let attachment = &[0x18, 0x64];
        let collection_url = "https://example.com/api";
        let attachment_metadata = Attachment {
            filename: "abc".to_string(),
            mimetype: "application/json".to_string(),
            location: "tmp".to_string(),
            hash: format!("{:x}", Sha256::digest(attachment)),
            size: attachment.len() as u64,
        };

        // Store attachment
        storage.set_attachment(collection_url, &attachment_metadata.location, attachment)?;

        // Get attachment
        let fetched_attachment = storage.get_attachment(collection_url, attachment_metadata)?;
        assert!(fetched_attachment.is_some());
        let fetched_attachment = fetched_attachment.unwrap();
        assert_eq!(fetched_attachment, attachment);

        Ok(())
    }

    #[test]
    fn test_storage_set_and_replace_attachment() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";

        let attachment_1 = &[0x18, 0x64];
        let attachment_2 = &[0x12, 0x48];

        let attachment_metadata_1 = Attachment {
            filename: "abc".to_string(),
            mimetype: "application/json".to_string(),
            location: "tmp".to_string(),
            hash: format!("{:x}", Sha256::digest(attachment_1)),
            size: attachment_1.len() as u64,
        };

        let attachment_metadata_2 = Attachment {
            filename: "def".to_string(),
            mimetype: "application/json".to_string(),
            location: "tmp".to_string(),
            hash: format!("{:x}", Sha256::digest(attachment_2)),
            size: attachment_2.len() as u64,
        };

        // Store first attachment
        storage.set_attachment(
            collection_url,
            &attachment_metadata_1.location,
            attachment_1,
        )?;

        // Replace attachment with new data
        storage.set_attachment(
            collection_url,
            &attachment_metadata_2.location,
            attachment_2,
        )?;

        // Get attachment
        let fetched_attachment = storage.get_attachment(collection_url, attachment_metadata_2)?;
        assert!(fetched_attachment.is_some());
        let fetched_attachment = fetched_attachment.unwrap();
        assert_eq!(fetched_attachment, attachment_2);

        Ok(())
    }

    #[test]
    fn test_storage_set_attachment_delete_others() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url_1 = "https://example.com/api1";
        let collection_url_2 = "https://example.com/api2";

        let attachment_1 = &[0x18, 0x64];
        let attachment_2 = &[0x12, 0x48];

        let attachment_metadata_1 = Attachment {
            filename: "abc".to_string(),
            mimetype: "application/json".to_string(),
            location: "first_tmp".to_string(),
            hash: format!("{:x}", Sha256::digest(attachment_1)),
            size: attachment_1.len() as u64,
        };

        let attachment_metadata_2 = Attachment {
            filename: "def".to_string(),
            mimetype: "application/json".to_string(),
            location: "second_tmp".to_string(),
            hash: format!("{:x}", Sha256::digest(attachment_2)),
            size: attachment_2.len() as u64,
        };

        // Set attachments for two different collections
        storage.set_attachment(
            collection_url_1,
            &attachment_metadata_1.location,
            attachment_1,
        )?;
        storage.set_attachment(
            collection_url_2,
            &attachment_metadata_2.location,
            attachment_2,
        )?;

        // Verify that only the attachment for the second collection remains
        let fetched_attachment_1 =
            storage.get_attachment(collection_url_1, attachment_metadata_1)?;
        assert!(fetched_attachment_1.is_none());

        let fetched_attachment_2 =
            storage.get_attachment(collection_url_2, attachment_metadata_2)?;
        assert!(fetched_attachment_2.is_some());
        let fetched_attachment_2 = fetched_attachment_2.unwrap();
        assert_eq!(fetched_attachment_2, attachment_2);

        Ok(())
    }

    #[test]
    fn test_storage_get_attachment_not_found() -> Result<()> {
        let storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";
        let metadata = Attachment::default();

        // Get attachment that doesn't exist
        let fetched_attachment = storage.get_attachment(collection_url, metadata)?;
        assert!(fetched_attachment.is_none());

        Ok(())
    }

    #[test]
    fn test_storage_empty() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";
        let attachment = &[0x18, 0x64];

        let records = vec![
            RemoteSettingsRecord {
                id: "1".to_string(),
                last_modified: 100,
                deleted: false,
                attachment: None,
                fields: serde_json::json!({"key": "value1"})
                    .as_object()
                    .unwrap()
                    .clone(),
            },
            RemoteSettingsRecord {
                id: "2".to_string(),
                last_modified: 200,
                deleted: false,
                attachment: Some(Attachment {
                    filename: "abc".to_string(),
                    mimetype: "application/json".to_string(),
                    location: "tmp".to_string(),
                    hash: format!("{:x}", Sha256::digest(attachment)),
                    size: attachment.len() as u64,
                }),
                fields: serde_json::json!({"key": "value2"})
                    .as_object()
                    .unwrap()
                    .clone(),
            },
        ];

        let metadata = records[1]
            .clone()
            .attachment
            .expect("No attachment metadata for record");

        // Set records and attachment
        storage.set_records(collection_url, &records)?;
        storage.set_attachment(collection_url, &metadata.location, attachment)?;

        // Verify they are stored
        let fetched_records = storage.get_records(collection_url)?;
        assert!(fetched_records.is_some());
        let fetched_attachment = storage.get_attachment(collection_url, metadata.clone())?;
        assert!(fetched_attachment.is_some());

        // Empty the storage
        storage.empty()?;

        // Verify they are deleted
        let fetched_records = storage.get_records(collection_url)?;
        assert!(fetched_records.is_none());
        let fetched_attachment = storage.get_attachment(collection_url, metadata)?;
        assert!(fetched_attachment.is_none());

        Ok(())
    }

    #[test]
    fn test_storage_collection_url_isolation() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url1 = "https://example.com/api1";
        let collection_url2 = "https://example.com/api2";
        let records_collection_url1 = vec![RemoteSettingsRecord {
            id: "1".to_string(),
            last_modified: 100,
            deleted: false,
            attachment: None,
            fields: serde_json::json!({"key": "value1"})
                .as_object()
                .unwrap()
                .clone(),
        }];
        let records_collection_url2 = vec![RemoteSettingsRecord {
            id: "2".to_string(),
            last_modified: 200,
            deleted: false,
            attachment: None,
            fields: serde_json::json!({"key": "value2"})
                .as_object()
                .unwrap()
                .clone(),
        }];

        // Set records for collection_url1
        storage.set_records(collection_url1, &records_collection_url1)?;

        // Verify records for collection_url1
        let fetched_records = storage.get_records(collection_url1)?;
        assert!(fetched_records.is_some());
        let fetched_records = fetched_records.unwrap();
        assert_eq!(fetched_records.len(), 1);
        assert_eq!(fetched_records, records_collection_url1);

        // Set records for collection_url2, which will clear records for all collections
        storage.set_records(collection_url2, &records_collection_url2)?;

        // Verify that records for collection_url1 have been cleared
        let fetched_records = storage.get_records(collection_url1)?;
        assert!(fetched_records.is_none());

        // Verify records for collection_url2 are correctly stored
        let fetched_records = storage.get_records(collection_url2)?;
        assert!(fetched_records.is_some());
        let fetched_records = fetched_records.unwrap();
        assert_eq!(fetched_records.len(), 1);
        assert_eq!(fetched_records, records_collection_url2);

        // Verify last modified timestamps only for collection_url2
        let last_modified1 = storage.get_last_modified_timestamp(collection_url1)?;
        assert_eq!(last_modified1, None);
        let last_modified2 = storage.get_last_modified_timestamp(collection_url2)?;
        assert_eq!(last_modified2, Some(200));

        Ok(())
    }

    #[test]
    fn test_storage_set_records() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";
        let initial_records = vec![RemoteSettingsRecord {
            id: "2".to_string(),
            last_modified: 200,
            deleted: false,
            attachment: None,
            fields: serde_json::json!({"key": "value2"})
                .as_object()
                .unwrap()
                .clone(),
        }];

        // Set initial records
        storage.set_records(collection_url, &initial_records)?;

        // Verify initial records
        let fetched_records = storage.get_records(collection_url)?;
        assert!(fetched_records.is_some());
        assert_eq!(fetched_records.unwrap(), initial_records);

        // Update records
        let updated_records = vec![RemoteSettingsRecord {
            id: "2".to_string(),
            last_modified: 200,
            deleted: false,
            attachment: None,
            fields: serde_json::json!({"key": "value2_updated"})
                .as_object()
                .unwrap()
                .clone(),
        }];
        storage.set_records(collection_url, &updated_records)?;

        // Verify updated records
        let fetched_records = storage.get_records(collection_url)?;
        assert!(fetched_records.is_some());
        assert_eq!(fetched_records.unwrap(), updated_records);

        // Verify last modified timestamp
        let last_modified = storage.get_last_modified_timestamp(collection_url)?;
        assert_eq!(last_modified, Some(200));

        Ok(())
    }

    // Quick way to generate the fields data for our mock records
    fn test_fields(data: &str) -> RsJsonObject {
        let mut map = serde_json::Map::new();
        map.insert("data".into(), data.into());
        map
    }

    #[test]
    fn test_storage_merge_records() -> Result<()> {
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = "https://example.com/api";

        let initial_records = vec![
            RemoteSettingsRecord {
                id: "a".into(),
                last_modified: 100,
                deleted: false,
                attachment: None,
                fields: test_fields("a"),
            },
            RemoteSettingsRecord {
                id: "b".into(),
                last_modified: 200,
                deleted: false,
                attachment: None,
                fields: test_fields("b"),
            },
            RemoteSettingsRecord {
                id: "c".into(),
                last_modified: 300,
                deleted: false,
                attachment: None,
                fields: test_fields("c"),
            },
        ];
        let updated_records = vec![
            // d is new
            RemoteSettingsRecord {
                id: "d".into(),
                last_modified: 1300,
                deleted: false,
                attachment: None,
                fields: test_fields("d"),
            },
            // b was deleted
            RemoteSettingsRecord {
                id: "b".into(),
                last_modified: 1200,
                deleted: true,
                attachment: None,
                fields: RsJsonObject::new(),
            },
            // a was updated
            RemoteSettingsRecord {
                id: "a".into(),
                last_modified: 1100,
                deleted: false,
                attachment: None,
                fields: test_fields("a-with-new-data"),
            },
            // c was not modified, so it's not present in the new response
        ];
        let expected_records = vec![
            // a was updated
            RemoteSettingsRecord {
                id: "a".into(),
                last_modified: 1100,
                deleted: false,
                attachment: None,
                fields: test_fields("a-with-new-data"),
            },
            RemoteSettingsRecord {
                id: "c".into(),
                last_modified: 300,
                deleted: false,
                attachment: None,
                fields: test_fields("c"),
            },
            RemoteSettingsRecord {
                id: "d".into(),
                last_modified: 1300,
                deleted: false,
                attachment: None,
                fields: test_fields("d"),
            },
        ];

        // Set initial records
        storage.set_records(collection_url, &initial_records)?;

        // Verify initial records
        let fetched_records = storage.get_records(collection_url)?.unwrap();
        assert_eq!(fetched_records, initial_records);

        // Update records
        storage.merge_records(collection_url, &updated_records)?;

        // Verify updated records
        let mut fetched_records = storage.get_records(collection_url)?.unwrap();
        fetched_records.sort_by_cached_key(|r| r.id.clone());
        assert_eq!(fetched_records, expected_records);

        // Verify last modified timestamp
        let last_modified = storage.get_last_modified_timestamp(collection_url)?;
        assert_eq!(last_modified, Some(1300));

        Ok(())
    }
}
