// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::time::Duration;

#[cfg(target_arch = "wasm32")]
use wasm_bindgen::prelude::*;

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct MlsTime {
    seconds: u64,
}

impl MlsTime {
    /// Create a timestamp from a duration since unix epoch.
    pub fn from_duration_since_epoch(duration: Duration) -> MlsTime {
        Self {
            seconds: duration.as_secs(),
        }
    }

    /// Number of seconds since the unix epoch.
    pub fn seconds_since_epoch(&self) -> u64 {
        self.seconds
    }
}

#[cfg(all(not(target_arch = "wasm32"), feature = "std"))]
impl MlsTime {
    /// Current system time.
    pub fn now() -> Self {
        Self {
            seconds: std::time::SystemTime::now()
                .duration_since(std::time::SystemTime::UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
        }
    }
}

impl From<u64> for MlsTime {
    fn from(value: u64) -> Self {
        Self { seconds: value }
    }
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen(inline_js = r#"
export function date_now() {
  return Date.now();
}"#)]
extern "C" {
    fn date_now() -> f64;
}

#[cfg(target_arch = "wasm32")]
impl MlsTime {
    pub fn now() -> Self {
        Self {
            seconds: (date_now() / 1000.0) as u64,
        }
    }
}
