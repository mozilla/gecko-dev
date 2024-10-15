/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Compress a set of u32 integers and output the (approximate) size of compressed bytes.
/// # Arguments
///
/// * `filepath` - The input file containing integers in range [0, max(a_i)].
/// ```txt
/// a_1 \n
/// ...
/// a_n \n
/// ```
/// The universe size will be the maximum value of the input file.
///
/// # Usage
///
/// To run this example, use the following command:
/// ```bash
/// cargo run --features builder --example uint_no_partition
/// ```
///

use clubcard::*;
use sha2::{Digest, Sha256};
use std::cmp::max;
use std::collections::HashSet;
use std::{env, fs, io, process};
use std::io::BufRead;
use clubcard::builder::{ApproximateRibbon, ClubcardBuilder, ExactRibbon};

struct Universe {
    bound: u32,
}
const W: usize = 4;
const BLOCK: [u8; 0] = [];

struct Int {
    val: [u8; 4], // u32
    included: bool, // whether it's in the subset
}

impl Int {
    fn new(num: u32, included: bool) -> Int {
        Int {
            val: num.to_le_bytes(),
            included,
        }
    }
}

impl ApproximateSizeOf for Universe {}

impl AsQuery<W> for Int {
    fn as_query(&self, m: usize) -> Equation<W> {
        let mut digest = [0u8; 32];
        let mut hasher = Sha256::new();
        hasher.update(&self.val);
        hasher.finalize_into((&mut digest).into());

        let mut a = [0u64; W]; // block of columns
        for (i, x) in digest
            .chunks_exact(8)
            .map(|x| TryInto::<[u8; 8]>::try_into(x).unwrap())
            .map(u64::from_le_bytes)
            .enumerate()
        {
            a[i] = x;
        }

        a[0] |= 1;
        let s = (a[W - 1] as usize) % max(1, m); //
        let b = if self.included { 0 } else { 1 };
        Equation::inhomogeneous(s, a, b)
    }

    fn block(&self) -> &[u8] {
        &BLOCK
    }

    fn discriminant(&self) -> &[u8] {
        &self.val
    }
}

impl Filterable<W> for Int {
    fn included(&self) -> bool {
        self.included
    }
}

impl Queryable<W> for Int {
    type UniverseMetadata = Universe;
    type PartitionMetadata = ();

    fn in_universe(&self, meta: &Self::UniverseMetadata) -> bool {
        u32::from_le_bytes(self.val) < meta.bound
    }
}

// Parse the file into a set of integers, return the hash set and maximum element + 1 in set
fn parse_args() -> (u32, HashSet<u32>) {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        println!("Please specify the name of input file.");
        process::exit(1);
    }
    let file_path = &args[1];
    let file = fs::File::open(file_path).expect("Error opening the file.");
    let mut lines = io::BufReader::new(file).lines();

    let mut numbers = HashSet::new();
    let mut max_val = 0;

    while let Some(Ok(line)) = lines.next() {
        let number = line.trim().parse::<u32>().expect("Failed to parse 32-bit integer");
        if number > max_val {
            max_val = number;
        }
        numbers.insert(number);
    }

    (max_val + 1, numbers)
}

fn main() {
    let (universe_size, numbers) = parse_args();

    let mut clubcard_builder = ClubcardBuilder::new();
    let mut approx_builder = clubcard_builder.new_approx_builder(&BLOCK);

    // Build approx filter
    for num in &numbers {
        let int = Int::new(num.clone(), true);
        approx_builder.insert(int)
    }
    approx_builder.set_universe_size(universe_size.try_into().unwrap());
    clubcard_builder.collect_approx_ribbons(vec![ApproximateRibbon::from(approx_builder)]);

    // Build exact filter
    let mut exact_builder = clubcard_builder.new_exact_builder(&BLOCK);
    for num in 0..universe_size {
        let int = Int::new(num.clone(), numbers.contains(&num));
        exact_builder.insert(int);
    }
    clubcard_builder.collect_exact_ribbons(vec![ExactRibbon::from(exact_builder)]);

    let clubcard = clubcard_builder.build::<Int>(
        Universe { bound: universe_size },
        (),
    );

    println!("Generated {}", clubcard);
    println!("The size of compressed set (of {} elements) with max value {} is {} bytes.",
             numbers.len(), universe_size - 1, clubcard.approximate_size_of());
}