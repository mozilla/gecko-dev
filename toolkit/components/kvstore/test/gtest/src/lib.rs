/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

mod latch;
mod store;

#[no_mangle]
pub extern "C" fn Rust_SkvStoreTestUnderMaintenance() {
    store::under_maintenance();
}

#[no_mangle]
pub extern "C" fn Rust_SkvStoreTestClosingDuringMaintenance() {
    store::close_during_maintenance();
}

#[no_mangle]
pub extern "C" fn Rust_SkvStoreTestMaintenanceSucceeds() {
    store::maintenance_succeeds();
}

#[no_mangle]
pub extern "C" fn Rust_SkvStoreTestMaintenanceFails() {
    store::maintenance_fails();
}

#[no_mangle]
pub extern "C" fn Rust_SkvStoreTestRenamesCorruptDatabaseFile() {
    store::renames_corrupt_database_file();
}
