// Copyright 2019 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use num_traits::Float;
use crate::{uniform::SampleUniform, Distribution, Uniform};
use rand::Rng;

/// Samples uniformly from the unit ball (surface and interior) in three
/// dimensions.
///
/// Implemented via rejection sampling.
///
///
/// # Example
///
/// ```
/// use rand_distr::{UnitBall, Distribution};
///
/// let v: [f64; 3] = UnitBall.sample(&mut rand::thread_rng());
/// println!("{:?} is from the unit ball.", v)
/// ```
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct UnitBall;

impl<F: Float + SampleUniform> Distribution<[F; 3]> for UnitBall {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> [F; 3] {
        let uniform = Uniform::new(F::from(-1.).unwrap(), F::from(1.).unwrap());
        let mut x1;
        let mut x2;
        let mut x3;
        loop {
            x1 = uniform.sample(rng);
            x2 = uniform.sample(rng);
            x3 = uniform.sample(rng);
            if x1 * x1 + x2 * x2 + x3 * x3 <= F::from(1.).unwrap() {
                break;
            }
        }
        [x1, x2, x3]
    }
}
