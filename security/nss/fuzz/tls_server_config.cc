/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_server_config.h"

#include <cstddef>
#include <cstdint>

#include "sslt.h"

const uint32_t CONFIG_ENABLE_EXTENDED_MS = 1 << 0;
const uint32_t CONFIG_REQUEST_CERTIFICATE = 1 << 1;
const uint32_t CONFIG_REQUIRE_CERTIFICATE = 1 << 2;
const uint32_t CONFIG_ENABLE_DEFLATE = 1 << 3;
const uint32_t CONFIG_ENABLE_CBC_RANDOM_IV = 1 << 4;
const uint32_t CONFIG_REQUIRE_SAFE_NEGOTIATION = 1 << 5;
const uint32_t CONFIG_NO_CACHE = 1 << 6;
const uint32_t CONFIG_ENABLE_GREASE = 1 << 7;
const uint32_t CONFIG_SET_CERTIFICATION_COMPRESSION_ALGORITHM = 1 << 8;
const uint32_t CONFIG_VERSION_RANGE_SET = 1 << 9;
const uint32_t CONFIG_ADD_EXTERNAL_PSK = 1 << 10;
const uint32_t CONFIG_ENABLE_ZERO_RTT = 1 << 11;
const uint32_t CONFIG_ENABLE_ALPN = 1 << 12;
const uint32_t CONFIG_ENABLE_FALLBACK_SCSV = 1 << 13;
const uint32_t CONFIG_ENABLE_SESSION_TICKETS = 1 << 14;
const uint32_t CONFIG_NO_LOCKS = 1 << 15;

// XOR 64-bit chunks of data to build a bitmap of config options derived from
// the fuzzing input. This seems the only way to fuzz various options while
// still maintaining compatibility with BoringSSL or OpenSSL fuzzers.
ServerConfig::ServerConfig(const uint8_t* data, size_t len) {
  union {
    uint64_t bitmap;
    struct {
      uint32_t config;
      uint16_t ssl_version_range_min;
      uint16_t ssl_version_range_max;
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

bool ServerConfig::EnableExtendedMasterSecret() {
  return config_ & CONFIG_ENABLE_EXTENDED_MS;
}

bool ServerConfig::RequestCertificate() {
  return config_ & CONFIG_REQUEST_CERTIFICATE;
}

bool ServerConfig::RequireCertificate() {
  return config_ & CONFIG_REQUIRE_CERTIFICATE;
}

bool ServerConfig::EnableDeflate() { return config_ & CONFIG_ENABLE_DEFLATE; }

bool ServerConfig::EnableCbcRandomIv() {
  return config_ & CONFIG_ENABLE_CBC_RANDOM_IV;
}

bool ServerConfig::RequireSafeNegotiation() {
  return config_ & CONFIG_REQUIRE_SAFE_NEGOTIATION;
}

bool ServerConfig::NoCache() { return config_ & CONFIG_NO_CACHE; }

bool ServerConfig::EnableGrease() { return config_ & CONFIG_ENABLE_GREASE; }

bool ServerConfig::SetCertificateCompressionAlgorithm() {
  return config_ & CONFIG_SET_CERTIFICATION_COMPRESSION_ALGORITHM;
}

bool ServerConfig::SetVersionRange() {
  return config_ & CONFIG_VERSION_RANGE_SET;
}

bool ServerConfig::AddExternalPsk() {
  return config_ & CONFIG_ADD_EXTERNAL_PSK;
}

bool ServerConfig::EnableZeroRtt() { return config_ & CONFIG_ENABLE_ZERO_RTT; }

bool ServerConfig::EnableAlpn() { return config_ & CONFIG_ENABLE_ALPN; }

bool ServerConfig::EnableFallbackScsv() {
  return config_ & CONFIG_ENABLE_FALLBACK_SCSV;
}

bool ServerConfig::EnableSessionTickets() {
  return config_ & CONFIG_ENABLE_SESSION_TICKETS;
}

bool ServerConfig::NoLocks() { return config_ & CONFIG_NO_LOCKS; }

SSLHashType ServerConfig::PskHashType() {
  if (config_ % 2) return ssl_hash_sha256;

  return ssl_hash_sha384;
}

const SSLVersionRange& ServerConfig::VersionRange() {
  return ssl_version_range_;
}
