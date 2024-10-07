/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use rusqlite::{functions::FunctionFlags, types::ToSqlOutput, ToSql};

/// Registers all custom Skv SQL functions.
pub fn register(conn: &mut rusqlite::Connection) -> rusqlite::Result<()> {
    conn.create_scalar_function(
        // `throw(message)` throws an error with the given message.
        "throw",
        1,
        FunctionFlags::SQLITE_UTF8
            | FunctionFlags::SQLITE_DETERMINISTIC
            | FunctionFlags::SQLITE_DIRECTONLY,
        |context| -> rusqlite::Result<Never> { Err(FunctionError::Throw(context.get(0)?).into()) },
    )?;

    Ok(())
}

/// The `Ok` type for the `throw(message)` SQL function, which
/// always returns an `Err`.
///
/// This type can be removed once the ["never" type][1] (`!`) is
/// stabilized.
///
/// [1]: https://github.com/rust-lang/rust/issues/35121
enum Never {}

impl ToSql for Never {
    fn to_sql(&self) -> rusqlite::Result<ToSqlOutput<'_>> {
        unreachable!()
    }
}

#[derive(thiserror::Error, Debug)]
enum FunctionError {
    #[error("throw: {0}")]
    Throw(String),
}

impl Into<rusqlite::Error> for FunctionError {
    fn into(self) -> rusqlite::Error {
        rusqlite::Error::UserFunctionError(self.into())
    }
}
