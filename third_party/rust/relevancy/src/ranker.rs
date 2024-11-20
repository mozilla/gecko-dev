use std::cmp::max;

use crate::interest::{Interest, InterestVector};

/// Calculate score for a piece of categorized content based on a user interest vector.
///
/// This scoring function is of the following properties:
///   - The score ranges from 0.0 to 1.0
///   - The score is monotonically increasing for the accumulated interest count
///
/// Params:
///   - `interest_vector`: a user interest vector that can be fetched via
///     `RelevancyStore::user_interest_vector()`.
///   - `content_categories`: a list of categories (interests) of the give content.
/// Return:
///   - A score ranges in [0, 1].
#[uniffi::export]
pub fn score(interest_vector: InterestVector, content_categories: Vec<Interest>) -> f64 {
    let n = content_categories
        .iter()
        .fold(0, |acc, &category| acc + interest_vector[category]);

    // Apply base 10 logarithm to the accumulated count so its hyperbolic tangent is more
    // evenly distributed in [0, 1]. Note that `max(n, 1)` is used to avoid negative scores.
    (max(n, 1) as f64).log10().tanh()
}

#[cfg(test)]
mod test {
    use crate::interest::{Interest, InterestVector};

    use super::*;

    const EPSILON: f64 = 1e-10;
    const SUBEPSILON: f64 = 1e-6;

    #[test]
    fn test_score_lower_bound() {
        // Empty interest vector yields score 0.
        let s = score(InterestVector::default(), vec![Interest::Food]);
        let delta = (s - 0_f64).abs();

        assert!(delta < EPSILON);

        // No overlap also yields score 0.
        let s = score(
            InterestVector {
                animals: 10,
                ..InterestVector::default()
            },
            vec![Interest::Food],
        );
        let delta = (s - 0_f64).abs();

        assert!(delta < EPSILON);
    }

    #[test]
    fn test_score_upper_bound() {
        let score = score(
            InterestVector {
                animals: 1_000_000_000,
                ..InterestVector::default()
            },
            vec![Interest::Animals],
        );
        let delta = (score - 1.0_f64).abs();

        // Can get very close to the upper bound 1.0 but not over.
        assert!(delta < SUBEPSILON);
    }

    #[test]
    fn test_score_monotonic() {
        let l = score(
            InterestVector {
                animals: 1,
                ..InterestVector::default()
            },
            vec![Interest::Animals],
        );

        let r = score(
            InterestVector {
                animals: 5,
                ..InterestVector::default()
            },
            vec![Interest::Animals],
        );

        assert!(l < r);
    }

    #[test]
    fn test_score_multi_categories() {
        let l = score(
            InterestVector {
                animals: 100,
                food: 100,
                ..InterestVector::default()
            },
            vec![Interest::Animals, Interest::Food],
        );

        let r = score(
            InterestVector {
                animals: 200,
                ..InterestVector::default()
            },
            vec![Interest::Animals],
        );
        let delta = (l - r).abs();

        assert!(delta < EPSILON);
    }
}
