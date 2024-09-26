/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! **UNSTABLE / EXPERIMENTAL**
//!
//! Clubcard is a compact data-structure that solves the exact membership query problem.
//!
//! Given some universe of objects U, a subset R of U, and two hash functions defined on U (as
//! described below), Clubcard outputs a compact encoding of the function `f : U -> {0, 1}` defined
//! by `f(x) = 0 if and only if x ∈ R`.
//!
//! Clubcard is based on the Ribbon filters from
//! - <https://arxiv.org/pdf/2103.02515>, and
//! - <https://arxiv.org/pdf/2109.01892>
//!
//! And some ideas from Mike Hamburg's RWC 2022 talk
//! - <https://rwc.iacr.org/2022/program.php#abstract-talk-39>
//! - <https://youtu.be/Htms5rNy7B8?list=PLeeS-3Ml-rpovBDh6do693We_CP3KTnHU&t=2357>
//!
//! The construction will be described in detail in a forthcoming paper.
//!
//! At a high level, a clubcard is a pair of matrices (X, Y) defined over GF(2). The hash
//! functions h and g map elements of U to vectors in the domain of X and Y respectively.
//!
//! The matrix X is a solution to `H * X = 0` where the rows of H are obtained by hashing the
//! elements of R with h. The number of columns in X is floor(lg(|U\R| / |R|)).
//!
//! The matrix Y is a solution to `G * Y = C` where the rows of G are obtained by hashing, with g,
//! the elements u ∈ U for which h(u) * X = 0. The matrix Y has one column. The rows of C encode
//! membership in R.
//!
//! Given (X, Y) and an element u ∈ U, we have that u ∈ R iff h(u) * X == 0 and g(u) * Y = 0.
//!
//! Clubcard was developed to replace the use of Bloom cascades in CRLite. In a preliminary
//! experiment using a real-world collection of 12.2M revoked certs and 789.2M non-revoked certs,
//! the currently-deployed Bloom cascade implementation of CRLite produces a 19.8MB filter in 293
//! seconds (on a Ryzen 3975WX with 64GB of RAM). This Clubcard implementation produces an 8.5MB
//! filter in 200 seconds.
//!
//#![warn(missing_docs)]

#[cfg(feature = "builder")]
pub mod builder;

mod clubcard;
pub use clubcard::{ApproximateSizeOf, Clubcard, ClubcardIndexEntry, Membership};

mod equation;
pub use equation::Equation;

mod query;
pub use query::{AsQuery, Filterable, Queryable};
