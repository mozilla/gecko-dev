/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::borrow::Cow;

use nsstring::{nsACString, nsCString};
use rusqlite::{
    types::{FromSql, FromSqlResult, ToSqlOutput, ValueRef},
    ToSql,
};

#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Key(String);

impl TryFrom<&nsACString> for Key {
    type Error = KeyError;

    fn try_from(key: &nsACString) -> Result<Self, Self::Error> {
        Self::try_from(key.to_utf8())
    }
}

impl<'a> TryFrom<Cow<'a, str>> for Key {
    type Error = KeyError;

    fn try_from(key: Cow<'a, str>) -> Result<Self, Self::Error> {
        if key.starts_with(char::is_whitespace) || key.ends_with(char::is_whitespace) {
            return Err(KeyError::Untrimmed);
        }
        Ok(Self(key.into_owned()))
    }
}

impl From<Key> for nsCString {
    fn from(key: Key) -> Self {
        key.0.into()
    }
}

impl ToSql for Key {
    fn to_sql(&self) -> rusqlite::Result<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(self.0.as_str()))
    }
}

impl FromSql for Key {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        Ok(Self(String::column_result(value)?))
    }
}

#[derive(thiserror::Error, Debug)]
pub enum KeyError {
    #[error("untrimmed")]
    Untrimmed,
}
