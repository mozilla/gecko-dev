# rand_distr

[![Test Status](https://github.com/rust-random/rand/workflows/Tests/badge.svg?event=push)](https://github.com/rust-random/rand/actions)
[![Latest version](https://img.shields.io/crates/v/rand_distr.svg)](https://crates.io/crates/rand_distr)
[![Book](https://img.shields.io/badge/book-master-yellow.svg)](https://rust-random.github.io/book/)
[![API](https://img.shields.io/badge/api-master-yellow.svg)](https://rust-random.github.io/rand/rand_distr)
[![API](https://docs.rs/rand_distr/badge.svg)](https://docs.rs/rand_distr)
[![Minimum rustc version](https://img.shields.io/badge/rustc-1.36+-lightgray.svg)](https://github.com/rust-random/rand#rust-version-requirements)

Implements a full suite of random number distribution sampling routines.

This crate is a superset of the [rand::distributions] module, including support
for sampling from Beta, Binomial, Cauchy, ChiSquared, Dirichlet, Exponential,
FisherF, Gamma, Geometric, Hypergeometric, InverseGaussian, LogNormal, Normal,
Pareto, PERT, Poisson, StudentT, Triangular and Weibull distributions.  Sampling
from the unit ball, unit circle, unit disc and unit sphere surfaces is also
supported.

It is worth mentioning the [statrs] crate which provides similar functionality
along with various support functions, including PDF and CDF computation. In
contrast, this `rand_distr` crate focuses on sampling from distributions.

## Portability and libm

The floating point functions from `num_traits` and `libm` are used to support
`no_std` environments and ensure reproducibility. If the floating point
functions from `std` are preferred, which may provide better accuracy and
performance but may produce different random values, the `std_math` feature
can be enabled.

## Crate features

-   `std` (enabled by default): `rand_distr` implements the `Error` trait for
    its error types. Implies `alloc` and `rand/std`.
-   `alloc` (enabled by default): required for some distributions when not using
    `std` (in particular, `Dirichlet` and `WeightedAliasIndex`).
-   `std_math`: see above on portability and libm
-   `serde1`: implement (de)seriaialization using `serde`

## Links

-   [API documentation (master)](https://rust-random.github.io/rand/rand_distr)
-   [API documentation (docs.rs)](https://docs.rs/rand_distr)
-   [Changelog](CHANGELOG.md)
-   [The Rand project](https://github.com/rust-random/rand)


[statrs]: https://github.com/boxtown/statrs
[rand::distributions]: https://rust-random.github.io/rand/rand/distributions/index.html

## License

`rand_distr` is distributed under the terms of both the MIT license and the
Apache License (Version 2.0).

See [LICENSE-APACHE](LICENSE-APACHE) and [LICENSE-MIT](LICENSE-MIT), and
[COPYRIGHT](COPYRIGHT) for details.
