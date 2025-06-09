/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Trait interface test.  This is the like the `callback_interfaces` module, except it uses a
//! trait that has `with_foreign` enabled (AKA a trait interface).  This means UniFFI supports both
//! Rust and foreign language implementations of the trait.

use std::sync::{
    atomic::{AtomicU32, Ordering},
    Arc,
};

use crate::callback_interfaces::CallbackInterfaceNumbers;
use crate::errors::TestError;

#[uniffi::export(with_foreign)]
pub trait TestTraitInterface: Send + Sync {
    /// No-op function, this tests if that we can make calls at all
    fn noop(&self);
    /// Get the internal value
    fn get_value(&self) -> u32;
    /// Set the internal value
    fn set_value(&self, value: u32);
    /// Method aimed at maximizing the complexity
    ///
    /// This should return an error if `numbers.a == numbers.b` otherwise it should return numbers back
    /// unchanged.
    fn throw_if_equal(
        &self,
        numbers: CallbackInterfaceNumbers,
    ) -> Result<CallbackInterfaceNumbers, TestError>;
}

#[uniffi::export]
fn invoke_test_trait_interface_noop(int: Arc<dyn TestTraitInterface>) {
    int.noop()
}

#[uniffi::export]
fn invoke_test_trait_interface_get_value(int: Arc<dyn TestTraitInterface>) -> u32 {
    int.get_value()
}

#[uniffi::export]
fn invoke_test_trait_interface_set_value(int: Arc<dyn TestTraitInterface>, value: u32) {
    int.set_value(value)
}

#[uniffi::export]
fn invoke_test_trait_interface_throw_if_equal(
    int: Arc<dyn TestTraitInterface>,
    numbers: CallbackInterfaceNumbers,
) -> Result<CallbackInterfaceNumbers, TestError> {
    int.throw_if_equal(numbers)
}

/// Create an implementation of the interface in Rust
#[uniffi::export]
fn create_test_trait_interface(value: u32) -> Arc<dyn TestTraitInterface> {
    Arc::new(TestTraitInterfaceImpl {
        value: AtomicU32::new(value),
    })
}

struct TestTraitInterfaceImpl {
    value: AtomicU32,
}

impl TestTraitInterface for TestTraitInterfaceImpl {
    fn noop(&self) {}

    fn get_value(&self) -> u32 {
        self.value.load(Ordering::Relaxed)
    }

    fn set_value(&self, value: u32) {
        self.value.store(value, Ordering::Relaxed);
    }

    fn throw_if_equal(
        &self,
        numbers: CallbackInterfaceNumbers,
    ) -> Result<CallbackInterfaceNumbers, TestError> {
        if numbers.a == numbers.b {
            Err(TestError::Failure1)
        } else {
            Ok(numbers)
        }
    }
}

/// Async version of `TestTraitInterface`
#[uniffi::export(with_foreign)]
#[async_trait::async_trait]
pub trait AsyncTestTraitInterface: Send + Sync {
    /// No-op function, this tests if that we can make calls at all
    async fn noop(&self);
    /// Get the internal value
    async fn get_value(&self) -> u32;
    /// Set the internal value
    async fn set_value(&self, value: u32);
    /// Method aimed at maximizing the complexity
    ///
    /// This should return an error if `numbers.a == numbers.b` otherwise it should return numbers back
    /// unchanged.
    async fn throw_if_equal(
        &self,
        numbers: CallbackInterfaceNumbers,
    ) -> Result<CallbackInterfaceNumbers, TestError>;
}

#[uniffi::export]
fn create_async_test_trait_interface(value: u32) -> Arc<dyn AsyncTestTraitInterface> {
    Arc::new(TestTraitInterfaceImpl {
        value: AtomicU32::new(value),
    })
}

#[uniffi::export]
async fn invoke_async_test_trait_interface_noop(int: Arc<dyn AsyncTestTraitInterface>) {
    int.noop().await
}

#[uniffi::export]
async fn invoke_async_test_trait_interface_get_value(int: Arc<dyn AsyncTestTraitInterface>) -> u32 {
    int.get_value().await
}

#[uniffi::export]
async fn invoke_async_test_trait_interface_set_value(
    int: Arc<dyn AsyncTestTraitInterface>,
    value: u32,
) {
    println!("**** invoke_async_test_trait_interface_set_value {value}");
    int.set_value(value).await
}

#[uniffi::export]
async fn invoke_async_test_trait_interface_throw_if_equal(
    int: Arc<dyn AsyncTestTraitInterface>,
    numbers: CallbackInterfaceNumbers,
) -> Result<CallbackInterfaceNumbers, TestError> {
    int.throw_if_equal(numbers).await
}

#[async_trait::async_trait]
impl AsyncTestTraitInterface for TestTraitInterfaceImpl {
    async fn noop(&self) {}

    async fn get_value(&self) -> u32 {
        self.value.load(Ordering::Relaxed)
    }

    async fn set_value(&self, value: u32) {
        self.value.store(value, Ordering::Relaxed);
    }

    async fn throw_if_equal(
        &self,
        numbers: CallbackInterfaceNumbers,
    ) -> Result<CallbackInterfaceNumbers, TestError> {
        if numbers.a == numbers.b {
            Err(TestError::Failure1)
        } else {
            Ok(numbers)
        }
    }
}
