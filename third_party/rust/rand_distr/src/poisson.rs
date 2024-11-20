// Copyright 2018 Developers of the Rand project.
// Copyright 2016-2017 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The Poisson distribution.

use num_traits::{Float, FloatConst};
use crate::{Cauchy, Distribution, Standard};
use rand::Rng;
use core::fmt;

/// The Poisson distribution `Poisson(lambda)`.
///
/// This distribution has a density function:
/// `f(k) = lambda^k * exp(-lambda) / k!` for `k >= 0`.
///
/// # Example
///
/// ```
/// use rand_distr::{Poisson, Distribution};
///
/// let poi = Poisson::new(2.0).unwrap();
/// let v = poi.sample(&mut rand::thread_rng());
/// println!("{} is from a Poisson(2) distribution", v);
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct Poisson<F>
where F: Float + FloatConst, Standard: Distribution<F>
{
    lambda: F,
    // precalculated values
    exp_lambda: F,
    log_lambda: F,
    sqrt_2lambda: F,
    magic_val: F,
}

/// Error type returned from `Poisson::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// `lambda <= 0` or `nan`.
    ShapeTooSmall,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Error::ShapeTooSmall => "lambda is not positive in Poisson distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

impl<F> Poisson<F>
where F: Float + FloatConst, Standard: Distribution<F>
{
    /// Construct a new `Poisson` with the given shape parameter
    /// `lambda`.
    pub fn new(lambda: F) -> Result<Poisson<F>, Error> {
        if !(lambda > F::zero()) {
            return Err(Error::ShapeTooSmall);
        }
        let log_lambda = lambda.ln();
        Ok(Poisson {
            lambda,
            exp_lambda: (-lambda).exp(),
            log_lambda,
            sqrt_2lambda: (F::from(2.0).unwrap() * lambda).sqrt(),
            magic_val: lambda * log_lambda - crate::utils::log_gamma(F::one() + lambda),
        })
    }
}

impl<F> Distribution<F> for Poisson<F>
where F: Float + FloatConst, Standard: Distribution<F>
{
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        // using the algorithm from Numerical Recipes in C

        // for low expected values use the Knuth method
        if self.lambda < F::from(12.0).unwrap() {
            let mut result = F::zero();
            let mut p = F::one();
            while p > self.exp_lambda {
                p = p*rng.gen::<F>();
                result = result + F::one();
            }
            result - F::one()
        }
        // high expected values - rejection method
        else {
            // we use the Cauchy distribution as the comparison distribution
            // f(x) ~ 1/(1+x^2)
            let cauchy = Cauchy::new(F::zero(), F::one()).unwrap();
            let mut result;

            loop {
                let mut comp_dev;

                loop {
                    // draw from the Cauchy distribution
                    comp_dev = rng.sample(cauchy);
                    // shift the peak of the comparison distribution
                    result = self.sqrt_2lambda * comp_dev + self.lambda;
                    // repeat the drawing until we are in the range of possible values
                    if result >= F::zero() {
                        break;
                    }
                }
                // now the result is a random variable greater than 0 with Cauchy distribution
                // the result should be an integer value
                result = result.floor();

                // this is the ratio of the Poisson distribution to the comparison distribution
                // the magic value scales the distribution function to a range of approximately 0-1
                // since it is not exact, we multiply the ratio by 0.9 to avoid ratios greater than 1
                // this doesn't change the resulting distribution, only increases the rate of failed drawings
                let check = F::from(0.9).unwrap()
                    * (F::one() + comp_dev * comp_dev)
                    * (result * self.log_lambda
                        - crate::utils::log_gamma(F::one() + result)
                        - self.magic_val)
                        .exp();

                // check with uniform random value - if below the threshold, we are within the target distribution
                if rng.gen::<F>() <= check {
                    break;
                }
            }
            result
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn test_poisson_avg_gen<F: Float + FloatConst>(lambda: F, tol: F)
        where Standard: Distribution<F>
    {
        let poisson = Poisson::new(lambda).unwrap();
        let mut rng = crate::test::rng(123);
        let mut sum = F::zero();
        for _ in 0..1000 {
            sum = sum + poisson.sample(&mut rng);
        }
        let avg = sum / F::from(1000.0).unwrap();
        assert!((avg - lambda).abs() < tol);
    }

    #[test]
    fn test_poisson_avg() {
        test_poisson_avg_gen::<f64>(10.0, 0.5);
        test_poisson_avg_gen::<f64>(15.0, 0.5);
        test_poisson_avg_gen::<f32>(10.0, 0.5);
        test_poisson_avg_gen::<f32>(15.0, 0.5);
    }

    #[test]
    #[should_panic]
    fn test_poisson_invalid_lambda_zero() {
        Poisson::new(0.0).unwrap();
    }

    #[test]
    #[should_panic]
    fn test_poisson_invalid_lambda_neg() {
        Poisson::new(-10.0).unwrap();
    }
}
