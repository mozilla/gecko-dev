// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use glean::traits::Boolean;

use crate::ipc::with_ipc_payload;
use crate::private::{BooleanMetric, MetricId};
use std::collections::HashMap;

/// A boolean metric that knows it's a labeled_boolean's submetric.
///
/// It has special work to do when in a non-parent process.
/// When on the parent process, it dispatches calls to the normal BooleanMetric.
#[derive(Clone)]
pub enum LabeledBooleanMetric {
    Parent(BooleanMetric),
    Child,
    UnorderedChild { id: MetricId, label: String },
}

impl LabeledBooleanMetric {
    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        match self {
            LabeledBooleanMetric::Parent(p) => p.metric_id(),
            LabeledBooleanMetric::UnorderedChild { id, .. } => *id,
            _ => panic!("Can't get metric_id from child labeled_boolean in tests."),
        }
    }
}

#[inherent]
impl Boolean for LabeledBooleanMetric {
    pub fn set(&self, value: bool) {
        match self {
            LabeledBooleanMetric::Parent(p) => p.set(value),
            LabeledBooleanMetric::Child => {
                log::error!("Unable to set boolean metric in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.
                assert!(!crate::ipc::is_in_automation(), "Attempted to set boolean metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error.
            }
            LabeledBooleanMetric::UnorderedChild { id, label } => {
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.labeled_booleans.get_mut(id) {
                        if let Some(v) = map.get_mut(label) {
                            *v = value;
                        } else {
                            map.insert(label.to_string(), value);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(label.to_string(), value);
                        payload.labeled_booleans.insert(*id, map);
                    }
                });
            }
        }
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<bool> {
        match self {
            LabeledBooleanMetric::Parent(p) => p.test_get_value(ping_name),
            _ => {
                panic!("Cannot get test value for a labeled_boolean in non-parent process!")
            }
        }
    }

    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            LabeledBooleanMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            _ => panic!(
                "Cannot get the number of recorded errors for a labeled_boolean in non-parent process!"
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_labeled_boolean_value_parent() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::an_unordered_labeled_boolean;
        metric.get("a_label").set(true);

        assert!(metric.get("a_label").test_get_value("store1").unwrap());
    }

    #[test]
    fn sets_labeled_boolean_value_child() {
        let _lock = lock_test();

        let label = "some_label";

        let parent_metric = &metrics::test_only_ipc::an_unordered_labeled_boolean;
        let submetric = parent_metric.get(label);
        assert!(matches!(*submetric, super::LabeledBooleanMetric::Parent(_)));
        submetric.set(true);

        {
            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // clear the per-process submetric cache,
            // or else we'll be given the parent-process child metric.
            {
                let mut map = crate::metrics::__glean_metric_maps::submetric_maps::BOOLEAN_MAP
                    .write()
                    .expect("Write lock for BOOLEAN_MAP was poisoned");
                map.clear();
            }

            let child_metric = parent_metric.get(label);

            assert!(matches!(
                *child_metric,
                super::LabeledBooleanMetric::UnorderedChild { .. }
            ));

            let metric_id = child_metric.metric_id();

            child_metric.set(false);

            ipc::with_ipc_payload(move |payload| {
                assert!(
                    !*payload
                        .labeled_booleans
                        .get(&metric_id)
                        .unwrap()
                        .get(label)
                        .unwrap(),
                    "Stored the correct value in the ipc payload"
                );
            });

            // clear the per-process submetric cache again,
            // otherwise we'll supply the child metric instead of the parent, below.
            {
                let mut map = crate::metrics::__glean_metric_maps::submetric_maps::BOOLEAN_MAP
                    .write()
                    .expect("Write lock for BOOLEAN_MAP was poisoned");
                map.clear();
            }
        }

        assert!(
            false == ipc::need_ipc(),
            "RAII dropped, should not need ipc any more"
        );
        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert!(
            !parent_metric.get(label).test_get_value("store1").unwrap(),
            "Later value takes precedence."
        );
    }
}
