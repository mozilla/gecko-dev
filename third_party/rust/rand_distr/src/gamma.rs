// Copyright 2018 Developers of the Rand project.
// Copyright 2013 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The Gamma and derived distributions.

// We use the variable names from the published reference, therefore this
// warning is not helpful.
#![allow(clippy::many_single_char_names)]

use self::ChiSquaredRepr::*;
use self::GammaRepr::*;

use crate::normal::StandardNormal;
use num_traits::Float;
use crate::{Distribution, Exp, Exp1, Open01};
use rand::Rng;
use core::fmt;
#[cfg(feature = "serde1")]
use serde::{Serialize, Deserialize};

/// The Gamma distribution `Gamma(shape, scale)` distribution.
///
/// The density function of this distribution is
///
/// ```text
/// f(x) =  x^(k - 1) * exp(-x / θ) / (Γ(k) * θ^k)
/// ```
///
/// where `Γ` is the Gamma function, `k` is the shape and `θ` is the
/// scale and both `k` and `θ` are strictly positive.
///
/// The algorithm used is that described by Marsaglia & Tsang 2000[^1],
/// falling back to directly sampling from an Exponential for `shape
/// == 1`, and using the boosting technique described in that paper for
/// `shape < 1`.
///
/// # Example
///
/// ```
/// use rand_distr::{Distribution, Gamma};
///
/// let gamma = Gamma::new(2.0, 5.0).unwrap();
/// let v = gamma.sample(&mut rand::thread_rng());
/// println!("{} is from a Gamma(2, 5) distribution", v);
/// ```
///
/// [^1]: George Marsaglia and Wai Wan Tsang. 2000. "A Simple Method for
///       Generating Gamma Variables" *ACM Trans. Math. Softw.* 26, 3
///       (September 2000), 363-372.
///       DOI:[10.1145/358407.358414](https://doi.acm.org/10.1145/358407.358414)
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub struct Gamma<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    repr: GammaRepr<F>,
}

/// Error type returned from `Gamma::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// `shape <= 0` or `nan`.
    ShapeTooSmall,
    /// `scale <= 0` or `nan`.
    ScaleTooSmall,
    /// `1 / scale == 0`.
    ScaleTooLarge,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Error::ShapeTooSmall => "shape is not positive in gamma distribution",
            Error::ScaleTooSmall => "scale is not positive in gamma distribution",
            Error::ScaleTooLarge => "scale is infinity in gamma distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
enum GammaRepr<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    Large(GammaLargeShape<F>),
    One(Exp<F>),
    Small(GammaSmallShape<F>),
}

// These two helpers could be made public, but saving the
// match-on-Gamma-enum branch from using them directly (e.g. if one
// knows that the shape is always > 1) doesn't appear to be much
// faster.

/// Gamma distribution where the shape parameter is less than 1.
///
/// Note, samples from this require a compulsory floating-point `pow`
/// call, which makes it significantly slower than sampling from a
/// gamma distribution where the shape parameter is greater than or
/// equal to 1.
///
/// See `Gamma` for sampling from a Gamma distribution with general
/// shape parameters.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
struct GammaSmallShape<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Open01: Distribution<F>,
{
    inv_shape: F,
    large_shape: GammaLargeShape<F>,
}

/// Gamma distribution where the shape parameter is larger than 1.
///
/// See `Gamma` for sampling from a Gamma distribution with general
/// shape parameters.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
struct GammaLargeShape<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Open01: Distribution<F>,
{
    scale: F,
    c: F,
    d: F,
}

impl<F> Gamma<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Construct an object representing the `Gamma(shape, scale)`
    /// distribution.
    #[inline]
    pub fn new(shape: F, scale: F) -> Result<Gamma<F>, Error> {
        if !(shape > F::zero()) {
            return Err(Error::ShapeTooSmall);
        }
        if !(scale > F::zero()) {
            return Err(Error::ScaleTooSmall);
        }

        let repr = if shape == F::one() {
            One(Exp::new(F::one() / scale).map_err(|_| Error::ScaleTooLarge)?)
        } else if shape < F::one() {
            Small(GammaSmallShape::new_raw(shape, scale))
        } else {
            Large(GammaLargeShape::new_raw(shape, scale))
        };
        Ok(Gamma { repr })
    }
}

impl<F> GammaSmallShape<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Open01: Distribution<F>,
{
    fn new_raw(shape: F, scale: F) -> GammaSmallShape<F> {
        GammaSmallShape {
            inv_shape: F::one() / shape,
            large_shape: GammaLargeShape::new_raw(shape + F::one(), scale),
        }
    }
}

impl<F> GammaLargeShape<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Open01: Distribution<F>,
{
    fn new_raw(shape: F, scale: F) -> GammaLargeShape<F> {
        let d = shape - F::from(1. / 3.).unwrap();
        GammaLargeShape {
            scale,
            c: F::one() / (F::from(9.).unwrap() * d).sqrt(),
            d,
        }
    }
}

impl<F> Distribution<F> for Gamma<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        match self.repr {
            Small(ref g) => g.sample(rng),
            One(ref g) => g.sample(rng),
            Large(ref g) => g.sample(rng),
        }
    }
}
impl<F> Distribution<F> for GammaSmallShape<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        let u: F = rng.sample(Open01);

        self.large_shape.sample(rng) * u.powf(self.inv_shape)
    }
}
impl<F> Distribution<F> for GammaLargeShape<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        // Marsaglia & Tsang method, 2000
        loop {
            let x: F = rng.sample(StandardNormal);
            let v_cbrt = F::one() + self.c * x;
            if v_cbrt <= F::zero() {
                // a^3 <= 0 iff a <= 0
                continue;
            }

            let v = v_cbrt * v_cbrt * v_cbrt;
            let u: F = rng.sample(Open01);

            let x_sqr = x * x;
            if u < F::one() - F::from(0.0331).unwrap() * x_sqr * x_sqr
                || u.ln() < F::from(0.5).unwrap() * x_sqr + self.d * (F::one() - v + v.ln())
            {
                return self.d * v * self.scale;
            }
        }
    }
}

/// The chi-squared distribution `χ²(k)`, where `k` is the degrees of
/// freedom.
///
/// For `k > 0` integral, this distribution is the sum of the squares
/// of `k` independent standard normal random variables. For other
/// `k`, this uses the equivalent characterisation
/// `χ²(k) = Gamma(k/2, 2)`.
///
/// # Example
///
/// ```
/// use rand_distr::{ChiSquared, Distribution};
///
/// let chi = ChiSquared::new(11.0).unwrap();
/// let v = chi.sample(&mut rand::thread_rng());
/// println!("{} is from a χ²(11) distribution", v)
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub struct ChiSquared<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    repr: ChiSquaredRepr<F>,
}

/// Error type returned from `ChiSquared::new` and `StudentT::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub enum ChiSquaredError {
    /// `0.5 * k <= 0` or `nan`.
    DoFTooSmall,
}

impl fmt::Display for ChiSquaredError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            ChiSquaredError::DoFTooSmall => {
                "degrees-of-freedom k is not positive in chi-squared distribution"
            }
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for ChiSquaredError {}

#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
enum ChiSquaredRepr<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    // k == 1, Gamma(alpha, ..) is particularly slow for alpha < 1,
    // e.g. when alpha = 1/2 as it would be for this case, so special-
    // casing and using the definition of N(0,1)^2 is faster.
    DoFExactlyOne,
    DoFAnythingElse(Gamma<F>),
}

impl<F> ChiSquared<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Create a new chi-squared distribution with degrees-of-freedom
    /// `k`.
    pub fn new(k: F) -> Result<ChiSquared<F>, ChiSquaredError> {
        let repr = if k == F::one() {
            DoFExactlyOne
        } else {
            if !(F::from(0.5).unwrap() * k > F::zero()) {
                return Err(ChiSquaredError::DoFTooSmall);
            }
            DoFAnythingElse(Gamma::new(F::from(0.5).unwrap() * k, F::from(2.0).unwrap()).unwrap())
        };
        Ok(ChiSquared { repr })
    }
}
impl<F> Distribution<F> for ChiSquared<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        match self.repr {
            DoFExactlyOne => {
                // k == 1 => N(0,1)^2
                let norm: F = rng.sample(StandardNormal);
                norm * norm
            }
            DoFAnythingElse(ref g) => g.sample(rng),
        }
    }
}

/// The Fisher F distribution `F(m, n)`.
///
/// This distribution is equivalent to the ratio of two normalised
/// chi-squared distributions, that is, `F(m,n) = (χ²(m)/m) /
/// (χ²(n)/n)`.
///
/// # Example
///
/// ```
/// use rand_distr::{FisherF, Distribution};
///
/// let f = FisherF::new(2.0, 32.0).unwrap();
/// let v = f.sample(&mut rand::thread_rng());
/// println!("{} is from an F(2, 32) distribution", v)
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub struct FisherF<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    numer: ChiSquared<F>,
    denom: ChiSquared<F>,
    // denom_dof / numer_dof so that this can just be a straight
    // multiplication, rather than a division.
    dof_ratio: F,
}

/// Error type returned from `FisherF::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub enum FisherFError {
    /// `m <= 0` or `nan`.
    MTooSmall,
    /// `n <= 0` or `nan`.
    NTooSmall,
}

impl fmt::Display for FisherFError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            FisherFError::MTooSmall => "m is not positive in Fisher F distribution",
            FisherFError::NTooSmall => "n is not positive in Fisher F distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for FisherFError {}

impl<F> FisherF<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Create a new `FisherF` distribution, with the given parameter.
    pub fn new(m: F, n: F) -> Result<FisherF<F>, FisherFError> {
        let zero = F::zero();
        if !(m > zero) {
            return Err(FisherFError::MTooSmall);
        }
        if !(n > zero) {
            return Err(FisherFError::NTooSmall);
        }

        Ok(FisherF {
            numer: ChiSquared::new(m).unwrap(),
            denom: ChiSquared::new(n).unwrap(),
            dof_ratio: n / m,
        })
    }
}
impl<F> Distribution<F> for FisherF<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        self.numer.sample(rng) / self.denom.sample(rng) * self.dof_ratio
    }
}

/// The Student t distribution, `t(nu)`, where `nu` is the degrees of
/// freedom.
///
/// # Example
///
/// ```
/// use rand_distr::{StudentT, Distribution};
///
/// let t = StudentT::new(11.0).unwrap();
/// let v = t.sample(&mut rand::thread_rng());
/// println!("{} is from a t(11) distribution", v)
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub struct StudentT<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    chi: ChiSquared<F>,
    dof: F,
}

impl<F> StudentT<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    /// Create a new Student t distribution with `n` degrees of
    /// freedom.
    pub fn new(n: F) -> Result<StudentT<F>, ChiSquaredError> {
        Ok(StudentT {
            chi: ChiSquared::new(n)?,
            dof: n,
        })
    }
}
impl<F> Distribution<F> for StudentT<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Exp1: Distribution<F>,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        let norm: F = rng.sample(StandardNormal);
        norm * (self.dof / self.chi.sample(rng)).sqrt()
    }
}

/// The algorithm used for sampling the Beta distribution.
///
/// Reference:
///
/// R. C. H. Cheng (1978).
/// Generating beta variates with nonintegral shape parameters.
/// Communications of the ACM 21, 317-322.
/// https://doi.org/10.1145/359460.359482
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
enum BetaAlgorithm<N> {
    BB(BB<N>),
    BC(BC<N>),
}

/// Algorithm BB for `min(alpha, beta) > 1`.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
struct BB<N> {
    alpha: N,
    beta: N,
    gamma: N,
}

/// Algorithm BC for `min(alpha, beta) <= 1`.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
struct BC<N> {
    alpha: N,
    beta: N,
    delta: N,
    kappa1: N,
    kappa2: N,
}

/// The Beta distribution with shape parameters `alpha` and `beta`.
///
/// # Example
///
/// ```
/// use rand_distr::{Distribution, Beta};
///
/// let beta = Beta::new(2.0, 5.0).unwrap();
/// let v = beta.sample(&mut rand::thread_rng());
/// println!("{} is from a Beta(2, 5) distribution", v);
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub struct Beta<F>
where
    F: Float,
    Open01: Distribution<F>,
{
    a: F, b: F, switched_params: bool,
    algorithm: BetaAlgorithm<F>,
}

/// Error type returned from `Beta::new`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde1", derive(Serialize, Deserialize))]
pub enum BetaError {
    /// `alpha <= 0` or `nan`.
    AlphaTooSmall,
    /// `beta <= 0` or `nan`.
    BetaTooSmall,
}

impl fmt::Display for BetaError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            BetaError::AlphaTooSmall => "alpha is not positive in beta distribution",
            BetaError::BetaTooSmall => "beta is not positive in beta distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for BetaError {}

impl<F> Beta<F>
where
    F: Float,
    Open01: Distribution<F>,
{
    /// Construct an object representing the `Beta(alpha, beta)`
    /// distribution.
    pub fn new(alpha: F, beta: F) -> Result<Beta<F>, BetaError> {
        if !(alpha > F::zero()) {
            return Err(BetaError::AlphaTooSmall);
        }
        if !(beta > F::zero()) {
            return Err(BetaError::BetaTooSmall);
        }
        // From now on, we use the notation from the reference,
        // i.e. `alpha` and `beta` are renamed to `a0` and `b0`.
        let (a0, b0) = (alpha, beta);
        let (a, b, switched_params) = if a0 < b0 {
            (a0, b0, false)
        } else {
            (b0, a0, true)
        };
        if a > F::one() {
            // Algorithm BB
            let alpha = a + b;
            let beta = ((alpha - F::from(2.).unwrap())
                        / (F::from(2.).unwrap()*a*b - alpha)).sqrt();
            let gamma = a + F::one() / beta;

            Ok(Beta {
                a, b, switched_params,
                algorithm: BetaAlgorithm::BB(BB {
                    alpha, beta, gamma,
                })
            })
        } else {
            // Algorithm BC
            //
            // Here `a` is the maximum instead of the minimum.
            let (a, b, switched_params) = (b, a, !switched_params);
            let alpha = a + b;
            let beta = F::one() / b;
            let delta = F::one() + a - b;
            let kappa1 = delta
                * (F::from(1. / 18. / 4.).unwrap() + F::from(3. / 18. / 4.).unwrap()*b)
                / (a*beta - F::from(14. / 18.).unwrap());
            let kappa2 = F::from(0.25).unwrap()
                + (F::from(0.5).unwrap() + F::from(0.25).unwrap()/delta)*b;

            Ok(Beta {
                a, b, switched_params,
                algorithm: BetaAlgorithm::BC(BC {
                    alpha, beta, delta, kappa1, kappa2,
                })
            })
        }
    }
}

impl<F> Distribution<F> for Beta<F>
where
    F: Float,
    Open01: Distribution<F>,
{
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> F {
        let mut w;
        match self.algorithm {
            BetaAlgorithm::BB(algo) => {
                loop {
                    // 1.
                    let u1 = rng.sample(Open01);
                    let u2 = rng.sample(Open01);
                    let v = algo.beta * (u1 / (F::one() - u1)).ln();
                    w = self.a * v.exp();
                    let z = u1*u1 * u2;
                    let r = algo.gamma * v - F::from(4.).unwrap().ln();
                    let s = self.a + r - w;
                    // 2.
                    if s + F::one() + F::from(5.).unwrap().ln()
                        >= F::from(5.).unwrap() * z {
                        break;
                    }
                    // 3.
                    let t = z.ln();
                    if s >= t {
                        break;
                    }
                    // 4.
                    if !(r + algo.alpha * (algo.alpha / (self.b + w)).ln() < t) {
                        break;
                    }
                }
            },
            BetaAlgorithm::BC(algo) => {
                loop {
                    let z;
                    // 1.
                    let u1 = rng.sample(Open01);
                    let u2 = rng.sample(Open01);
                    if u1 < F::from(0.5).unwrap() {
                        // 2.
                        let y = u1 * u2;
                        z = u1 * y;
                        if F::from(0.25).unwrap() * u2 + z - y >= algo.kappa1 {
                            continue;
                        }
                    } else {
                        // 3.
                        z = u1 * u1 * u2;
                        if z <= F::from(0.25).unwrap() {
                            let v = algo.beta * (u1 / (F::one() - u1)).ln();
                            w = self.a * v.exp();
                            break;
                        }
                        // 4.
                        if z >= algo.kappa2 {
                            continue;
                        }
                    }
                    // 5.
                    let v = algo.beta * (u1 / (F::one() - u1)).ln();
                    w = self.a * v.exp();
                    if !(algo.alpha * ((algo.alpha / (self.b + w)).ln() + v)
                         - F::from(4.).unwrap().ln() < z.ln()) {
                        break;
                    };
                }
            },
        };
        // 5. for BB, 6. for BC
        if !self.switched_params {
            if w == F::infinity() {
                // Assuming `b` is finite, for large `w`:
                return F::one();
            }
            w / (self.b + w)
        } else {
            self.b / (self.b + w)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_chi_squared_one() {
        let chi = ChiSquared::new(1.0).unwrap();
        let mut rng = crate::test::rng(201);
        for _ in 0..1000 {
            chi.sample(&mut rng);
        }
    }
    #[test]
    fn test_chi_squared_small() {
        let chi = ChiSquared::new(0.5).unwrap();
        let mut rng = crate::test::rng(202);
        for _ in 0..1000 {
            chi.sample(&mut rng);
        }
    }
    #[test]
    fn test_chi_squared_large() {
        let chi = ChiSquared::new(30.0).unwrap();
        let mut rng = crate::test::rng(203);
        for _ in 0..1000 {
            chi.sample(&mut rng);
        }
    }
    #[test]
    #[should_panic]
    fn test_chi_squared_invalid_dof() {
        ChiSquared::new(-1.0).unwrap();
    }

    #[test]
    fn test_f() {
        let f = FisherF::new(2.0, 32.0).unwrap();
        let mut rng = crate::test::rng(204);
        for _ in 0..1000 {
            f.sample(&mut rng);
        }
    }

    #[test]
    fn test_t() {
        let t = StudentT::new(11.0).unwrap();
        let mut rng = crate::test::rng(205);
        for _ in 0..1000 {
            t.sample(&mut rng);
        }
    }

    #[test]
    fn test_beta() {
        let beta = Beta::new(1.0, 2.0).unwrap();
        let mut rng = crate::test::rng(201);
        for _ in 0..1000 {
            beta.sample(&mut rng);
        }
    }

    #[test]
    #[should_panic]
    fn test_beta_invalid_dof() {
        Beta::new(0., 0.).unwrap();
    }

    #[test]
    fn test_beta_small_param() {
        let beta = Beta::<f64>::new(1e-3, 1e-3).unwrap();
        let mut rng = crate::test::rng(206);
        for i in 0..1000 {
            assert!(!beta.sample(&mut rng).is_nan(), "failed at i={}", i);
        }
    }
}
