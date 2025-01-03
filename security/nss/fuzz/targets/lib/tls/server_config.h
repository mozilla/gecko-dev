/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_SERVER_CONFIG_H_
#define TLS_SERVER_CONFIG_H_

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

namespace TlsServer {

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
  bool EnableExtendedMasterSecret() { return config_ & (1 << 0); };
  bool RequestCertificate() { return config_ & (1 << 1); };
  bool RequireCertificate() { return config_ & (1 << 2); };
  bool EnableDeflate() { return config_ & (1 << 3); };
  bool EnableCbcRandomIv() { return config_ & (1 << 4); };
  bool RequireSafeNegotiation() { return config_ & (1 << 5); };
  bool NoCache() { return config_ & (1 << 6); };
  bool EnableGrease() { return config_ & (1 << 7); };
  bool SetCertificateCompressionAlgorithm() { return config_ & (1 << 8); };
  bool SetVersionRange() { return config_ & (1 << 9); };
  bool AddExternalPsk() { return config_ & (1 << 10); };
  bool EnableZeroRtt() { return config_ & (1 << 11); };
  bool EnableAlpn() { return config_ & (1 << 12); };
  bool EnableFallbackScsv() { return config_ & (1 << 13); };
  bool EnableSessionTickets() { return config_ & (1 << 14); };
  bool NoLocks() { return config_ & (1 << 15); };
  bool FailCertificateAuthentication() { return config_ & (1 << 16); }
  bool EnableTls13BackendEch() { return config_ & (1 << 17); }
  bool EnableDelegatedCredentials() { return config_ & (1 << 18); };
  bool EnableDtlsShortHeader() { return config_ & (1 << 19); };

 private:
  uint32_t config_;
  SSLVersionRange ssl_version_range_;
};

std::ostream& operator<<(std::ostream& out, Config& config);

}  // namespace TlsServer

#endif  // TLS_SERVER_CONFIG_H_
