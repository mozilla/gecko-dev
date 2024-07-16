/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub mod http;
mod libcurl;
pub mod ping;
pub mod report;

#[cfg(test)]
pub fn can_load_libcurl() -> bool {
    libcurl::load().is_ok()
}
