/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TLS_SOCKET_H_
#define TLS_SOCKET_H_

#include <cstdint>

#include "dummy_io.h"
#include "prinrval.h"
#include "prio.h"

namespace TlsSocket {

class DummyPrSocket : public DummyIOLayerMethods {
 public:
  DummyPrSocket(const uint8_t *buf, size_t len) : buf_(buf), len_(len) {}

  int32_t Read(PRFileDesc *fd, void *data, int32_t len) override;
  int32_t Write(PRFileDesc *fd, const void *buf, int32_t length) override;
  int32_t Recv(PRFileDesc *fd, void *buf, int32_t buflen, int32_t flags,
               PRIntervalTime to) override;

 private:
  const uint8_t *buf_;
  size_t len_;
};

}  // namespace TlsSocket

#endif  // TLS_SOCKET_H_
