/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAHttpConnection.h"

#include "mozilla/Components.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"

namespace mozilla::net {

NS_IMPL_ADDREF(nsAHttpConnection)
NS_IMETHODIMP_(MozExternalRefCountType)
nsAHttpConnection::Release() {
  nsrefcnt count;
  MOZ_ASSERT(0 != mRefCnt, "dup release");
  count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "nsAHttpConnection");
  if (0 == count) {
    mRefCnt = 1; /* stablize */
    // it is essential that the connection be destroyed on the socket thread.
    DeleteSelfOnSocketThread();
    return 0;
  }
  return count;
}

NS_IMPL_QUERY_INTERFACE0(nsAHttpConnection)

nsAHttpConnection::~nsAHttpConnection() = default;

class DeleteAHttpConnection : public Runnable {
 public:
  explicit DeleteAHttpConnection(nsAHttpConnection* aConn)
      : Runnable("net::DeleteAHttpConnection"), mConn(aConn) {}

  NS_IMETHOD Run() override {
    delete mConn;
    return NS_OK;
  }

 private:
  nsAHttpConnection* mConn;
};

void nsAHttpConnection::DeleteSelfOnSocketThread() {
  if (OnSocketThread()) {
    delete this;
    return;
  }

  nsCOMPtr<nsIEventTarget> sts =
      mozilla::components::SocketTransport::Service();
  nsCOMPtr<nsIRunnable> event = new DeleteAHttpConnection(this);
  Unused << NS_WARN_IF(
      NS_FAILED(sts->Dispatch(event.forget(), NS_DISPATCH_NORMAL)));
}

}  // namespace mozilla::net
