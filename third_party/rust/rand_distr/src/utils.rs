// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Math helper functions

use crate::ziggurat_tables;
use rand::distributions::hidden_export::IntoFloat;
use rand::Rng;
use num_traits::Float;

/// Calculates ln(gamma(x)) (natural logarithm of the gamma
/// function) using the Lanczos approximation.
///
/// The approximation expresses the gamma function as:
/// `gamma(z+1) = sqrt(2*pi)*(z+g+0.5)^(z+0.5)*exp(-z-g-0.5)*Ag(z)`
/// `g` is an arbitrary constant; we use the approximation with `g=5`.
///
/// Noting that `gamma(z+1) = z*gamma(z)` and applying `ln` to both sides:
/// `ln(gamma(z)) = (z+0.5)*ln(z+g+0.5)-(z+g+0.5) + ln(sqrt(2*pi)*Ag(z)/z)`
///
/// `Ag(z)` is an infinite series with coefficients that can be calculated
/// ahead of time - we use just the first 6 terms, which is good enough
/// for most purposes.
pub(crate) fn log_gamma<F: Float>(x: F) -> F {
    // precalculated 6 coefficients for the first 6 terms of the series
    let coefficients: [F; 6] = [
        F::from(76.18009172947146).unwrap(),
        F::from(-86.50532032941677).unwrap(),
        F::from(24.01409824083091).unwrap(),
        F::from(-1.231739572450155).unwrap(),
        F::from(0.1208650973866179e-2).unwrap(),
        F::from(-0.5395239384953e-5).unwrap(),
    ];

    // (x+0.5)*ln(x+g+0.5)-(x+g+0.5)
    let tmp = x + F::from(5.5).unwrap();
    let log = (x + F::from(0.5).unwrap()) * tmp.ln() - tmp;

    // the first few terms of the series for Ag(x)
    let mut a = F::from(1.000000000190015).unwrap();
    let mut denom = x;
    for &coeff in &coefficients {
        denom = denom + F::one();
        a = a + (coeff / denom);
    }

    // get everything together
    // a is Ag(x)
    // 2.5066... is sqrt(2pi)
    log + (F::from(2.5066282746310005).unwrap() * a / x).ln()
}

/// Sample a random number using the Ziggurat method (specifically the
/// ZIGNOR variant from Doornik 2005). Most of the arguments are
/// directly from the paper:
///
/// * `rng`: source of randomness
/// * `symmetric`: whether this is a symmetric distribution, or one-sided with P(x < 0) = 0.
/// * `X`: the $x_i$ abscissae.
/// * `F`: precomputed values of the PDF at the $x_i$, (i.e. $f(x_i)$)
/// * `F_DIFF`: precomputed values of $f(x_i) - f(x_{i+1})$
/// * `pdf`: the probability density function
/// * `zero_case`: manual sampling from the tail when we chose the
///    bottom box (i.e. i == 0)

// the perf improvement (25-50%) is definitely worth the extra code
// size from force-inlining.
#[inline(always)]
pub(crate) fn ziggurat<R: Rng + ?Sized, P, Z>(
    rng: &mut R,
    symmetric: bool,
    x_tab: ziggurat_tables::ZigTable,
    f_tab: ziggurat_tables::ZigTable,
    mut pdf: P,
    mut zero_case: Z
) -> f64
where
    P: FnMut(f64) -> f64,
    Z: FnMut(&mut R, f64) -> f64,
{
    loop {
        // As an optimisation we re-implement the conversion to a f64.
        // From the remaining 12 most significant bits we use 8 to construct `i`.
        // This saves us generating a whole extra random number, while the added
        // precision of using 64 bits for f64 does not buy us much.
        let bits = rng.next_u64();
        let i = bits as usize & 0xff;

        let u = if symmetric {
            // Convert to a value in the range [2,4) and subtract to get [-1,1)
            // We can't convert to an open range directly, that would require
            // subtracting `3.0 - EPSILON`, which is not representable.
            // It is possible with an extra step, but an open range does not
            // seem necessary for the ziggurat algorithm anyway.
            (bits >> 12).into_float_with_exponent(1) - 3.0
        } else {
            // Convert to a value in the range [1,2) and subtract to get (0,1)
            (bits >> 12).into_float_with_exponent(0) - (1.0 - core::f64::EPSILON / 2.0)
        };
        let x = u * x_tab[i];

        let test_x = if symmetric { x.abs() } else { x };

        // algebraically equivalent to |u| < x_tab[i+1]/x_tab[i] (or u < x_tab[i+1]/x_tab[i])
        if test_x < x_tab[i + 1] {
            return x;
        }
        if i == 0 {
            return zero_case(rng, u);
        }
        // algebraically equivalent to f1 + DRanU()*(f0 - f1) < 1
        if f_tab[i + 1] + (f_tab[i] - f_tab[i + 1]) * rng.gen::<f64>() < pdf(x) {
            return x;
        }
    }
}
