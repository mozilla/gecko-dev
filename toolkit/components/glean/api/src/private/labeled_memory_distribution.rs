// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use glean::traits::MemoryDistribution;

use crate::ipc::with_ipc_payload;
use crate::private::{DistributionData, MemoryDistributionMetric, MetricId};
use std::collections::HashMap;

/// A memory distribution metric that knows it's a labeled memory distribution's submetric.
///
/// It has special work to do when in a non-parent process.
/// (When on the parent process it merely dispatches calls to the normal MemoryDistributionMetric.)
#[derive(Clone)]
pub enum LabeledMemoryDistributionMetric {
    Parent(MemoryDistributionMetric),
    Child { id: MetricId, label: String },
}

impl LabeledMemoryDistributionMetric {
    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        match self {
            LabeledMemoryDistributionMetric::Parent(p) => p.metric_id(),
            LabeledMemoryDistributionMetric::Child { id, .. } => *id,
        }
    }

    pub fn accumulate_samples(&self, samples: Vec<u64>) {
        match self {
            LabeledMemoryDistributionMetric::Parent(p) => p.accumulate_samples(samples),
            LabeledMemoryDistributionMetric::Child { id, label } => {
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.labeled_memory_samples.get_mut(id) {
                        if let Some(v) = map.get_mut(label) {
                            v.extend(samples);
                        } else {
                            map.insert(label.to_string(), samples);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(label.to_string(), samples);
                        payload.labeled_memory_samples.insert(*id, map);
                    }
                });
            }
        }
    }
}

#[inherent]
impl MemoryDistribution for LabeledMemoryDistributionMetric {
    pub fn accumulate(&self, sample: u64) {
        match self {
            LabeledMemoryDistributionMetric::Parent(p) => p.accumulate(sample),
            LabeledMemoryDistributionMetric::Child { id, label } => {
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.labeled_memory_samples.get_mut(id) {
                        if let Some(v) = map.get_mut(label) {
                            v.push(sample);
                        } else {
                            map.insert(label.to_string(), vec![sample]);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(label.to_string(), vec![sample]);
                        payload.labeled_memory_samples.insert(*id, map);
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
            LabeledMemoryDistributionMetric::Parent(p) => p.test_get_value(ping_name),
            LabeledMemoryDistributionMetric::Child { id, .. } => {
                panic!("Cannot get test value for labeled_memory_distribution {:?} in non-parent process!", id)
            }
        }
    }

    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            LabeledMemoryDistributionMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            LabeledMemoryDistributionMetric::Child {id, .. } => panic!(
                "Cannot get the number of recorded errors for labeled_memory_distribution {:?} in non-parent process!",
                id
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_labeled_memory_distribution() {
        let _lock = lock_test();

        let metric = &metrics::test_only::what_do_you_remember;

        metric.get("happy").accumulate(42);

        let metric_data = metric.get("happy").test_get_value(None).unwrap();
        assert_eq!(1, metric_data.values[&43514714]);
        assert_eq!(44040192, metric_data.sum);
    }

    #[test]
    fn memory_distribution_child() {
        let _lock = lock_test();

        let label = "nostalgic";

        let parent_metric = &metrics::test_only::what_do_you_remember;
        parent_metric.get(label).accumulate(42);

        {
            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // clear the per-process submetric cache,
            // or else we'll be given the parent-process child metric.
            {
                let mut map =
                    crate::metrics::__glean_metric_maps::submetric_maps::MEMORY_DISTRIBUTION_MAP
                        .write()
                        .expect("Write lock for MEMORY_DISTRIBUTION_MAP was poisoned");
                map.clear();
            }

            let child_metric = parent_metric.get(label);
            child_metric.accumulate(13 * 9);

            ipc::with_ipc_payload(move |payload| {
                assert_eq!(
                    vec![13 * 9],
                    *payload
                        .labeled_memory_samples
                        .get(&child_metric.metric_id())
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
                    crate::metrics::__glean_metric_maps::submetric_maps::MEMORY_DISTRIBUTION_MAP
                        .write()
                        .expect("Write lock for MEMORY_DISTRIBUTION_MAP was poisoned");
                map.clear();
            }
        }

        let metric_data = parent_metric.get(label).test_get_value(None).unwrap();
        assert_eq!(1, metric_data.values[&43514714]);
        assert_eq!(44040192, metric_data.sum);

        // Single-process IPC machine goes brrrrr...
        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric
            .get(label)
            .test_get_value(None)
            .expect("must have data");
        assert_eq!(2, data.values.values().fold(0, |acc, count| acc + count));
        assert_eq!(1, data.values[&43514714]);
        assert_eq!(1, data.values[&117860087]);
        assert_eq!(166723584, data.sum);
    }
}
