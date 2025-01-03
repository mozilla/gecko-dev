/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cassert>
#include <cstdint>
#include <iostream>

#include "blapi.h"
#include "seccomon.h"
#include "ssl.h"
#include "sslimpl.h"

#include "shared.h"
#include "tls_common.h"
#include "tls_mutators.h"
#include "tls_server_certs.h"
#include "tls_server_config.h"
#include "tls_socket.h"

#ifdef IS_DTLS_FUZZ
__attribute__((constructor)) static void set_is_dtls() {
  TlsMutators::SetIsDTLS();
}

#define ImportFD DTLS_ImportFD
#else
#define ImportFD SSL_ImportFD
#endif

class SSLServerSessionCache {
 public:
  SSLServerSessionCache() {
    assert(SSL_ConfigServerSessionIDCache(1024, 0, 0, ".") == SECSuccess);
  }

  ~SSLServerSessionCache() {
    assert(SSL_ShutdownServerSessionIDCache() == SECSuccess);
  }
};

static PRStatus InitModelSocket(void* arg) {
  PRFileDesc* fd = reinterpret_cast<PRFileDesc*>(arg);

  EnableAllCipherSuites(fd);
  InstallServerCertificates(fd);

  return PR_SUCCESS;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len) {
  static NSSDatabase db = NSSDatabase();
  static SSLServerSessionCache cache = SSLServerSessionCache();
  static PRDescIdentity id = PR_GetUniqueIdentity("fuzz-server");

  // Create model socket.
  static ScopedPRFileDesc model(ImportFD(nullptr, PR_NewTCPSocket()));
  assert(model);

  // Initialize the model socket once.
  static PRCallOnceType initModelOnce;
  PR_CallOnceWithArg(&initModelOnce, InitModelSocket, model.get());

  EnableAllProtocolVersions();

  // Create and import dummy socket.
  DummyPrSocket socket = DummyPrSocket(data, len);
  ScopedPRFileDesc prFd(DummyIOLayerMethods::CreateFD(id, &socket));
  PRFileDesc* sslFd = ImportFD(model.get(), prFd.get());
  assert(sslFd == prFd.get());

  // Derive server config from input data.
  ServerConfig config = ServerConfig(data, len);

  if (ssl_trace >= 90) {
    std::cerr << config << "\n";
  }

  // Keeping things determinstic.
  assert(RNG_RandomUpdate(NULL, 0) == SECSuccess);
  assert(SSL_SetURL(sslFd, "fuzz.server") == SECSuccess);

  FixTime(sslFd);
  EnableAllCipherSuites(sslFd);

  // Set socket options from server config.
  config.SetCallbacks(sslFd);
  config.SetSocketOptions(sslFd);

  // Perform the acutal handshake.
  DoHandshake(sslFd, true);

  // Clear the cache. We never want to resume as we couldn't reproduce that.
  SSL_ClearSessionCache();

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size,
                                          size_t max_size, unsigned int seed) {
  return CustomMutate(
      {TlsMutators::DropRecord, TlsMutators::ShuffleRecords,
       TlsMutators::DuplicateRecord, TlsMutators::TruncateRecord,
       TlsMutators::FragmentRecord},
      data, size, max_size, seed);
}

extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t* data1, size_t size1,
                                            const uint8_t* data2, size_t size2,
                                            uint8_t* out, size_t max_out_size,
                                            unsigned int seed) {
  return TlsMutators::CrossOver(data1, size1, data2, size2, out, max_out_size,
                                seed);
}
