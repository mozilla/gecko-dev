/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::Serialize;
use std::{
    io::{self, Write},
    num::NonZeroUsize,
};

/// A quantity of items that can fit into a payload, accounting for
/// serialization, encryption, and Base64-encoding overhead.
pub enum Fit {
    /// All items can fit into the payload.
    All,

    /// Some, but not all, items can fit into the payload without
    /// exceeding the maximum payload size.
    Some(NonZeroUsize),

    /// The maximum payload size is too small to hold any items.
    None,

    /// The serialized size of the items couldn't be determined because of
    /// a serialization error.
    Err(serde_json::Error),
}

impl Fit {
    /// If `self` is [`Fit::Some`], returns the number of items that can fit
    /// into the payload without exceeding its maximum size. Otherwise,
    /// returns `None`.
    #[inline]
    pub fn as_some(&self) -> Option<NonZeroUsize> {
        match self {
            Fit::Some(count) => Some(*count),
            _ => None,
        }
    }
}

/// A writer that counts the number of bytes it's asked to write, and discards
/// the data. Used to compute the serialized size of an item.
#[derive(Clone, Copy, Default)]
struct ByteCountWriter(usize);

impl ByteCountWriter {
    #[inline]
    pub fn count(self) -> usize {
        self.0
    }
}

impl Write for ByteCountWriter {
    #[inline]
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.0 += buf.len();
        Ok(buf.len())
    }

    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

/// Returns the size of the given value, in bytes, when serialized to JSON.
pub fn compute_serialized_size<T: Serialize + ?Sized>(value: &T) -> serde_json::Result<usize> {
    let mut w = ByteCountWriter::default();
    serde_json::to_writer(&mut w, value)?;
    Ok(w.count())
}

/// Calculates the maximum number of items that can fit within
/// `max_payload_size` when serialized to JSON.
pub fn try_fit_items<T: Serialize>(items: &[T], max_payload_size: usize) -> Fit {
    let size = match compute_serialized_size(&items) {
        Ok(size) => size,
        Err(e) => return Fit::Err(e),
    };
    // See bug 535326 comment 8 for an explanation of the estimation
    let max_serialized_size = match ((max_payload_size / 4) * 3).checked_sub(1500) {
        Some(max_serialized_size) => max_serialized_size,
        None => return Fit::None,
    };
    if size > max_serialized_size {
        // Estimate a little more than the direct fraction to maximize packing
        let mut cutoff = (items.len() * max_serialized_size - 1) / size + 1;
        // Keep dropping off the last entry until the data fits.
        while cutoff > 0 {
            let size = match compute_serialized_size(&items[..cutoff]) {
                Ok(size) => size,
                Err(e) => return Fit::Err(e),
            };
            if size <= max_serialized_size {
                break;
            }
            cutoff -= 1;
        }
        match NonZeroUsize::new(cutoff) {
            Some(count) => Fit::Some(count),
            None => Fit::None,
        }
    } else {
        Fit::All
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use serde_derive::Serialize;

    #[derive(Serialize)]
    struct CommandRecord {
        #[serde(rename = "command")]
        name: &'static str,
        #[serde(default)]
        args: &'static [Option<&'static str>],
        #[serde(default, rename = "flowID", skip_serializing_if = "Option::is_none")]
        flow_id: Option<&'static str>,
    }

    const COMMANDS: &[CommandRecord] = &[
        CommandRecord {
            name: "wipeEngine",
            args: &[Some("bookmarks")],
            flow_id: Some("flow"),
        },
        CommandRecord {
            name: "resetEngine",
            args: &[Some("history")],
            flow_id: Some("flow"),
        },
        CommandRecord {
            name: "logout",
            args: &[],
            flow_id: None,
        },
    ];

    #[test]
    fn test_compute_serialized_size() {
        assert_eq!(compute_serialized_size(&1).unwrap(), 1);
        assert_eq!(compute_serialized_size(&"hi").unwrap(), 4);
        assert_eq!(
            compute_serialized_size(&["hi", "hello", "bye"]).unwrap(),
            20
        );

        let sizes = COMMANDS
            .iter()
            .map(|c| compute_serialized_size(c).unwrap())
            .collect::<Vec<_>>();
        assert_eq!(sizes, &[61, 60, 30]);
    }

    #[test]
    fn test_try_fit_items() {
        // 4096 bytes is enough to fit all three commands.
        assert!(matches!(try_fit_items(COMMANDS, 4096), Fit::All));

        // `logout` won't fit within 2168 bytes.
        assert_eq!(try_fit_items(COMMANDS, 2168).as_some().unwrap().get(), 2);

        // `resetEngine` won't fit within 2084 bytes.
        assert_eq!(try_fit_items(COMMANDS, 2084).as_some().unwrap().get(), 1);

        // `wipeEngine` won't fit at all.
        assert!(matches!(try_fit_items(COMMANDS, 1024), Fit::None));
    }
}
