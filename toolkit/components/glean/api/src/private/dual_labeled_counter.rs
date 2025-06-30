// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{atomic::Ordering, Arc};

use super::{
    BaseMetricId, ChildMetricMeta, CommonMetricData, CounterMetric, DualLabeledCounterSubMetric,
    SubMetricId,
};
use crate::ipc::need_ipc;
use crate::metrics::__glean_metric_maps::submetric_maps;
use std::borrow::Cow;

/// A dual_labeled_counter metric.
///
/// Used for counting things per key, per category.
#[derive(Clone)]
pub enum DualLabeledCounterMetric {
    Parent {
        id: BaseMetricId,
        inner: Arc<glean::private::DualLabeledCounterMetric>,
    },
    Child(ChildMetricMeta),
}

impl DualLabeledCounterMetric {
    /// Create a new dual_labeled_counter metric.
    pub fn new(
        id: BaseMetricId,
        meta: CommonMetricData,
        keys: Option<Vec<Cow<'static, str>>>,
        categories: Option<Vec<Cow<'static, str>>>,
    ) -> Self {
        if need_ipc() {
            DualLabeledCounterMetric::Child(ChildMetricMeta::from_common_metric_data(id, meta))
        } else {
            let inner = Arc::new(glean::private::DualLabeledCounterMetric::new(
                meta, keys, categories,
            ));
            DualLabeledCounterMetric::Parent { id: id, inner }
        }
    }

    pub(crate) fn get_submetric_id(&self, key: &str, category: &str) -> u32 {
        self.get_submetric_and_id(key, category).1 .0
    }

    pub fn get(&self, key: &str, category: &str) -> Arc<DualLabeledCounterSubMetric> {
        self.get_submetric_and_id(key, category).0
    }

    fn get_submetric_and_id(
        &self,
        key: &str,
        category: &str,
    ) -> (Arc<DualLabeledCounterSubMetric>, SubMetricId) {
        // The author acknowledges that perhaps this could be better if it were
        // * merged with labeled::submetric_id_for
        // * not concatenating the key and category and instead used a separate map
        //   that takes (id, key, category)
        // * <maybe some separate, third thing>
        // but that none of these are guaranteed to be sufficiently better to be worth
        // the toil at this point in time.
        let label = format!("{key}\u{001E}{category}");
        let id = match self {
            DualLabeledCounterMetric::Parent { id, .. } => *id,
            DualLabeledCounterMetric::Child(meta) => meta.id,
        };
        let tuple = (id, label);
        let mut map = submetric_maps::LABELED_METRICS_TO_IDS
            .write()
            .expect("Write lock of submetric ids was poisoned");
        let submetric_id = (*map.entry(tuple).or_insert_with(|| {
            submetric_maps::NEXT_LABELED_SUBMETRIC_ID
                .fetch_add(1, Ordering::SeqCst)
                .into()
        }))
        .into();

        let mut map = submetric_maps::DUAL_COUNTER_MAP
            .write()
            .expect("write lock of DUAL_COUNTER_MAP was poisoned");
        let submetric = map.entry(submetric_id).or_insert_with(|| {
            let submetric = if need_ipc() {
                DualLabeledCounterSubMetric::Child {
                    id,
                    dual_labels: (key.to_string(), category.to_string()),
                }
            } else {
                let inner = match self {
                    DualLabeledCounterMetric::Parent { inner, .. } => inner,
                    DualLabeledCounterMetric::Child(_) => {
                        panic!("Can't create parent submetric in child metric")
                    }
                };
                DualLabeledCounterSubMetric::Parent(CounterMetric::Parent {
                    id: submetric_id.into(),
                    inner: inner.get(key, category),
                })
            };
            Arc::new(submetric)
        });
        (Arc::clone(submetric), submetric_id)
    }
}
