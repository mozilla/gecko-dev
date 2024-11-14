// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use glean::traits::CustomDistribution;

use crate::ipc::with_ipc_payload;
use crate::private::{CustomDistributionMetric, DistributionData, MetricId};
use std::collections::HashMap;

/// A custom_distribution metric that knows it's a labeled custom distribution's submetric.
///
/// It has special work to do when in a non-parent process.
/// When on the parent process, it dispatches calls to the normal CustomDistributionMetric.
#[derive(Clone)]
pub enum LabeledCustomDistributionMetric {
    Parent(CustomDistributionMetric),
    Child { id: MetricId, label: String },
}

impl LabeledCustomDistributionMetric {
    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        match self {
            LabeledCustomDistributionMetric::Parent(p) => p.metric_id(),
            LabeledCustomDistributionMetric::Child { id, .. } => *id,
        }
    }
}

#[inherent]
impl CustomDistribution for LabeledCustomDistributionMetric {
    pub fn accumulate_samples_signed(&self, samples: Vec<i64>) {
        match self {
            LabeledCustomDistributionMetric::Parent(p) => p.accumulate_samples_signed(samples),
            LabeledCustomDistributionMetric::Child { id, label } => {
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.labeled_custom_samples.get_mut(id) {
                        if let Some(v) = map.get_mut(label) {
                            v.extend(samples);
                        } else {
                            map.insert(label.to_string(), samples);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(label.to_string(), samples);
                        payload.labeled_custom_samples.insert(*id, map);
                    }
                });
            }
        }
    }

    pub fn accumulate_single_sample_signed(&self, sample: i64) {
        match self {
            LabeledCustomDistributionMetric::Parent(p) => p.accumulate_single_sample_signed(sample),
            LabeledCustomDistributionMetric::Child { id, label } => {
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.labeled_custom_samples.get_mut(id) {
                        if let Some(v) = map.get_mut(label) {
                            v.push(sample);
                        } else {
                            map.insert(label.to_string(), vec![sample]);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(label.to_string(), vec![sample]);
                        payload.labeled_custom_samples.insert(*id, map);
                    }
                });
            }
        }
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        match self {
            LabeledCustomDistributionMetric::Parent(p) => p.test_get_value(ping_name),
            LabeledCustomDistributionMetric::Child { id, .. } => {
                panic!("Cannot get test value for labeled_custom_distribution {:?} in non-parent process!", id)
            }
        }
    }

    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            LabeledCustomDistributionMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            LabeledCustomDistributionMetric::Child { id, .. } => panic!(
                "Cannot get the number of recorded errors for labeled_custom_distribution {:?} in non-parent process!",
                id
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_labeled_custom_distribution_value_parent() {
        let _lock = lock_test();

        let metric = &metrics::test_only::mabels_custom_label_lengths;
        metric.get("a_label").accumulate_single_sample_signed(1);

        assert_eq!(1, metric.get("a_label").test_get_value(None).unwrap().sum);
    }

    #[test]
    fn sets_labeled_custom_distribution_value_child() {
        let _lock = lock_test();

        let label = "some_label";

        let parent_metric = &metrics::test_only::mabels_custom_label_lengths;
        parent_metric.get(label).accumulate_single_sample_signed(3);

        {
            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // clear the per-process submetric cache,
            // or else we'll be given the parent-process child metric.
            {
                let mut map =
                    crate::metrics::__glean_metric_maps::submetric_maps::CUSTOM_DISTRIBUTION_MAP
                        .write()
                        .expect("Write lock for CUSTOM_DISTRIBUTION_MAP was poisoned");
                map.clear();
            }

            let child_metric = parent_metric.get(label);

            let metric_id = child_metric.metric_id();

            child_metric.accumulate_single_sample_signed(42);

            ipc::with_ipc_payload(move |payload| {
                assert_eq!(
                    vec![42],
                    *payload
                        .labeled_custom_samples
                        .get(&metric_id)
                        .unwrap()
                        .get(label)
                        .unwrap(),
                    "Stored the correct value in the ipc payload"
                );
            });

            // clear the per-process submetric cache again,
            // or else we'll be given the child-process child metric below.
            {
                let mut map =
                    crate::metrics::__glean_metric_maps::submetric_maps::CUSTOM_DISTRIBUTION_MAP
                        .write()
                        .expect("Write lock for CUSTOM_DISTRIBUTION_MAP was poisoned");
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
            parent_metric.get(label).test_get_value(None).unwrap().sum,
            "Values from the 'processes' should be summed"
        );
    }
}
