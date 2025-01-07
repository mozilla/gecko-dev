/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_CLIENT_CONFIG_H_
#define TLS_CLIENT_CONFIG_H_

#include <cstddef>
#include <cstdint>
#include <ostream>

#include "prio.h"
#include "sslt.h"

#ifdef IS_DTLS_FUZZ
#define SSL_VERSION_RANGE_MIN_VALID 0x0302
#else
#define SSL_VERSION_RANGE_MIN_VALID 0x0301
#endif
#define SSL_VERSION_RANGE_MAX_VALID 0x0304

namespace TlsClient {

class Config {
 public:
  Config(const uint8_t* data, size_t len);

  void SetCallbacks(PRFileDesc* fd);
  void SetSocketOptions(PRFileDesc* fd);

  SSLHashType PskHashType() {
    if (config_ % 2) return ssl_hash_sha256;

    return ssl_hash_sha384;
  };
  SSLVersionRange SslVersionRange() { return ssl_version_range_; };

  // NOTE: When adding more config options here, don't forget to print
  // them in the "<<"-overloaded operator.
  bool FailCertificateAuthentication() { return config_ & (1 << 0); };
  bool EnableExtendedMasterSecret() { return config_ & (1 << 1); };
  bool RequireDhNamedGroups() { return config_ & (1 << 2); };
  bool EnableFalseStart() { return config_ & (1 << 3); };
  bool EnableDeflate() { return config_ & (1 << 4); };
  bool CbcRandomIv() { return config_ & (1 << 5); };
  bool RequireSafeNegotiation() { return config_ & (1 << 6); };
  bool NoCache() { return config_ & (1 << 7); };
  bool EnableGrease() { return config_ & (1 << 8); };
  bool EnableCHExtensionPermutation() { return config_ & (1 << 9); };
  bool SetCertificateCompressionAlgorithm() { return config_ & (1 << 10); };
  bool SetClientEchConfigs() { return config_ & (1 << 11); };
  bool SetVersionRange() { return config_ & (1 << 12); };
  bool AddExternalPsk() { return config_ & (1 << 13); };
  bool EnablePostHandshakeAuth() { return config_ & (1 << 14); };
  bool EnableZeroRtt() { return config_ & (1 << 15); };
  bool EnableAlpn() { return config_ & (1 << 16); };
  bool EnableFallbackScsv() { return config_ & (1 << 17); };
  bool EnableOcspStapling() { return config_ & (1 << 18); };
  bool EnableSessionTickets() { return config_ & (1 << 19); };
  bool EnableTls13CompatMode() { return config_ & (1 << 20); };
  bool NoLocks() { return config_ & (1 << 21); };
  bool EnableTls13GreaseEch() { return config_ & (1 << 22); };
  bool SetDtls13VersionWorkaround() { return config_ & (1 << 23); };
  bool EnableDelegatedCredentials() { return config_ & (1 << 24); };
  bool EnableDtlsShortHeader() { return config_ & (1 << 25); };

 private:
  uint32_t config_;
  SSLVersionRange ssl_version_range_;
};

std::ostream& operator<<(std::ostream& out, Config& config);

}  // namespace TlsClient

#endif  // TLS_CLIENT_CONFIG_H_
