// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::connection_strategy::ConnectionStrategy;
use crate::SqLiteDataStorageError;
use rusqlite::Connection;

use hex::ToHex;
use zeroize::{ZeroizeOnDrop, Zeroizing};

#[allow(dead_code)]
#[derive(Debug, ZeroizeOnDrop, Clone)]
/// Representation of a SQLCipher key used to unlock a database.
pub enum SqlCipherKey {
    /// Passphrase based key.
    Passphrase(String),
    /// Raw key material without a salt value.
    RawKey([u8; 32]),
    /// Raw key material with a salt value.
    RawKeyWithSalt([u8; 48]),
}

fn blob_string_repr(val: &[u8]) -> String {
    format!("x'{}'", val.encode_hex_upper::<String>())
}

impl SqlCipherKey {
    fn to_key_pragma_value(&self) -> Zeroizing<String> {
        Zeroizing::new(match self {
            SqlCipherKey::Passphrase(pass) => pass.clone(),
            SqlCipherKey::RawKey(key) => blob_string_repr(key.as_slice()),
            SqlCipherKey::RawKeyWithSalt(key) => blob_string_repr(key.as_slice()),
        })
    }
}

#[derive(Debug, Clone)]
/// SQLCipher connection config.
pub struct SqlCipherConfig {
    key: SqlCipherKey,
    plaintext_header_size: u8,
}

impl SqlCipherConfig {
    /// Create a new config with a specific key.
    pub fn new(key: SqlCipherKey) -> SqlCipherConfig {
        SqlCipherConfig {
            key,
            plaintext_header_size: 0,
        }
    }

    /// Adjust the plaintext header size.
    pub fn with_plaintext_header(self, size: u8) -> SqlCipherConfig {
        SqlCipherConfig {
            plaintext_header_size: size,
            ..self
        }
    }
}

/// Encrypted database connection with SQLCipher.
pub struct CipheredConnectionStrategy<I>
where
    I: ConnectionStrategy,
{
    inner: I,
    cipher_config: SqlCipherConfig,
}

impl<CS> CipheredConnectionStrategy<CS>
where
    CS: ConnectionStrategy,
{
    /// Create a new SQLCipher connection that inherits another connection strategy.
    pub fn new(strategy: CS, cipher_config: SqlCipherConfig) -> CipheredConnectionStrategy<CS> {
        CipheredConnectionStrategy {
            inner: strategy,
            cipher_config,
        }
    }
}

impl<I> ConnectionStrategy for CipheredConnectionStrategy<I>
where
    I: ConnectionStrategy,
{
    fn make_connection(&self) -> Result<Connection, SqLiteDataStorageError> {
        if self.cipher_config.plaintext_header_size > 0
            && !matches!(self.cipher_config.key, SqlCipherKey::RawKeyWithSalt(_))
        {
            return Err(SqLiteDataStorageError::SqlCipherKeyInvalidWithHeader);
        }

        let connection = self.inner.make_connection()?;

        connection
            .pragma_update(
                None,
                "key",
                self.cipher_config.key.to_key_pragma_value().as_str(),
            )
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))?;

        connection
            .pragma_update(
                None,
                "cipher_plaintext_header_size",
                self.cipher_config.plaintext_header_size,
            )
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))?;

        // Verify that the database is keyed correctly
        connection
            .query_row("SELECT count(*) FROM sqlite_master", [], |_| Ok(()))
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))?;

        Ok(connection)
    }
}

#[cfg(test)]
mod tests {
    use assert_matches::assert_matches;
    use tempfile::NamedTempFile;

    use crate::cipher::SqlCipherConfig;
    use crate::connection_strategy::{ConnectionStrategy, MemoryStrategy};
    use crate::test_utils::gen_rand_bytes;
    use crate::{connection_strategy::FileConnectionStrategy, SqLiteDataStorageError};

    use super::{CipheredConnectionStrategy, SqlCipherKey};

    fn sql_cipher_test(config: SqlCipherConfig) {
        let temp_file = NamedTempFile::new().unwrap();

        let mut sqlcipher_strategy =
            CipheredConnectionStrategy::new(FileConnectionStrategy::new(temp_file.path()), config);

        // Test first connection
        let connection = sqlcipher_strategy.make_connection().unwrap();
        connection.execute("CREATE TABLE test(item)", []).unwrap();

        // Test reopen for another connection
        sqlcipher_strategy.make_connection().unwrap();

        // Verify plaintext header size
        assert_eq!(
            connection
                .pragma_query_value(None, "cipher_plaintext_header_size", |row| {
                    row.get::<_, String>(0)
                })
                .unwrap(),
            sqlcipher_strategy
                .cipher_config
                .plaintext_header_size
                .to_string()
        );

        // Test incorrect key
        sqlcipher_strategy.cipher_config.key = match sqlcipher_strategy.cipher_config.key {
            SqlCipherKey::Passphrase(_) => SqlCipherKey::Passphrase("incorrect".to_string()),
            SqlCipherKey::RawKey(_) => SqlCipherKey::RawKey(gen_rand_bytes(32).try_into().unwrap()),
            SqlCipherKey::RawKeyWithSalt(_) => {
                SqlCipherKey::RawKeyWithSalt(gen_rand_bytes(48).try_into().unwrap())
            }
        };

        assert_matches!(
            sqlcipher_strategy.make_connection(),
            Err(SqLiteDataStorageError::SqlEngineError(_))
        );
    }

    #[test]
    fn sql_cipher_passphrase() {
        let config = SqlCipherConfig::new(SqlCipherKey::Passphrase("correct".to_string()));

        sql_cipher_test(config);
    }

    #[test]
    fn sql_cipher_raw_key() {
        let config =
            SqlCipherConfig::new(SqlCipherKey::RawKey(gen_rand_bytes(32).try_into().unwrap()));

        sql_cipher_test(config);
    }

    #[test]
    fn sql_cipher_raw_key_salt() {
        let config = SqlCipherConfig::new(SqlCipherKey::RawKeyWithSalt(
            gen_rand_bytes(48).try_into().unwrap(),
        ));

        sql_cipher_test(config);
    }

    #[test]
    fn sql_cipher_plaintext_header() {
        let config = SqlCipherConfig::new(SqlCipherKey::RawKeyWithSalt(
            gen_rand_bytes(48).try_into().unwrap(),
        ))
        .with_plaintext_header(32);

        sql_cipher_test(config);
    }

    #[test]
    fn sql_cipher_invalid_key_plaintext_header() {
        let config = SqlCipherConfig::new(SqlCipherKey::Passphrase("correct".to_string()))
            .with_plaintext_header(32);

        let res = CipheredConnectionStrategy::new(MemoryStrategy, config).make_connection();

        assert_matches!(
            res,
            Err(SqLiteDataStorageError::SqlCipherKeyInvalidWithHeader)
        );
    }
}
