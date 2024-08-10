/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    fmt::Display,
    ops::{Bound, RangeBounds},
};

use rusqlite::ToSql;

/// Formats a pair of range bounds as an SQL expression that
/// constrains the range of a projected column.
///
/// Depending on whether the range is bounded or unbounded in
/// either direction, the fragment can bind zero, one, or two
/// SQL parameters.
pub struct RangeFragment<'a, T> {
    column: &'a str,
    start: Bound<&'a T>,
    end: Bound<&'a T>,
}

impl<'a, T> RangeFragment<'a, T>
where
    T: ToSql,
{
    /// If the range has a lower bound, returns the name and value of the
    /// SQL parameter to add to the `[rusqlite::Params]` slice for the
    /// statement containing this fragment.
    pub fn start_param(&self) -> Option<(&'static str, &dyn ToSql)> {
        match &self.start {
            Bound::Included(key) | Bound::Excluded(key) => Some((":start", key)),
            Bound::Unbounded => None,
        }
    }

    /// If the range has an upper bound, returns the name and value of the
    /// SQL parameter to add to the `[rusqlite::Params]` slice for the
    /// statement containing this fragment.
    pub fn end_param(&self) -> Option<(&'static str, &dyn ToSql)> {
        match &self.end {
            Bound::Included(key) | Bound::Excluded(key) => Some((":end", key)),
            Bound::Unbounded => None,
        }
    }
}

impl<'a, T> RangeFragment<'a, T> {
    pub fn new<R>(column: &'a str, range: &'a R) -> Self
    where
        R: RangeBounds<T>,
    {
        RangeFragment {
            column,
            start: range.start_bound(),
            end: range.end_bound(),
        }
    }
}

impl<'a, T> Display for RangeFragment<'a, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match (&self.start, &self.end) {
            (Bound::Unbounded, Bound::Unbounded) => {
                // `1` is a no-op that's always true.
                f.write_str("1")
            }
            (Bound::Unbounded, Bound::Included(_)) => write!(f, "({0} <= :end)", self.column),
            (Bound::Unbounded, Bound::Excluded(_)) => write!(f, "({0} < :end)", self.column),
            (Bound::Included(_), Bound::Unbounded) => write!(f, "({0} >= :start)", self.column),
            (Bound::Included(_), Bound::Included(_)) => {
                // `BETWEEN` is inclusive in SQL.
                write!(f, "({0} BETWEEN :start AND :end)", self.column)
            }
            (Bound::Included(_), Bound::Excluded(_)) => {
                write!(f, "({0} >= :start AND {0} < :end)", self.column)
            }
            (Bound::Excluded(_), Bound::Unbounded) => write!(f, "({0} > :start)", self.column),
            (Bound::Excluded(_), Bound::Included(_)) => {
                write!(f, "({0} > :start AND {0} <= :end)", self.column)
            }
            (Bound::Excluded(_), Bound::Excluded(_)) => {
                write!(f, "({0} > :start AND {0} < :end)", self.column)
            }
        }
    }
}
