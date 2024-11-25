/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod future_tester;
mod oneshot;
mod roundtrip;
mod wrapped_sync_call;

pub use future_tester::*;
pub use wrapped_sync_call::*;
pub use roundtrip::*;

uniffi::setup_scaffolding!("futures");
