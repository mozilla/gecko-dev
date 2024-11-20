/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::RemoteSettingsResponse;
use std::collections::HashSet;

/// Merge a cached RemoteSettingsResponse and a newly downloaded one to get a merged response
///
/// cached is a previously downloaded remote settings response (possibly run through merge_cache_and_response).
/// new is a newly downloaded remote settings response (with `_expected` set to the last_modified
/// time of the cached response).
///
/// This will merge the records from both responses, handle deletions/tombstones, and return a
/// response that has:
///   - The newest `last_modified_date`
///   - A record list containing the newest version of all live records.  Deleted records will not
///     be present in this list.
///
/// If everything is working properly, the returned value will exactly match what the server would
/// have returned if there was no `_expected` param.
pub fn merge_cache_and_response(
    cached: RemoteSettingsResponse,
    new: RemoteSettingsResponse,
) -> RemoteSettingsResponse {
    let new_record_ids = new
        .records
        .iter()
        .map(|r| r.id.as_str())
        .collect::<HashSet<&str>>();
    // Start with any cached records that don't appear in new.
    let mut records = cached
        .records
        .into_iter()
        .filter(|r| !new_record_ids.contains(r.id.as_str()))
        // deleted should always be false, check it just in case
        .filter(|r| !r.deleted)
        .collect::<Vec<_>>();
    // Add all (non-deleted) records from new
    records.extend(new.records.into_iter().filter(|r| !r.deleted));

    RemoteSettingsResponse {
        last_modified: new.last_modified,
        records,
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{RemoteSettingsRecord, RsJsonObject};

    // Quick way to generate the fields data for our mock records
    fn fields(data: &str) -> RsJsonObject {
        let mut map = serde_json::Map::new();
        map.insert("data".into(), data.into());
        map
    }

    #[test]
    fn test_combine_cache_and_response() {
        let cached_response = RemoteSettingsResponse {
            last_modified: 1000,
            records: vec![
                RemoteSettingsRecord {
                    id: "a".into(),
                    last_modified: 100,
                    deleted: false,
                    attachment: None,
                    fields: fields("a"),
                },
                RemoteSettingsRecord {
                    id: "b".into(),
                    last_modified: 200,
                    deleted: false,
                    attachment: None,
                    fields: fields("b"),
                },
                RemoteSettingsRecord {
                    id: "c".into(),
                    last_modified: 300,
                    deleted: false,
                    attachment: None,
                    fields: fields("c"),
                },
            ],
        };
        let new_response = RemoteSettingsResponse {
            last_modified: 2000,
            records: vec![
                // d is new
                RemoteSettingsRecord {
                    id: "d".into(),
                    last_modified: 1300,
                    deleted: false,
                    attachment: None,
                    fields: fields("d"),
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
                    fields: fields("a-with-new-data"),
                },
                // c was not modified, so it's not present in the new response
            ],
        };
        let mut merged = merge_cache_and_response(cached_response, new_response);
        // Sort the records to make the assertion easier
        merged.records.sort_by_key(|r| r.id.clone());
        assert_eq!(
            merged,
            RemoteSettingsResponse {
                last_modified: 2000,
                records: vec![
                    // a was updated
                    RemoteSettingsRecord {
                        id: "a".into(),
                        last_modified: 1100,
                        deleted: false,
                        attachment: None,
                        fields: fields("a-with-new-data"),
                    },
                    RemoteSettingsRecord {
                        id: "c".into(),
                        last_modified: 300,
                        deleted: false,
                        attachment: None,
                        fields: fields("c"),
                    },
                    RemoteSettingsRecord {
                        id: "d".into(),
                        last_modified: 1300,
                        deleted: false,
                        attachment: None,
                        fields: fields("d"),
                    },
                ],
            }
        );
    }
}
