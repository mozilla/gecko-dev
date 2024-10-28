/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Extension traits that are shared across multiple render targets
use crate::Config;
use extend::ext;
use uniffi_bindgen::interface::{Function, Method, Object};

/// Check if a JS function should be async.
///
/// `uniffi-bindgen-gecko-js` has special async handling.  Many non-async Rust functions end up
/// being async in js
fn use_async_wrapper(config: &Config, spec: &str) -> bool {
    config.async_wrappers.enable && !config.async_wrappers.main_thread.contains(spec)
}

#[ext]
pub impl Function {
    fn use_async_wrapper(&self, config: &Config) -> bool {
        use_async_wrapper(config, self.name())
    }
}

#[ext]
pub impl Object {
    fn use_async_wrapper_for_constructor(&self, config: &Config) -> bool {
        use_async_wrapper(config, self.name())
    }

    fn use_async_wrapper_for_method(&self, method: &Method, config: &Config) -> bool {
        use_async_wrapper(config, &format!("{}.{}", self.name(), method.name()))
    }
}
