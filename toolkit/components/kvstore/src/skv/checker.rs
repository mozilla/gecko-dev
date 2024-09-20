/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::skv::{
    connection::{ConnectionIncident, ConnectionIncidents, ConnectionMaintenanceTask},
    maintenance::{Maintenance, MaintenanceError},
};

/// Checks an SQLite database for corruption.
#[derive(Debug)]
pub struct Checker {
    checks: Checks,
}

impl ConnectionMaintenanceTask for Checker {
    type Error = MaintenanceError;

    fn run(self, conn: &mut rusqlite::Connection) -> Result<(), Self::Error> {
        let maintenance = Maintenance::new(conn);
        if self.checks.reindex {
            maintenance.reindex()?;
        }
        match self.checks.consistency {
            Some(ConsistencyCheck::Quick) => maintenance.quick_check()?,
            Some(ConsistencyCheck::Full) => maintenance.integrity_check()?,
            None => (),
        }
        if self.checks.foreign_keys {
            maintenance.foreign_key_check()?;
        }
        Ok(())
    }
}

/// Determines whether to check an SQLite database for corruption,
/// and which checks to run.
pub trait IntoChecker<C> {
    fn into_checker(self) -> CheckerAction<C>;
}

impl IntoChecker<Checker> for ConnectionIncidents<'_> {
    fn into_checker(self) -> CheckerAction<Checker> {
        self.map(|incidents| {
            let (penalty, checks) = incidents.iter().fold(
                (Penalty::default(), Checks::default()),
                |(penalty, checks), incident| (penalty.adding(incident), checks.adding(incident)),
            );
            if penalty < Penalty::MIN {
                // No incidents, or maybe some transient errors.
                // Keep any existing incidents, and skip checks for now.
                CheckerAction::Skip
            } else if penalty < Penalty::MAX {
                // Check the database for potential corruption.
                incidents.resolve();
                CheckerAction::Check(Checker { checks })
            } else {
                // Too many incidents; replace the database.
                incidents.resolve();
                CheckerAction::Replace
            }
        })
    }
}

/// Whether to skip checking, check, or replace a corrupt SQLite database.
#[derive(Debug)]
pub enum CheckerAction<C> {
    Skip,
    Check(C),
    Replace,
}

/// The penalty for one or more [`ConnectionIncidents`].
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
struct Penalty(usize);

impl Penalty {
    /// The minimum penalty needed to check a database for errors.
    const MIN: Self = Penalty(5);

    /// The maximum penalty before replacing a database.
    const MAX: Self = Penalty(20);

    fn adding(self, incident: ConnectionIncident) -> Self {
        // Each incident contributes a (completely arbitrary) amount to
        // the penalty, depending on the severity and frequency.
        Self(
            self.0
                + match incident {
                    ConnectionIncident::CorruptFile => 3,
                    ConnectionIncident::CorruptIndex => 2,
                    ConnectionIncident::CorruptForeignKey => 1,
                },
        )
    }
}

const _: () = {
    assert!(
        Penalty::MIN.0 < Penalty::MAX.0,
        "`Penalty::MIN` should be less than `Penalty::MAX`"
    );
};

/// Checks to run on a potentially corrupt SQLite database.
#[derive(Clone, Copy, Debug, Default)]
struct Checks {
    reindex: bool,
    consistency: Option<ConsistencyCheck>,
    foreign_keys: bool,
}

impl Checks {
    fn adding(self, incident: ConnectionIncident) -> Self {
        match incident {
            ConnectionIncident::CorruptFile => Self {
                consistency: Some(
                    // If we haven't seen index corruption, a quick check will
                    // likely find other problems faster than a full check.
                    self.consistency
                        .unwrap_or(ConsistencyCheck::Quick)
                        .and(ConsistencyCheck::Quick),
                ),
                ..self
            },
            ConnectionIncident::CorruptIndex => Self {
                // Try to rebuild the indexes.
                reindex: true,
                consistency: Some(
                    // If we have seen index corruption, we need to run a
                    // full check.
                    self.consistency
                        .unwrap_or(ConsistencyCheck::Full)
                        .and(ConsistencyCheck::Full),
                ),
                ..self
            },
            ConnectionIncident::CorruptForeignKey => Self {
                // Neither quick nor full checks look for foreign key errors.
                foreign_keys: true,
                ..self
            },
        }
    }
}

#[derive(Clone, Copy, Debug)]
enum ConsistencyCheck {
    Quick,
    Full,
}

impl ConsistencyCheck {
    fn and(self, other: Self) -> Self {
        match (self, other) {
            (Self::Quick, Self::Quick) => Self::Quick,
            // A full consistency check subsumes a quick check.
            _ => Self::Full,
        }
    }
}
