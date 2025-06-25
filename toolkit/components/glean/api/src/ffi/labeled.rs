// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use crate::metrics::__glean_metric_maps as metric_maps;
use nsstring::nsACString;

fn is_jog_id(id: u32) -> bool {
    id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
}

#[no_mangle]
pub extern "C" fn fog_labeled_enum_to_str(id: u32, label: u16, value: &mut nsACString) {
    let val = metric_maps::labeled_enum_to_str(id, label);
    value.assign(&val);
}

#[no_mangle]
pub extern "C" fn fog_labeled_boolean_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_BOOLEAN_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_boolean_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}

#[no_mangle]
pub extern "C" fn fog_labeled_counter_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_COUNTER_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_counter_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}

#[no_mangle]
pub extern "C" fn fog_labeled_custom_distribution_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_CUSTOM_DISTRIBUTION_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_custom_distribution_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}

#[no_mangle]
pub extern "C" fn fog_labeled_memory_distribution_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_MEMORY_DISTRIBUTION_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_memory_distribution_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}

#[no_mangle]
pub extern "C" fn fog_labeled_string_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_STRING_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_string_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}

#[no_mangle]
pub extern "C" fn fog_labeled_timing_distribution_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_TIMING_DISTRIBUTION_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_timing_distribution_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}

#[no_mangle]
pub extern "C" fn fog_labeled_quantity_get(id: u32, label: &nsACString) -> u32 {
    let label = &label.to_utf8();
    if is_jog_id(id) {
        just_with_jog_metric!(
            LABELED_QUANTITY_MAP,
            id,
            metric,
            metric.get_submetric_id(label)
        )
    } else {
        metric_maps::labeled_submetric_id_get(id, label)
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_quantity_enum_get(id: u32, label: u16) -> u32 {
    assert!(!is_jog_id(id), "No enum_get support for JOG");
    metric_maps::labeled_submetric_id_get(id, metric_maps::labeled_enum_to_str(id, label))
}
