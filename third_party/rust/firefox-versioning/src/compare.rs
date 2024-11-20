/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::VersionParsingError;
use crate::version::Version;
use serde_json::{json, Value};
use std::convert::TryFrom;

pub type Result<T, E = VersionParsingError> = std::result::Result<T, E>;

pub fn version_compare(args: &[Value]) -> Result<Value> {
    let curr_version = args.first().ok_or_else(|| {
        VersionParsingError::ParseError("current version doesn't exist in jexl transform".into())
    })?;
    let curr_version = curr_version.as_str().ok_or_else(|| {
        VersionParsingError::ParseError("current version in jexl transform is not a string".into())
    })?;
    let min_version = args.get(1).ok_or_else(|| {
        VersionParsingError::ParseError("minimum version doesn't exist in jexl transform".into())
    })?;
    let min_version = min_version.as_str().ok_or_else(|| {
        VersionParsingError::ParseError("minimum version is not a string in jexl transform".into())
    })?;
    let min_version = Version::try_from(min_version)?;
    let curr_version = Version::try_from(curr_version)?;
    Ok(json!(if curr_version > min_version {
        1
    } else if curr_version < min_version {
        -1
    } else {
        0
    }))
}
