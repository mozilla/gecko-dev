/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::fmt::Display;

// Getting a metric instance and associated metadata, based on an ID, is
// non-trivial. This module provides a set of traits, functions, macros, and
// everything else that Rust provides, to implement this.
//
// In short, the implementation goes like this:

// Metric types implement a trait called `MetricMetadataGetterImpl`, via a
// macro, that describes how to look up a metric by its id from the various
// metric maps in `metrics.rs` and `factory.rs`. In some situations, such as
// labeled metrics which do not have metric ids but instead have submetric
// ids, looking up a metric isn't possible in this way: we need to have some
// way to get one metric, and then query it for another metric. This is where
// `BaseMetric` and `BaseMetricResult` come in. Types that implement
// `BaseMetric` provides a way to a) describe the type of the base metric
// (c.f. labeled submetrics), and b) look up a base metric instance, given a
// sub-metric instance. This can be necessary for (e.g.) child metrics that
// just store a label and metric ID.

// This all happens within the implementations in `get_base_metric_metadata_by_id` and
// `get_sub_metric_metadata_by_id`, defined within `define_metric_metadata_getter`. If this
// fails in some way, a `LookupError` will be returned.

// Once we have a metric instance, within this macro, we need to look up the
// associated metadata. This is done through the `MetricNamer` trait, that
// describes (for a given metric type), how to access the metadata. In
// parents, this is often through an `inner` glean instance, and in children,
// through a `meta` member. This is often pretty standard, so a macro
// `define_metric_namer` is provided to implement this for the majority of
// metrics. Metrics that don't split into just `Parent` and `Child` variants
// (like `BooleanMetric`) have to implement this themselves.

// `get_metadata` then returns a `MetricMetadata` struct instance,
// containing owned instances of the category/name/(optional) label. It would
// be nicer to do this with just referenced string types, but this becomes
// tricky to do while also satisfying the borrow checker. Label strings, for
// instance, can come from metrics, their metadata, or the labeled hashmap.
// Finding a way to keep all three sufficiently "in scope" is tricky, so we
// pay the cost of doing some more copies to keep the code a little simpler.

// Finally, the `MetricMetadataGetterImpl` definitions return this metadata,
// which can then be used to write the various category/name/label fields
// within a profiler marker.

// Note: This procedure does not work for `Object` and `Event` metric types.
// We rely on the fact that all other metric types have instances that are
// stored in `metrics.rs` (for static metrics) and `factory.rs` (for dynamic
// metrics), which we can use some form of metric ID to index when we stream
// a profiler marker. Object, and Event, metrics are not stored in maps in
// the same way, as they are indexed by a variety of types, and Rust does not
// support storing hetrogenous types in dynamic containers. Because of this,
// names and categories for `Object` and `Event` are extracted from the JS
// Metric lookup tables, using `lookup_canonical_metric_name`. These names
// are in the JS conjugation, as opposed to the YAML conjugation for other
// metric types.

/// Define a useful set of Error values that should make it eaiser to trace
/// lookup errors through the various bits of machinery.
#[derive(Debug)]
pub enum LookupError {
    FOGMetricMapWasUninit,
    FOGMetricMapLookupFailed,
    FOGSubmetricMapWasUninit,
    FOGSubmetricMapLockWasPoisoned,
    FOGSubmetricLookupFailed,
    JOGMetricMapWasUninit,
    JOGMetricMapLockWasPoisoned,
    JOGMetricMapLookupFailed,
    ReverseSubmetricLookupFailed,
    LookupUnlabledBySubId,
    SubmetricIdIsDynamic,
    LabeledBaseMetricIsNotDynamic,
    SubMetricLookupFailed,
    FOGMetricIdLookupFailed,
    NoBaseMetricForThisLabeledType,
}

impl Display for LookupError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            LookupError::FOGMetricMapWasUninit => write!(f, "FOGMetricMapWasUninit"),
            LookupError::FOGMetricMapLookupFailed => write!(f, "FOGMetricMapLookupFailed"),
            LookupError::FOGSubmetricMapWasUninit => write!(f, "FOGSubmetricMapWasUninit"),
            LookupError::FOGSubmetricMapLockWasPoisoned => {
                write!(f, "FOGSubmetricMapLockWasPoisoned")
            }
            LookupError::FOGSubmetricLookupFailed => write!(f, "FOGSubmetricLookupFailed"),
            LookupError::JOGMetricMapWasUninit => write!(f, "JOGMetricMapWasUninit"),
            LookupError::JOGMetricMapLockWasPoisoned => write!(f, "JOGMetricMapLockWasPoisoned"),
            LookupError::JOGMetricMapLookupFailed => write!(f, "JOGMetricMapLookupFailed"),
            LookupError::ReverseSubmetricLookupFailed => write!(f, "ReverseSubmetricLookupFailed"),
            LookupError::LookupUnlabledBySubId => write!(f, "LookupUnlabledBySubId"),
            LookupError::SubmetricIdIsDynamic => write!(f, "SubmetricIdIsDynamic"),
            LookupError::LabeledBaseMetricIsNotDynamic => {
                write!(f, "LabeledBaseMetricIsNotDynamic")
            }
            LookupError::SubMetricLookupFailed => write!(f, "SubMetricLookupFailed"),
            LookupError::FOGMetricIdLookupFailed => write!(f, "FOGMetricIdLookupFailed"),
            LookupError::NoBaseMetricForThisLabeledType => {
                write!(f, "NoBaseMetricForThisLabeledType")
            }
        }
    }
}

impl Error for LookupError {}

pub type LookupResult<T> = std::result::Result<T, LookupError>;

/// Define a structured way to refer to metric metadata, using owned types.
#[derive(Debug)]
pub struct MetricMetadata {
    pub category: String,
    pub name: String,
    pub label: Option<String>,
}

impl MetricMetadata {
    /// Construct a metric identifier from a triple of (category, name, option
    /// (label)) string references, and make copies of each.
    pub fn from_triple(t: (&str, &str, Option<&str>)) -> MetricMetadata {
        MetricMetadata {
            category: t.0.to_owned(),
            name: t.1.to_owned(),
            label: t.2.map(str::to_string),
        }
    }

    /// Construct a metric identifier that contains some error that surfaced
    /// while trying to retrieve the identifiers for this metric.
    pub fn from_error(error: LookupError) -> MetricMetadata {
        MetricMetadata {
            category: "".to_owned(),
            name: error.to_string(),
            label: None,
        }
    }

    /// Construct a metric identifier that contains some error string that
    /// surfaced while trying to retrieve the identifiers for this metric.
    pub fn from_error_str(error: &str) -> MetricMetadata {
        MetricMetadata {
            category: "".to_owned(),
            name: error.to_owned(),
            label: None,
        }
    }

    pub fn with_ref_label(self, other: Option<&str>) -> Self {
        MetricMetadata {
            category: self.category,
            name: self.name,
            label: self.label.or(other.map(str::to_string)),
        }
    }

    pub fn with_owned_label(self, other: Option<String>) -> Self {
        MetricMetadata {
            category: self.category,
            name: self.name,
            label: self.label.or(other),
        }
    }
}

/// Define, for a given metric, how we can extract the metadata from the
/// metric. This may be dependent on specific child metric implementations,
/// so each metric should implement this themselves. Note that the majority
/// of metrics can use the `define_metric_namer` macro given below to
/// implement this trait.
pub trait MetricNamer {
    fn get_metadata(&self) -> MetricMetadata;
}

/// Helper function for defining the metric namer for metric types, with two
/// distinct overrides for common patterns of metric types.
#[macro_export]
macro_rules! define_metric_namer {
    // Define a metric namer for metric types that have parent and child
    // metrics, where the child contains some metadata.
    ($metric_type:ident) => {
        impl crate::private::MetricNamer for $metric_type {
            fn get_metadata(&self) -> crate::private::MetricMetadata {
                use glean::MetricIdentifier;
                match self {
                    $metric_type::Parent { inner, .. } => {
                        crate::private::MetricMetadata::from_triple(inner.get_identifiers())
                    }
                    $metric_type::Child(meta) => {
                        crate::private::MetricMetadata::from_triple(meta.get_identifiers())
                    }
                }
            }
        }
    };

    // Define a metric namer for metric types that only have a parent metric,
    // with the child metric used only for IPC, and thus containing no
    // metadata.
    ($metric_type:ident, PARENT_ONLY) => {
        impl crate::private::MetricNamer for $metric_type {
            fn get_metadata(&self) -> crate::private::MetricMetadata {
                use glean::MetricIdentifier;
                match self {
                    $metric_type::Parent { inner, .. } => {
                        crate::private::MetricMetadata::from_triple(inner.get_identifiers())
                    }
                    _ => crate::private::MetricMetadata::from_error_str(concat!(
                        "Cannot get child identifiers for parent only type ",
                        stringify!($metric_type)
                    )),
                }
            }
        }
    };

    // Define a metric namer for labeled metric types. Defer to the underlying
    // base metric to get the actual identifiers/information.
    ($metric_type:ident, LABELED) => {
        impl crate::private::MetricNamer for $metric_type {
            fn get_metadata(&self) -> crate::private::MetricMetadata {
                match self.get_base_metric() {
                    BaseMetricResult::BaseMetric(metric) => metric.get_metadata(),
                    BaseMetricResult::BaseMetricWithLabel(metric, inner_label) => {
                        metric.get_metadata().with_ref_label(Some(inner_label))
                    }
                    BaseMetricResult::IndexLabelPair(id, child_label) => {
                        match <$metric_type as BaseMetric>::BaseMetricT::get_base_metric_metadata_by_id(id) {
                            Ok((metadata, inner_label)) => metadata
                                .with_owned_label(inner_label)
                                .with_ref_label(Some(child_label)),
                            Err(e) => crate::private::MetricMetadata::from_error(e),
                        }
                    }
                    BaseMetricResult::None => crate::private::MetricMetadata::from_error(
                        crate::private::metric_getter::LookupError::NoBaseMetricForThisLabeledType,
                    ),
                }
            }
        }
    };
}

/// Given a metric type and id, try to get the metadata associated with the
/// metric that this refers to, along with a label, if the metric and label
/// live separately. This is non-trivial to implement, so a macro is given
/// for implementing the `MetricMetadataGetterImpl` trait, which will
/// generate implementation for a given metric (and/or submetric) and the
/// right set of metric maps.
pub trait MetricMetadataGetter {
    fn get_metric_metadata_by_id<T: Into<MetricId>>(
        id: T,
    ) -> LookupResult<(MetricMetadata, Option<String>)>
    where
        Self: MetricNamer;
}

// Provide a blanket implementation for MetricMetadataGetter over types that
// implement MetricMetadataGetterImpl. This way, users won't be tempted to
// try and implement it themselves!
impl<MetricT> MetricMetadataGetter for MetricT
where
    MetricT: MetricMetadataGetterImpl + MetricNamer,
{
    fn get_metric_metadata_by_id<T: Into<MetricId>>(
        id: T,
    ) -> LookupResult<(MetricMetadata, Option<String>)> {
        match id.into() {
            MetricId::Id(baseid) => Self::get_base_metric_metadata_by_id(baseid),
            MetricId::SubId(subid) => Self::get_sub_metric_metadata_by_id(subid),
        }
    }
}

/// Implementation trait for MetricMetadataGetter. Metrics should define this.
/// Note, the macro `define_metric_metadata_getter` should be used to define the
/// implementations for this trait. It is non-trivial to implement.
pub trait MetricMetadataGetterImpl {
    fn get_base_metric_metadata_by_id(
        id: BaseMetricId,
    ) -> LookupResult<(MetricMetadata, Option<String>)>
    where
        Self: MetricNamer;
    fn get_sub_metric_metadata_by_id(
        id: SubMetricId,
    ) -> LookupResult<(MetricMetadata, Option<String>)>
    where
        Self: MetricNamer;
}

pub enum BaseMetricResult<'a, BaseMetricT> {
    BaseMetric(&'a BaseMetricT),
    BaseMetricWithLabel(&'a BaseMetricT, &'a str),
    IndexLabelPair(BaseMetricId, &'a str),
    None,
}

/// The BaseMetric trait gives us an interface for interacting with labeled
/// metrics, where we may have an instance of the underlying metric
/// (which knows its own label), or an id and label for a child metric.
pub trait BaseMetric {
    type BaseMetricT;
    fn get_base_metric<'a>(&'a self) -> BaseMetricResult<'a, Self::BaseMetricT>;
}

/// Given the type of a metric, the name of a FOG metric map for said metric,
/// an id, look up the metric from the map by id and return the metadata
/// (if found)
#[macro_export]
macro_rules! metadata_from_static_map {
    ($metric_type:ident, $metric_map:ident, $metric_id:ident) => {{
        let static_map =
            once_cell::sync::Lazy::get(&crate::metrics::__glean_metric_maps::$metric_map)
                .ok_or(crate::private::LookupError::FOGMetricMapWasUninit)?;
        let metric: &$metric_type = static_map
            .get(&$metric_id)
            .and_then(|thunk: &&once_cell::sync::Lazy<$metric_type>| {
                once_cell::sync::Lazy::get(*thunk)
            })
            .ok_or(crate::private::LookupError::FOGMetricMapLookupFailed)?;
        Ok((metric.get_metadata(), None))
    }};
}

/// Given the name of a JOG metric map, and an id, look up the metric metadata
/// from the map by id, and return it (if found)
#[macro_export]
macro_rules! metadata_from_dynamic_map {
    ($metric_map:ident, $metric_id:ident) => {{
        // Find the dynamic map (given as part of the macro), and try to read
        // from it. We don't need to force it, as if it hasn't been
        // initialized, we won't have a metric in there to read anyway!
        let dynamic_map =
            once_cell::sync::Lazy::get(&crate::factory::__jog_metric_maps::$metric_map)
                .ok_or(crate::private::LookupError::JOGMetricMapWasUninit)?
                .read()
                .or(Err(
                    crate::private::LookupError::JOGMetricMapLockWasPoisoned,
                ))?;
        let metric: &Self = dynamic_map
            .get(&$metric_id)
            .ok_or(crate::private::LookupError::JOGMetricMapLookupFailed)?;
        Ok((metric.get_metadata(), None))
    }};
}

/// Define how to look up the metadata for a given metric, given a metric id.
/// There are subtle differences for /where/ this should be called, depending
/// on the metric (and submetric) which we want to look up:
/// - Given a metric type `SomethingMetric`, which cannot be labeled, this
///   macro should be called in `something.rs`, with an invocation that looks
///   like `define_metric_metadata_getter!(SomethingMetric, SOMETHING_MAP)`
/// - Given a metric type `SomeotherMetric`, which *can* be labeled, but which
///   uses the same type for labeled instances (see `String` for an example),
///   the macro should be callled in `someother.rs`, with an invocation that
///   looks like `define_metric_metadata_getter!
///   (SomeotherMetric,SOMEOTHER_MAP,LABELED_SOMEOTHER_MAP)`
/// - Finally, given a metric type `AnotherMetric`, which can be labeled, and
///   has a different type (`LabeledAnotherMetric`) when labeled (see, for
///   example `MemoryDistribution` and `LabeledMemoryDistribution`), this
///   macro should be called in `labeled_another_metric.rs`, as this is the
///   only place where *both* metric types are in scope. The invocation
///   should look like `define_metric_metadata_getter!
///   (AnotherMetric, LabeledAnotherMetric, ANOTHER_MAP,
///   LABELED_ANOTHER_MAP)
#[macro_export]
macro_rules! define_metric_metadata_getter {
    // Metric getter for metrics that cannot be labeled
    ($metric_type:ident, $metric_map:ident) => {
        impl crate::private::MetricMetadataGetterImpl for $metric_type {
            fn get_base_metric_metadata_by_id(
                id: crate::private::BaseMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;
                if id.is_dynamic() {
                    crate::metadata_from_dynamic_map!($metric_map, id)
                } else {
                    crate::metadata_from_static_map!($metric_type, $metric_map, id)
                }
            }

            fn get_sub_metric_metadata_by_id(
                _id: crate::private::SubMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                // Unlabeled metrics cannot look up submetrics
                Err(crate::private::LookupError::LookupUnlabledBySubId)
            }
        }
    };

    // Metric getter for metrics that can be labeled, but appear as the same
    // type (e.g. "StringMetric")
    ($metric_type:ident, $metric_map:ident, $labeled_map:ident) => {
        impl crate::private::MetricMetadataGetterImpl for $metric_type {
            fn get_base_metric_metadata_by_id(
                id: crate::private::BaseMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;
                if id.is_dynamic() {
                    crate::metadata_from_dynamic_map!($metric_map, id)
                } else {
                    crate::metadata_from_static_map!($metric_type, $metric_map, id)
                }
            }

            fn get_sub_metric_metadata_by_id(
                id: crate::private::SubMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;
                // We should have a non-dynamic ID here, as dynamic
                // metrics always have base metric IDs, but we should
                // check anyway
                if id.is_dynamic() {
                    return Err(crate::private::LookupError::SubmetricIdIsDynamic);
                }
                // Re-use the $metric_map name to find the sub-metric map
                let submetric_map = once_cell::sync::Lazy::get(
                    &crate::metrics::__glean_metric_maps::submetric_maps::$metric_map,
                )
                .ok_or(crate::private::LookupError::FOGSubmetricMapWasUninit)?
                .read()
                .or(Err(
                    crate::private::LookupError::FOGSubmetricMapLockWasPoisoned,
                ))?;

                let metric = submetric_map
                    .get(&id)
                    .map(|arcm: &std::sync::Arc<$metric_type>| arcm.as_ref())
                    .ok_or(crate::private::LookupError::FOGSubmetricLookupFailed)?;
                Ok((metric.get_metadata(), None))
            }
        }
    };

    // Metric getter for metrics that can be labeled, but appear as a
    // DIFFERENT type when they are labeled
    ($metric_type:ident, $submetric_type: ident, $metric_map:ident, $labeled_map:ident) => {
        // Define `MetricMetadataGetter` for the base type, with awareness of the
        // other type (i.e. Counter is aware of LabeledCounter).
        impl crate::private::MetricMetadataGetterImpl for $metric_type {
            fn get_base_metric_metadata_by_id(
                id: crate::private::BaseMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;
                if id.is_dynamic() {
                    crate::metadata_from_dynamic_map!($metric_map, id)
                } else {
                    crate::metadata_from_static_map!($metric_type, $metric_map, id)
                }
            }

            fn get_sub_metric_metadata_by_id(
                id: crate::private::SubMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;
                // We should have a non-dynamic ID here, as dynamic
                // metrics always have base metric IDs, but we should
                // check anyway
                if id.is_dynamic() {
                    return Err(crate::private::LookupError::SubmetricIdIsDynamic);
                }
                // Re-use the $metric_map name to find the sub-metric map
                let submetric_map = once_cell::sync::Lazy::get(
                    &crate::metrics::__glean_metric_maps::submetric_maps::$metric_map,
                )
                .ok_or(crate::private::LookupError::FOGSubmetricMapWasUninit)?
                .read()
                .or(Err(
                    crate::private::LookupError::FOGSubmetricMapLockWasPoisoned,
                ))?;

                let submetric: &$submetric_type = submetric_map
                    .get(&id)
                    .map(|arcm: &std::sync::Arc<$submetric_type>| arcm.as_ref())
                    .ok_or(crate::private::LookupError::FOGSubmetricLookupFailed)?;

                match submetric.get_base_metric() {
                    BaseMetricResult::BaseMetric(metric) => Ok((metric.get_metadata(), None)),
                    BaseMetricResult::BaseMetricWithLabel(metric, label) => {
                        Ok((metric.get_metadata(), Some(label.to_string())))
                    }
                    BaseMetricResult::IndexLabelPair(id, label) => {
                        match $metric_type::get_base_metric_metadata_by_id(id) {
                            Ok((metadata, _)) => Ok((metadata, Some(label.to_string()))),
                            e => e,
                        }
                    }
                    BaseMetricResult::None => {
                        Err(crate::private::LookupError::NoBaseMetricForThisLabeledType)
                    }
                }
            }
        }

        impl crate::private::MetricMetadataGetterImpl for $submetric_type {
            fn get_base_metric_metadata_by_id(
                id: crate::private::BaseMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;

                // A base metric id means that we have a labeled JOG metric,
                // so it must be dynamic. Report an error if condition is not
                // met.
                if !id.is_dynamic() {
                    return Err(crate::private::LookupError::LabeledBaseMetricIsNotDynamic);
                }

                // A dynamic ID means that we have a JOG metric
                // Look up the wrapped labeled metric from the dynamic maps
                let dynamic_map =
                    once_cell::sync::Lazy::get(&crate::factory::__jog_metric_maps::$labeled_map)
                        .ok_or(crate::private::LookupError::JOGMetricMapWasUninit)?
                        .read()
                        .or(Err(
                            crate::private::LookupError::JOGMetricMapLockWasPoisoned,
                        ))?;
                let labeled = dynamic_map
                    .get(&id)
                    .ok_or(crate::private::LookupError::JOGMetricMapLookupFailed)?;

                // We can't directly use labeled to get a metric instance, as
                // we don't know the label. Instead, we use the base metric
                // id to find the label using the static submetric map.
                let map =
                    crate::metrics::__glean_metric_maps::submetric_maps::LABELED_METRICS_TO_IDS
                        .read()
                        .or(Err(
                            crate::private::LookupError::FOGSubmetricMapLockWasPoisoned,
                        ))?;

                // Iterate over the hash table to find the ID, and extract the
                // corresponding label.
                let label = map
                    .iter()
                    .find(|((id, _), _)| id == id)
                    .map(|((_, label), _)| label.clone())
                    .ok_or(crate::private::LookupError::ReverseSubmetricLookupFailed)?;

                let metric = labeled.get(label.as_ref());

                Ok((metric.as_ref().get_metadata(), Some(label.clone())))
            }

            fn get_sub_metric_metadata_by_id(
                id: crate::private::SubMetricId,
            ) -> crate::private::LookupResult<(crate::private::MetricMetadata, Option<String>)>
            {
                use crate::private::metric_getter::MetricNamer;
                // We should have a non-dynamic ID here, as dynamic
                // metrics always have base metric IDs, but we should
                // check anyway
                if id.is_dynamic() {
                    return Err(crate::private::LookupError::SubmetricIdIsDynamic);
                }

                // Re-use the $metric_map name to find the sub-metric map
                let submetric_map = once_cell::sync::Lazy::get(
                    &crate::metrics::__glean_metric_maps::submetric_maps::$metric_map,
                )
                .ok_or(crate::private::LookupError::FOGSubmetricMapWasUninit)?
                .read()
                .or(Err(
                    crate::private::LookupError::FOGSubmetricMapLockWasPoisoned,
                ))?;

                let submetric = submetric_map
                    .get(&id)
                    .map(|arcm: &std::sync::Arc<$submetric_type>| arcm.as_ref())
                    .ok_or(crate::private::LookupError::FOGSubmetricLookupFailed)?;

                Ok((submetric.get_metadata(), None))
            }
        }
    };
}

/// Uniquely identify a metric so that we can look up names, labels (etc) and
/// perform IPC
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
pub enum MetricId {
    Id(BaseMetricId),
    SubId(SubMetricId),
}

impl MetricId {
    /// Extract the underlying metric_id, if there is one.
    pub fn base_metric_id(self) -> Option<BaseMetricId> {
        match self {
            MetricId::Id(base_metric_id) => Some(base_metric_id),
            _ => None,
        }
    }

    pub fn is_base_metric_id(&self) -> bool {
        matches!(self, MetricId::Id(_))
    }

    pub fn is_dynamic(self) -> bool {
        *self & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
    }
}

#[cfg(feature = "with_gecko")]
impl MetricId {
    /// Given a metric getter, retrieve the name and (optionally) label of the
    /// underlying metric. Note, this currently returns the name of the
    /// metric in the so-called "JavaScript conjugation", while labels are
    /// returned in the so-called "YAML conjugation". Bug 1938145 captures
    /// the work for looking up the actual metric, after which Bug 1934880
    /// will allow us to get both in the yaml conjugation.
    pub fn get_identifiers(&self) -> (String, Option<String>) {
        match self {
            MetricId::Id(id) => (id.get_name(), None),
            MetricId::SubId(sid) => match sid.lookup_metric_id_and_label() {
                Some((id, label)) => (id.get_name(), Some(label)),
                None => (String::from("Could not find submetric in maps"), None),
            },
        }
    }
}

impl From<&MetricId> for MetricId {
    fn from(base_metric_id: &MetricId) -> MetricId {
        *base_metric_id
    }
}

impl From<BaseMetricId> for MetricId {
    fn from(base_metric_id: BaseMetricId) -> MetricId {
        MetricId::Id(base_metric_id)
    }
}

impl From<SubMetricId> for MetricId {
    fn from(submetric_id: SubMetricId) -> MetricId {
        MetricId::SubId(submetric_id)
    }
}

impl std::ops::Deref for MetricId {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        match self {
            MetricId::Id(BaseMetricId(m)) => m,
            MetricId::SubId(SubMetricId(m)) => m,
        }
    }
}

/// Uniquely identifies a single metric across all metric types.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct BaseMetricId(pub(crate) u32);

impl BaseMetricId {
    pub fn new(id: u32) -> Self {
        Self(id)
    }

    pub fn is_dynamic(self) -> bool {
        self.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
    }
}

#[cfg(feature = "with_gecko")]
impl BaseMetricId {
    // Wraps the result of `lookup_canonical_metric_name` so that it's
    // slightly easier for consumers to use. Also provides a slightly more
    // abstracted interface, so that in future we can use other ways to get
    // the name of a metric.
    pub(crate) fn get_name(&self) -> String {
        String::from(
            self.lookup_canonical_metric_name()
                .unwrap_or("id not found"),
        )
    }
    pub(crate) fn lookup_canonical_metric_name(&self) -> Option<&'static str> {
        #[allow(unused)]
        use std::ffi::{c_char, CStr};
        extern "C" {
            fn FOG_GetMetricIdentifier(id: u32) -> *const c_char;
        }
        // SAFETY: We check to make sure that the returned pointer is not null
        // before trying to construct a string from it. As the string array
        // that `FOG_GetMetricIdentifier` references is statically defined
        // and allocated, we know that any strings will be guaranteed to have
        // a null terminator, and will have the same lifetime as the program,
        // meaning we're safe to return a static lifetime, knowing that they
        // won't be changed "underneath" us. Additionally, we surface any
        // errors from parsing the string as utf8.
        unsafe {
            let raw_name_ptr = FOG_GetMetricIdentifier(**self);
            if raw_name_ptr.is_null() {
                None
            } else {
                match CStr::from_ptr(raw_name_ptr).to_str() {
                    Ok(s) => Some(s),
                    // This is a UTF8 parse error, that we can't handle.
                    Err(_) => None,
                }
            }
        }
    }
}

impl From<u32> for BaseMetricId {
    fn from(id: u32) -> Self {
        Self(id)
    }
}

impl std::ops::Deref for BaseMetricId {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

/// Uniquely identifies a sub-metric across all metric types.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct SubMetricId(pub(crate) u32);

impl SubMetricId {
    pub fn new(id: u32) -> Self {
        Self(id)
    }

    pub fn is_dynamic(self) -> bool {
        self.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
    }
}

#[cfg(feature = "with_gecko")]
impl SubMetricId {
    /// Given a submetric id, use the glean submetric maps to look up the
    /// underlying metric id, and label. Note that this essentially performs
    /// the reverse of `private::submetric_id_for`.
    pub(crate) fn lookup_metric_id_and_label(&self) -> Option<(BaseMetricId, String)> {
        let map = crate::metrics::__glean_metric_maps::submetric_maps::LABELED_METRICS_TO_IDS
            .read()
            .expect("read lock of submetric ids was poisoned");
        map.iter()
            .find(|(_, &value)| value == *self)
            .map(|(key, _)| key.clone())
    }
}

impl From<u32> for SubMetricId {
    fn from(id: u32) -> Self {
        Self(id)
    }
}

impl std::ops::Deref for SubMetricId {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
