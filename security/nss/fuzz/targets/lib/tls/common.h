/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_COMMON_H_
#define TLS_COMMON_H_

#include <cstddef>

#include "prio.h"
#include "seccomon.h"

namespace TlsCommon {

void FixTime(PRFileDesc* fd);
void EnableAllProtocolVersions();
void EnableAllCipherSuites(PRFileDesc* fd);
void DoHandshake(PRFileDesc* fd, bool isServer);

SECStatus DummyCompressionEncode(const SECItem* input, SECItem* output);
SECStatus DummyCompressionDecode(const SECItem* input, unsigned char* output,
                                 size_t outputLen, size_t* usedLen);

}  // namespace TlsCommon

#endif  // TLS_COMMON_H_
