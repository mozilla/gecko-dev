// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! The PERT distribution.

use num_traits::Float;
use crate::{Beta, Distribution, Exp1, Open01, StandardNormal};
use rand::Rng;
use core::fmt;

/// The PERT distribution.
///
/// Similar to the [`Triangular`] distribution, the PERT distribution is
/// parameterised by a range and a mode within that range. Unlike the
/// [`Triangular`] distribution, the probability density function of the PERT
/// distribution is smooth, with a configurable weighting around the mode.
///
/// # Example
///
/// ```rust
/// use rand_distr::{Pert, Distribution};
///
/// let d = Pert::new(0., 5., 2.5).unwrap();
/// let v = d.sample(&mut rand::thread_rng());
/// println!("{} is from a PERT distribution", v);
/// ```
///
/// [`Triangular`]: crate::Triangular
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct Pert<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    min: F,
    range: F,
    beta: Beta<F>,
}

/// Error type returned from [`Pert`] constructors.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PertError {
    /// `max < min` or `min` or `max` is NaN.
    RangeTooSmall,
    /// `mode < min` or `mode > max` or `mode` is NaN.
    ModeRange,
    /// `shape < 0` or `shape` is NaN
    ShapeTooSmall,
}

impl fmt::Display for PertError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            PertError::RangeTooSmall => "requirement min < max is not met in PERT distribution",
            PertError::ModeRange => "mode is outside [min, max] in PERT distribution",
            PertError::ShapeTooSmall => "shape < 0 or is NaN in PERT distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for PertError {}

impl<F> Pert<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Set up the PERT distribution with defined `min`, `max` and `mode`.
    ///
    /// This is equivalent to calling `Pert::new_shape` with `shape == 4.0`.
    #[inline]
    pub fn new(min: F, max: F, mode: F) -> Result<Pert<F>, PertError> {
        Pert::new_with_shape(min, max, mode, F::from(4.).unwrap())
    }

    /// Set up the PERT distribution with defined `min`, `max`, `mode` and
    /// `shape`.
    pub fn new_with_shape(min: F, max: F, mode: F, shape: F) -> Result<Pert<F>, PertError> {
        if !(max > min) {
            return Err(PertError::RangeTooSmall);
        }
        if !(mode >= min && max >= mode) {
            return Err(PertError::ModeRange);
        }
        if !(shape >= F::from(0.).unwrap()) {
            return Err(PertError::ShapeTooSmall);
        }

        let range = max - min;
        let mu = (min + max + shape * mode) / (shape + F::from(2.).unwrap());
        let v = if mu == mode {
            shape * F::from(0.5).unwrap() + F::from(1.).unwrap()
        } else {
            (mu - min) * (F::from(2.).unwrap() * mode - min - max) / ((mode - mu) * (max - min))
        };
        let w = v * (max - mu) / (mu - min);
        let beta = Beta::new(v, w).map_err(|_| PertError::RangeTooSmall)?;
        Ok(Pert { min, range, beta })
    }
}

impl<F> Distribution<F> for Pert<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        self.beta.sample(rng) * self.range + self.min
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_pert() {
        for &(min, max, mode) in &[
            (-1., 1., 0.),
            (1., 2., 1.),
            (5., 25., 25.),
        ] {
            let _distr = Pert::new(min, max, mode).unwrap();
            // TODO: test correctness
        }

        for &(min, max, mode) in &[
            (-1., 1., 2.),
            (-1., 1., -2.),
            (2., 1., 1.),
        ] {
            assert!(Pert::new(min, max, mode).is_err());
        }
    }
}
