/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_common.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "prio.h"
#include "ssl.h"
#include "sslexp.h"

static PRTime FixedTime(void*) { return 1234; }

// Fix the time input, to avoid any time-based variation.
void FixTime(PRFileDesc* fd) {
  SECStatus rv = SSL_SetTimeFunc(fd, FixedTime, nullptr);
  assert(rv == SECSuccess);
}

void EnableAllProtocolVersions() {
  SSLVersionRange supported;
  SECStatus rv;

  // Enable all supported versions for TCP.
  rv = SSL_VersionRangeGetSupported(ssl_variant_stream, &supported);
  assert(rv == SECSuccess);

  rv = SSL_VersionRangeSetDefault(ssl_variant_stream, &supported);
  assert(rv == SECSuccess);

  // Enable all supported versions for UDP.
  rv = SSL_VersionRangeGetSupported(ssl_variant_datagram, &supported);
  assert(rv == SECSuccess);

  rv = SSL_VersionRangeSetDefault(ssl_variant_datagram, &supported);
  assert(rv == SECSuccess);
}

void EnableAllCipherSuites(PRFileDesc* fd) {
  for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    SECStatus rv = SSL_CipherPrefSet(fd, SSL_ImplementedCiphers[i], true);
    assert(rv == SECSuccess);
  }
}

void DoHandshake(PRFileDesc* fd, bool isServer) {
  SECStatus rv = SSL_ResetHandshake(fd, isServer);
  assert(rv == SECSuccess);

  do {
    rv = SSL_ForceHandshake(fd);
  } while (rv != SECSuccess && PR_GetError() == PR_WOULD_BLOCK_ERROR);

  // If the handshake succeeds, let's read some data from the server, if any.
  if (rv == SECSuccess) {
    uint8_t block[1024];
    int32_t nb;

    // Read application data and echo it back.
    while ((nb = PR_Read(fd, block, sizeof(block))) > 0) {
      PR_Write(fd, block, nb);
    }
  }
}

SECStatus DummyCompressionEncode(const SECItem* input, SECItem* output) {
  if (!input || !input->data || input->len == 0 || !output) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  SECITEM_CopyItem(nullptr, output, input);

  return SECSuccess;
}

SECStatus DummyCompressionDecode(const SECItem* input, unsigned char* output,
                                 size_t outputLen, size_t* usedLen) {
  if (!input || !input->data || input->len == 0 || !output || outputLen == 0) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  if (input->len > outputLen) {
    PR_SetError(SEC_ERROR_BAD_DATA, 0);
    return SECFailure;
  }

  PORT_Memcpy(output, input->data, input->len);
  *usedLen = input->len;

  return SECSuccess;
}
