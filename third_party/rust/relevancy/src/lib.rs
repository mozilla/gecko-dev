/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Proposed API for the relevancy component (validation phase)
//!
//! The goal here is to allow us to validate that we can reliably detect user interests from
//! history data, without spending too much time building the API out.  There's some hand-waving
//! towards how we would use this data to rank search results, but we don't need to come to a final
//! decision on that yet.

mod db;
mod error;
mod ingest;
mod interest;
mod ranker;
mod rs;
mod schema;
pub mod url_hash;

use rand_distr::{Beta, Distribution};

pub use db::RelevancyDb;
pub use error::{ApiResult, Error, RelevancyApiError, Result};
pub use interest::{Interest, InterestVector};
pub use ranker::score;

use error_support::handle_error;

uniffi::setup_scaffolding!();

#[derive(uniffi::Object)]
pub struct RelevancyStore {
    db: RelevancyDb,
}

/// Top-level API for the Relevancy component
// Impl block to be exported via `UniFFI`.
#[uniffi::export]
impl RelevancyStore {
    /// Construct a new RelevancyStore
    ///
    /// This is non-blocking since databases and other resources are lazily opened.
    #[uniffi::constructor]
    pub fn new(db_path: String) -> Self {
        Self {
            db: RelevancyDb::new(db_path),
        }
    }

    /// Close any open resources (for example databases)
    ///
    /// Calling `close` will interrupt any in-progress queries on other threads.
    pub fn close(&self) {
        self.db.close()
    }

    /// Interrupt any current database queries
    pub fn interrupt(&self) {
        self.db.interrupt()
    }

    /// Ingest top URLs to build the user's interest vector.
    ///
    /// Consumer should pass a list of the user's top URLs by frecency to this method.  It will
    /// then:
    ///
    ///  - Download the URL interest data from remote settings.  Eventually this should be cached /
    ///    stored in the database, but for now it would be fine to download fresh data each time.
    ///  - Match the user's top URls against the interest data to build up their interest vector.
    ///  - Store the user's interest vector in the database.
    ///
    ///  This method may execute for a long time and should only be called from a worker thread.
    #[handle_error(Error)]
    pub fn ingest(&self, top_urls_by_frecency: Vec<String>) -> ApiResult<InterestVector> {
        ingest::ensure_interest_data_populated(&self.db)?;
        let interest_vec = self.classify(top_urls_by_frecency)?;
        self.db
            .read_write(|dao| dao.update_frecency_user_interest_vector(&interest_vec))?;
        Ok(interest_vec)
    }

    /// Calculate metrics for the validation phase
    ///
    /// This runs after [Self::ingest].  It takes the interest vector that ingest created and
    /// calculates a set of metrics that we can report to glean.
    #[handle_error(Error)]
    pub fn calculate_metrics(&self) -> ApiResult<InterestMetrics> {
        todo!()
    }

    /// Get the user's interest vector directly.
    ///
    /// This runs after [Self::ingest].  It returns the interest vector directly so that the
    /// consumer can show it in an `about:` page.
    #[handle_error(Error)]
    pub fn user_interest_vector(&self) -> ApiResult<InterestVector> {
        self.db.read(|dao| dao.get_frecency_user_interest_vector())
    }

    /// Initializes probability distributions for any uninitialized items (arms) within a bandit model.
    ///
    /// This method takes a `bandit` identifier and a list of `arms` (items) and ensures that each arm
    /// in the list has an initialized probability distribution in the database. For each arm, if the
    /// probability distribution does not already exist, it will be created, using Beta(1,1) as default,
    /// which represents uniform distribution.
    #[handle_error(Error)]
    pub fn bandit_init(&self, bandit: String, arms: &[String]) -> ApiResult<()> {
        self.db.read_write(|dao| {
            for arm in arms {
                dao.initialize_multi_armed_bandit(&bandit, arm)?;
            }
            Ok(())
        })?;

        Ok(())
    }

    /// Selects the optimal item (arm) to display to the user based on a multi-armed bandit model.
    ///
    /// This method takes in a `bandit` identifier and a list of possible `arms` (items) and uses a
    /// Thompson sampling approach to select the arm with the highest probability of success.
    /// For each arm, it retrieves the Beta distribution parameters (alpha and beta) from the
    /// database, creates a Beta distribution, and samples from it to estimate the arm's probability
    /// of success. The arm with the highest sampled probability is selected and returned.
    #[handle_error(Error)]
    pub fn bandit_select(&self, bandit: String, arms: &[String]) -> ApiResult<String> {
        // we should cache the distribution so we don't retrieve each time

        let mut best_sample = f64::MIN;
        let mut selected_arm = String::new();

        for arm in arms {
            let (alpha, beta) = self
                .db
                .read(|dao| dao.retrieve_bandit_arm_beta_distribution(&bandit, arm))?;
            // this creates a Beta distribution for an alpha & beta pair
            let beta_dist = Beta::new(alpha as f64, beta as f64)
                .expect("computing betas dist unexpectedly failed");

            // Sample from the Beta distribution
            let sampled_prob = beta_dist.sample(&mut rand::thread_rng());

            if sampled_prob > best_sample {
                best_sample = sampled_prob;
                selected_arm.clone_from(arm);
            }
        }

        return Ok(selected_arm);
    }

    /// Updates the bandit model's arm data based on user interaction (selection or non-selection).
    ///
    /// This method takes in a `bandit` identifier, an `arm` identifier, and a `selected` flag.
    /// If `selected` is true, it updates the model to reflect a successful selection of the arm,
    /// reinforcing its positive reward probability. If `selected` is false, it updates the
    /// beta (failure) distribution of the arm, reflecting a lack of selection and reinforcing
    /// its likelihood of a negative outcome.
    #[handle_error(Error)]
    pub fn bandit_update(&self, bandit: String, arm: String, selected: bool) -> ApiResult<()> {
        self.db
            .read_write(|dao| dao.update_bandit_arm_data(&bandit, &arm, selected))?;
        Ok(())
    }
}

impl RelevancyStore {
    /// Download the interest data from remote settings if needed
    #[handle_error(Error)]
    pub fn ensure_interest_data_populated(&self) -> ApiResult<()> {
        ingest::ensure_interest_data_populated(&self.db)?;
        Ok(())
    }

    pub fn classify(&self, top_urls_by_frecency: Vec<String>) -> Result<InterestVector> {
        let mut interest_vector = InterestVector::default();
        for url in top_urls_by_frecency {
            let interest_count = self.db.read(|dao| dao.get_url_interest_vector(&url))?;
            log::trace!("classified: {url} {}", interest_count.summary());
            interest_vector = interest_vector + interest_count;
        }
        Ok(interest_vector)
    }
}

/// Interest metrics that we want to send to Glean as part of the validation process.  These contain
/// the cosine similarity when comparing the user's interest against various interest vectors that
/// consumers may use.
///
/// Cosine similarly was chosen because it seems easy to calculate.  This was then matched against
/// some semi-plausible real-world interest vectors that consumers might use.  This is all up for
/// debate and we may decide to switch to some other metrics.
///
/// Similarity values are transformed to integers by multiplying the floating point value by 1000 and
/// rounding.  This is to make them compatible with Glean's distribution metrics.
#[derive(uniffi::Record)]
pub struct InterestMetrics {
    /// Similarity between the user's interest vector and an interest vector where the element for
    /// the user's top interest is copied, but all other interests are set to zero.  This measures
    /// the highest possible similarity with consumers that used interest vectors with a single
    /// interest set.
    pub top_single_interest_similarity: u32,
    /// The same as before, but the top 2 interests are copied. This measures the highest possible
    /// similarity with consumers that used interest vectors with a two interests (note: this means
    /// they would need to choose the user's top two interests and have the exact same proportion
    /// between them as the user).
    pub top_2interest_similarity: u32,
    /// The same as before, but the top 3 interests are copied.
    pub top_3interest_similarity: u32,
}

#[cfg(test)]
mod test {
    use crate::url_hash::hash_url;

    use super::*;
    use rand::Rng;
    use std::collections::HashMap;

    fn make_fixture() -> Vec<(String, Interest)> {
        vec![
            ("https://food.com/".to_string(), Interest::Food),
            ("https://hello.com".to_string(), Interest::Inconclusive),
            ("https://pasta.com".to_string(), Interest::Food),
            ("https://dog.com".to_string(), Interest::Animals),
        ]
    }

    fn expected_interest_vector() -> InterestVector {
        InterestVector {
            inconclusive: 1,
            animals: 1,
            food: 2,
            ..InterestVector::default()
        }
    }

    fn setup_store(test_id: &'static str) -> RelevancyStore {
        let relevancy_store =
            RelevancyStore::new(format!("file:test_{test_id}_data?mode=memory&cache=shared"));
        relevancy_store
            .db
            .read_write(|dao| {
                for (url, interest) in make_fixture() {
                    dao.add_url_interest(hash_url(&url).unwrap(), interest)?;
                }
                Ok(())
            })
            .expect("Insert should succeed");

        relevancy_store
    }

    #[test]
    fn test_ingest() {
        let relevancy_store = setup_store("ingest");
        let (top_urls, _): (Vec<String>, Vec<Interest>) = make_fixture().into_iter().unzip();

        assert_eq!(
            relevancy_store.ingest(top_urls).unwrap(),
            expected_interest_vector()
        );
    }

    #[test]
    fn test_get_user_interest_vector() {
        let relevancy_store = setup_store("get_user_interest_vector");
        let (top_urls, _): (Vec<String>, Vec<Interest>) = make_fixture().into_iter().unzip();

        relevancy_store
            .ingest(top_urls)
            .expect("Ingest should succeed");

        assert_eq!(
            relevancy_store.user_interest_vector().unwrap(),
            expected_interest_vector()
        );
    }

    #[test]
    fn test_thompson_sampling_convergence() {
        let relevancy_store = setup_store("thompson_sampling_convergence");

        let arms_to_ctr_map: HashMap<String, f64> = [
            ("wiki".to_string(), 0.1),        // 10% CTR
            ("geolocation".to_string(), 0.3), // 30% CTR
            ("weather".to_string(), 0.8),     // 80% CTR
        ]
        .into_iter()
        .collect();

        let arm_names: Vec<String> = arms_to_ctr_map.keys().cloned().collect();

        let bandit = "provider".to_string();

        // initialize bandit
        relevancy_store
            .bandit_init(bandit.clone(), &arm_names)
            .unwrap();

        let mut rng = rand::thread_rng();

        // Create a HashMap to map arm names to their selection counts
        let mut selection_counts: HashMap<String, usize> =
            arm_names.iter().map(|name| (name.clone(), 0)).collect();

        // Simulate 1000 rounds of Thompson Sampling
        for _ in 0..1000 {
            // Use Thompson Sampling to select an arm
            let selected_arm_name = relevancy_store
                .bandit_select(bandit.clone(), &arm_names)
                .expect("Failed to select arm");

            // increase the selection count for the selected arm
            *selection_counts.get_mut(&selected_arm_name).unwrap() += 1;

            // get the true CTR for the selected arm
            let true_ctr = &arms_to_ctr_map[&selected_arm_name];

            // simulate a click or no-click based on the true CTR
            let clicked = rng.gen_bool(*true_ctr);

            // update beta distribution for arm based on click/no click
            relevancy_store
                .bandit_update(bandit.clone(), selected_arm_name, clicked)
                .expect("Failed to update beta distribution for arm");
        }

        //retrieve arm with maximum selection count
        let most_selected_arm_name = selection_counts
            .iter()
            .max_by_key(|(_, count)| *count)
            .unwrap()
            .0;

        assert_eq!(
            most_selected_arm_name, "weather",
            "Thompson Sampling did not favor the best-performing arm"
        );
    }
}
