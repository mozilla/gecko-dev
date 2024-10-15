/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::query::Queryable;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fmt;
use std::mem::size_of;

#[derive(PartialEq, Eq)]
pub enum Membership {
    Member,
    Nonmember,
    NotInUniverse,
    NoData,
}

impl From<bool> for Membership {
    fn from(b: bool) -> Membership {
        match b {
            true => Membership::Member,
            false => Membership::Nonmember,
        }
    }
}

/// Metadata needed to compute membership in a clubcard.
#[derive(Default, Serialize, Deserialize)]
pub struct ClubcardIndexEntry {
    /// Description of the hash function h.
    pub approx_filter_m: usize,
    /// Description of the hash function g.
    pub exact_filter_m: usize,
    /// The number of columns in X.
    pub approx_filter_rank: usize,
    /// An offset t such that [0^t || h(u)] * X = h(u) * Xi, where i is the block identifier.
    pub approx_filter_offset: usize,
    /// An offset t such that [0^t || g(u)] * Y = g(u) * Yi, where i is the block identifier.
    pub exact_filter_offset: usize,
    /// Whether to invert the output of queries to this block.
    pub inverted: bool,
    /// A list of elements of Ui \ Ri that are not correctly encoded by this block.
    pub exceptions: Vec<Vec<u8>>,
}

/// Lookup table from block identifiers to block metadata.
pub type ClubcardIndex = BTreeMap</* block id */ Vec<u8>, ClubcardIndexEntry>;

/// A queryable Clubcard
#[derive(Serialize, Deserialize)]
pub struct Clubcard<const W: usize, UniverseMetadata, PartitionMetadata> {
    /// Metadata for determining whether a Queryable is in the encoded universe.
    pub(crate) universe: UniverseMetadata,
    /// Metadata for determining the block to which a Queryable belongs.
    pub(crate) partition: PartitionMetadata,
    /// Lookup table for per-block metadata.
    pub(crate) index: ClubcardIndex,
    /// The matrix X
    pub(crate) approx_filter: Vec<Vec<u64>>,
    /// The matrix Y
    pub(crate) exact_filter: Vec<u64>,
}

impl<const W: usize, UniverseMetadata, PartitionMetadata> fmt::Display
    for Clubcard<W, UniverseMetadata, PartitionMetadata>
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let approx_size = 8 * self.approx_filter.iter().map(|x| x.len()).sum::<usize>();
        let exact_size = 8 * self.exact_filter.len();
        let exceptions = self
            .index
            .values()
            .map(|meta| meta.exceptions.len())
            .sum::<usize>();
        writeln!(
            f,
            "Clubcard of size {} ({} + {}) with {} exceptions",
            approx_size + exact_size,
            approx_size,
            exact_size,
            exceptions
        )
    }
}

impl<const W: usize, UniverseMetadata, PartitionMetadata>
    Clubcard<W, UniverseMetadata, PartitionMetadata>
{
    /// Perform a membership query without checking whether the item is in the universe.
    /// The result is undefined if the item is not in the universe. The result is also
    /// undefined if U's implementation of AsQuery differs from T's.
    pub fn unchecked_contains<T>(&self, item: &T) -> bool
    where
        T: Queryable<W, PartitionMetadata = PartitionMetadata>,
    {
        let Some(meta) = self.index.get(item.block()) else {
            return false;
        };

        let result = (|| {
            // All queries evaluate to 0 on an empty filter, but logically
            // such a filter does not include anything. So we handle it as a
            // special case.
            if meta.approx_filter_m == 0 {
                return false;
            }

            // Check if h(item) * X is 0
            let approx_query = item.as_approx_query(meta);
            for i in 0..meta.approx_filter_rank {
                if approx_query.eval(&self.approx_filter[i]) != 0 {
                    return false;
                }
            }

            // Check if g(item) * X is 0
            let exact_query = item.as_exact_query(meta);
            if exact_query.eval(&self.exact_filter) != 0 {
                return false;
            }

            for exception in &meta.exceptions {
                if exception == item.discriminant() {
                    return false;
                }
            }
            true
        })();

        result ^ meta.inverted
    }

    /// Check that the item is in the appropriate universe, and then perform a membership query.
    pub fn contains<T>(&self, item: &T) -> Membership
    where
        T: Queryable<W, UniverseMetadata = UniverseMetadata, PartitionMetadata = PartitionMetadata>,
    {
        if !item.in_universe(&self.universe) {
            return Membership::NotInUniverse;
        };

        if !self.index.contains_key(item.block()) {
            return Membership::NoData;
        };

        self.unchecked_contains(item).into()
    }

    pub fn universe(&self) -> &UniverseMetadata {
        &self.universe
    }

    pub fn partition(&self) -> &PartitionMetadata {
        &self.partition
    }

    pub fn index(&self) -> &ClubcardIndex {
        &self.index
    }
}

/// Helper trait for (approximate) heap memory usage analysis in Firefox
pub trait ApproximateSizeOf {
    fn approximate_size_of(&self) -> usize
    where
        Self: Sized,
    {
        size_of::<Self>()
    }
}

impl ApproximateSizeOf for () {}

impl ApproximateSizeOf for ClubcardIndex {
    fn approximate_size_of(&self) -> usize {
        size_of::<ClubcardIndex>() + self.len() * size_of::<ClubcardIndexEntry>()
    }
}

impl<const W: usize, UniverseMetadata, PartitionMetadata> ApproximateSizeOf
    for Clubcard<W, UniverseMetadata, PartitionMetadata>
where
    UniverseMetadata: ApproximateSizeOf,
    PartitionMetadata: ApproximateSizeOf,
{
    fn approximate_size_of(&self) -> usize {
        self.universe.approximate_size_of()
            + self.partition.approximate_size_of()
            + self.index.approximate_size_of()
            + size_of::<Vec<Vec<u8>>>()
            + 8 * self.approx_filter.iter().map(|x| x.len()).sum::<usize>()
            + size_of::<Vec<u8>>()
            + 8 * self.exact_filter.len()
    }
}
