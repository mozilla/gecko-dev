/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! ## Firefox Version Comparison
//! This module was ported from the Firefox Desktop implementation. You can find the Desktop implementation
//! in [this C++ file](https://searchfox.org/mozilla-central/rev/468a65168dd0bc3c7d602211a566c16e66416cce/xpcom/base/nsVersionComparator.cpp)
//! There's also some more documentation in the [IDL](https://searchfox.org/mozilla-central/rev/468a65168dd0bc3c7d602211a566c16e66416cce/xpcom/base/nsIVersionComparator.idl#9-31)
//!
//! ## How versioning works
//! This module defines one main struct, the [`Version`] struct. A version is represented by a list of
//! dot separated **Version Parts**s. When comparing two versions, we compare each version part in order.
//! If one of the versions has a version part, but the other has run out (i.e we have reached the end of the list of version parts)
//! we compare the existing version part with the default version part, which is the `0`. For example,
//! `1.0` is equivalent to `1.0.0.0`.
//!
//! For information what version parts are composed of, and how they are compared, read the [next section](#the-version-part).
//!
//! ### Example Versions
//! The following are all valid versions:
//! - `1` (one version part, representing the `1`)
//! - `` (one version part, representing the empty string, which is equal to `0`)
//! - `12+` (one version part, representing `12+` which is equal to `13pre`)
//! - `98.1` (two version parts, one representing `98` and another `1`)
//! - `98.2pre1.0-beta` (three version parts, one for `98`, one for `2pre1` and one for `0-beta`)
//!
//!
//! ## The Version Part
//! A version part is made from 4 elements that directly follow each other:
//! - `num_a`: A 32-bit base-10 formatted number that is at the start of the part
//! - `str_b`: A non-numeric ascii-encoded string that starts after `num_a`
//! - `num_c`: Another 32-bit base-10 formatted number that follows `str_b`
//! - `extra_d`: The rest of the version part as an ascii-encoded string
//!
//! When two version parts are compared, each of `num_a`, `str_b`, `num_c` and `extra_d` are compared
//! in order. `num_a` and `num_c` are compared by normal integer comparison, `str_b` and `extra_b` are compared
//! by normal byte string comparison.
//!
//! ### Special values and cases
//! There two special characters that can be used in version parts:
//! 1. The `*`. This can be used to represent the whole version part. If used, it will set the `num_a` to be
//!     the maximum value possible ([`i32::MAX`]). This can only be used as the whole version part string. It will parsed
//!     normally as the `*` ascii character if it is preceded or followed by any other characters.
//! 1. The `+`. This can be used as the `str_b`. Whenever a `+` is used as a `str_b`, it increments the `num_a` by 1 and sets
//!     the `str_b` to be equal to `pre`. For example, `2+` is the same as `3pre`
//! 1. An empty `str_b` is always **greater** than a `str_b` with a value. For example, `93` > `93pre`
//!
//! ## Example version comparisons
//! The following comparisons are taken directly from [the brief documentation in Mozilla-Central](https://searchfox.org/mozilla-central/rev/468a65168dd0bc3c7d602211a566c16e66416cce/xpcom/base/nsIVersionComparator.idl#9-31)
//! ```
//! use firefox_versioning::version::Version;
//! let v1 = Version::try_from("1.0pre1").unwrap();
//! let v2 = Version::try_from("1.0pre2").unwrap();
//! let v3 = Version::try_from("1.0").unwrap();
//! let v4 = Version::try_from("1.0.0").unwrap();
//! let v5 = Version::try_from("1.0.0.0").unwrap();
//! let v6 = Version::try_from("1.1pre").unwrap();
//! let v7 = Version::try_from("1.1pre0").unwrap();
//! let v8 = Version::try_from("1.0+").unwrap();
//! let v9 = Version::try_from("1.1pre1a").unwrap();
//! let v10 = Version::try_from("1.1pre1").unwrap();
//! let v11 = Version::try_from("1.1pre10a").unwrap();
//! let v12 = Version::try_from("1.1pre10").unwrap();
//! assert!(v1 < v2);
//! assert!(v2 < v3);
//! assert!(v3 == v4);
//! assert!(v4 == v5);
//! assert!(v5 < v6);
//! assert!(v6 == v7);
//! assert!(v7 == v8);
//! assert!(v8 < v9);
//! assert!(v9 < v10);
//! assert!(v10 < v11);
//! assert!(v11 < v12);
//! ```
//! What the above is comparing is:
//! 1.0pre1
//! < 1.0pre2
//!   < 1.0 == 1.0.0 == 1.0.0.0
//!     < 1.1pre == 1.1pre0 == 1.0+
//!       < 1.1pre1a
//!         < 1.1pre1
//!           < 1.1pre10a
//!             < 1.1pre10

use crate::error::VersionParsingError;
use std::cmp::Ordering;

#[derive(Debug, Default, Clone, PartialEq)]
pub struct VersionPart {
    pub num_a: i32,
    pub str_b: String,
    pub num_c: i32,
    pub extra_d: String,
}

/// Represents a version in the form of a sequence of version parts.
///
/// The `Version` struct is used to compare application versions that follow
/// a dot-separated format (e.g., `1.0.0`, `98.2pre1.0-beta`). Each part of the version
/// is represented by a `VersionPart`.
#[derive(Debug, Default, Clone)]
pub struct Version(pub Vec<VersionPart>);

impl PartialEq for Version {
    fn eq(&self, other: &Self) -> bool {
        let default_version_part: VersionPart = Default::default();
        let mut curr_idx = 0;
        while curr_idx < self.0.len() || curr_idx < other.0.len() {
            let version_part = self.0.get(curr_idx).unwrap_or(&default_version_part);
            let other_version_part = other.0.get(curr_idx).unwrap_or(&default_version_part);
            if !version_part.eq(other_version_part) {
                return false;
            }
            curr_idx += 1
        }
        true
    }
}

impl PartialOrd for Version {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        let mut idx = 0;
        let default_version: VersionPart = Default::default();
        while idx < self.0.len() || idx < other.0.len() {
            let version_part = self.0.get(idx).unwrap_or(&default_version);
            let other_version_part = other.0.get(idx).unwrap_or(&default_version);
            let ord = version_part.partial_cmp(other_version_part);
            match ord {
                Some(Ordering::Greater) | Some(Ordering::Less) => return ord,
                _ => (),
            }
            idx += 1;
        }
        Some(Ordering::Equal)
    }
}

impl PartialOrd for VersionPart {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        let num_a_ord = self.num_a.partial_cmp(&other.num_a);
        match num_a_ord {
            Some(Ordering::Greater) | Some(Ordering::Less) => return num_a_ord,
            _ => (),
        };

        if self.str_b.is_empty() && !other.str_b.is_empty() {
            return Some(Ordering::Greater);
        } else if other.str_b.is_empty() && !self.str_b.is_empty() {
            return Some(Ordering::Less);
        }
        let str_b_ord = self.str_b.partial_cmp(&other.str_b);
        match str_b_ord {
            Some(Ordering::Greater) | Some(Ordering::Less) => return str_b_ord,
            _ => (),
        };

        let num_c_ord = self.num_c.partial_cmp(&other.num_c);
        match num_c_ord {
            Some(Ordering::Greater) | Some(Ordering::Less) => return num_c_ord,
            _ => (),
        };

        if self.extra_d.is_empty() && !other.extra_d.is_empty() {
            return Some(Ordering::Greater);
        } else if other.extra_d.is_empty() && !self.extra_d.is_empty() {
            return Some(Ordering::Less);
        }
        let extra_d_ord = self.extra_d.partial_cmp(&other.extra_d);
        match extra_d_ord {
            Some(Ordering::Greater) | Some(Ordering::Less) => return extra_d_ord,
            _ => (),
        };
        Some(Ordering::Equal)
    }
}

impl TryFrom<&'_ str> for Version {
    type Error = VersionParsingError;
    fn try_from(value: &'_ str) -> Result<Self, Self::Error> {
        let versions = value
            .split('.')
            .map(TryInto::try_into)
            .collect::<Result<Vec<_>, _>>()?;
        Ok(Version(versions))
    }
}

impl TryFrom<String> for Version {
    type Error = VersionParsingError;
    fn try_from(curr_part: String) -> Result<Self, Self::Error> {
        curr_part.as_str().try_into()
    }
}

fn char_at(value: &str, idx: usize) -> Result<char, VersionParsingError> {
    value.chars().nth(idx).ok_or_else(|| {
        VersionParsingError::Overflow(format!(
            "Tried to access character {} in string {}, but it has size {}",
            idx,
            value,
            value.len()
        ))
    })
}

fn is_num_c(c: char) -> bool {
    // TODO: why is the dash here?
    // this makes `1-beta` end up
    // having num_a = 1, str_b = "", num_c = 0 and extra_d = "-beta"
    // is that correct?
    // Taken from: https://searchfox.org/mozilla-central/rev/77efe87174ee82dad43da56d71a717139b9f19ee/xpcom/base/nsVersionComparator.cpp#107
    c.is_numeric() || c == '+' || c == '-'
}

fn parse_version_num(val: i32, res: &mut i32) -> Result<(), VersionParsingError> {
    if *res == 0 {
        *res = val;
    } else {
        let res_l = *res as i64;
        if (res_l * 10) + val as i64 > i32::MAX as i64 {
            return Err(VersionParsingError::Overflow(
                "Number parsing overflows an i32".into(),
            ));
        }
        *res *= 10;
        *res += val;
    }
    Ok(())
}

impl TryFrom<&'_ str> for VersionPart {
    type Error = VersionParsingError;

    fn try_from(value: &'_ str) -> Result<Self, Self::Error> {
        if !value.is_ascii() {
            return Err(VersionParsingError::ParseError(format!(
                "version string {} contains non-ascii characters",
                value
            )));
        }
        if value.is_empty() {
            return Ok(Default::default());
        }

        let mut res: VersionPart = Default::default();
        // if the string value is the special "*",
        // then we set the num_a to be the highest possible value
        // handle that case before we start
        if value == "*" {
            res.num_a = i32::MAX;
            return Ok(res);
        }
        // Step 1: Parse the num_a, it's guaranteed to be
        // a base-10 number, if it exists
        let mut curr_idx = 0;
        while curr_idx < value.len() && char_at(value, curr_idx)?.is_numeric() {
            parse_version_num(
                char_at(value, curr_idx)?.to_digit(10).unwrap() as i32,
                &mut res.num_a,
            )?;
            curr_idx += 1;
        }
        if curr_idx >= value.len() {
            return Ok(res);
        }
        // Step 2: Parse the str_b. If str_b starts with a "+"
        // then we increment num_a, and set str_b to be "pre"
        let first_char = char_at(value, curr_idx)?;
        if first_char == '+' {
            res.num_a += 1;
            res.str_b = "pre".into();
            return Ok(res);
        }
        // otherwise, we parse until we either finish the string
        // or we find a numeric number, indicating the start of num_c
        while curr_idx < value.len() && !is_num_c(char_at(value, curr_idx)?) {
            res.str_b.push(char_at(value, curr_idx)?);
            curr_idx += 1;
        }

        if curr_idx >= value.len() {
            return Ok(res);
        }

        // Step 3: Parse the num_c, similar to how we parsed num_a
        while curr_idx < value.len() && char_at(value, curr_idx)?.is_numeric() {
            parse_version_num(
                char_at(value, curr_idx)?.to_digit(10).unwrap() as i32,
                &mut res.num_c,
            )?;
            curr_idx += 1;
        }
        if curr_idx >= value.len() {
            return Ok(res);
        }

        // Step 4: Assign all the remaining to extra_d
        res.extra_d = value[curr_idx..].into();
        Ok(res)
    }
}
