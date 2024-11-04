/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[uniffi::export(callback_interface)]
pub trait Logger: Send + Sync {
    fn log(&self, message: String);
    // Log a message N times each prefixed with the current index, except when that index is one of
    // the items in the exclude vec. The point here is to test sending a bunch of arguments at once
    fn log_repeat(&self, message: String, count: u32, exclude: Vec<u32>);
    fn finished(&self);
}

#[uniffi::export]
fn log_even_numbers(logger: Box<dyn Logger>, items: Vec<i32>) {
    for i in items {
        if i % 2 == 0 {
            logger.log(format!("Saw even number: {i}"))
        }
    }
    logger.finished();
}

#[uniffi::export]
fn log_even_numbers_main_thread(logger: Box<dyn Logger>, items: Vec<i32>) {
    log_even_numbers(logger, items)
}

#[uniffi::export]
fn call_log_repeat(logger: Box<dyn Logger>, message: String, count: u32, exclude: Vec<u32>) {
    logger.log_repeat(message, count, exclude);
    logger.finished();
}

uniffi::setup_scaffolding!("fixture_callbacks");
