/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_SERVER_CONFIG_H_
#define TLS_SERVER_CONFIG_H_

#include <cstddef>
#include <cstdint>

#include "sslt.h"

#ifdef IS_DTLS_FUZZ
#define SSL_VERSION_RANGE_MIN_VALID 0x0302
#else
#define SSL_VERSION_RANGE_MIN_VALID 0x0301
#endif
#define SSL_VERSION_RANGE_MAX_VALID 0x0304

class ServerConfig {
 public:
  ServerConfig(const uint8_t* data, size_t len);

  bool EnableExtendedMasterSecret();
  bool RequestCertificate();
  bool RequireCertificate();
  bool EnableDeflate();
  bool EnableCbcRandomIv();
  bool RequireSafeNegotiation();
  bool NoCache();
  bool EnableGrease();
  bool SetCertificateCompressionAlgorithm();
  bool SetVersionRange();
  bool AddExternalPsk();
  bool EnableZeroRtt();
  bool EnableAlpn();
  bool EnableFallbackScsv();
  bool EnableSessionTickets();
  bool NoLocks();

  SSLHashType PskHashType();
  const SSLVersionRange& VersionRange();

 private:
  uint32_t config_;
  SSLVersionRange ssl_version_range_;
};

#endif  // TLS_SERVER_CONFIG_H_
