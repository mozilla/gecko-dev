// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::{fmt::Debug, hash::Hash};
use mls_rs_codec::{MlsDecode, MlsEncode};

use super::node::LeafIndex;

pub trait TreeIndex:
    Send + Sync + Eq + Clone + Debug + Default + MlsEncode + MlsDecode + Hash + Ord
{
    fn root(&self) -> Self;

    fn left_unchecked(&self) -> Self;
    fn right_unchecked(&self) -> Self;

    fn parent_sibling(&self, leaf_count: &Self) -> Option<ParentSibling<Self>>;
    fn is_leaf(&self) -> bool;
    fn is_in_tree(&self, root: &Self) -> bool;

    #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
    fn zero() -> Self;

    #[cfg(any(feature = "secret_tree_access", feature = "private_message", test))]
    fn left(&self) -> Option<Self> {
        (!self.is_leaf()).then(|| self.left_unchecked())
    }

    #[cfg(any(feature = "secret_tree_access", feature = "private_message", test))]
    fn right(&self) -> Option<Self> {
        (!self.is_leaf()).then(|| self.right_unchecked())
    }

    fn direct_copath(&self, leaf_count: &Self) -> Vec<CopathNode<Self>> {
        let root = leaf_count.root();

        if !self.is_in_tree(&root) {
            return Vec::new();
        }

        let mut path = Vec::new();
        let mut parent = self.clone();

        while let Some(ps) = parent.parent_sibling(leaf_count) {
            path.push(CopathNode::new(ps.parent.clone(), ps.sibling));
            parent = ps.parent;
        }

        path
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct CopathNode<T> {
    pub path: T,
    pub copath: T,
}

impl<T: Clone + PartialEq + Eq + core::fmt::Debug> CopathNode<T> {
    pub fn new(path: T, copath: T) -> CopathNode<T> {
        CopathNode { path, copath }
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct ParentSibling<T> {
    pub parent: T,
    pub sibling: T,
}

impl<T: Clone + PartialEq + Eq + core::fmt::Debug> ParentSibling<T> {
    pub fn new(parent: T, sibling: T) -> ParentSibling<T> {
        ParentSibling { parent, sibling }
    }
}

macro_rules! impl_tree_stdint {
    ($t:ty) => {
        impl TreeIndex for $t {
            fn root(&self) -> $t {
                *self - 1
            }

            /// Panicks if `x` is even in debug, overflows in release.
            fn left_unchecked(&self) -> Self {
                *self ^ (0x01 << (self.trailing_ones() - 1))
            }

            /// Panicks if `x` is even in debug, overflows in release.
            fn right_unchecked(&self) -> Self {
                *self ^ (0x03 << (self.trailing_ones() - 1))
            }

            fn parent_sibling(&self, leaf_count: &Self) -> Option<ParentSibling<Self>> {
                if self == &leaf_count.root() {
                    return None;
                }

                let lvl = self.trailing_ones();
                let p = (self & !(1 << (lvl + 1))) | (1 << lvl);

                let s = if *self < p {
                    p.right_unchecked()
                } else {
                    p.left_unchecked()
                };

                Some(ParentSibling::new(p, s))
            }

            fn is_leaf(&self) -> bool {
                self & 1 == 0
            }

            fn is_in_tree(&self, root: &Self) -> bool {
                *self <= 2 * root
            }

            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            fn zero() -> Self {
                0
            }
        }
    };
}

impl_tree_stdint!(u32);

#[cfg(test)]
impl_tree_stdint!(u64);

pub fn leaf_lca_level(x: u32, y: u32) -> u32 {
    let mut xn = x;
    let mut yn = y;
    let mut k = 0;

    while xn != yn {
        xn >>= 1;
        yn >>= 1;
        k += 1;
    }

    k
}

pub fn subtree(x: u32) -> (LeafIndex, LeafIndex) {
    let breadth = 1 << x.trailing_ones();
    (
        LeafIndex((x + 1 - breadth) >> 1),
        LeafIndex(((x + breadth) >> 1) + 1),
    )
}

pub struct BfsIterTopDown {
    level: usize,
    mask: usize,
    level_end: usize,
    ctr: usize,
}

impl BfsIterTopDown {
    pub fn new(num_leaves: usize) -> Self {
        let depth = num_leaves.trailing_zeros() as usize;
        Self {
            level: depth + 1,
            mask: (1 << depth) - 1,
            level_end: 1,
            ctr: 0,
        }
    }
}

impl Iterator for BfsIterTopDown {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        if self.ctr == self.level_end {
            if self.level == 1 {
                return None;
            }
            self.level_end = (((self.level_end - 1) << 1) | 1) + 1;
            self.level -= 1;
            self.ctr = 0;
            self.mask >>= 1;
        }
        let res = Some((self.ctr << self.level) | self.mask);
        self.ctr += 1;
        res
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use itertools::Itertools;
    use serde::{Deserialize, Serialize};

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[derive(Serialize, Deserialize)]
    struct TestCase {
        n_leaves: u32,
        n_nodes: u32,
        root: u32,
        left: Vec<Option<u32>>,
        right: Vec<Option<u32>>,
        parent: Vec<Option<u32>>,
        sibling: Vec<Option<u32>>,
    }

    pub fn node_width(n: u32) -> u32 {
        if n == 0 {
            0
        } else {
            2 * (n - 1) + 1
        }
    }

    #[test]
    fn test_bfs_iterator() {
        let expected = [7, 3, 11, 1, 5, 9, 13, 0, 2, 4, 6, 8, 10, 12, 14];
        let bfs = BfsIterTopDown::new(8);
        assert_eq!(bfs.collect::<Vec<_>>(), expected);
    }

    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_tree_math_test_cases() -> Vec<TestCase> {
        let mut test_cases = Vec::new();

        for log_n_leaves in 0..8 {
            let n_leaves = 1 << log_n_leaves;
            let n_nodes = node_width(n_leaves);
            let left = (0..n_nodes).map(|x| x.left()).collect::<Vec<_>>();
            let right = (0..n_nodes).map(|x| x.right()).collect::<Vec<_>>();

            let (parent, sibling) = (0..n_nodes)
                .map(|x| {
                    x.parent_sibling(&n_leaves)
                        .map(|ps| (ps.parent, ps.sibling))
                        .unzip()
                })
                .unzip();

            test_cases.push(TestCase {
                n_leaves,
                n_nodes,
                root: n_leaves.root(),
                left,
                right,
                parent,
                sibling,
            })
        }

        test_cases
    }

    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(tree_math, generate_tree_math_test_cases())
    }

    #[test]
    fn test_tree_math() {
        let test_cases = load_test_cases();

        for case in test_cases {
            assert_eq!(node_width(case.n_leaves), case.n_nodes);
            assert_eq!(case.n_leaves.root(), case.root);

            for x in 0..case.n_nodes {
                assert_eq!(x.left(), case.left[x as usize]);
                assert_eq!(x.right(), case.right[x as usize]);

                let (p, s) = x
                    .parent_sibling(&case.n_leaves)
                    .map(|ps| (ps.parent, ps.sibling))
                    .unzip();

                assert_eq!(p, case.parent[x as usize]);
                assert_eq!(s, case.sibling[x as usize]);
            }
        }
    }

    #[test]
    fn test_direct_path() {
        let expected: Vec<Vec<u32>> = [
            [0x01, 0x03, 0x07, 0x0f].to_vec(),
            [0x03, 0x07, 0x0f].to_vec(),
            [0x01, 0x03, 0x07, 0x0f].to_vec(),
            [0x07, 0x0f].to_vec(),
            [0x05, 0x03, 0x07, 0x0f].to_vec(),
            [0x03, 0x07, 0x0f].to_vec(),
            [0x05, 0x03, 0x07, 0x0f].to_vec(),
            [0x0f].to_vec(),
            [0x09, 0x0b, 0x07, 0x0f].to_vec(),
            [0x0b, 0x07, 0x0f].to_vec(),
            [0x09, 0x0b, 0x07, 0x0f].to_vec(),
            [0x07, 0x0f].to_vec(),
            [0x0d, 0x0b, 0x07, 0x0f].to_vec(),
            [0x0b, 0x07, 0x0f].to_vec(),
            [0x0d, 0x0b, 0x07, 0x0f].to_vec(),
            [].to_vec(),
            [0x11, 0x13, 0x17, 0x0f].to_vec(),
            [0x13, 0x17, 0x0f].to_vec(),
            [0x11, 0x13, 0x17, 0x0f].to_vec(),
            [0x17, 0x0f].to_vec(),
            [0x15, 0x13, 0x17, 0x0f].to_vec(),
            [0x13, 0x17, 0x0f].to_vec(),
            [0x15, 0x13, 0x17, 0x0f].to_vec(),
            [0x0f].to_vec(),
            [0x19, 0x1b, 0x17, 0x0f].to_vec(),
            [0x1b, 0x17, 0x0f].to_vec(),
            [0x19, 0x1b, 0x17, 0x0f].to_vec(),
            [0x17, 0x0f].to_vec(),
            [0x1d, 0x1b, 0x17, 0x0f].to_vec(),
            [0x1b, 0x17, 0x0f].to_vec(),
            [0x1d, 0x1b, 0x17, 0x0f].to_vec(),
        ]
        .to_vec();

        for (i, item) in expected.iter().enumerate() {
            let path = (i as u32)
                .direct_copath(&16)
                .into_iter()
                .map(|cp| cp.path)
                .collect_vec();

            assert_eq!(item, &path)
        }
    }

    #[test]
    fn test_copath_path() {
        let expected: Vec<Vec<u32>> = [
            [0x02, 0x05, 0x0b, 0x17].to_vec(),
            [0x05, 0x0b, 0x17].to_vec(),
            [0x00, 0x05, 0x0b, 0x17].to_vec(),
            [0x0b, 0x17].to_vec(),
            [0x06, 0x01, 0x0b, 0x17].to_vec(),
            [0x01, 0x0b, 0x17].to_vec(),
            [0x04, 0x01, 0x0b, 0x17].to_vec(),
            [0x17].to_vec(),
            [0x0a, 0x0d, 0x03, 0x17].to_vec(),
            [0x0d, 0x03, 0x17].to_vec(),
            [0x08, 0x0d, 0x03, 0x17].to_vec(),
            [0x03, 0x17].to_vec(),
            [0x0e, 0x09, 0x03, 0x17].to_vec(),
            [0x09, 0x03, 0x17].to_vec(),
            [0x0c, 0x09, 0x03, 0x17].to_vec(),
            [].to_vec(),
            [0x12, 0x15, 0x1b, 0x07].to_vec(),
            [0x15, 0x1b, 0x07].to_vec(),
            [0x10, 0x15, 0x1b, 0x07].to_vec(),
            [0x1b, 0x07].to_vec(),
            [0x16, 0x11, 0x1b, 0x07].to_vec(),
            [0x11, 0x1b, 0x07].to_vec(),
            [0x14, 0x11, 0x1b, 0x07].to_vec(),
            [0x07].to_vec(),
            [0x1a, 0x1d, 0x13, 0x07].to_vec(),
            [0x1d, 0x13, 0x07].to_vec(),
            [0x18, 0x1d, 0x13, 0x07].to_vec(),
            [0x13, 0x07].to_vec(),
            [0x1e, 0x19, 0x13, 0x07].to_vec(),
            [0x19, 0x13, 0x07].to_vec(),
            [0x1c, 0x19, 0x13, 0x07].to_vec(),
        ]
        .to_vec();

        for (i, item) in expected.iter().enumerate() {
            let copath = (i as u32)
                .direct_copath(&16)
                .into_iter()
                .map(|cp| cp.copath)
                .collect_vec();

            assert_eq!(item, &copath)
        }
    }
}
