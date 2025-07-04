/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SignedCertificateTimestamp.h"

#include "CTUtils.h"

namespace mozilla {
namespace ct {

pkix::Result SignedCertificateTimestamp::DecodeExtensions() {
  if (extensions.empty()) {
    return pkix::Success;
  }

  // `extensions` is a sequence of Extension:
  //     struct {
  //         ExtensionType extension_type;
  //         opaque extension_data<0..2^16-1>;
  //     } Extension;
  const size_t kExtensionDataLengthBytes = 2;
  // Currently, the only supported extension type is `leaf_index`. Others are
  // ignored.
  //     enum {
  //         leaf_index(0), (255)
  //     } ExtensionType;
  const size_t kExtensionTypeLength = 1;
  const uint8_t kExtensionTypeLeafIndex = 0;

  pkix::Input input;
  pkix::Result rv = input.Init(extensions.data(), extensions.size());
  if (rv != pkix::Success) {
    return rv;
  }
  pkix::Reader reader(input);
  while (!reader.AtEnd()) {
    uint8_t extensionType;
    rv = ReadUint<kExtensionTypeLength>(reader, extensionType);
    if (rv != pkix::Success) {
      return rv;
    }
    pkix::Input extensionData;
    rv = ReadVariableBytes<kExtensionDataLengthBytes>(reader, extensionData);
    if (rv != pkix::Success) {
      return rv;
    }
    if (extensionType == kExtensionTypeLeafIndex) {
      // Duplicate extensions are not allowed.
      if (leafIndex.isSome()) {
        return pkix::Result::ERROR_EXTENSION_VALUE_INVALID;
      }
      // A leaf index is a big-endian, unsigned 40-bit value. In other words,
      // it is 5 8-bit bytes, like so:
      //     uint8 uint40[5];
      //     uint40 LeafIndex;
      const size_t kLeafIndexLength = 5;
      uint64_t leafIndexValue;
      pkix::Reader leafIndexReader(extensionData);
      rv = ReadUint<kLeafIndexLength>(leafIndexReader, leafIndexValue);
      if (rv != pkix::Success) {
        return rv;
      }
      if (!leafIndexReader.AtEnd()) {
        return pkix::Result::ERROR_EXTENSION_VALUE_INVALID;
      }
      leafIndex.emplace(leafIndexValue);
    }
  }
  return pkix::Success;
}

void LogEntry::Reset() {
  type = LogEntry::Type::X509;
  leafCertificate.clear();
  issuerKeyHash.clear();
  tbsCertificate.clear();
}

bool DigitallySigned::SignatureParametersMatch(
    HashAlgorithm aHashAlgorithm,
    SignatureAlgorithm aSignatureAlgorithm) const {
  return (hashAlgorithm == aHashAlgorithm) &&
         (signatureAlgorithm == aSignatureAlgorithm);
}

}  // namespace ct
}  // namespace mozilla
