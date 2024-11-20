// Copyright 2018 Developers of the Rand project.
// Copyright 2013 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The dirichlet distribution.
#![cfg(feature = "alloc")]
use num_traits::Float;
use crate::{Distribution, Exp1, Gamma, Open01, StandardNormal};
use rand::Rng;
use core::fmt;
use alloc::{boxed::Box, vec, vec::Vec};

/// The Dirichlet distribution `Dirichlet(alpha)`.
///
/// The Dirichlet distribution is a family of continuous multivariate
/// probability distributions parameterized by a vector alpha of positive reals.
/// It is a multivariate generalization of the beta distribution.
///
/// # Example
///
/// ```
/// use rand::prelude::*;
/// use rand_distr::Dirichlet;
///
/// let dirichlet = Dirichlet::new(&[1.0, 2.0, 3.0]).unwrap();
/// let samples = dirichlet.sample(&mut rand::thread_rng());
/// println!("{:?} is from a Dirichlet([1.0, 2.0, 3.0]) distribution", samples);
/// ```
#[cfg_attr(doc_cfg, doc(cfg(feature = "alloc")))]
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct Dirichlet<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Concentration parameters (alpha)
    alpha: Box<[F]>,
}

/// Error type returned from `Dirchlet::new`.
#[cfg_attr(doc_cfg, doc(cfg(feature = "alloc")))]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// `alpha.len() < 2`.
    AlphaTooShort,
    /// `alpha <= 0.0` or `nan`.
    AlphaTooSmall,
    /// `size < 2`.
    SizeTooSmall,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Error::AlphaTooShort | Error::SizeTooSmall => {
                "less than 2 dimensions in Dirichlet distribution"
            }
            Error::AlphaTooSmall => "alpha is not positive in Dirichlet distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

impl<F> Dirichlet<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Construct a new `Dirichlet` with the given alpha parameter `alpha`.
    ///
    /// Requires `alpha.len() >= 2`.
    #[inline]
    pub fn new(alpha: &[F]) -> Result<Dirichlet<F>, Error> {
        if alpha.len() < 2 {
            return Err(Error::AlphaTooShort);
        }
        for &ai in alpha.iter() {
            if !(ai > F::zero()) {
                return Err(Error::AlphaTooSmall);
            }
        }

        Ok(Dirichlet { alpha: alpha.to_vec().into_boxed_slice() })
    }

    /// Construct a new `Dirichlet` with the given shape parameter `alpha` and `size`.
    ///
    /// Requires `size >= 2`.
    #[inline]
    pub fn new_with_size(alpha: F, size: usize) -> Result<Dirichlet<F>, Error> {
        if !(alpha > F::zero()) {
            return Err(Error::AlphaTooSmall);
        }
        if size < 2 {
            return Err(Error::SizeTooSmall);
        }
        Ok(Dirichlet {
            alpha: vec![alpha; size].into_boxed_slice(),
        })
    }
}

impl<F> Distribution<Vec<F>> for Dirichlet<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> Vec<F> {
        let n = self.alpha.len();
        let mut samples = vec![F::zero(); n];
        let mut sum = F::zero();

        for (s, &a) in samples.iter_mut().zip(self.alpha.iter()) {
            let g = Gamma::new(a, F::one()).unwrap();
            *s = g.sample(rng);
            sum =  sum + (*s);
        }
        let invacc = F::one() / sum;
        for s in samples.iter_mut() {
            *s = (*s)*invacc;
        }
        samples
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_dirichlet() {
        let d = Dirichlet::new(&[1.0, 2.0, 3.0]).unwrap();
        let mut rng = crate::test::rng(221);
        let samples = d.sample(&mut rng);
        let _: Vec<f64> = samples
            .into_iter()
            .map(|x| {
                assert!(x > 0.0);
                x
            })
            .collect();
    }

    #[test]
    fn test_dirichlet_with_param() {
        let alpha = 0.5f64;
        let size = 2;
        let d = Dirichlet::new_with_size(alpha, size).unwrap();
        let mut rng = crate::test::rng(221);
        let samples = d.sample(&mut rng);
        let _: Vec<f64> = samples
            .into_iter()
            .map(|x| {
                assert!(x > 0.0);
                x
            })
            .collect();
    }

    #[test]
    #[should_panic]
    fn test_dirichlet_invalid_length() {
        Dirichlet::new_with_size(0.5f64, 1).unwrap();
    }

    #[test]
    #[should_panic]
    fn test_dirichlet_invalid_alpha() {
        Dirichlet::new_with_size(0.0f64, 2).unwrap();
    }
}
