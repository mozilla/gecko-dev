/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCKCATEGORY_H_
#define DOM_QUOTA_DIRECTORYLOCKCATEGORY_H_

#include "mozilla/EnumSet.h"

namespace mozilla::dom::quota {

enum class DirectoryLockCategory : uint8_t {
  None = 0,
  // Used by operations which uninitialize storage.
  UninitStorage,
  // Used by operations which uninitialize origins.
  UninitOrigins,
  // Used by operations which uninitialize clients.
  UninitClients,
};

// Pre-defined sets used for doing IsBlockedBy checks in OpenClientDirectory
// and in individual initialization methods.
//
// These sets account for the containment hierarchy between uninitialization
// operations:
// - Storage uninitialization implicitly uninitializes all origins and clients.
// - Origin uninitialization implicitly uninitializes all clients.
// Therefore, checks for a given category must include any broader categories
// that would also invalidate the target state.

// Used to check if creation and execution of storage initialization can be
// avoided if the storage has been already initialized.
constexpr EnumSet<DirectoryLockCategory> kUninitStorageOnlyCategory = {
    DirectoryLockCategory::UninitStorage};

// Used to check if creation and execution of origin initialization can be
// avoided if the origin has been already initialized.
//
// Includes UninitStorage because storage-level uninitialization also
// uninitializes origins.
constexpr EnumSet<DirectoryLockCategory> kUninitOriginsAndBroaderCategories = {
    DirectoryLockCategory::UninitOrigins, DirectoryLockCategory::UninitStorage};

// Used to check if creation and execution of client initialization can be
// avoided if the client has been already initialized.
//
// Includes UninitOrigins and UninitStorage because both implicitly uninitialize
// clients.
constexpr EnumSet<DirectoryLockCategory> kUninitClientsAndBroaderCategories = {
    DirectoryLockCategory::UninitClients, DirectoryLockCategory::UninitOrigins,
    DirectoryLockCategory::UninitStorage};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCKCATEGORY_H_
