/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    clubcard::ClubcardIndex, Clubcard, ClubcardIndexEntry, Equation, Filterable, Queryable,
};
use rand::{thread_rng, Rng};
use std::collections::BTreeMap;
use std::fmt;

/// Marker type for checking that, for example, only Exact ribbons are passed to functions such as
/// Clubcard::collect_exact_ribbons.
pub struct Exact;

/// A Ribbon Filter that encodes a one bit value for every element of the associated universe.
pub type ExactRibbon<const W: usize, T> = Ribbon<W, T, Exact>;

/// Marker type for checking that, for example, only Approximate ribbons are passed to functions such as
/// Clubcard::collect_approximate_ribbons.
pub struct Approximate;

/// A Ribbon Filter that identifies a subset of a universe with a false positive rate of
/// roughly |subset| / |universe|.
pub type ApproximateRibbon<const W: usize, T> = Ribbon<W, T, Approximate>;

/// A RibbonBuilder collects a set of items for insertion into a Ribbon. If the optional filter is
/// provided, then only items that are contained in the filter will be inserted.
pub struct RibbonBuilder<'a, const W: usize, T: Filterable<W>> {
    /// block id.
    id: Vec<u8>,
    /// items to be inserted.
    items: Vec<T>,
    /// filter for pruning insertions.
    filter: Option<&'a PartitionedRibbonFilter<W, T, Approximate>>,
    /// size of the universe that contains self.items
    universe_size: usize,
    /// Whether queries against this ribbon indicate membership in R (inverted = false) or
    /// membership in U \ R (inverted = true).
    inverted: bool,
}

impl<'a, const W: usize, T: Filterable<W>> RibbonBuilder<'a, W, T> {
    fn new(
        id: &[u8],
        filter: Option<&'a PartitionedRibbonFilter<W, T, Approximate>>,
    ) -> RibbonBuilder<'a, W, T> {
        RibbonBuilder {
            id: AsRef::<[u8]>::as_ref(id).to_vec(),
            items: vec![],
            filter,
            universe_size: 0,
            inverted: false,
        }
    }

    /// Queue `item` for insertion into the ribbon (if it is contained in the provided filter).
    pub fn insert(&mut self, item: T) {
        if let Some(filter) = self.filter {
            if filter.contains(&item) {
                self.items.push(item);
            }
        } else {
            self.items.push(item);
        }
    }

    /// Set the size of the universe. This only needs to be called if you
    /// are constructing an ApproximateRibbon.
    pub fn set_universe_size(&mut self, universe_size: usize) {
        self.universe_size = universe_size;
    }
}

impl<'a, const W: usize, T: Filterable<W>> From<RibbonBuilder<'a, W, T>>
    for ApproximateRibbon<W, T>
{
    /// Denote the inserted set by R and the universe by U.
    /// The ribbon returned by ApproximateRibbon::from encodes a function f : U -> {0, 1} where
    /// f(x) = 0 if and only if x is in R union S where S is a (random) subset of U \ R of size
    /// ~|R|. In other words, the ribbon solves the approximate membership query problem with a
    /// false positive rate roughly 2^-r = |R| / (|U| - |R|).
    /// The size of this ribbon is proportional to r|R|.
    fn from(mut builder: RibbonBuilder<'a, W, T>) -> ApproximateRibbon<W, T> {
        assert!(builder.items.len() <= builder.universe_size);
        if builder.items.len() == builder.universe_size {
            ApproximateRibbon::new(&builder.id, 0, builder.universe_size, !builder.inverted)
        } else {
            let mut out = ApproximateRibbon::new(
                &builder.id,
                builder.items.len(),
                builder.universe_size,
                builder.inverted,
            );
            for item in builder.items.drain(..) {
                out.insert(item);
            }
            // Insertions should not fail for a homogeneous system.
            assert!(out.exceptions.is_empty());
            out
        }
    }
}

impl<'a, const W: usize, T: Filterable<W>> From<RibbonBuilder<'a, W, T>> for ExactRibbon<W, T> {
    /// Denote the inserted set by R and the universe by U.
    /// The ribbon returned by ExactRibbon::from encodes the function "f(x) = 0 iff x in R". The
    /// size of this ribbon is proportional to |U|. In the typical use case, the set U is the
    /// result of filtering a larger universe with a false positive rate of 2^-r. This allows for
    /// exact encoding of R-membership using a pair of filters of total size ~(r+2)|R|.
    fn from(mut builder: RibbonBuilder<'a, W, T>) -> ExactRibbon<W, T> {
        assert!(builder.universe_size == 0 || builder.universe_size == builder.items.len());
        if let Some(filter) = builder.filter {
            if filter.block_is_empty(&builder.id) {
                // The approximate filter is empty, so it gives a definitive result on every
                // item and there's nothing to encode in the exact filter.
                return ExactRibbon::new(&builder.id, 0, filter.block_is_inverted(&builder.id));
            }
        }
        let mut out = ExactRibbon::new(&builder.id, builder.items.len(), builder.inverted);
        // By inserting the included items first, we ensure that any exceptions that occur during
        // insertion are for excluded items.
        let mut excluded = vec![];
        for item in builder.items.drain(..) {
            if item.included() {
                out.insert(item);
            } else {
                excluded.push(item);
            }
        }
        for item in excluded.drain(..) {
            out.insert(item);
        }
        out
    }
}

/// A compact representation of a linear system AX = B
pub struct Ribbon<const W: usize, T: Filterable<W>, ApproxOrExact> {
    /// A block identifier. Used to build an index for partitioned filters.
    id: Vec<u8>,
    /// The overhead.
    epsilon: f64,
    /// Equal to (1+epsilon) * |R|
    m: usize,
    /// The rank is round(-log2(subset_size / (universe_size - subset_size)))
    rank: usize,
    /// A linear system in which each equation has s in {0, ..., m-1}
    rows: Vec<Equation<W>>,
    /// A (typically short) list of items that failed insertion
    exceptions: Vec<T>,
    /// Whether queries against this ribbon indicate membership in R (inverted = false) or
    /// membership in U \ R (inverted = true).
    inverted: bool,
    /// Marker for whether this is an Approximate or an Exact filter.
    phantom: std::marker::PhantomData<ApproxOrExact>,
}

impl<const W: usize, T: Filterable<W>, ApproxOrExact> fmt::Display for Ribbon<W, T, ApproxOrExact> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "ribbon({:?}): m: {}, rows: {}, rank: {}, exceptions: {}, epsilon: {}, overhead {}",
            self.id,
            self.m,
            self.rows.len(),
            self.rank,
            self.exceptions.len(),
            self.epsilon,
            (self.rows.iter().filter(|eq| eq.is_zero()).count() as f64 / (self.rows.len() as f64))
        )
    }
}

impl<const W: usize, T: Filterable<W>> ApproximateRibbon<W, T> {
    /// Construct an empty ribbon to encode a set R of size `subset_size` in a universe U of size
    /// `universe_size`.
    fn new(id: &[u8], subset_size: usize, universe_size: usize, inverted: bool) -> Self {
        assert!(subset_size <= universe_size);

        // TODO: Tune epsilon as a function of the inputs. Numerical experiments?
        let epsilon = 0.02;
        let m = ((1.0 + epsilon) * (subset_size as f64)).floor() as usize;

        let rank = if subset_size == 0 || 2 * subset_size >= universe_size {
            0
        } else {
            (((universe_size - subset_size) as f64) / (subset_size as f64))
                .log2()
                .floor() as usize
        };

        Ribbon {
            id: AsRef::<[u8]>::as_ref(id).to_vec(),
            rows: vec![Equation::zero(); m],
            m,
            epsilon,
            rank,
            exceptions: vec![],
            inverted,
            phantom: std::marker::PhantomData,
        }
    }
}

impl<const W: usize, T: Filterable<W>> ExactRibbon<W, T> {
    /// Construct an empty ribbon to encode a set R of size `subset_size` in a universe U of size
    /// `universe_size`.
    fn new(id: &impl AsRef<[u8]>, size: usize, inverted: bool) -> Self {
        // TODO: Tune epsilon as a function of the inputs. Numerical experiments?
        let epsilon = 0.02;
        let m = ((1.0 + epsilon) * (size as f64)).floor() as usize;

        Ribbon {
            id: AsRef::<[u8]>::as_ref(id).to_vec(),
            rows: vec![Equation::zero(); m],
            m,
            epsilon,
            rank: 1,
            exceptions: vec![],
            inverted,
            phantom: std::marker::PhantomData,
        }
    }
}

impl<const W: usize, T: Filterable<W>, ApproxOrExact> Ribbon<W, T, ApproxOrExact> {
    /// Hash the item to an Equation and insert it into the system.
    fn insert(&mut self, item: T) -> bool {
        let mut eq = item.as_query(self.m);
        eq.b = if item.included() { 0 } else { 1 };
        assert!(eq.is_zero() || eq.a[0] & 1 == 1);
        let rv = self.insert_equation(eq);
        if !rv {
            self.exceptions.push(item)
        }
        rv
    }

    /// Insert an equation into the system using Algorithm 1 from <https://arxiv.org/pdf/2103.02515>
    fn insert_equation(&mut self, mut eq: Equation<W>) -> bool {
        loop {
            if eq.is_zero() {
                return eq.b == 0; /* redundant (b=0) or inconsistent (b!=0) */
            }
            if eq.s >= self.rows.len() {
                // TODO: could be smarter here
                self.rows.resize_with(eq.s + 1, Equation::zero);
            }
            let cur = &mut self.rows[eq.s];
            if cur.is_zero() {
                *cur = eq;
                return true; /* inserted */
            }
            eq.add(cur);
        }
    }

    /// Solve the system using back-substitution. If this is a block in a larger system, the `tail`
    /// argument should be set to the the solution vector for the block to the right of this one.
    fn solve(&self, tail: &[u64]) -> Vec<u64> {
        let mut z = vec![0u64; ((self.rows.len() + 63) / 64) + tail.len()];
        // insert tail into z starting at bit self.rows.len()
        let k = self.rows.len() / 64;
        let p = self.rows.len() % 64;
        if p == 0 {
            z[k..(tail.len() + k)].copy_from_slice(tail);
        } else {
            for i in 0..tail.len() {
                z[k + i] |= tail[i] << p;
                z[k + i + 1] = tail[i] >> (64 - p)
            }
        }

        // Solve by back substitution
        for i in (0..self.rows.len()).rev() {
            let limb = i / 64;
            let pos = i % 64;
            let z_i = if self.rows[i].is_zero() {
                // Row i has a zero in column i, so we're free to choose.
                // TODO: We want multiple calls to solve() to give a different
                // solutions (when the system is suitably under-determined),
                // but it might be nice if this was deterministic.
                thread_rng().gen::<u8>()
            } else {
                // Let z' be the vector we get by setting bit i of z to z'_i.
                // Since z_i is zero, and row i has a one in column i, we have
                // row_i(z') = z'_i ^ row_i(z).
                // We want row_i(z') = b, so we must choose
                // z'_i = row_i(z) ^ b.
                self.rows[i].eval(&z) ^ self.rows[i].b
            };
            z[limb] |= ((z_i & 1) as u64) << pos;
        }
        z
    }
}

#[derive(Debug)]
struct PartitionedRibbonFilterIndexEntry {
    offset: usize,
    m: usize,
    rank: usize,
    exceptions: Vec<Vec<u8>>,
    inverted: bool,
}

type PartitionedRibbonFilterIndex =
    BTreeMap</* block id */ Vec<u8>, PartitionedRibbonFilterIndexEntry>;

/// A solution to a ribbon system, along with metadata necessary for querying it.
struct PartitionedRibbonFilter<const W: usize, T: Filterable<W>, ApproxOrExact> {
    index: PartitionedRibbonFilterIndex,
    solution: Vec<Vec<u64>>,
    phantom: std::marker::PhantomData<T>,
    phantom2: std::marker::PhantomData<ApproxOrExact>,
}

impl<const W: usize, T: Filterable<W>, ApproxOrExact> fmt::Display
    for PartitionedRibbonFilter<W, T, ApproxOrExact>
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "PartitionedRibbonFilter({:?})", self.index)
    }
}

impl<const W: usize, T: Filterable<W>, Approximate> PartitionedRibbonFilter<W, T, Approximate> {
    fn block_is_empty(&self, block: &[u8]) -> bool {
        let Some(entry) = self.index.get(block) else {
            return false;
        };
        entry.m == 0
    }

    fn block_is_inverted(&self, block: &[u8]) -> bool {
        let Some(entry) = self.index.get(block) else {
            return false;
        };
        entry.inverted
    }

    /// Check if this filter contains the given item in the given block.
    fn contains(&self, item: &T) -> bool {
        let Some(entry) = self.index.get(item.block()) else {
            return false;
        };
        let result = (|| {
            // Empty blocks do not contain anything,
            // despite having inner product 0 with everything.
            if entry.m == 0 {
                return false;
            }
            let mut eq = item.as_query(entry.m);
            eq.s += entry.offset;
            for i in 0..entry.rank {
                if eq.eval(&self.solution[i]) != 0 {
                    return false;
                }
            }
            for exception in &entry.exceptions {
                if exception == item.discriminant() {
                    return false;
                }
            }
            true
        })();
        result ^ entry.inverted
    }
}

impl<const W: usize, T: Filterable<W>, ApproxOrExact> From<Vec<Ribbon<W, T, ApproxOrExact>>>
    for PartitionedRibbonFilter<W, T, ApproxOrExact>
{
    fn from(
        mut blocks: Vec<Ribbon<W, T, ApproxOrExact>>,
    ) -> PartitionedRibbonFilter<W, T, ApproxOrExact> {
        // Sort ribbons by descending rank (descending simplifies indexing).
        blocks.sort_unstable_by(|a, b| b.rank.cmp(&a.rank));

        // Solve the (block) system.
        // The blocks are sorted by descending rank. We need at least one solution (i.e. column
        // vector) per block, but we need no more than i solutions for a block of rank i.
        // Concretely, suppose the ranks are [4, 2, 1, 0]. Then our solution can look like
        //      block 0: | | | |
        //      block 1: | | 0 0
        //      block 2: | 0 0 0
        //      block 3: | 0 0 0
        // Since we serialize the block identifiers, offsets, and ranks in the final filter, we
        // don't need to encode the zeros.
        let mut solution = vec![];
        for i in 0..blocks.first().map_or(0, |first| first.rank) {
            // Back substitution across blocks.
            let mut tail = vec![];
            for j in (0..blocks.len()).rev() {
                if blocks[j].rank > i {
                    tail = blocks[j].solve(&tail);
                }
            }
            solution.push(tail);
        }

        // construct the index---a map from a block identifier to that
        // block's offset in the solution vector.
        let mut index = PartitionedRibbonFilterIndex::new();
        let mut offset = 0;
        for block in &blocks {
            let exceptions = block
                .exceptions
                .iter()
                .map(|x| x.discriminant().to_vec())
                .collect();
            index.insert(
                block.id.clone(),
                PartitionedRibbonFilterIndexEntry {
                    offset,
                    m: block.m,
                    rank: block.rank,
                    exceptions,
                    inverted: block.inverted,
                },
            );
            offset += block.rows.len();
        }

        PartitionedRibbonFilter {
            index,
            solution,
            phantom: std::marker::PhantomData,
            phantom2: std::marker::PhantomData,
        }
    }
}

/// A pair of ribbon filters that, together, solve the exact membership query problem.
pub struct ClubcardBuilder<const W: usize, T: Filterable<W>> {
    /// An approximate membership query filter to whittle down the universe
    /// to a managable size.
    approx_filter: Option<PartitionedRibbonFilter<W, T, Approximate>>,
    /// An exact membership query filter to confirm membership in R for items that
    /// pass through the approximate filter.
    exact_filter: Option<PartitionedRibbonFilter<W, T, Exact>>,
}

impl<const W: usize, T: Filterable<W>> Default for ClubcardBuilder<W, T> {
    fn default() -> Self {
        ClubcardBuilder {
            approx_filter: None,
            exact_filter: None,
        }
    }
}

impl<const W: usize, T: Filterable<W>> ClubcardBuilder<W, T> {
    pub fn new() -> Self {
        ClubcardBuilder::default()
    }

    pub fn new_approx_builder(&self, block: &[u8]) -> RibbonBuilder<'static, W, T> {
        assert!(self.approx_filter.is_none());
        RibbonBuilder::new(block, None)
    }

    pub fn new_exact_builder<'a>(&'a self, block: &[u8]) -> RibbonBuilder<'a, W, T> {
        RibbonBuilder::new(block, self.approx_filter.as_ref())
    }

    pub fn collect_approx_ribbons(&mut self, ribbons: Vec<ApproximateRibbon<W, T>>) {
        self.approx_filter = Some(PartitionedRibbonFilter::from(ribbons));
    }

    pub fn collect_exact_ribbons(&mut self, ribbons: Vec<Ribbon<W, T, Exact>>) {
        self.exact_filter = Some(PartitionedRibbonFilter::from(ribbons));
    }

    pub fn build<U: Queryable<W>>(
        self,
        universe: U::UniverseMetadata,
        partition: U::PartitionMetadata,
    ) -> Clubcard<W, U::UniverseMetadata, U::PartitionMetadata> {
        let mut index: ClubcardIndex = BTreeMap::new();

        assert!(self.approx_filter.is_some());
        let approx_filter = self.approx_filter.unwrap();
        for (block, entry) in approx_filter.index {
            let meta = ClubcardIndexEntry {
                approx_filter_offset: entry.offset,
                approx_filter_m: entry.m,
                approx_filter_rank: entry.rank,
                exact_filter_offset: 0,
                exact_filter_m: 0,
                inverted: entry.inverted,
                exceptions: entry.exceptions,
            };
            index.insert(block, meta);
        }

        assert!(self.exact_filter.is_some());
        let mut exact_filter = self.exact_filter.unwrap();
        for (block, entry) in exact_filter.index {
            assert!(entry.rank == 1);
            let meta = index.get_mut(&block).unwrap();
            meta.exact_filter_offset = entry.offset;
            meta.exact_filter_m = entry.m;
            assert!(meta.inverted == entry.inverted);
            meta.exceptions.extend(entry.exceptions);
        }

        assert!(exact_filter.solution.len() == 1);
        let exact_filter = exact_filter.solution.pop().unwrap();

        Clubcard {
            universe,
            partition,
            index,
            approx_filter: approx_filter.solution,
            exact_filter,
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::builder::*;
    use crate::*;
    use rand::distributions::{Distribution, Uniform};
    use rand::Rng;

    // Construct the equation a(x) = x_i
    fn std_eq<const W: usize>(i: usize) -> Equation<W> {
        let mut a = [0u64; W];
        a[0] = 1;
        Equation::homogeneous(i, a)
    }

    // Construct an random aligned equation using the given distribution for s.
    fn rand<const W: usize>(s_dist: &impl Distribution<usize>) -> Equation<W> {
        let mut rng = rand::thread_rng();
        let s = s_dist.sample(&mut rng);
        let mut a = [0u64; W];
        for a_i in a.iter_mut() {
            *a_i = rng.gen();
        }
        a[0] |= 1;
        Equation::inhomogeneous(s, a, rng.gen::<u8>() & 1)
    }

    impl<const W: usize> AsQuery<W> for Equation<W> {
        fn as_query(&self, _m: usize) -> Equation<W> {
            self.clone()
        }

        fn block(&self) -> &[u8] {
            &[]
        }

        fn discriminant(&self) -> &[u8] {
            unsafe { std::mem::transmute(&self.a[..]) }
        }
    }

    impl<const W: usize> Filterable<W> for Equation<W> {
        fn included(&self) -> bool {
            self.b == 0
        }
    }

    impl<const W: usize> Queryable<W> for Equation<W> {
        type UniverseMetadata = ();
        type PartitionMetadata = ();

        fn in_universe(&self, _meta: &Self::UniverseMetadata) -> bool {
            true
        }
    }

    #[test]
    fn test_solve_identity() {
        let n = 1024;
        let mut builder = RibbonBuilder::new(&[], None);
        for i in 0usize..n {
            let eq: Equation<1> = std_eq(i);
            builder.insert(eq);
        }
        let ribbon = ExactRibbon::from(builder);
        let filter = PartitionedRibbonFilter::from(vec![ribbon]);
        for i in 0usize..n {
            let eq: Equation<1> = std_eq(i);
            assert!(eq.eval(&filter.solution[0]) == 0);
        }
    }

    #[test]
    fn test_solve_empty() {
        let builder = RibbonBuilder::<4, Equation<4>>::new(&[0], None);
        let ribbon = ApproximateRibbon::from(builder);
        let filter = PartitionedRibbonFilter::from(vec![ribbon]);
        assert!(!filter.contains(&std_eq(0)));
    }

    #[test]
    fn test_solve_random() {
        let n = 1024;
        const W: usize = 2;
        let mut r = Ribbon::<W, Equation<W>, Exact>::new(&[0], n, false);
        let mut s_dist = Uniform::new(0, r.m);
        let mut eqs = Vec::with_capacity(n);
        for _ in 0..n {
            let eq = rand(&mut s_dist);
            eqs.push(eq.clone());
            r.insert(eq);
        }
        let x = r.solve(&[]);
        for eq in &eqs {
            assert!(eq.eval(&x) == eq.b);
        }
    }

    #[test]
    fn test_total_approx_filter() {
        // test that approximate filters that encode R=U are encoded
        // as a zero-length solution vector with m=0 and inverted=true
        // in the metadata.
        let n = 1024;
        let mut approx_builder = RibbonBuilder::new(&[], None);
        approx_builder.set_universe_size(n);
        for i in 0usize..n {
            let eq: Equation<1> = std_eq(i);
            approx_builder.insert(eq);
        }

        let approx_ribbon = ApproximateRibbon::from(approx_builder);
        let approx_filter = PartitionedRibbonFilter::from(vec![approx_ribbon]);
        let approx_index_entry = approx_filter
            .index
            .get(&vec![])
            .expect("should have metadata");
        assert!(approx_index_entry.m == 0);
        assert!(approx_index_entry.rank == 0);
        assert!(approx_index_entry.exceptions.is_empty());
        assert!(approx_index_entry.inverted);
        for i in 0usize..n {
            let eq = std_eq(i);
            assert!(approx_filter.contains(&eq));
        }
        assert!(approx_filter.solution.len() == 0);

        let mut exact_builder = RibbonBuilder::new(&[], Some(&approx_filter));
        for i in 0usize..n {
            let mut eq = std_eq(i);
            eq.b = 0;
            exact_builder.insert(eq);
        }
        let exact_ribbon = ExactRibbon::from(exact_builder);
        let exact_filter = PartitionedRibbonFilter::from(vec![exact_ribbon]);
        let exact_index_entry = exact_filter
            .index
            .get(&vec![])
            .expect("should have metadata");
        assert!(exact_index_entry.m == 0);
        assert!(exact_index_entry.rank == 1);
        assert!(exact_index_entry.exceptions.is_empty());
        assert!(exact_index_entry.inverted);
        for i in 0usize..n {
            let eq = std_eq(i);
            assert!(exact_filter.contains(&eq));
        }
        assert!(exact_filter.solution.len() == 1);
        assert!(exact_filter.solution[0].len() == 0);
    }

    #[test]
    fn test_rank_0_approx_filter() {
        let n = 1024;
        let mut builder = RibbonBuilder::new(&[], None);
        builder.set_universe_size(n);
        for i in 0usize..768 {
            let eq: Equation<1> = std_eq(i);
            builder.insert(eq);
        }

        let ribbon = ApproximateRibbon::from(builder);
        let filter = PartitionedRibbonFilter::from(vec![ribbon]);
        let entry = filter.index.get(&vec![]).expect("should have metadata");
        assert!(entry.rank == 0);
        assert!(!entry.inverted);
        assert!(filter.solution.len() == 0);
        for i in 0usize..n {
            let eq = std_eq(i);
            assert!(filter.contains(&eq));
        }
    }
}
