/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Extension traits that are shared across multiple render targets
use crate::Config;
use extend::ext;
use uniffi_bindgen::interface::{Callable, Constructor, Function, Method, Object};

/// How should we call a Rust function from JS?
pub enum CallStyle {
    /// Sync Rust function
    Sync,
    /// Async Rust function
    Async,
    /// Sync Rust function, wrapped to be async
    AsyncWrapper,
}

impl CallStyle {
    /// Is the JS version of this function async?
    pub fn is_js_async(&self) -> bool {
        matches!(self, Self::Async | Self::AsyncWrapper)
    }
}

fn call_style(callable: impl Callable, config: &Config, spec: &str) -> CallStyle {
    if callable.is_async() {
        CallStyle::Async
    } else if config.async_wrappers.enable && !config.async_wrappers.main_thread.contains(spec) {
        CallStyle::AsyncWrapper
    } else {
        CallStyle::Sync
    }
}

/// Map Rust crate names to UniFFI namespaces.
pub fn crate_name_to_namespace(crate_name: &str) -> &str {
    // TODO: remove this hack, we should be able to calculate this by walking the CI data.
    match crate_name {
        "uniffi_geometry" => "geometry",
        "uniffi_sprites" => "sprites",
        s => s,
    }
}

#[ext]
pub impl Function {
    fn call_style(&self, config: &Config) -> CallStyle {
        call_style(self, config, self.name())
    }
}

#[ext]
pub impl Object {
    fn call_style_for_constructor(&self, cons: &Constructor, config: &Config) -> CallStyle {
        call_style(cons, config, &format!("{}.{}", self.name(), cons.name()))
    }

    fn call_style_for_method(&self, method: &Method, config: &Config) -> CallStyle {
        call_style(
            method,
            config,
            &format!("{}.{}", self.name(), method.name()),
        )
    }
}
