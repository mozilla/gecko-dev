/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CLIENTSTORAGESCOPE_H_
#define DOM_QUOTA_CLIENTSTORAGESCOPE_H_

#include "mozilla/Assertions.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/quota/Client.h"

namespace mozilla::dom::quota {

/**
 * Represents a scope within an origin directory, currently covering either a
 * specific client (`Client`), metadata (`Metadata`), or a match-all scope
 * (`Null`).
 *
 * The use of "Storage" in the class name is intentional. Unlike
 * `PersistenceScope` and `OriginScope`, which match only specific directories,
 * this scope is meant to cover all entries within an origin directory. That
 * includes client specific folders (e.g., idb/, fs/) and, in the future, files
 * like metadata that exist alongside them.
 *
 * The special `Metadata` scope exists because adding the metadata type to
 * client types would complicate other aspects of the system. A special client
 * implementation just for working with the metadata file would be overkill.
 * However, we need a way to lock just the metadata file. Since metadata files
 * reside alongside client directories under the same origin directory, it
 * makes sense to include them in the `ClientStorageScope`.
 *
 * This class provides operations to check the current scope type
 * (`Client`, `Metadata`, or `Null`), set the scope type, retrieve a client
 * type, and match it with another scope.
 */
class ClientStorageScope {
  class Client {
    quota::Client::Type mClientType;

   public:
    explicit Client(quota::Client::Type aClientType)
        : mClientType(aClientType) {}

    quota::Client::Type GetClientType() const { return mClientType; }
  };

  struct Metadata {};

  struct Null {};

  using DataType = Variant<Client, Metadata, Null>;

  DataType mData;

 public:
  ClientStorageScope() : mData(Null()) {}

  static ClientStorageScope CreateFromClient(quota::Client::Type aClientType) {
    return ClientStorageScope(std::move(Client(aClientType)));
  }

  static ClientStorageScope CreateFromMetadata() {
    return ClientStorageScope(std::move(Metadata()));
  }

  static ClientStorageScope CreateFromNull() {
    return ClientStorageScope(std::move(Null()));
  }

  bool IsClient() const { return mData.is<Client>(); }

  bool IsMetadata() const { return mData.is<Metadata>(); }

  bool IsNull() const { return mData.is<Null>(); }

  void SetFromClient(quota::Client::Type aClientType) {
    mData = AsVariant(Client(aClientType));
  }

  void SetFromNull() { mData = AsVariant(Null()); }

  quota::Client::Type GetClientType() const {
    MOZ_ASSERT(IsClient());

    return mData.as<Client>().GetClientType();
  }

  bool Matches(const ClientStorageScope& aOther) const {
    struct Matcher {
      const ClientStorageScope& mThis;

      explicit Matcher(const ClientStorageScope& aThis) : mThis(aThis) {}

      bool operator()(const Client& aOther) {
        return mThis.MatchesClient(aOther);
      }

      bool operator()(const Metadata& aOther) {
        return mThis.MatchesMetadata(aOther);
      }

      bool operator()(const Null& aOther) { return true; }
    };

    return aOther.mData.match(Matcher(*this));
  }

 private:
  // Move constructors
  explicit ClientStorageScope(const Client&& aClient) : mData(aClient) {}

  explicit ClientStorageScope(const Metadata&& aMetadata) : mData(aMetadata) {}

  explicit ClientStorageScope(const Null&& aNull) : mData(aNull) {}

  // Copy constructor
  explicit ClientStorageScope(const DataType& aOther) : mData(aOther) {}

  bool MatchesClient(const Client& aOther) const {
    struct ClientMatcher {
      const Client& mOther;

      explicit ClientMatcher(const Client& aOther) : mOther(aOther) {}

      bool operator()(const Client& aThis) {
        return aThis.GetClientType() == mOther.GetClientType();
      }

      bool operator()(const Metadata& aThis) { return false; }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(ClientMatcher(aOther));
  }

  bool MatchesMetadata(const Metadata& aOther) const {
    struct MetadataMatcher {
      const Metadata& mOther;

      explicit MetadataMatcher(const Metadata& aOther) : mOther(aOther) {}

      bool operator()(const Client& aThis) { return false; }

      bool operator()(const Metadata& aThis) { return true; }

      bool operator()(const Null& aThis) {
        // Null covers everything.
        return true;
      }
    };

    return mData.match(MetadataMatcher(aOther));
  }

  bool operator==(const ClientStorageScope& aOther) = delete;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CLIENTSTORAGESCOPE_H_
