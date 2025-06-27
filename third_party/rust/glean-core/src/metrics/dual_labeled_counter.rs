// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::borrow::Cow;
use std::char;
use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex};

use crate::common_metric_data::{CommonMetricData, CommonMetricDataInternal, DynamicLabelType};
use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::{CounterMetric, Metric, MetricType};
use crate::Glean;

const MAX_LABELS: usize = 16;
const OTHER_LABEL: &str = "__other__";
const MAX_LABEL_LENGTH: usize = 111;
pub(crate) const RECORD_SEPARATOR: char = '\x1E';

/// A dual labled metric
///
/// Dual labled metrics allow recording multiple sub-metrics of the same type, in relation
/// to two dimensions rather than the single label provided by the standard labeled type.
#[derive(Debug)]
pub struct DualLabeledCounterMetric {
    keys: Option<Vec<Cow<'static, str>>>,
    categories: Option<Vec<Cow<'static, str>>>,
    /// Type of the underlying metric
    /// We hold on to an instance of it, which is cloned to create new modified instances.
    counter: CounterMetric,

    /// A map from a unique ID for the dual labeled submetric to a handle of an instantiated
    /// metric type.
    dual_label_map: Mutex<HashMap<(String, String), Arc<CounterMetric>>>,
}

impl ::malloc_size_of::MallocSizeOf for DualLabeledCounterMetric {
    fn size_of(&self, _ops: &mut malloc_size_of::MallocSizeOfOps) -> usize {
        // let map = self.dual_label_map.lock().unwrap();
        // let keys: Vec<String> = map.keys().collect();
        // let categories = map.values().for_each(|category| -> String  { category.keys() });

        // // Copy of `MallocShallowSizeOf` implementation for `HashMap<K, V>` in `wr_malloc_size_of`.
        // // Note: An instantiated submetric is behind an `Arc`.
        // // `size_of` should only be called from a single thread to avoid double-counting.
        // let shallow_size = if ops.has_malloc_enclosing_size_of() {
        //     map.values()
        //         .next()
        //         .map_or(0, |v| unsafe { ops.malloc_enclosing_size_of(v) })
        // } else {
        //     map.capacity()
        //         * (mem::size_of::<String>() + mem::size_of::<T>() + mem::size_of::<usize>())
        // };

        // let mut map_size = shallow_size;
        // for (k, v) in map.iter() {
        //     map_size += k.size_of(ops);
        //     map_size += v.size_of(ops);
        // }

        // self.labels.size_of(ops) + self.submetric.size_of(ops) + map_size
        0
    }
}

impl DualLabeledCounterMetric {
    /// Creates a new dual labeled counter from the given metric instance and optional list of labels.
    pub fn new(
        meta: CommonMetricData,
        keys: Option<Vec<Cow<'static, str>>>,
        catgories: Option<Vec<Cow<'static, str>>>,
    ) -> DualLabeledCounterMetric {
        let submetric = CounterMetric::new(meta);
        DualLabeledCounterMetric::new_inner(submetric, keys, catgories)
    }

    fn new_inner(
        counter: CounterMetric,
        keys: Option<Vec<Cow<'static, str>>>,
        categories: Option<Vec<Cow<'static, str>>>,
    ) -> DualLabeledCounterMetric {
        let dual_label_map = Default::default();
        DualLabeledCounterMetric {
            keys,
            categories,
            counter,
            dual_label_map,
        }
    }

    /// Creates a new metric with a specific key and category, validating against
    /// the static or dynamic labels where needed.
    fn new_counter_metric(&self, key: &str, category: &str) -> CounterMetric {
        match (&self.keys, &self.categories) {
            (None, None) => self
                .counter
                .with_dynamic_label(DynamicLabelType::KeyAndCategory(
                    make_label_from_key_and_category(key, category),
                )),
            (None, _) => {
                let static_category = self.static_category(category);
                self.counter.with_dynamic_label(DynamicLabelType::KeyOnly(
                    make_label_from_key_and_category(key, static_category),
                ))
            }
            (_, None) => {
                let static_key = self.static_key(key);
                self.counter
                    .with_dynamic_label(DynamicLabelType::CategoryOnly(
                        make_label_from_key_and_category(static_key, category),
                    ))
            }
            (_, _) => {
                // Both labels are static and can be validated now
                let static_key = self.static_key(key);
                let static_category = self.static_category(category);
                let name = combine_base_identifier_and_labels(
                    self.counter.meta().inner.name.as_str(),
                    static_key,
                    static_category,
                );
                self.counter.with_name(name)
            }
        }
    }

    /// Creates a static label for the key dimension.
    ///
    /// # Safety
    ///
    /// Should only be called when static labels are available on this metric.
    ///
    /// # Arguments
    ///
    /// * `key` - The requested key
    ///
    /// # Returns
    ///
    /// The requested key if it is in the list of allowed labels.
    /// Otherwise `OTHER_LABEL` is returned.
    fn static_key<'a>(&self, key: &'a str) -> &'a str {
        debug_assert!(self.keys.is_some());
        let keys = self.keys.as_ref().unwrap();
        if keys.iter().any(|l| l == key) {
            key
        } else {
            OTHER_LABEL
        }
    }

    /// Creates a static label for the category dimension.
    ///
    /// # Safety
    ///
    /// Should only be called when static labels are available on this metric.
    ///
    /// # Arguments
    ///
    /// * `category` - The requested category
    ///
    /// # Returns
    ///
    /// The requested category if it is in the list of allowed labels.
    /// Otherwise `OTHER_LABEL` is returned.
    fn static_category<'a>(&self, category: &'a str) -> &'a str {
        debug_assert!(self.categories.is_some());
        let categories = self.categories.as_ref().unwrap();
        if categories.iter().any(|l| l == category) {
            category
        } else {
            OTHER_LABEL
        }
    }

    /// Gets a specific metric for a given key/category combination.
    ///
    /// If a set of acceptable labels were specified in the `metrics.yaml` file,
    /// and the given label is not in the set, it will be recorded under the special `OTHER_LABEL` label.
    ///
    /// If a set of acceptable labels was not specified in the `metrics.yaml` file,
    /// only the first 16 unique labels will be used.
    /// After that, any additional labels will be recorded under the special `OTHER_LABEL` label.
    ///
    /// Labels must have a maximum of 111 characters, and may comprise any printable ASCII characters.
    /// If an invalid label is used, the metric will be recorded in the special `OTHER_LABEL` label.
    pub fn get<S: AsRef<str>>(&self, key: S, category: S) -> Arc<CounterMetric> {
        let key = key.as_ref();
        let category = category.as_ref();

        let mut map = self.dual_label_map.lock().unwrap();
        map.entry((key.to_string(), category.to_string()))
            .or_insert_with(|| {
                let metric = self.new_counter_metric(key, category);
                Arc::new(metric)
            })
            .clone()
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.counter.meta(), error).unwrap_or(0)
        })
    }
}

/// Combines a metric's base identifier and label
pub fn combine_base_identifier_and_labels(
    base_identifer: &str,
    key: &str,
    category: &str,
) -> String {
    format!(
        "{}{}",
        base_identifer,
        make_label_from_key_and_category(key, category)
    )
}

/// Separate label into key and category components.
/// Must validate the label format before calling this to ensure it doesn't contain
/// any ASCII record separator characters aside from the one's we put there.
pub fn separate_label_into_key_and_category(label: &str) -> Option<(&str, &str)> {
    label
        .strip_prefix(RECORD_SEPARATOR)
        .unwrap_or(label)
        .split_once(RECORD_SEPARATOR)
}

/// Construct and return a label from a given key and category with the RECORD_SEPARATOR
/// characters in the format: <RS><key><RS><category>
pub fn make_label_from_key_and_category(key: &str, category: &str) -> String {
    format!(
        "{}{}{}{}",
        RECORD_SEPARATOR, key, RECORD_SEPARATOR, category
    )
}

/// Validates a dynamic label, changing it to `OTHER_LABEL` if it's invalid.
///
/// Checks the requested label against limitations, such as the label length and allowed
/// characters.
///
/// # Arguments
///
/// * `label` - The requested label
///
/// # Returns
///
/// The entire identifier for the metric, including the base identifier and the corrected label.
/// The errors are logged.
pub fn validate_dynamic_key_and_or_category(
    glean: &Glean,
    meta: &CommonMetricDataInternal,
    base_identifier: &str,
    label: DynamicLabelType,
) -> String {
    // We should have exactly 3 elements when splitting by `RECORD_SEPARATOR`, since the label should begin with one and
    // then the key and category are separated by one. Split should contain an empty string, the key, and the category.
    // If we have more than 3 elements, then the consuming app must have used this character as part of a label and we
    // cannot determine whether it was the key or the category at this point, so we record an `InvalidLabel` error and
    // return `OTHER_LABEL` for both key and category.
    if label.split(RECORD_SEPARATOR).count() != 3 {
        let msg = "Label cannot contain the ASCII record separator character (0x1E)".to_string();
        record_error(glean, meta, ErrorType::InvalidLabel, msg, None);
        return combine_base_identifier_and_labels(base_identifier, OTHER_LABEL, OTHER_LABEL);
    }

    // Pick out the key and category from the supplied label
    if let Some((mut key, mut category)) = separate_label_into_key_and_category(&label) {
        // Loop through the stores we expect to find this metric in, and if we
        // find it then just return the full metric identifier that was found
        for store in &meta.inner.send_in_pings {
            if glean.storage().has_metric(meta.inner.lifetime, store, key) {
                return combine_base_identifier_and_labels(base_identifier, key, category);
            }
        }

        // Count the number of distinct keys and categories already recorded, we can figure out which
        // one(s) to check based on the label variant.
        let (seen_keys, seen_categories) = get_seen_keys_and_categories(meta, glean);
        match label {
            DynamicLabelType::Label(ref label) => {
                record_error(
                    glean,
                    meta,
                    ErrorType::InvalidLabel,
                    format!("Invalid `DualLabeledCounter` label format: {label:?}"),
                    None,
                );
                key = OTHER_LABEL;
                category = OTHER_LABEL;
            }
            DynamicLabelType::KeyOnly(_) => {
                if (!seen_keys.contains(key) && seen_keys.len() >= MAX_LABELS)
                    || !label_is_valid(key, glean, meta)
                {
                    key = OTHER_LABEL;
                }
            }
            DynamicLabelType::CategoryOnly(_) => {
                if (!seen_categories.contains(category) && seen_categories.len() >= MAX_LABELS)
                    || !label_is_valid(category, glean, meta)
                {
                    category = OTHER_LABEL;
                }
            }
            DynamicLabelType::KeyAndCategory(_) => {
                if (!seen_keys.contains(key) && seen_keys.len() >= MAX_LABELS)
                    || !label_is_valid(key, glean, meta)
                {
                    key = OTHER_LABEL;
                }
                if (!seen_categories.contains(category) && seen_categories.len() >= MAX_LABELS)
                    || !label_is_valid(category, glean, meta)
                {
                    category = OTHER_LABEL;
                }
            }
        }
        combine_base_identifier_and_labels(base_identifier, key, category)
    } else {
        record_error(
            glean,
            meta,
            ErrorType::InvalidLabel,
            "Invalid `DualLabeledCounter` label format, unable to determine key and/or category",
            None,
        );
        combine_base_identifier_and_labels(base_identifier, OTHER_LABEL, OTHER_LABEL)
    }
}

fn label_is_valid(label: &str, glean: &Glean, meta: &CommonMetricDataInternal) -> bool {
    if label.len() > MAX_LABEL_LENGTH {
        let msg = format!(
            "label length {} exceeds maximum of {}",
            label.len(),
            MAX_LABEL_LENGTH
        );
        record_error(glean, meta, ErrorType::InvalidLabel, msg, None);
        false
    } else {
        true
    }
}

fn get_seen_keys_and_categories(
    meta: &CommonMetricDataInternal,
    glean: &Glean,
) -> (HashSet<String>, HashSet<String>) {
    let base_identifier = &meta.base_identifier();
    let prefix = format!("{base_identifier}{RECORD_SEPARATOR}");
    let mut seen_keys: HashSet<String> = HashSet::new();
    let mut seen_categories: HashSet<String> = HashSet::new();
    let mut snapshotter = |metric_id: &[u8], _: &Metric| {
        let metric_id_str = String::from_utf8_lossy(metric_id);

        // Split full identifier on the ASCII Record Separator (\x1e)
        let parts: Vec<&str> = metric_id_str.split(RECORD_SEPARATOR).collect();

        if parts.len() == 2 {
            seen_keys.insert(parts[0].into());
            seen_categories.insert(parts[1].into());
        } else {
            record_error(
                glean,
                meta,
                ErrorType::InvalidLabel,
                "Dual Labeled Counter label doesn't contain exactly 2 parts".to_string(),
                None,
            );
        }
    };

    let lifetime = meta.inner.lifetime;
    for store in &meta.inner.send_in_pings {
        glean
            .storage()
            .iter_store_from(lifetime, store, Some(&prefix), &mut snapshotter);
    }

    (seen_keys, seen_categories)
}
