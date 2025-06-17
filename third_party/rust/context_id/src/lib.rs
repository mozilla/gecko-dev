/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod error;

pub use error::{ApiError, ApiResult, Error, Result};

use chrono::{DateTime, Duration, Utc};
use error_support::{error, handle_error};
use parking_lot::{Mutex, RwLock};
use uuid::Uuid;

uniffi::setup_scaffolding!("context_id");

mod callback;
pub use callback::{ContextIdCallback, DefaultContextIdCallback};

mod mars;
use mars::{MARSClient, SimpleMARSClient};

/// Top-level API for the context_id component
#[derive(uniffi::Object)]
pub struct ContextIDComponent {
    inner: Mutex<ContextIDComponentInner>,
}

#[uniffi::export]
impl ContextIDComponent {
    /// Construct a new [ContextIDComponent].
    ///
    /// If no creation timestamp is provided, the current time will be used.
    #[uniffi::constructor]

    pub fn new(
        init_context_id: &str,
        creation_timestamp_s: i64,
        running_in_test_automation: bool,
        callback: Box<dyn ContextIdCallback>,
    ) -> Self {
        Self {
            inner: Mutex::new(ContextIDComponentInner::new(
                init_context_id,
                creation_timestamp_s,
                running_in_test_automation,
                callback,
                Utc::now(),
                Box::new(SimpleMARSClient::new()),
            )),
        }
    }

    /// Return the current context ID string.
    #[handle_error(Error)]
    pub fn request(&self, rotation_days_in_s: u8) -> ApiResult<String> {
        let mut inner = self.inner.lock();
        inner.request(rotation_days_in_s, Utc::now())
    }

    /// Regenerate the context ID.
    #[handle_error(Error)]
    pub fn force_rotation(&self) -> ApiResult<()> {
        let mut inner = self.inner.lock();
        inner.force_rotation(Utc::now());
        Ok(())
    }

    /// Unset the callbacks set during construction, and use a default
    /// no-op ContextIdCallback instead.
    #[handle_error(Error)]
    pub fn unset_callback(&self) -> ApiResult<()> {
        let mut inner = self.inner.lock();
        inner.unset_callback();
        Ok(())
    }
}

struct ContextIDComponentInner {
    context_id: String,
    creation_timestamp: DateTime<Utc>,
    callback_handle: RwLock<Box<dyn ContextIdCallback>>,
    mars_client: Box<dyn MARSClient>,
    running_in_test_automation: bool,
}

impl ContextIDComponentInner {
    pub fn new(
        init_context_id: &str,
        creation_timestamp_s: i64,
        running_in_test_automation: bool,
        callback: Box<dyn ContextIdCallback>,
        now: DateTime<Utc>,
        mars_client: Box<dyn MARSClient>,
    ) -> Self {
        // Some historical context IDs are stored within opening and closing
        // braces, and our endpoints have tolerated this, but ideally we'd
        // send without the braces, so we strip any off here.
        let (context_id, generated_context_id) = match init_context_id
            .trim()
            .trim_start_matches('{')
            .trim_end_matches('}')
        {
            "" => (Uuid::new_v4().to_string(), true),
            // If the passed in string isn't empty, but still not a valid UUID,
            // just go ahead and generate a new UUID.
            s => match Uuid::parse_str(s) {
                Ok(_) => (s.to_string(), false),
                Err(_) => (Uuid::new_v4().to_string(), true),
            },
        };

        let (creation_timestamp, generated_creation_timestamp, force_rotation) =
            match (generated_context_id, creation_timestamp_s) {
                // We generated a new context ID then we need a fresh timestamp and no rotation needed
                (true, _) => (now, true, false),
                // Pre-existing context ID with a positive timestamp:
                // try parsing it (any real UNIX-epoch timestamp is orders of magnitude within chrono’s 262_000-year range),
                // and if it’s somehow out-of-range fall back to `now`  and force rotation
                // See: https://docs.rs/chrono/latest/chrono/naive/struct.NaiveDateTime.html#panics
                (false, secs) if secs > 0 => DateTime::<Utc>::from_timestamp(secs, 0)
                    .map(|ts| (ts, false, false))
                    .unwrap_or((now, true, true)),
                // Pre-existing context ID with zero timestamp then use current time and no rotation needed
                (false, 0) => (now, true, false),
                // Pre-existing context ID but INVALID timestamp then use current time but FORCE rotation
                (false, _) => (now, true, true),
            };

        let mut instance = Self {
            context_id,
            creation_timestamp,
            callback_handle: RwLock::new(callback),
            mars_client,
            running_in_test_automation,
        };

        if force_rotation {
            // force_rotation will cause a implicit persist
            instance.force_rotation(creation_timestamp)
        } else if generated_context_id || generated_creation_timestamp {
            // We only need to persist these if we just generated one.
            instance.persist();
        }

        instance
    }

    pub fn request(&mut self, rotation_days: u8, now: DateTime<Utc>) -> Result<String> {
        if rotation_days == 0 {
            return Ok(self.context_id.clone());
        }

        let age = now - self.creation_timestamp;
        if age >= Duration::days(rotation_days.into()) {
            self.rotate_context_id(now);
        }

        Ok(self.context_id.clone())
    }

    fn rotate_context_id(&mut self, now: DateTime<Utc>) {
        let original_context_id = self.context_id.clone();

        self.context_id = Uuid::new_v4().to_string();
        self.creation_timestamp = now;
        self.persist();

        // If we're running in test automation in the embedder, we don't want
        // to be sending actual network requests to MARS.
        if !self.running_in_test_automation {
            let _ = self
                .mars_client
                .delete(original_context_id.clone())
                .inspect_err(|e| error!("Failed to contact MARS: {}", e));
        }

        // In a perfect world, we'd call Glean ourselves here - however,
        // our uniffi / Rust infrastructure doesn't yet support doing that,
        // so we'll delegate to our embedder to send the Glean ping by
        // calling a `rotated` callback method.
        self.callback_handle
            .read()
            .rotated(original_context_id.clone());
    }

    pub fn force_rotation(&mut self, now: DateTime<Utc>) {
        self.rotate_context_id(now);
    }

    pub fn persist(&self) {
        self.callback_handle
            .read()
            .persist(self.context_id.clone(), self.creation_timestamp.timestamp());
    }

    pub fn unset_callback(&mut self) {
        self.callback_handle = RwLock::new(Box::new(DefaultContextIdCallback));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::callback::utils::SpyCallback;
    use chrono::Utc;
    use lazy_static::lazy_static;

    use std::sync::{Arc, Mutex};

    // 1745859061 ~= the timestamp for when this test was written (Apr 28, 2025)
    const FAKE_NOW_TS: i64 = 1745859061;
    const TEST_CONTEXT_ID: &str = "decafbad-0cd1-0cd2-0cd3-decafbad1000";
    // 1706763600 ~= Jan 1st, 2024, which is long ago compared to FAKE_NOW.
    const FAKE_LONG_AGO_TS: i64 = 1706763600;

    lazy_static! {
        static ref FAKE_NOW: DateTime<Utc> = DateTime::from_timestamp(FAKE_NOW_TS, 0).unwrap();
        static ref FAKE_LONG_AGO: DateTime<Utc> =
            DateTime::from_timestamp(FAKE_LONG_AGO_TS, 0).unwrap();
    }

    pub struct TestMARSClient {
        delete_called: Arc<Mutex<bool>>,
    }

    impl TestMARSClient {
        pub fn new(delete_called: Arc<Mutex<bool>>) -> Self {
            Self { delete_called }
        }
    }

    impl MARSClient for TestMARSClient {
        fn delete(&self, _context_id: String) -> crate::Result<()> {
            *self.delete_called.lock().unwrap() = true;
            Ok(())
        }
    }

    fn with_test_mars<F: FnOnce(Box<dyn MARSClient + Send + Sync>, Arc<Mutex<bool>>)>(test: F) {
        let delete_called = Arc::new(Mutex::new(false));
        let mars = Box::new(TestMARSClient::new(Arc::clone(&delete_called)));
        test(mars, delete_called);
    }

    #[test]
    fn test_creation_timestamp_with_some_value() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let creation_timestamp = FAKE_NOW_TS;
            let component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                creation_timestamp,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // We should have left the context_id and creation_timestamp
            // untouched if a creation_timestamp was passed.
            assert_eq!(component.context_id, TEST_CONTEXT_ID);
            assert_eq!(component.creation_timestamp.timestamp(), creation_timestamp);
            assert!(!*delete_called.lock().unwrap());
            // Neither persist nor rotate should have been called
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                0,
                "persist() should NOTE have been called"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                0,
                "rotated() should NOT have been called"
            );
        });
    }

    #[test]
    fn test_creation_timestamp_with_zero_value() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                0,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // If 0 was passed as the creation_timestamp, we'll interpret that
            // as there having been no stored creation_timestamp. In that case,
            // we'll use "now" as the creation_timestamp.
            assert_eq!(component.context_id, TEST_CONTEXT_ID);
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            assert!(!*delete_called.lock().unwrap());
            // zero‐timestamp should trigger a persist, but NOT a rotation
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                0,
                "rotated() should NOT have been called"
            );
        });
    }

    #[test]
    fn test_empty_initial_context_id() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let component =
                ContextIDComponentInner::new("", 0, false, spy.callback, *FAKE_NOW, mars);

            // We expect a new UUID to have been generated for context_id.
            assert!(Uuid::parse_str(&component.context_id).is_ok());
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                0,
                "rotated() should NOT have been called"
            );
            assert!(!*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_empty_initial_context_id_with_creation_date() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let component =
                ContextIDComponentInner::new("", 0, false, spy.callback, *FAKE_NOW, mars);

            // We expect a new UUID to have been generated for context_id.
            assert!(Uuid::parse_str(&component.context_id).is_ok());
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            assert!(!*delete_called.lock().unwrap());
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                0,
                "rotated() should NOT have been called"
            );
        });
    }

    #[test]
    fn test_invalid_context_id_with_no_creation_date() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let component = ContextIDComponentInner::new(
                "something-invalid",
                0,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // We expect a new UUID to have been generated for context_id.
            assert!(Uuid::parse_str(&component.context_id).is_ok());
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            // Also expect a persist to have been called, but not a rotation.
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                0,
                "rotated() should NOT have been called"
            );
            assert!(!*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_invalid_context_id_with_creation_date() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let component = ContextIDComponentInner::new(
                "something-invalid",
                FAKE_LONG_AGO_TS,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // We expect a new UUID to have been generated for context_id.
            assert!(Uuid::parse_str(&component.context_id).is_ok());
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            // Also expect a persist to have been called, but not a rotation.
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                0,
                "rotated() should NOT have been called"
            );
            assert!(!*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_context_id_with_invalid_creation_date() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            let component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                -1,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // A new UUID should have been generated.
            assert!(Uuid::parse_str(&component.context_id).is_ok());
            assert_ne!(component.context_id, TEST_CONTEXT_ID);

            // The creation timestamp must have been set to now.
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());

            // Also expect a persist to have been called and a rotation.
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                1,
                "rotated() should have been called once"
            );
            // Since we forced a rotation, the MARS delete should have been called.
            assert!(*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_context_id_with_out_of_range_creation_date() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            // Way beyond chrono’s 262_000-year range
            let huge_secs = i64::MAX;
            let component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                huge_secs,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // A new UUID should have been generated.
            assert!(Uuid::parse_str(&component.context_id).is_ok());
            assert_ne!(component.context_id, TEST_CONTEXT_ID);

            // The creation timestamp must have been set to now.
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());

            // Also expect a persist to have been called and a rotation.
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called once"
            );
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                1,
                "rotated() should have been called once"
            );
            // Since we forced a rotation on out-of-range, the MARS delete should have been called.
            assert!(*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_request_no_rotation() {
        with_test_mars(|mars, delete_called| {
            // Let's create a context_id with a creation date far in the past.
            let mut component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                FAKE_LONG_AGO_TS,
                false,
                Box::new(DefaultContextIdCallback),
                *FAKE_NOW,
                mars,
            );

            // We expect neither the UUID nor creation_timestamp to have been changed.
            assert_eq!(component.context_id, TEST_CONTEXT_ID);
            assert_eq!(component.creation_timestamp, FAKE_LONG_AGO.clone());

            // Now request the context_id, passing 0 for the rotation_days. We
            // interpret this to mean "do not rotate".
            assert_eq!(
                component.request(0, *FAKE_NOW).unwrap(),
                component.context_id
            );
            assert_eq!(component.creation_timestamp, FAKE_LONG_AGO.clone());
            assert!(!*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_request_with_rotation() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();
            // Let's create a context_id with a creation date far in the past.
            let mut component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                FAKE_LONG_AGO_TS,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            // We expect neither the UUID nor creation_timestamp to have been changed.
            assert_eq!(component.context_id, TEST_CONTEXT_ID);
            assert_eq!(component.creation_timestamp, FAKE_LONG_AGO.clone());

            // Now request the context_id, passing 2 for the rotation_days. Since
            // the number of days since FAKE_LONG_AGO is greater than 2 days, we
            // expect a new context_id to be generated, and the creation_timestamp
            // to update to now.
            assert!(Uuid::parse_str(&component.request(2, *FAKE_NOW).unwrap()).is_ok());
            assert_ne!(component.context_id, TEST_CONTEXT_ID);
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            assert!(*delete_called.lock().unwrap());

            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                1,
                "rotated() should have been called"
            );
            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called"
            );
            assert_eq!(
                spy.last_rotated_id.lock().unwrap().as_deref().unwrap(),
                TEST_CONTEXT_ID
            );
        });
    }

    #[test]
    fn test_force_rotation() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();

            let mut component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                FAKE_LONG_AGO_TS,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            component.force_rotation(*FAKE_NOW);
            assert!(Uuid::parse_str(&component.request(2, *FAKE_NOW).unwrap()).is_ok());
            assert_ne!(component.context_id, TEST_CONTEXT_ID);
            assert_eq!(component.creation_timestamp, FAKE_NOW.clone());
            assert!(*delete_called.lock().unwrap());
            assert_eq!(
                *spy.rotated_called.lock().unwrap(),
                1,
                "rotated() should have been called"
            );
            assert_eq!(
                spy.last_rotated_id.lock().unwrap().as_deref().unwrap(),
                TEST_CONTEXT_ID
            );
        });
    }

    #[test]
    fn test_accept_braces() {
        with_test_mars(|mars, delete_called| {
            // Some callers may store pre-existing context IDs with opening
            // and closing curly braces. Our component should accept them, but
            // return (and persist) UUIDs without such braces.
            let wrapped_context_id = ["{", TEST_CONTEXT_ID, "}"].concat();
            let mut component = ContextIDComponentInner::new(
                &wrapped_context_id,
                0,
                false,
                Box::new(DefaultContextIdCallback),
                *FAKE_NOW,
                mars,
            );

            // We expect to be storing TEST_CONTEXT_ID, and to return
            // TEST_CONTEXT_ID without the braces.
            assert_eq!(component.context_id, TEST_CONTEXT_ID);
            assert!(Uuid::parse_str(&component.request(0, *FAKE_NOW).unwrap()).is_ok());
            assert!(!*delete_called.lock().unwrap());
        });
    }

    #[test]
    fn test_persist_callback() {
        with_test_mars(|mars, delete_called| {
            let spy = SpyCallback::new();

            let mut component = ContextIDComponentInner::new(
                TEST_CONTEXT_ID,
                FAKE_LONG_AGO_TS,
                false,
                spy.callback,
                *FAKE_NOW,
                mars,
            );

            component.force_rotation(*FAKE_NOW);

            assert_eq!(
                *spy.persist_called.lock().unwrap(),
                1,
                "persist() should have been called"
            );

            assert!(
                Uuid::parse_str(spy.last_rotated_id.lock().unwrap().as_deref().unwrap()).is_ok(),
                "persist() should have received a valid context_id string"
            );

            assert_eq!(
                *spy.last_persist_ts.lock().unwrap(),
                Some(FAKE_NOW_TS),
                "persist() should have received the expected creation date"
            );
            assert!(*delete_called.lock().unwrap());
        });
    }
}
