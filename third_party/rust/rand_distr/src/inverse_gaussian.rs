use crate::{Distribution, Standard, StandardNormal};
use num_traits::Float;
use rand::Rng;
use core::fmt;

/// Error type returned from `InverseGaussian::new`
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    /// `mean <= 0` or `nan`.
    MeanNegativeOrNull,
    /// `shape <= 0` or `nan`.
    ShapeNegativeOrNull,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Error::MeanNegativeOrNull => "mean <= 0 or is NaN in inverse Gaussian distribution",
            Error::ShapeNegativeOrNull => "shape <= 0 or is NaN in inverse Gaussian distribution",
        })
    }
}

#[cfg(feature = "std")]
#[cfg_attr(doc_cfg, doc(cfg(feature = "std")))]
impl std::error::Error for Error {}

/// The [inverse Gaussian distribution](https://en.wikipedia.org/wiki/Inverse_Gaussian_distribution)
#[derive(Debug, Clone, Copy)]
#[cfg_attr(feature = "serde1", derive(serde::Serialize, serde::Deserialize))]
pub struct InverseGaussian<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Standard: Distribution<F>,
{
    mean: F,
    shape: F,
}

impl<F> InverseGaussian<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Standard: Distribution<F>,
{
    /// Construct a new `InverseGaussian` distribution with the given mean and
    /// shape.
    pub fn new(mean: F, shape: F) -> Result<InverseGaussian<F>, Error> {
        let zero = F::zero();
        if !(mean > zero) {
            return Err(Error::MeanNegativeOrNull);
        }

        if !(shape > zero) {
            return Err(Error::ShapeNegativeOrNull);
        }

        Ok(Self { mean, shape })
    }
}

impl<F> Distribution<F> for InverseGaussian<F>
where
    F: Float,
    StandardNormal: Distribution<F>,
    Standard: Distribution<F>,
{
    #[allow(clippy::many_single_char_names)]
    fn sample<R>(&self, rng: &mut R) -> F
    where R: Rng + ?Sized {
        let mu = self.mean;
        let l = self.shape;

        let v: F = rng.sample(StandardNormal);
        let y = mu * v * v;

        let mu_2l = mu / (F::from(2.).unwrap() * l);

        let x = mu + mu_2l * (y - (F::from(4.).unwrap() * l * y + y * y).sqrt());

        let u: F = rng.gen();

        if u <= mu / (mu + x) {
            return x;
        }

        mu * mu / x
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_inverse_gaussian() {
        let inv_gauss = InverseGaussian::new(1.0, 1.0).unwrap();
        let mut rng = crate::test::rng(210);
        for _ in 0..1000 {
            inv_gauss.sample(&mut rng);
        }
    }

    #[test]
    fn test_inverse_gaussian_invalid_param() {
        assert!(InverseGaussian::new(-1.0, 1.0).is_err());
        assert!(InverseGaussian::new(-1.0, -1.0).is_err());
        assert!(InverseGaussian::new(1.0, -1.0).is_err());
        assert!(InverseGaussian::new(1.0, 1.0).is_ok());
    }
}
