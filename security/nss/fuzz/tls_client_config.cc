/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdint>

#include "prtypes.h"

#include "tls_client_config.h"

const uint32_t CONFIG_FAIL_CERT_AUTH = 1 << 0;
const uint32_t CONFIG_ENABLE_EXTENDED_MS = 1 << 1;
const uint32_t CONFIG_REQUIRE_DH_NAMED_GROUPS = 1 << 2;
const uint32_t CONFIG_ENABLE_FALSE_START = 1 << 3;
const uint32_t CONFIG_ENABLE_DEFLATE = 1 << 4;
const uint32_t CONFIG_ENABLE_CBC_RANDOM_IV = 1 << 5;
const uint32_t CONFIG_REQUIRE_SAFE_NEGOTIATION = 1 << 6;
const uint32_t CONFIG_ENABLE_CACHE = 1 << 7;
const uint32_t CONFIG_ENABLE_GREASE = 1 << 8;
const uint32_t CONFIG_ENABLE_CH_EXTENSION_PERMUTATION = 1 << 9;
const uint32_t CONFIG_SET_CERTIFICATION_COMPRESSION_ALGORITHM = 1 << 10;
const uint32_t CONFIG_SET_CLIENT_ECH_CONFIGS = 1 << 11;
const uint32_t CONFIG_VERSION_RANGE_SET = 1 << 12;
const uint32_t CONFIG_ADD_EXTERNAL_PSK = 1 << 13;
const uint32_t CONFIG_ENABLE_POST_HANDSHAKE_AUTH = 1 << 14;

// XOR 64-bit chunks of data to build a bitmap of config options derived from
// the fuzzing input. This seems the only way to fuzz various options while
// still maintaining compatibility with BoringSSL or OpenSSL fuzzers.
ClientConfig::ClientConfig(const uint8_t* data, size_t len) {
  union {
    uint64_t bitmap;
    struct {
      uint32_t config;
      PRUint16 ssl_version_range_min;
      PRUint16 ssl_version_range_max;
    };
  };

  for (size_t i = 0; i < len; i++) {
    bitmap ^= static_cast<uint64_t>(data[i]) << (8 * (i % 8));
  }

  // Map SSL version values to a valid range.
  ssl_version_range_min =
      SSL_VERSION_RANGE_MIN_VALID +
      (ssl_version_range_min %
       (1 + SSL_VERSION_RANGE_MAX_VALID - SSL_VERSION_RANGE_MIN_VALID));
  ssl_version_range_max =
      ssl_version_range_min +
      (ssl_version_range_max %
       (1 + SSL_VERSION_RANGE_MAX_VALID - ssl_version_range_min));

  config_ = config;
  ssl_version_range_ = {
      .min = ssl_version_range_min,
      .max = ssl_version_range_max,
  };
}

bool ClientConfig::FailCertificateAuthentication() {
  return config_ & CONFIG_FAIL_CERT_AUTH;
}

bool ClientConfig::EnableExtendedMasterSecret() {
  return config_ & CONFIG_ENABLE_EXTENDED_MS;
}

bool ClientConfig::RequireDhNamedGroups() {
  return config_ & CONFIG_REQUIRE_DH_NAMED_GROUPS;
}

bool ClientConfig::EnableFalseStart() {
  return config_ & CONFIG_ENABLE_FALSE_START;
}

bool ClientConfig::EnableDeflate() { return config_ & CONFIG_ENABLE_DEFLATE; }

bool ClientConfig::EnableCbcRandomIv() {
  return config_ & CONFIG_ENABLE_CBC_RANDOM_IV;
}

bool ClientConfig::RequireSafeNegotiation() {
  return config_ & CONFIG_REQUIRE_SAFE_NEGOTIATION;
}

bool ClientConfig::EnableCache() { return config_ & CONFIG_ENABLE_CACHE; }

bool ClientConfig::EnableGrease() { return config_ & CONFIG_ENABLE_GREASE; }

bool ClientConfig::EnableCHExtensionPermutation() {
  return config_ & CONFIG_ENABLE_CH_EXTENSION_PERMUTATION;
};

bool ClientConfig::SetCertificateCompressionAlgorithm() {
  return config_ & CONFIG_SET_CERTIFICATION_COMPRESSION_ALGORITHM;
}

bool ClientConfig::SetClientEchConfigs() {
  return config_ & CONFIG_SET_CLIENT_ECH_CONFIGS;
}

bool ClientConfig::SetVersionRange() {
  return config_ & CONFIG_VERSION_RANGE_SET;
}

bool ClientConfig::AddExternalPsk() {
  return config_ & CONFIG_ADD_EXTERNAL_PSK;
}

bool ClientConfig::EnablePostHandshakeAuth() {
  return config_ & CONFIG_ENABLE_POST_HANDSHAKE_AUTH;
}

const SSLVersionRange& ClientConfig::VersionRange() {
  return ssl_version_range_;
}
