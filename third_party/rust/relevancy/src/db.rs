/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use crate::Error::BanditNotFound;
use crate::{
    interest::InterestVectorKind,
    schema::RelevancyConnectionInitializer,
    url_hash::{hash_url, UrlHash},
    Interest, InterestVector, Result,
};
use interrupt_support::SqlInterruptScope;
use rusqlite::{Connection, OpenFlags};
use sql_support::{ConnExt, LazyDb};
use std::path::Path;

/// A thread-safe wrapper around an SQLite connection to the Relevancy database
pub struct RelevancyDb {
    reader: LazyDb<RelevancyConnectionInitializer>,
    writer: LazyDb<RelevancyConnectionInitializer>,
}

#[derive(Debug, PartialEq, uniffi::Record)]
pub struct BanditData {
    pub bandit: String,
    pub arm: String,
    pub impressions: u64,
    pub clicks: u64,
    pub alpha: u64,
    pub beta: u64,
}

impl RelevancyDb {
    pub fn new(path: impl AsRef<Path>) -> Self {
        // Note: use `SQLITE_OPEN_READ_WRITE` for both read and write connections.
        // Even if we're opening a read connection, we may need to do a write as part of the
        // initialization process.
        //
        // The read-only nature of the connection is enforced by the fact that [RelevancyDb::read] uses a
        // shared ref to the `RelevancyDao`.
        let db_open_flags = OpenFlags::SQLITE_OPEN_URI
            | OpenFlags::SQLITE_OPEN_NO_MUTEX
            | OpenFlags::SQLITE_OPEN_CREATE
            | OpenFlags::SQLITE_OPEN_READ_WRITE;
        Self {
            reader: LazyDb::new(path.as_ref(), db_open_flags, RelevancyConnectionInitializer),
            writer: LazyDb::new(path.as_ref(), db_open_flags, RelevancyConnectionInitializer),
        }
    }

    pub fn close(&self) {
        self.reader.close(true);
        self.writer.close(true);
    }

    pub fn interrupt(&self) {
        self.reader.interrupt();
        self.writer.interrupt();
    }

    #[cfg(test)]
    pub fn new_for_test() -> Self {
        use std::sync::atomic::{AtomicU32, Ordering};
        static COUNTER: AtomicU32 = AtomicU32::new(0);
        let count = COUNTER.fetch_add(1, Ordering::Relaxed);
        Self::new(format!("file:test{count}.sqlite?mode=memory&cache=shared"))
    }

    /// Accesses the Suggest database in a transaction for reading.
    pub fn read<T>(&self, op: impl FnOnce(&RelevancyDao) -> Result<T>) -> Result<T> {
        let (mut conn, scope) = self.reader.lock()?;
        let tx = conn.transaction()?;
        let dao = RelevancyDao::new(&tx, scope);
        op(&dao)
    }

    /// Accesses the Suggest database in a transaction for reading and writing.
    pub fn read_write<T>(&self, op: impl FnOnce(&mut RelevancyDao) -> Result<T>) -> Result<T> {
        let (mut conn, scope) = self.writer.lock()?;
        let tx = conn.transaction()?;
        let mut dao = RelevancyDao::new(&tx, scope);
        let result = op(&mut dao)?;
        tx.commit()?;
        Ok(result)
    }
}

/// A data access object (DAO) that wraps a connection to the Relevancy database
///
/// Methods that only read from the database take an immutable reference to
/// `self` (`&self`), and methods that write to the database take a mutable
/// reference (`&mut self`).
pub struct RelevancyDao<'a> {
    pub conn: &'a Connection,
    pub scope: SqlInterruptScope,
}

impl<'a> RelevancyDao<'a> {
    fn new(conn: &'a Connection, scope: SqlInterruptScope) -> Self {
        Self { conn, scope }
    }

    /// Return Err(Interrupted) if we were interrupted
    pub fn err_if_interrupted(&self) -> Result<()> {
        Ok(self.scope.err_if_interrupted()?)
    }

    /// Associate a URL with an interest
    pub fn add_url_interest(&mut self, url_hash: UrlHash, interest: Interest) -> Result<()> {
        let sql = "
            INSERT OR REPLACE INTO url_interest(url_hash, interest_code)
            VALUES (?, ?)
        ";
        self.conn.execute(sql, (url_hash, interest as u32))?;
        Ok(())
    }

    /// Get an interest vector for a URL
    pub fn get_url_interest_vector(&self, url: &str) -> Result<InterestVector> {
        let hash = match hash_url(url) {
            Some(u) => u,
            None => return Ok(InterestVector::default()),
        };
        let mut stmt = self.conn.prepare_cached(
            "
            SELECT interest_code
            FROM url_interest
            WHERE url_hash=?
        ",
        )?;
        let interests = stmt.query_and_then((hash,), |row| -> Result<Interest> {
            row.get::<_, u32>(0)?.try_into()
        })?;

        let mut interest_vec = InterestVector::default();
        for interest in interests {
            interest_vec[interest?] += 1
        }
        Ok(interest_vec)
    }

    /// Do we need to load the interest data?
    pub fn need_to_load_url_interests(&self) -> Result<bool> {
        // TODO: we probably will need a better check than this.
        Ok(self
            .conn
            .query_one("SELECT NOT EXISTS (SELECT 1 FROM url_interest)")?)
    }

    /// Update the frecency user interest vector based on a new measurement.
    ///
    /// Right now this completely replaces the interest vector with the new data.  At some point,
    /// we may switch to incrementally updating it instead.
    pub fn update_frecency_user_interest_vector(&self, interests: &InterestVector) -> Result<()> {
        let mut stmt = self.conn.prepare(
            "
            INSERT OR REPLACE INTO user_interest(kind, interest_code, count)
            VALUES (?, ?, ?)
            ",
        )?;
        for (interest, count) in interests.as_vec() {
            stmt.execute((InterestVectorKind::Frecency, interest, count))?;
        }

        Ok(())
    }

    pub fn get_frecency_user_interest_vector(&self) -> Result<InterestVector> {
        let mut stmt = self
            .conn
            .prepare("SELECT interest_code, count FROM user_interest WHERE kind = ?")?;
        let mut interest_vec = InterestVector::default();
        let rows = stmt.query_and_then((InterestVectorKind::Frecency,), |row| {
            crate::Result::Ok((
                Interest::try_from(row.get::<_, u32>(0)?)?,
                row.get::<_, u32>(1)?,
            ))
        })?;
        for row in rows {
            let (interest_code, count) = row?;
            interest_vec.set(interest_code, count);
        }
        Ok(interest_vec)
    }

    /// Initializes a multi-armed bandit record in the database for a specific bandit and arm.
    ///
    /// This method inserts a new record into the `multi_armed_bandit` table with default probability
    /// distribution parameters (`alpha` and `beta` set to 1) and usage counters (`impressions` and
    /// `clicks` set to 0) for the specified `bandit` and `arm`. If a record for this bandit-arm pair
    /// already exists, the insertion is ignored, preserving the existing data.
    pub fn initialize_multi_armed_bandit(&mut self, bandit: &str, arm: &str) -> Result<()> {
        let mut new_statement = self.conn.prepare(
            "INSERT OR IGNORE INTO multi_armed_bandit (bandit, arm, alpha, beta, impressions, clicks) VALUES (?, ?, ?, ?, ?, ?)"
        )?;
        new_statement.execute((bandit, arm, 1, 1, 0, 0))?;

        Ok(())
    }

    /// Retrieves the Beta distribution parameters (`alpha` and `beta`) for a specific arm in a bandit model.
    ///
    /// If the specified `bandit` and `arm` do not exist in the table, an error is returned indicating
    /// that the record was not found.
    pub fn retrieve_bandit_arm_beta_distribution(
        &self,
        bandit: &str,
        arm: &str,
    ) -> Result<(u64, u64)> {
        let mut stmt = self
            .conn
            .prepare("SELECT alpha, beta FROM multi_armed_bandit WHERE bandit=? AND arm=?")?;

        let mut result = stmt.query((&bandit, &arm))?;

        match result.next()? {
            Some(row) => Ok((row.get(0)?, row.get(1)?)),
            None => Err(BanditNotFound {
                bandit: bandit.to_string(),
                arm: arm.to_string(),
            }),
        }
    }

    /// Retrieves the data for a specific bandit and arm combination from the database.
    ///
    /// This method queries the `multi_armed_bandit` table to find a row matching the given
    /// `bandit` and `arm` values. If a matching row is found, it extracts the corresponding
    /// fields (`bandit`, `arm`, `impressions`, `clicks`, `alpha`, `beta`) and returns them
    /// as a `BanditData` struct. If no matching row is found, it returns a `BanditNotFound`
    /// error.
    pub fn retrieve_bandit_data(&self, bandit: &str, arm: &str) -> Result<BanditData> {
        let mut stmt = self
            .conn
            .prepare("SELECT bandit, arm, impressions, clicks, alpha, beta FROM multi_armed_bandit WHERE bandit=? AND arm=?")?;

        let mut result = stmt.query((&bandit, &arm))?;

        match result.next()? {
            Some(row) => {
                let bandit = row.get::<_, String>(0)?;
                let arm = row.get::<_, String>(1)?;
                let impressions = row.get::<_, u64>(2)?;
                let clicks = row.get::<_, u64>(3)?;
                let alpha = row.get::<_, u64>(4)?;
                let beta = row.get::<_, u64>(5)?;

                Ok(BanditData {
                    bandit,
                    arm,
                    impressions,
                    clicks,
                    alpha,
                    beta,
                })
            }
            None => Err(BanditNotFound {
                bandit: bandit.to_string(),
                arm: arm.to_string(),
            }),
        }
    }

    /// Updates the Beta distribution parameters and counters for a specific arm in a bandit model based on user interaction.
    ///
    /// This method updates the `alpha` or `beta` parameters in the `multi_armed_bandit` table for the specified
    /// `bandit` and `arm` based on whether the arm was selected by the user. If `selected` is true, it increments
    /// both the `alpha` (indicating success) and the `clicks` and `impressions` counters. If `selected` is false,
    /// it increments `beta` (indicating failure) and only the `impressions` counter. This approach adjusts the
    /// distribution parameters to reflect the arm's performance over time.
    pub fn update_bandit_arm_data(&self, bandit: &str, arm: &str, selected: bool) -> Result<()> {
        let mut stmt = if selected {
            self
                .conn
                .prepare("UPDATE multi_armed_bandit SET alpha=alpha+1, clicks=clicks+1, impressions=impressions+1 WHERE bandit=? AND arm=?")?
        } else {
            self
                .conn
                .prepare("UPDATE multi_armed_bandit SET beta=beta+1, impressions=impressions+1 WHERE bandit=? AND arm=?")?
        };

        let result = stmt.execute((&bandit, &arm))?;

        if result == 0 {
            return Err(BanditNotFound {
                bandit: bandit.to_string(),
                arm: arm.to_string(),
            });
        }

        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use rusqlite::params;

    #[test]
    fn test_store_frecency_user_interest_vector() {
        let db = RelevancyDb::new_for_test();
        // Initially the interest vector should be blank
        assert_eq!(
            db.read_write(|dao| dao.get_frecency_user_interest_vector())
                .unwrap(),
            InterestVector::default()
        );

        let interest_vec = InterestVector {
            animals: 2,
            autos: 1,
            news: 5,
            ..InterestVector::default()
        };
        db.read_write(|dao| dao.update_frecency_user_interest_vector(&interest_vec))
            .unwrap();
        assert_eq!(
            db.read_write(|dao| dao.get_frecency_user_interest_vector())
                .unwrap(),
            interest_vec,
        );
    }

    #[test]
    fn test_update_frecency_user_interest_vector() {
        let db = RelevancyDb::new_for_test();
        let interest_vec1 = InterestVector {
            animals: 2,
            autos: 1,
            news: 5,
            ..InterestVector::default()
        };
        let interest_vec2 = InterestVector {
            animals: 1,
            career: 3,
            ..InterestVector::default()
        };
        // Update the first interest vec, then the second one
        db.read_write(|dao| dao.update_frecency_user_interest_vector(&interest_vec1))
            .unwrap();
        db.read_write(|dao| dao.update_frecency_user_interest_vector(&interest_vec2))
            .unwrap();
        // The current behavior is the second one should replace the first
        assert_eq!(
            db.read_write(|dao| dao.get_frecency_user_interest_vector())
                .unwrap(),
            interest_vec2,
        );
    }

    #[test]
    fn test_initialize_multi_armed_bandit() -> Result<()> {
        let db = RelevancyDb::new_for_test();

        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        let result = db.read(|dao| {
            let mut stmt = dao.conn.prepare("SELECT alpha, beta, impressions, clicks FROM multi_armed_bandit WHERE bandit=? AND arm=?")?;

            stmt.query_row(params![&bandit, &arm], |row| {
                let alpha: usize = row.get(0)?;
                let beta: usize = row.get(1)?;
                let impressions: usize = row.get(2)?;
                let clicks: usize = row.get(3)?;

                Ok((alpha, beta, impressions, clicks))
            }).map_err(|e| e.into())
        })?;

        assert_eq!(result.0, 1); // Default alpha
        assert_eq!(result.1, 1); // Default beta
        assert_eq!(result.2, 0); // Default impressions
        assert_eq!(result.3, 0); // Default clicks

        Ok(())
    }

    #[test]
    fn test_initialize_multi_armed_bandit_existing_data() -> Result<()> {
        let db = RelevancyDb::new_for_test();

        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        let result = db.read(|dao| {
            let mut stmt = dao.conn.prepare("SELECT alpha, beta, impressions, clicks FROM multi_armed_bandit WHERE bandit=? AND arm=?")?;

            stmt.query_row(params![&bandit, &arm], |row| {
                let alpha: usize = row.get(0)?;
                let beta: usize = row.get(1)?;
                let impressions: usize = row.get(2)?;
                let clicks: usize = row.get(3)?;

                Ok((alpha, beta, impressions, clicks))
            }).map_err(|e| e.into())
        })?;

        assert_eq!(result.0, 1); // Default alpha
        assert_eq!(result.1, 1); // Default beta
        assert_eq!(result.2, 0); // Default impressions
        assert_eq!(result.3, 0); // Default clicks

        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, true))?;

        let (alpha, beta) =
            db.read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, &arm))?;

        assert_eq!(alpha, 2);
        assert_eq!(beta, 1);

        // this should be a no-op since the same bandit-arm has already been initialized
        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        let (alpha, beta) =
            db.read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, &arm))?;

        // alpha & beta values for the bandit-arm should remain unchanged
        assert_eq!(alpha, 2);
        assert_eq!(beta, 1);

        Ok(())
    }

    #[test]
    fn test_retrieve_bandit_arm_beta_distribution() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, true))?;

        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, false))?;

        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, false))?;

        let (alpha, beta) =
            db.read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, &arm))?;

        assert_eq!(alpha, 2);
        assert_eq!(beta, 3);

        Ok(())
    }

    #[test]
    fn test_retrieve_bandit_arm_beta_distribution_not_found() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        let result = db.read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, &arm));

        match result {
            Ok((alpha, beta)) => panic!(
                "Expected BanditNotFound error, but got Ok result with alpha: {} and beta: {}",
                alpha, beta
            ),
            Err(BanditNotFound { bandit: b, arm: a }) => {
                assert_eq!(b, bandit);
                assert_eq!(a, arm);
            }
            _ => {}
        }

        Ok(())
    }

    #[test]
    fn test_update_bandit_arm_data_selected() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        let result = db.read(|dao| {
            let mut stmt = dao.conn.prepare("SELECT alpha, beta, impressions, clicks FROM multi_armed_bandit WHERE bandit=? AND arm=?")?;

            stmt.query_row(params![&bandit, &arm], |row| {
                let alpha: usize = row.get(0)?;
                let beta: usize = row.get(1)?;
                let impressions: usize = row.get(2)?;
                let clicks: usize = row.get(3)?;

                Ok((alpha, beta, impressions, clicks))
            }).map_err(|e| e.into())
        })?;

        assert_eq!(result.0, 1);
        assert_eq!(result.1, 1);
        assert_eq!(result.2, 0);
        assert_eq!(result.3, 0);

        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, true))?;

        let (alpha, beta) =
            db.read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, &arm))?;

        assert_eq!(alpha, 2);
        assert_eq!(beta, 1);

        Ok(())
    }

    #[test]
    fn test_update_bandit_arm_data_not_selected() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        let result = db.read(|dao| {
            let mut stmt = dao.conn.prepare("SELECT alpha, beta, impressions, clicks FROM multi_armed_bandit WHERE bandit=? AND arm=?")?;

            stmt.query_row(params![&bandit, &arm], |row| {
                let alpha: usize = row.get(0)?;
                let beta: usize = row.get(1)?;
                let impressions: usize = row.get(2)?;
                let clicks: usize = row.get(3)?;

                Ok((alpha, beta, impressions, clicks))
            }).map_err(|e| e.into())
        })?;

        assert_eq!(result.0, 1);
        assert_eq!(result.1, 1);
        assert_eq!(result.2, 0);
        assert_eq!(result.3, 0);

        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, false))?;

        let (alpha, beta) =
            db.read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, &arm))?;

        assert_eq!(alpha, 1);
        assert_eq!(beta, 2);

        Ok(())
    }

    #[test]
    fn test_update_bandit_arm_data_not_found() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        let result = db.read(|dao| dao.update_bandit_arm_data(&bandit, &arm, false));

        match result {
            Ok(()) => panic!("Expected BanditNotFound error, but got Ok result"),
            Err(BanditNotFound { bandit: b, arm: a }) => {
                assert_eq!(b, bandit);
                assert_eq!(a, arm);
            }
            _ => {}
        }

        Ok(())
    }

    #[test]
    fn test_retrieve_bandit_data() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        db.read_write(|dao| dao.initialize_multi_armed_bandit(&bandit, &arm))?;

        // Update the bandit arm data (simulate interactions)
        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, true))?;
        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, false))?;
        db.read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, false))?;

        let bandit_data = db.read(|dao| dao.retrieve_bandit_data(&bandit, &arm))?;

        let expected_bandit_data = BanditData {
            bandit: bandit.clone(),
            arm: arm.clone(),
            impressions: 3, // 3 updates (true + false + false)
            clicks: 1,      // 1 `true` interaction
            alpha: 2,
            beta: 3,
        };

        assert_eq!(bandit_data, expected_bandit_data);

        Ok(())
    }

    #[test]
    fn test_retrieve_bandit_data_not_found() -> Result<()> {
        let db = RelevancyDb::new_for_test();
        let bandit = "provider".to_string();
        let arm = "weather".to_string();

        let result = db.read(|dao| dao.retrieve_bandit_data(&bandit, &arm));

        match result {
            Ok(bandit_data) => panic!(
                "Expected BanditNotFound error, but got Ok result with alpha: {}, beta: {}, impressions: {}, clicks: {}, bandit: {}, arm: {}",
                bandit_data.alpha, bandit_data.beta, bandit_data.impressions, bandit_data.clicks, bandit_data.arm, bandit_data.arm
            ),
            Err(BanditNotFound { bandit: b, arm: a }) => {
                assert_eq!(b, bandit);
                assert_eq!(a, arm);
            }
            _ => {}
        }

        Ok(())
    }
}
