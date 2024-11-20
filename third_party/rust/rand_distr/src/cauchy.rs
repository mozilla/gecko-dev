// Copyright 2018 Developers of the Rand project.
// Copyright 2016-2017 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The Cauchy distribution.

use num_traits::{Float, FloatConst};
use crate::{Distribution, Standard};
use rand::Rng;
use core::fmt;

/// The Cauchy distribution `Cauchy(median, scale)`.
///
/// This distribution has a density function:
/// `f(x) = 1 / (pi * scale * (1 + ((x - median) / scale)^2))`
///
/// Note that at least for `f32`, results are not fully portable due to minor
/// differences in the target system's *tan* implementation, `tanf`.
///
/// # Example
///
/// ```
/// use rand_distr::{Cauchy, Distribution};
///
/// let cau = Cauchy::new(2.0, 5.0).unwrap();
/// let v = cau.sample(&mut rand::thread_rng());
/// println!("{} is from a Cauchy(2, 5) distribution", v);
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct Cauchy<F>
where F: Float + FloatConst, Standard: Distribution<F>
{
    median: F,
    scale: F,
}

/// Error type returned from `Cauchy::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// `scale <= 0` or `nan`.
    ScaleTooSmall,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Error::ScaleTooSmall => "scale is not positive in Cauchy distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

impl<F> Cauchy<F>
where F: Float + FloatConst, Standard: Distribution<F>
{
    /// Construct a new `Cauchy` with the given shape parameters
    /// `median` the peak location and `scale` the scale factor.
    pub fn new(median: F, scale: F) -> Result<Cauchy<F>, Error> {
        if !(scale > F::zero()) {
            return Err(Error::ScaleTooSmall);
        }
        Ok(Cauchy { median, scale })
    }
}

impl<F> Distribution<F> for Cauchy<F>
where F: Float + FloatConst, Standard: Distribution<F>
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        // sample from [0, 1)
        let x = Standard.sample(rng);
        // get standard cauchy random number
        // note that π/2 is not exactly representable, even if x=0.5 the result is finite
        let comp_dev = (F::PI() * x).tan();
        // shift and scale according to parameters
        self.median + self.scale * comp_dev
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn median(numbers: &mut [f64]) -> f64 {
        sort(numbers);
        let mid = numbers.len() / 2;
        numbers[mid]
    }

    fn sort(numbers: &mut [f64]) {
        numbers.sort_by(|a, b| a.partial_cmp(b).unwrap());
    }

    #[test]
    fn test_cauchy_averages() {
        // NOTE: given that the variance and mean are undefined,
        // this test does not have any rigorous statistical meaning.
        let cauchy = Cauchy::new(10.0, 5.0).unwrap();
        let mut rng = crate::test::rng(123);
        let mut numbers: [f64; 1000] = [0.0; 1000];
        let mut sum = 0.0;
        for number in &mut numbers[..] {
            *number = cauchy.sample(&mut rng);
            sum += *number;
        }
        let median = median(&mut numbers);
        #[cfg(feature = "std")]
        std::println!("Cauchy median: {}", median);
        assert!((median - 10.0).abs() < 0.4); // not 100% certain, but probable enough
        let mean = sum / 1000.0;
        #[cfg(feature = "std")]
        std::println!("Cauchy mean: {}", mean);
        // for a Cauchy distribution the mean should not converge
        assert!((mean - 10.0).abs() > 0.4); // not 100% certain, but probable enough
    }

    #[test]
    #[should_panic]
    fn test_cauchy_invalid_scale_zero() {
        Cauchy::new(0.0, 0.0).unwrap();
    }

    #[test]
    #[should_panic]
    fn test_cauchy_invalid_scale_neg() {
        Cauchy::new(0.0, -10.0).unwrap();
    }

    #[test]
    fn value_stability() {
        fn gen_samples<F: Float + FloatConst + core::fmt::Debug>(m: F, s: F, buf: &mut [F])
        where Standard: Distribution<F> {
            let distr = Cauchy::new(m, s).unwrap();
            let mut rng = crate::test::rng(353);
            for x in buf {
                *x = rng.sample(&distr);
            }
        }

        let mut buf = [0.0; 4];
        gen_samples(100f64, 10.0, &mut buf);
        assert_eq!(&buf, &[
            77.93369152808678,
            90.1606912098641,
            125.31516221323625,
            86.10217834773925
        ]);

        // Unfortunately this test is not fully portable due to reliance on the
        // system's implementation of tanf (see doc on Cauchy struct).
        let mut buf = [0.0; 4];
        gen_samples(10f32, 7.0, &mut buf);
        let expected = [15.023088, -5.446413, 3.7092876, 3.112482];
        for (a, b) in buf.iter().zip(expected.iter()) {
            assert_almost_eq!(*a, *b, 1e-5);
        }
    }
}
