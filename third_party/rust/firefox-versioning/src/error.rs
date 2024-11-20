/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// The `VersioningError` indicates that a Firefox Version, being passed down as a `String`,
/// cannot be parsed into a `Version`.
///
/// It can either be caused by a non-ASCII character or a integer overflow.
#[derive(Debug, PartialEq, thiserror::Error)]
pub enum VersionParsingError {
    /// Indicates that a number overflowed.
    #[error("Overflow Error: {0}")]
    Overflow(String),
    /// Indicates a general parsing error.
    #[error("Parsing Error: {0}")]
    ParseError(String),
}
