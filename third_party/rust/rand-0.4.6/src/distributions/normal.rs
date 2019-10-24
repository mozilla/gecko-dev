// Copyright 2013 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The normal and derived distributions.

use {Rng, Rand, Open01};
use distributions::{ziggurat, ziggurat_tables, Sample, IndependentSample};

/// A wrapper around an `f64` to generate N(0, 1) random numbers
/// (a.k.a.  a standard normal, or Gaussian).
///
/// See `Normal` for the general normal distribution.
///
/// Implemented via the ZIGNOR variant[1] of the Ziggurat method.
///
/// [1]: Jurgen A. Doornik (2005). [*An Improved Ziggurat Method to
/// Generate Normal Random
/// Samples*](http://www.doornik.com/research/ziggurat.pdf). Nuffield
/// College, Oxford
///
/// # Example
///
/// ```rust
/// use rand::distributions::normal::StandardNormal;
///
/// let StandardNormal(x) = rand::random();
/// println!("{}", x);
/// ```
#[derive(Clone, Copy, Debug)]
pub struct StandardNormal(pub f64);

impl Rand for StandardNormal {
    fn rand<R:Rng>(rng: &mut R) -> StandardNormal {
        #[inline]
        fn pdf(x: f64) -> f64 {
            (-x*x/2.0).exp()
        }
        #[inline]
        fn zero_case<R:Rng>(rng: &mut R, u: f64) -> f64 {
            // compute a random number in the tail by hand

            // strange initial conditions, because the loop is not
            // do-while, so the condition should be true on the first
            // run, they get overwritten anyway (0 < 1, so these are
            // good).
            let mut x = 1.0f64;
            let mut y = 0.0f64;

            while -2.0 * y < x * x {
                let Open01(x_) = rng.gen::<Open01<f64>>();
                let Open01(y_) = rng.gen::<Open01<f64>>();

                x = x_.ln() / ziggurat_tables::ZIG_NORM_R;
                y = y_.ln();
            }

            if u < 0.0 { x - ziggurat_tables::ZIG_NORM_R } else { ziggurat_tables::ZIG_NORM_R - x }
        }

        StandardNormal(ziggurat(
            rng,
            true, // this is symmetric
            &ziggurat_tables::ZIG_NORM_X,
            &ziggurat_tables::ZIG_NORM_F,
            pdf, zero_case))
    }
}

/// The normal distribution `N(mean, std_dev**2)`.
///
/// This uses the ZIGNOR variant of the Ziggurat method, see
/// `StandardNormal` for more details.
///
/// # Example
///
/// ```rust
/// use rand::distributions::{Normal, IndependentSample};
///
/// // mean 2, standard deviation 3
/// let normal = Normal::new(2.0, 3.0);
/// let v = normal.ind_sample(&mut rand::thread_rng());
/// println!("{} is from a N(2, 9) distribution", v)
/// ```
#[derive(Clone, Copy, Debug)]
pub struct Normal {
    mean: f64,
    std_dev: f64,
}

impl Normal {
    /// Construct a new `Normal` distribution with the given mean and
    /// standard deviation.
    ///
    /// # Panics
    ///
    /// Panics if `std_dev < 0`.
    #[inline]
    pub fn new(mean: f64, std_dev: f64) -> Normal {
        assert!(std_dev >= 0.0, "Normal::new called with `std_dev` < 0");
        Normal {
            mean: mean,
            std_dev: std_dev
        }
    }
}
impl Sample<f64> for Normal {
    fn sample<R: Rng>(&mut self, rng: &mut R) -> f64 { self.ind_sample(rng) }
}
impl IndependentSample<f64> for Normal {
    fn ind_sample<R: Rng>(&self, rng: &mut R) -> f64 {
        let StandardNormal(n) = rng.gen::<StandardNormal>();
        self.mean + self.std_dev * n
    }
}


/// The log-normal distribution `ln N(mean, std_dev**2)`.
///
/// If `X` is log-normal distributed, then `ln(X)` is `N(mean,
/// std_dev**2)` distributed.
///
/// # Example
///
/// ```rust
/// use rand::distributions::{LogNormal, IndependentSample};
///
/// // mean 2, standard deviation 3
/// let log_normal = LogNormal::new(2.0, 3.0);
/// let v = log_normal.ind_sample(&mut rand::thread_rng());
/// println!("{} is from an ln N(2, 9) distribution", v)
/// ```
#[derive(Clone, Copy, Debug)]
pub struct LogNormal {
    norm: Normal
}

impl LogNormal {
    /// Construct a new `LogNormal` distribution with the given mean
    /// and standard deviation.
    ///
    /// # Panics
    ///
    /// Panics if `std_dev < 0`.
    #[inline]
    pub fn new(mean: f64, std_dev: f64) -> LogNormal {
        assert!(std_dev >= 0.0, "LogNormal::new called with `std_dev` < 0");
        LogNormal { norm: Normal::new(mean, std_dev) }
    }
}
impl Sample<f64> for LogNormal {
    fn sample<R: Rng>(&mut self, rng: &mut R) -> f64 { self.ind_sample(rng) }
}
impl IndependentSample<f64> for LogNormal {
    fn ind_sample<R: Rng>(&self, rng: &mut R) -> f64 {
        self.norm.ind_sample(rng).exp()
    }
}

#[cfg(test)]
mod tests {
    use distributions::{Sample, IndependentSample};
    use super::{Normal, LogNormal};

    #[test]
    fn test_normal() {
        let mut norm = Normal::new(10.0, 10.0);
        let mut rng = ::test::rng();
        for _ in 0..1000 {
            norm.sample(&mut rng);
            norm.ind_sample(&mut rng);
        }
    }
    #[test]
    #[should_panic]
    fn test_normal_invalid_sd() {
        Normal::new(10.0, -1.0);
    }


    #[test]
    fn test_log_normal() {
        let mut lnorm = LogNormal::new(10.0, 10.0);
        let mut rng = ::test::rng();
        for _ in 0..1000 {
            lnorm.sample(&mut rng);
            lnorm.ind_sample(&mut rng);
        }
    }
    #[test]
    #[should_panic]
    fn test_log_normal_invalid_sd() {
        LogNormal::new(10.0, -1.0);
    }
}
