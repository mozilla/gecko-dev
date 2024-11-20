//! The geometric distribution.

use crate::Distribution;
use rand::Rng;
use core::fmt;
#[allow(unused_imports)]
use num_traits::Float;

/// The geometric distribution `Geometric(p)` bounded to `[0, u64::MAX]`.
/// 
/// This is the probability distribution of the number of failures before the
/// first success in a series of Bernoulli trials. It has the density function
/// `f(k) = (1 - p)^k p` for `k >= 0`, where `p` is the probability of success
/// on each trial.
/// 
/// This is the discrete analogue of the [exponential distribution](crate::Exp).
/// 
/// Note that [`StandardGeometric`](crate::StandardGeometric) is an optimised
/// implementation for `p = 0.5`.
///
/// # Example
///
/// ```
/// use rand_distr::{Geometric, Distribution};
///
/// let geo = Geometric::new(0.25).unwrap();
/// let v = geo.sample(&mut rand::thread_rng());
/// println!("{} is from a Geometric(0.25) distribution", v);
/// ```
#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct Geometric
{
    p: f64,
    pi: f64,
    k: u64
}

/// Error type returned from `Geometric::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// `p < 0 || p > 1` or `nan`
    InvalidProbability,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Error::InvalidProbability => "p is NaN or outside the interval [0, 1] in geometric distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

impl Geometric {
    /// Construct a new `Geometric` with the given shape parameter `p`
    /// (probability of success on each trial).
    pub fn new(p: f64) -> Result<Self, Error> {
        if !p.is_finite() || p < 0.0 || p > 1.0 {
            Err(Error::InvalidProbability)
        } else if p == 0.0 || p >= 2.0 / 3.0 {
            Ok(Geometric { p, pi: p, k: 0 })
        } else {
            let (pi, k) = {
                // choose smallest k such that pi = (1 - p)^(2^k) <= 0.5
                let mut k = 1;
                let mut pi = (1.0 - p).powi(2);
                while pi > 0.5 {
                    k += 1;
                    pi = pi * pi;
                }
                (pi, k)
            };

            Ok(Geometric { p, pi, k })
        }
    }
}

impl Distribution<u64> for Geometric
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u64 {
        if self.p >= 2.0 / 3.0 {
            // use the trivial algorithm:
            let mut failures = 0;
            loop {
                let u = rng.gen::<f64>();
                if u <= self.p { break; }
                failures += 1;
            }
            return failures;
        }
        
        if self.p == 0.0 { return core::u64::MAX; }

        let Geometric { p, pi, k } = *self;

        // Based on the algorithm presented in section 3 of
        // Karl Bringmann and Tobias Friedrich (July 2013) - Exact and Efficient
        // Generation of Geometric Random Variates and Random Graphs, published
        // in International Colloquium on Automata, Languages and Programming
        // (pp.267-278)
        // https://people.mpi-inf.mpg.de/~kbringma/paper/2013ICALP-1.pdf

        // Use the trivial algorithm to sample D from Geo(pi) = Geo(p) / 2^k:
        let d = {
            let mut failures = 0;
            while rng.gen::<f64>() < pi {
                failures += 1;
            }
            failures
        };

        // Use rejection sampling for the remainder M from Geo(p) % 2^k:
        // choose M uniformly from [0, 2^k), but reject with probability (1 - p)^M
        // NOTE: The paper suggests using bitwise sampling here, which is 
        // currently unsupported, but should improve performance by requiring
        // fewer iterations on average.                 ~ October 28, 2020
        let m = loop {
            let m = rng.gen::<u64>() & ((1 << k) - 1);
            let p_reject = if m <= core::i32::MAX as u64 {
                (1.0 - p).powi(m as i32)
            } else {
                (1.0 - p).powf(m as f64)
            };
            
            let u = rng.gen::<f64>();
            if u < p_reject {
                break m;
            }
        };

        (d << k) + m
    }
}

/// Samples integers according to the geometric distribution with success
/// probability `p = 0.5`. This is equivalent to `Geometeric::new(0.5)`,
/// but faster.
/// 
/// See [`Geometric`](crate::Geometric) for the general geometric distribution.
/// 
/// Implemented via iterated [Rng::gen::<u64>().leading_zeros()].
/// 
/// # Example
/// ```
/// use rand::prelude::*;
/// use rand_distr::StandardGeometric;
/// 
/// let v = StandardGeometric.sample(&mut thread_rng());
/// println!("{} is from a Geometric(0.5) distribution", v);
/// ```
#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct StandardGeometric;

impl Distribution<u64> for StandardGeometric {
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u64 {
        let mut result = 0;
        loop {
            let x = rng.gen::<u64>().leading_zeros() as u64;
            result += x;
            if x < 64 { break; }
        }
        result
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_geo_invalid_p() {
        assert!(Geometric::new(core::f64::NAN).is_err());
        assert!(Geometric::new(core::f64::INFINITY).is_err());
        assert!(Geometric::new(core::f64::NEG_INFINITY).is_err());

        assert!(Geometric::new(-0.5).is_err());
        assert!(Geometric::new(0.0).is_ok());
        assert!(Geometric::new(1.0).is_ok());
        assert!(Geometric::new(2.0).is_err());
    }

    fn test_geo_mean_and_variance<R: Rng>(p: f64, rng: &mut R) {
        let distr = Geometric::new(p).unwrap();

        let expected_mean = (1.0 - p) / p;
        let expected_variance = (1.0 - p) / (p * p);

        let mut results = [0.0; 10000];
        for i in results.iter_mut() {
            *i = distr.sample(rng) as f64;
        }

        let mean = results.iter().sum::<f64>() / results.len() as f64;
        assert!((mean as f64 - expected_mean).abs() < expected_mean / 40.0);

        let variance =
            results.iter().map(|x| (x - mean) * (x - mean)).sum::<f64>() / results.len() as f64;
        assert!((variance - expected_variance).abs() < expected_variance / 10.0);
    }

    #[test]
    fn test_geometric() {
        let mut rng = crate::test::rng(12345);

        test_geo_mean_and_variance(0.10, &mut rng);
        test_geo_mean_and_variance(0.25, &mut rng);
        test_geo_mean_and_variance(0.50, &mut rng);
        test_geo_mean_and_variance(0.75, &mut rng);
        test_geo_mean_and_variance(0.90, &mut rng);
    }

    #[test]
    fn test_standard_geometric() {
        let mut rng = crate::test::rng(654321);

        let distr = StandardGeometric;
        let expected_mean = 1.0;
        let expected_variance = 2.0;

        let mut results = [0.0; 1000];
        for i in results.iter_mut() {
            *i = distr.sample(&mut rng) as f64;
        }

        let mean = results.iter().sum::<f64>() / results.len() as f64;
        assert!((mean as f64 - expected_mean).abs() < expected_mean / 50.0);

        let variance =
            results.iter().map(|x| (x - mean) * (x - mean)).sum::<f64>() / results.len() as f64;
        assert!((variance - expected_variance).abs() < expected_variance / 10.0);
    }
}
