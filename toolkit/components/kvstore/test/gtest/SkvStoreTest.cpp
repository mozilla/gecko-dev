/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "gtest/gtest.h"

extern "C" {

void Rust_SkvStoreTestUnderMaintenance();
void Rust_SkvStoreTestClosingDuringMaintenance();
void Rust_SkvStoreTestMaintenanceSucceeds();
void Rust_SkvStoreTestMaintenanceFails();
void Rust_SkvStoreTestRenamesCorruptDatabaseFile();

}  // extern "C"

TEST(SkvStoreTest, UnderMaintenance)
{ Rust_SkvStoreTestUnderMaintenance(); }

TEST(SkvStoreTest, ClosingDuringMaintenance)
{ Rust_SkvStoreTestClosingDuringMaintenance(); }

TEST(SkvStoreTest, MaintenanceSucceeds)
{ Rust_SkvStoreTestMaintenanceSucceeds(); }

TEST(SkvStoreTest, MaintenanceFails)
{ Rust_SkvStoreTestMaintenanceFails(); }

TEST(SkvStoreTest, RenamesCorruptDatabaseFile)
{ Rust_SkvStoreTestRenamesCorruptDatabaseFile(); }
