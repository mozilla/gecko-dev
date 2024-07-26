/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_client_config_h__
#define tls_client_config_h__

#include <cstddef>
#include <cstdint>

#include "sslt.h"

#define SSL_VERSION_RANGE_MIN_VALID 0x0301
#define SSL_VERSION_RANGE_MAX_VALID 0x0304

class ClientConfig {
 public:
  ClientConfig(const uint8_t* data, size_t len);

  bool FailCertificateAuthentication();
  bool EnableExtendedMasterSecret();
  bool RequireDhNamedGroups();
  bool EnableFalseStart();
  bool EnableDeflate();
  bool EnableCbcRandomIv();
  bool RequireSafeNegotiation();
  bool EnableCache();
  bool EnableGrease();
  bool EnableCHExtensionPermutation();
  bool SetCertificateCompressionAlgorithm();
  bool SetClientEchConfigs();
  bool SetVersionRange();
  bool AddExternalPsk();
  bool EnablePostHandshakeAuth();

  const SSLVersionRange& VersionRange();

 private:
  uint32_t config_;
  SSLVersionRange ssl_version_range_;
};

#endif  // tls_client_config_h__
