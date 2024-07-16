/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::Result;
use payload_support::Fit;
use serde::Serialize;

/// Truncates `list` to fit within `payload_size_max_bytes` when serialized to
/// JSON.
pub fn shrink_to_fit<T: Serialize>(list: &mut Vec<T>, payload_size_max_bytes: usize) -> Result<()> {
    match payload_support::try_fit_items(list, payload_size_max_bytes) {
        Fit::All => {}
        Fit::Some(count) => list.truncate(count.get()),
        Fit::None => list.clear(),
        Fit::Err(e) => Err(e)?,
    };
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::super::record::CommandRecord;
    use super::*;

    #[test]
    fn test_shrink_to_fit() {
        let mut commands = vec![
            CommandRecord {
                name: "wipeEngine".into(),
                args: vec![Some("bookmarks".into())],
                flow_id: Some("flow".into()),
            },
            CommandRecord {
                name: "resetEngine".into(),
                args: vec![Some("history".into())],
                flow_id: Some("flow".into()),
            },
            CommandRecord {
                name: "logout".into(),
                args: Vec::new(),
                flow_id: None,
            },
        ];

        // 4096 bytes is enough to fit all three commands.
        shrink_to_fit(&mut commands, 4096).unwrap();
        assert_eq!(commands.len(), 3);

        // `logout` won't fit within 2168 bytes.
        shrink_to_fit(&mut commands, 2168).unwrap();
        assert_eq!(commands.len(), 2);

        // `resetEngine` won't fit within 2084 bytes.
        shrink_to_fit(&mut commands, 2084).unwrap();
        assert_eq!(commands.len(), 1);

        // `wipeEngine` won't fit at all.
        shrink_to_fit(&mut commands, 1024).unwrap();
        assert!(commands.is_empty());
    }
}
