// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

use crate::factory;
use crate::metrics::__glean_metric_maps as metric_maps;

#[no_mangle]
pub extern "C" fn fog_object_set_string(id: u32, value: &nsACString) {
    let value = value.to_utf8().to_string();

    if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        let map = factory::__jog_metric_maps::OBJECT_MAP
            .read()
            .expect("Read lock for dynamic metric map was poisoned");
        match map.get(&id.into()) {
            Some(metric) => metric.set_string(value),
            None => panic!("No (dynamic) metric for id {}", id),
        }

        return;
    }

    if metric_maps::set_object_by_id(id, value).is_err() {
        panic!("No object for id {}", id);
    }
}

#[no_mangle]
pub unsafe extern "C" fn fog_object_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };
    if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        let map = factory::__jog_metric_maps::OBJECT_MAP
            .read()
            .expect("Read lock for dynamic metric map was poisoned");
        match map.get(&id.into()) {
            Some(metric) => metric.test_get_value(storage.as_deref()).is_some(),
            None => panic!("No (dynamic) metric for id {}", id),
        }
    } else {
        metric_maps::object_test_get_value(id, storage).is_some()
    }
}

#[no_mangle]
pub extern "C" fn fog_object_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut nsACString,
) {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };

    if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        let map = factory::__jog_metric_maps::OBJECT_MAP
            .read()
            .expect("Read lock for dynamic metric map was poisoned");
        match map.get(&id.into()) {
            Some(metric) => match metric.test_get_value_as_str(storage.as_deref()) {
                Some(object) => value.assign(&object),
                None => return,
            },
            None => panic!("No (dynamic) metric for id {}", id),
        }
    } else {
        let object = match metric_maps::object_test_get_value(id, storage) {
            Some(object) => object,
            None => return,
        };
        value.assign(&object);
    }
}

#[no_mangle]
pub extern "C" fn fog_object_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        let map = factory::__jog_metric_maps::OBJECT_MAP
            .read()
            .expect("Read lock for dynamic metric map was poisoned");
        match map.get(&id.into()) {
            Some(metric) => test_get_errors!(metric),
            None => panic!("No (dynamic) metric for id {}", id),
        }
    } else {
        metric_maps::object_test_get_error(id)
    };
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
