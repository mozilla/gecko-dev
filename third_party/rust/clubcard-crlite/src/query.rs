/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use clubcard::{ApproximateSizeOf, AsQuery, Clubcard, Equation, Membership, Queryable};
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

#[derive(Clone, Debug)]
pub struct CRLiteQuery<'a> {
    pub(crate) issuer: &'a IssuerSpkiHash,
    pub(crate) serial: &'a [u8],
    pub(crate) log_timestamp: Option<(&'a LogId, Timestamp)>,
}

impl<'a> CRLiteQuery<'a> {
    pub fn new(
        issuer: &'a IssuerSpkiHash,
        serial: &'a [u8],
        log_timestamp: Option<(&'a LogId, u64)>,
    ) -> CRLiteQuery<'a> {
        CRLiteQuery {
            issuer,
            serial,
            log_timestamp,
        }
    }
}

impl<'a> AsQuery<W> for CRLiteQuery<'a> {
    fn block(&self) -> &[u8] {
        self.issuer.as_ref()
    }

    fn as_query(&self, m: usize) -> Equation<W> {
        let mut digest = [0u8; 32];
        let mut hasher = Sha256::new();
        hasher.update(self.issuer);
        hasher.update(self.serial);
        hasher.finalize_into((&mut digest).into());

        let mut a = [0u64; 4];
        for (i, x) in digest
            .chunks_exact(8) // TODO: use array_chunks::<8>() when stable
            .map(|x| TryInto::<[u8; 8]>::try_into(x).unwrap())
            .map(u64::from_le_bytes)
            .enumerate()
        {
            a[i] = x;
        }
        a[0] |= 1;
        let s = (a[3] as usize) % max(1, m);
        Equation::homogeneous(s, a)
    }

    fn discriminant(&self) -> &[u8] {
        self.serial
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
        self.0.fmt(f)
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

    pub fn contains<'a>(
        &self,
        issuer_spki_hash: &'a IssuerSpkiHash,
        serial: &'a [u8],
        timestamps: impl Iterator<Item = (&'a LogId, Timestamp)>,
    ) -> CRLiteStatus {
        for (log_id, timestamp) in timestamps {
            let crlite_key = CRLiteQuery::new(issuer_spki_hash, serial, Some((log_id, timestamp)));
            let status = self.0.contains(&crlite_key).into();
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
