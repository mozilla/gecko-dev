/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_SERVER_CERT_H_
#define TLS_SERVER_CERT_H_

#include "prio.h"

namespace TlsServer {

void InstallServerCertificates(PRFileDesc* fd);

}  // namespace TlsServer

#endif  // TLS_SERVER_CERT_H_
