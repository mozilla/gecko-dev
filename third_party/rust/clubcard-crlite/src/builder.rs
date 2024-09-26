/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::query::{CRLiteCoverage, CRLiteQuery};
use clubcard::{AsQuery, Equation, Filterable};
use serde::Deserialize;
use std::collections::HashMap;

use base64::Engine;
use std::io::Read;

impl CRLiteCoverage {
    // The ct-logs.json file tells us which CT logs the ct-fetch process
    // monitored. For each log, it lists
    //   (1) the contiguous range of indices of Merkle tree leaves that
    //       ct-fetch downloaded,
    //   (2) the earliest and latest timestamps on those Merkle tree
    //       leaves, and
    //   (3) the maximum merge delay (MMD).
    //
    // Intuitively, "coverage" should reflect the [MinEntry, MaxEntry] range.
    // However, certificates only include timestamps, not indices, and
    // timestamps do not increase monotonically with leaf index.
    //
    // The timestamp in an embedded SCT is a promise from a log that it will
    // assign an index in the next MMD window. So if
    //   timestamp(Cert A) + MMD <= timestamp(Cert B)
    // then
    //   index(Cert A) < index(Cert B).
    //
    // It follows that a certificate has an index in [MinEntry, MaxEntry] if
    //   MinTimestamp + MMD <= timestamp(certificate) <= MaxTimestamp - MMD
    //
    // In the event that MinEntry = 0, we can refine this to
    //   0 <= timestamp(certificate) <= MaxTimestamp - MMD
    //
    pub fn from_mozilla_ct_logs_json<T>(reader: T) -> Self
    where
        T: Read,
    {
        #[allow(non_snake_case)]
        #[derive(Deserialize)]
        struct MozillaCtLogsJson {
            LogID: String,
            MaxTimestamp: u64,
            MinTimestamp: u64,
            MMD: u64,
            MinEntry: u64,
        }

        let mut coverage = HashMap::new();
        let json_entries: Vec<MozillaCtLogsJson> = match serde_json::from_reader(reader) {
            Ok(json_entries) => json_entries,
            _ => return CRLiteCoverage(Default::default()),
        };
        for entry in json_entries {
            let mut log_id = [0u8; 32];
            match base64::prelude::BASE64_STANDARD.decode(&entry.LogID) {
                Ok(bytes) if bytes.len() == 32 => log_id.copy_from_slice(&bytes),
                _ => continue,
            };
            let min_covered = if entry.MinEntry == 0 {
                0
            } else {
                entry.MinTimestamp + entry.MMD
            };
            let max_covered = entry.MaxTimestamp.saturating_sub(entry.MMD);
            if min_covered < max_covered {
                coverage.insert(log_id, (min_covered, max_covered));
            }
        }
        CRLiteCoverage(coverage)
    }
}

pub struct CRLiteBuilderItem {
    /// issuer spki hash
    issuer: [u8; 32],
    /// serial number. TODO: smallvec?
    serial: Vec<u8>,
    /// revocation status
    revoked: bool,
}

impl CRLiteBuilderItem {
    pub fn revoked(issuer: [u8; 32], serial: Vec<u8>) -> Self {
        Self {
            issuer,
            serial,
            revoked: true,
        }
    }

    pub fn not_revoked(issuer: [u8; 32], serial: Vec<u8>) -> Self {
        Self {
            issuer,
            serial,
            revoked: false,
        }
    }
}

impl<'a> From<&'a CRLiteBuilderItem> for CRLiteQuery<'a> {
    fn from(item: &'a CRLiteBuilderItem) -> Self {
        Self {
            issuer: &item.issuer,
            serial: &item.serial,
            log_timestamp: None,
        }
    }
}

impl AsQuery<4> for CRLiteBuilderItem {
    fn as_query(&self, m: usize) -> Equation<4> {
        CRLiteQuery::from(self).as_query(m)
    }

    fn block(&self) -> &[u8] {
        &self.issuer
    }

    fn discriminant(&self) -> &[u8] {
        &self.serial
    }
}

impl Filterable<4> for CRLiteBuilderItem {
    fn included(&self) -> bool {
        self.revoked
    }
}

#[cfg(test)]
mod tests {
    use crate::builder::*;
    use clubcard::builder::*;
    use clubcard::Membership;
    use std::collections::HashMap;

    #[test]
    fn test_crlite_clubcard() {
        let subset_sizes = [1 << 17, 1 << 16, 1 << 15, 1 << 14, 1 << 13];
        let universe_size = 1 << 18;

        let mut clubcard_builder = ClubcardBuilder::new();
        let mut approx_builders = vec![];
        for (i, n) in subset_sizes.iter().enumerate() {
            let mut r = clubcard_builder.new_approx_builder(&[i as u8; 32]);
            for j in 0usize..*n {
                let eq = CRLiteBuilderItem::revoked([i as u8; 32], j.to_le_bytes().to_vec());
                r.insert(eq);
            }
            r.set_universe_size(universe_size);
            approx_builders.push(r)
        }

        let approx_ribbons = approx_builders
            .drain(..)
            .map(ApproximateRibbon::from)
            .collect();

        println!("Approx ribbons:");
        for r in &approx_ribbons {
            println!("\t{}", r);
        }

        clubcard_builder.collect_approx_ribbons(approx_ribbons);

        let mut exact_builders = vec![];
        for (i, n) in subset_sizes.iter().enumerate() {
            let mut r = clubcard_builder.new_exact_builder(&[i as u8; 32]);
            for j in 0usize..universe_size {
                let item = if j < *n {
                    CRLiteBuilderItem::revoked([i as u8; 32], j.to_le_bytes().to_vec())
                } else {
                    CRLiteBuilderItem::not_revoked([i as u8; 32], j.to_le_bytes().to_vec())
                };
                r.insert(item);
            }
            exact_builders.push(r)
        }

        let exact_ribbons = exact_builders.drain(..).map(ExactRibbon::from).collect();

        println!("Exact ribbons:");
        for r in &exact_ribbons {
            println!("\t{}", r);
        }

        clubcard_builder.collect_exact_ribbons(exact_ribbons);

        let mut log_coverage = HashMap::new();
        log_coverage.insert([0u8; 32], (0u64, u64::MAX));

        let clubcard =
            clubcard_builder.build::<CRLiteQuery>(CRLiteCoverage(log_coverage), Default::default());
        println!("{}", clubcard);

        let sum_subset_sizes: usize = subset_sizes.iter().sum();
        let sum_universe_sizes: usize = subset_sizes.len() * universe_size;
        let min_size = (sum_subset_sizes as f64)
            * ((sum_universe_sizes as f64) / (sum_subset_sizes as f64)).log2()
            + 1.44 * ((sum_subset_sizes) as f64);
        println!("Size lower bound {}", min_size);
        println!("Checking construction");
        println!(
            "\texpecting {} included, {} excluded",
            sum_subset_sizes,
            subset_sizes.len() * universe_size - sum_subset_sizes
        );

        let mut included = 0;
        let mut excluded = 0;
        for i in 0..subset_sizes.len() {
            let issuer = [i as u8; 32];
            for j in 0..universe_size {
                let serial = j.to_le_bytes();
                let item = CRLiteQuery {
                    issuer: &issuer,
                    serial: &serial,
                    log_timestamp: None,
                };
                if clubcard.unchecked_contains(&item) {
                    included += 1;
                } else {
                    excluded += 1;
                }
            }
        }
        println!("\tfound {} included, {} excluded", included, excluded);
        assert!(sum_subset_sizes == included);
        assert!(sum_universe_sizes - sum_subset_sizes == excluded);

        // Test that querying a serial from a never-before-seen issuer results in a non-member return.
        let issuer = [subset_sizes.len() as u8; 32];
        let serial = 0usize.to_le_bytes();
        let item = CRLiteQuery {
            issuer: &issuer,
            serial: &serial,
            log_timestamp: None,
        };
        assert!(!clubcard.unchecked_contains(&item));

        assert!(subset_sizes.len() > 0 && subset_sizes[0] > 0 && subset_sizes[0] < universe_size);
        let issuer = [0u8; 32];
        let revoked_serial = 0usize.to_le_bytes();
        let nonrevoked_serial = (universe_size - 1).to_le_bytes();

        // Test that calling contains() a without a timestamp results in a NotInUniverse return
        let item = CRLiteQuery {
            issuer: &issuer,
            serial: &revoked_serial,
            log_timestamp: None,
        };
        assert!(matches!(
            clubcard.contains(&item),
            Membership::NotInUniverse
        ));

        // Test that calling contains() without a timestamp in a covered interval results in a
        // Member return.
        let log_id = [0u8; 32];
        let timestamp = (&log_id, 100);
        let item = CRLiteQuery {
            issuer: &issuer,
            serial: &revoked_serial,
            log_timestamp: Some(timestamp),
        };
        assert!(matches!(clubcard.contains(&item), Membership::Member));

        // Test that calling contains() without a timestamp in a covered interval results in a
        // Member return.
        let timestamp = (&log_id, 100);
        let item = CRLiteQuery {
            issuer: &issuer,
            serial: &nonrevoked_serial,
            log_timestamp: Some(timestamp),
        };
        assert!(matches!(clubcard.contains(&item), Membership::Nonmember));

        // Test that calling contains() without a timestamp in a covered interval results in a
        // Member return.
        let log_id = [1u8; 32];
        let timestamp = (&log_id, 100);
        let item = CRLiteQuery {
            issuer: &issuer,
            serial: &revoked_serial,
            log_timestamp: Some(timestamp),
        };
        assert!(matches!(
            clubcard.contains(&item),
            Membership::NotInUniverse
        ));
    }
}
