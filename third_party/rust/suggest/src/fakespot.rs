/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Fakespot-specific logic
///
/// Score used to order Fakespot suggestions
///
/// FakespotScore contains several components, each in the range of [0, 1]
pub struct FakespotScore {
    /// Did the query match the `keywords` field exactly?
    keywords_score: f64,
    /// How well did the query match the `product_type` field?
    product_type_score: f64,
    /// Fakespot score from the RS data, this reflects the average review, number of reviews,
    /// Fakespot grade, etc.
    fakespot_score: f64,
}

impl FakespotScore {
    pub fn new(query: &str, keywords: String, product_type: String, fakespot_score: f64) -> Self {
        let query = query.to_lowercase();
        let query_terms = split_terms(&query);
        Self {
            keywords_score: calc_keywords_score(&query_terms, &keywords),
            product_type_score: calc_product_type_score(&query_terms, &product_type),
            fakespot_score,
        }
    }

    /// Convert a FakespotScore into the value to use in `Sugggestion::Fakespot::score`
    ///
    /// This converts FakespotScore into a single float that:
    ///   - Is > 0.3 so that Fakespot suggestions are preferred to AMP ones
    ///   - Reflects the Fakespot ordering:
    ///     - Suggestions with higher keywords_score are greater
    ///     - If keywords_score is tied, then suggestions with higher product_type_scores are greater
    ///     - If both are tied, then suggestions with higher fakespot_score are greater
    pub fn as_suggest_score(&self) -> f64 {
        0.30 + (0.01 * self.keywords_score)
            + (0.001 * self.product_type_score)
            + (0.0001 * self.fakespot_score)
    }
}

/// Split a string containing terms into a list of individual terms, normalized to lowercase
fn split_terms(string: &str) -> Vec<&str> {
    string.split_whitespace().collect()
}

fn calc_keywords_score(query_terms: &[&str], keywords: &str) -> f64 {
    // Note: We can assume keywords is lower-case, since we do that during ingestion
    let keyword_terms = split_terms(keywords);
    if keyword_terms.is_empty() {
        return 0.0;
    }

    if query_terms == keyword_terms {
        1.0
    } else {
        0.0
    }
}

fn calc_product_type_score(query_terms: &[&str], product_type: &str) -> f64 {
    // Note: We can assume product_type is lower-case, since we do that during ingestion
    let product_type_terms = split_terms(product_type);
    if product_type_terms.is_empty() {
        return 0.0;
    }
    let count = product_type_terms
        .iter()
        .filter(|t| query_terms.contains(t))
        .count() as f64;
    count / product_type_terms.len() as f64
}

#[cfg(test)]
mod tests {
    use super::*;

    struct KeywordsTestCase {
        keywords: &'static str,
        query: &'static str,
        expected: f64,
    }

    impl KeywordsTestCase {
        fn test(&self) {
            let actual =
                calc_keywords_score(&split_terms(&self.query.to_lowercase()), self.keywords);
            assert_eq!(
                actual, self.expected,
                "keywords: {} query: {} expected: {} actual: {actual}",
                self.keywords, self.query, self.expected,
            );
        }
    }

    #[test]
    fn test_keywords_score() {
        // Keyword score 1.0 on exact matches, 0.0 otherwise
        KeywordsTestCase {
            keywords: "apple",
            query: "apple",
            expected: 1.0,
        }
        .test();
        KeywordsTestCase {
            keywords: "apple",
            query: "android",
            expected: 0.0,
        }
        .test();
        KeywordsTestCase {
            keywords: "apple",
            query: "apple phone",
            expected: 0.0,
        }
        .test();
        // Empty keywords should always score 0.0
        KeywordsTestCase {
            keywords: "",
            query: "",
            expected: 0.0,
        }
        .test();
        KeywordsTestCase {
            keywords: "",
            query: "apple",
            expected: 0.0,
        }
        .test();
        // Matching should be case insensitive
        KeywordsTestCase {
            keywords: "apple",
            query: "Apple",
            expected: 1.0,
        }
        .test();
    }

    struct ProductTypeTestCase {
        query: &'static str,
        product_type: &'static str,
        expected: f64,
    }
    impl ProductTypeTestCase {
        fn test(&self) {
            let actual = calc_product_type_score(
                &split_terms(&self.query.to_lowercase()),
                self.product_type,
            );
            assert_eq!(
                actual, self.expected,
                "product_type: {} query: {} expected: {} actual: {actual}",
                self.product_type, self.query, self.expected,
            );
        }
    }

    #[test]
    fn test_product_type_score() {
        // Product type scores based on the percentage of terms in the product type that are also
        // present in the query
        ProductTypeTestCase {
            product_type: "standing desk",
            query: "standing desk",
            expected: 1.0,
        }
        .test();
        ProductTypeTestCase {
            product_type: "standing desk",
            query: "desk",
            expected: 0.5,
        }
        .test();
        ProductTypeTestCase {
            product_type: "standing desk",
            query: "desk desk desk",
            expected: 0.5,
        }
        .test();
        ProductTypeTestCase {
            product_type: "standing desk",
            query: "standing",
            expected: 0.5,
        }
        .test();
        ProductTypeTestCase {
            product_type: "standing desk",
            query: "phone",
            expected: 0.0,
        }
        .test();
        // Extra terms in the query are ignored
        ProductTypeTestCase {
            product_type: "standing desk",
            query: "standing desk for my office",
            expected: 1.0,
        }
        .test();
        // Empty product_type should always score 0.0
        ProductTypeTestCase {
            product_type: "",
            query: "",
            expected: 0.0,
        }
        .test();
        // Matching should be case insensitive
        ProductTypeTestCase {
            product_type: "desk",
            query: "Desk",
            expected: 1.0,
        }
        .test();
        // Extra spaces are ignored
        ProductTypeTestCase {
            product_type: "desk",
            query: "  desk  ",
            expected: 1.0,
        }
        .test();
    }
}
