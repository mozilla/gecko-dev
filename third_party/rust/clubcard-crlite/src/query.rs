/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use base64::Engine;
use clubcard::{
    ApproximateSizeOf, AsQuery, Clubcard, ClubcardIndex, Equation, Membership, Queryable,
};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::cmp::max;
use std::collections::HashMap;
use std::fmt;
use std::mem::size_of;

const W: usize = 4;

type IssuerSpkiHash = [u8; 32];
type LogId = [u8; 32];
type Timestamp = u64;
type TimestampInterval = (Timestamp, Timestamp);

#[derive(Serialize, Deserialize)]
pub struct CRLiteCoverage(pub(crate) HashMap<LogId, TimestampInterval>);

#[derive(Debug)]
pub struct CRLiteKey<'a> {
    pub(crate) issuer: &'a IssuerSpkiHash,
    pub(crate) serial: &'a [u8],
    pub(crate) issuer_serial_hash: [u8; 32],
}

impl<'a> CRLiteKey<'a> {
    pub fn new(issuer: &'a IssuerSpkiHash, serial: &'a [u8]) -> CRLiteKey<'a> {
        let mut issuer_serial_hash = [0u8; 32];
        let mut hasher = Sha256::new();
        hasher.update(issuer);
        hasher.update(serial);
        hasher.finalize_into((&mut issuer_serial_hash).into());
        CRLiteKey {
            issuer,
            serial,
            issuer_serial_hash,
        }
    }
}

#[derive(Clone, Debug)]
pub struct CRLiteQuery<'a> {
    pub(crate) key: &'a CRLiteKey<'a>,
    pub(crate) log_timestamp: Option<(&'a LogId, Timestamp)>,
}

impl<'a> CRLiteQuery<'a> {
    pub fn new(key: &'a CRLiteKey<'a>, log_timestamp: Option<(&'a LogId, u64)>) -> CRLiteQuery<'a> {
        CRLiteQuery { key, log_timestamp }
    }
}

impl<'a> AsQuery<W> for CRLiteQuery<'a> {
    fn block(&self) -> &[u8] {
        self.key.issuer.as_ref()
    }

    fn as_query(&self, m: usize) -> Equation<W> {
        let mut a = [0u64; 4];
        for (i, x) in self
            .key
            .issuer_serial_hash
            .chunks_exact(8) // TODO: use array_chunks::<8>() when stable
            .map(|x| TryInto::<[u8; 8]>::try_into(x).unwrap())
            .map(u64::from_le_bytes)
            .enumerate()
        {
            a[i] = x;
        }
        a[0] |= 1;
        let s = (a[3] % (max(1, m) as u64)) as usize;
        Equation::homogeneous(s, a)
    }

    fn discriminant(&self) -> &[u8] {
        self.key.serial
    }
}

impl<'a> Queryable<W> for CRLiteQuery<'a> {
    type UniverseMetadata = CRLiteCoverage;

    // The set of CRLiteKeys is partitioned by issuer, and each
    // CRLiteKey knows its issuer. So there's no need for additional
    // partition metadata.
    type PartitionMetadata = ();

    fn in_universe(&self, universe: &Self::UniverseMetadata) -> bool {
        let Some((log_id, timestamp)) = self.log_timestamp else {
            return false;
        };
        if let Some((low, high)) = universe.0.get(log_id) {
            if *low <= timestamp && timestamp <= *high {
                return true;
            }
        }
        false
    }
}

#[derive(Debug)]
pub enum ClubcardError {
    Serialize,
    Deserialize,
    UnsupportedVersion,
}

#[derive(Debug, PartialEq, Eq)]
pub enum CRLiteStatus {
    Good,
    NotCovered,
    NotEnrolled,
    Revoked,
}

impl From<Membership> for CRLiteStatus {
    fn from(membership: Membership) -> CRLiteStatus {
        match membership {
            Membership::Nonmember => CRLiteStatus::Good,
            Membership::NotInUniverse => CRLiteStatus::NotCovered,
            Membership::NoData => CRLiteStatus::NotEnrolled,
            Membership::Member => CRLiteStatus::Revoked,
        }
    }
}

pub struct CRLiteClubcard(Clubcard<W, CRLiteCoverage, ()>);

impl From<Clubcard<W, CRLiteCoverage, ()>> for CRLiteClubcard {
    fn from(inner: Clubcard<W, CRLiteCoverage, ()>) -> CRLiteClubcard {
        CRLiteClubcard(inner)
    }
}

impl AsRef<Clubcard<W, CRLiteCoverage, ()>> for CRLiteClubcard {
    fn as_ref(&self) -> &Clubcard<W, CRLiteCoverage, ()> {
        &self.0
    }
}

impl std::fmt::Display for CRLiteClubcard {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(f, "{}", self.0)?;
        writeln!(f, "{:=^80}", " Coverage ")?;
        writeln!(
            f,
            "{: ^46}  {: >16}{: >16}",
            "CT Log ID", "Min Time", "Max Time"
        )?;
        writeln!(f, "{:-<80}", "")?;
        let mut coverage_data = self
            .universe()
            .0
            .iter()
            .map(|(log_id, (low, high))| {
                (base64::prelude::BASE64_STANDARD.encode(log_id), *low, *high)
            })
            .collect::<Vec<(String, u64, u64)>>();
        coverage_data.sort_by_key(|x| u64::MAX - x.2);
        for (log_id, low, high) in coverage_data {
            writeln!(f, "{: >46},{: >16},{: >16}", log_id, low, high)?;
        }
        writeln!(f)?;
        writeln!(f, "{:=^80}", " Index ")?;
        writeln!(
            f,
            "{: ^46}{: >10}{: >10}{: >14}",
            "Issuer ID", "Exceptions", "Rank", "Bits"
        )?;
        writeln!(f, "{:-<80}", "")?;
        let mut index_data = self
            .0
            .index()
            .iter()
            .map(|(block, entry)| {
                let filter_size =
                    entry.approx_filter_m * entry.approx_filter_rank + entry.exact_filter_m;
                (
                    base64::prelude::BASE64_URL_SAFE.encode(block),
                    entry.approx_filter_rank,
                    entry.exceptions.len(),
                    filter_size,
                )
            })
            .collect::<Vec<(String, usize, usize, usize)>>();
        index_data.sort_by_key(|x| usize::MAX - x.3);

        for (issuer, rank, exceptions, filter_size) in &index_data {
            writeln!(
                f,
                "{: >46},{: >9},{: >9},{: >13}",
                issuer, exceptions, rank, filter_size
            )?;
        }
        Ok(())
    }
}

impl CRLiteClubcard {
    // Cascade-based CRLite filters use version numbers 0x0000, 0x0001, and 0x0002.
    const SERIALIZATION_VERSION: u16 = 0x0003;

    /// Serialize this clubcard.
    pub fn to_bytes(&self) -> Result<Vec<u8>, ClubcardError> {
        let mut out = u16::to_le_bytes(Self::SERIALIZATION_VERSION).to_vec();
        bincode::serialize_into(&mut out, &self.0).map_err(|_| ClubcardError::Serialize)?;
        Ok(out)
    }

    /// Deserialize a clubcard.
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, ClubcardError> {
        if bytes.len() < size_of::<u16>() {
            return Err(ClubcardError::Deserialize);
        }
        let (version_bytes, rest) = bytes.split_at(size_of::<u16>());
        let Ok(version_bytes) = version_bytes.try_into() else {
            return Err(ClubcardError::Deserialize);
        };
        let version = u16::from_le_bytes(version_bytes);
        if version != Self::SERIALIZATION_VERSION {
            return Err(ClubcardError::UnsupportedVersion);
        }
        bincode::deserialize(rest)
            .map(CRLiteClubcard)
            .map_err(|_| ClubcardError::Deserialize)
    }

    pub fn universe(&self) -> &CRLiteCoverage {
        self.0.universe()
    }

    pub fn index(&self) -> &ClubcardIndex {
        self.0.index()
    }

    pub fn contains<'a>(
        &self,
        key: &'a CRLiteKey<'a>,
        timestamps: impl Iterator<Item = (&'a LogId, Timestamp)>,
    ) -> CRLiteStatus {
        for (log_id, timestamp) in timestamps {
            let crlite_query = CRLiteQuery::new(key, Some((log_id, timestamp)));
            let status = self.0.contains(&crlite_query).into();
            if status == CRLiteStatus::NotCovered {
                continue;
            }
            return status;
        }
        CRLiteStatus::NotCovered
    }
}

impl ApproximateSizeOf for CRLiteCoverage {
    fn approximate_size_of(&self) -> usize {
        size_of::<HashMap<LogId, TimestampInterval>>()
            + self.0.len() * (size_of::<LogId>() + size_of::<TimestampInterval>())
    }
}

impl ApproximateSizeOf for CRLiteClubcard {
    fn approximate_size_of(&self) -> usize {
        self.0.approximate_size_of()
    }
}
