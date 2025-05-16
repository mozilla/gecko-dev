/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[uniffi::export(callback_interface)]
pub trait ContextIdCallback: Sync + Send {
    fn persist(&self, context_id: String, creation_date: i64);
    fn rotated(&self, old_context_id: String);
}

pub struct DefaultContextIdCallback;
impl ContextIdCallback for DefaultContextIdCallback {
    fn persist(&self, _context_id: String, _creation_date: i64) {}
    fn rotated(&self, _old_context_id: String) {}
}
