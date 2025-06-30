// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use glean::traits::Counter;

use crate::ipc::with_ipc_payload;
#[cfg(test)]
use crate::private::MetricId;
use crate::private::{BaseMetricId, CounterMetric};

use std::collections::HashMap;

/// A counter metric that knows it's a dual_labeled_counter's submetric.
///
/// It has special work to do when in a non-parent process.
/// When on the parent process, it dispatches calls to the normal CounterMetric.
#[derive(Clone)]
pub enum DualLabeledCounterSubMetric {
    Parent(CounterMetric),
    Child {
        id: BaseMetricId,
        dual_labels: (String, String),
    },
}

impl DualLabeledCounterSubMetric {
    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        match self {
            DualLabeledCounterSubMetric::Parent(p) => p.metric_id(),
            DualLabeledCounterSubMetric::Child { id, .. } => (*id).into(),
        }
    }
}

#[inherent]
impl Counter for DualLabeledCounterSubMetric {
    /// Increase the counter by `amount`.
    ///
    /// ## Arguments
    ///
    /// * `amount` - The amount to increase by. Should be positive.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is 0 or negative.
    pub fn add(&self, amount: i32) {
        match self {
            DualLabeledCounterSubMetric::Parent(p) => p.add(amount),
            DualLabeledCounterSubMetric::Child { id, dual_labels } => {
                /* bug 1973287 glean::DualLabeledCounterMetric doesn't impl glean::MetricType
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "LabeledCounter::add",
                        super::profiler_utils::TelemetryProfilerCategory,
                        Default::default(),
                        super::profiler_utils::IntLikeMetricMarker::<DualLabeledCounterSubMetric, i32>::new(
                            (*id).into(),
                            Some(key + ", " + category),
                            amount,
                        ),
                    );
                }*/
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.dual_labeled_counters.get_mut(id) {
                        if let Some(v) = map.get_mut(dual_labels) {
                            *v += amount;
                        } else {
                            map.insert(dual_labels.clone(), amount);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(dual_labels.clone(), amount);
                        payload.dual_labeled_counters.insert(*id, map);
                    }
                });
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as an integer.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `ping_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<i32> {
        match self {
            DualLabeledCounterSubMetric::Parent(p) => p.test_get_value(ping_name),
            DualLabeledCounterSubMetric::Child { id, .. } => {
                panic!("Cannot get test value for {:?} in non-parent process!", id)
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            DualLabeledCounterSubMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            DualLabeledCounterSubMetric::Child { id, .. } => panic!(
                "Cannot get the number of recorded errors for {:?} in non-parent process!",
                id
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_dual_labeled_counter_value_parent() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_dual_labeled_counter;
        metric.get("a_key", "a_category").add(1);

        assert_eq!(
            1,
            metric
                .get("a_key", "a_category")
                .test_get_value("test-ping")
                .unwrap()
        );
    }

    #[test]
    fn sets_dual_labeled_counter_value_child() {
        let _lock = lock_test();

        let key = "some_key";
        let category = "some_category";

        let parent_metric = &metrics::test_only_ipc::a_dual_labeled_counter;
        parent_metric.get(key, category).add(3);

        {
            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // clear the per-process submetric cache,
            // or else we'll be given the parent-process child metric.
            {
                let mut map = crate::metrics::__glean_metric_maps::submetric_maps::DUAL_COUNTER_MAP
                    .write()
                    .expect("Write lock for COUNTER_MAP was poisoned");
                map.clear();
            }

            let child_metric = parent_metric.get(key, category);

            let metric_id = child_metric
                .metric_id()
                .base_metric_id()
                .expect("Cannot perform IPC calls without a BaseMetricId");

            child_metric.add(42);

            ipc::with_ipc_payload(move |payload| {
                assert_eq!(
                    42,
                    *payload
                        .dual_labeled_counters
                        .get(&metric_id)
                        .unwrap()
                        .get(&(key.to_string(), category.to_string()))
                        .unwrap(),
                    "Stored the correct value in the ipc payload"
                );
            });

            // clear the per-process submetric cache again,
            // or else we'll be given the child-process child metric below.
            {
                let mut map = crate::metrics::__glean_metric_maps::submetric_maps::DUAL_COUNTER_MAP
                    .write()
                    .expect("Write lock for COUNTER_MAP was poisoned");
                map.clear();
            }
        }

        assert!(
            false == ipc::need_ipc(),
            "RAII dropped, should not need ipc any more"
        );
        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert_eq!(
            45,
            parent_metric
                .get(key, category)
                .test_get_value("test-ping")
                .unwrap(),
            "Values from the 'processes' should be summed"
        );
    }
}
