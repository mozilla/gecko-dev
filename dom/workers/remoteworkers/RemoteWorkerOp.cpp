/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerOp.h"

namespace mozilla::dom::remoteworker {

WorkerPrivateAccessibleState::~WorkerPrivateAccessibleState() {
  // We should now only be performing state transitions on the main thread, so
  // we should assert we're only releasing on the main thread.
  MOZ_ASSERT(!mWorkerPrivate || NS_IsMainThread());
  // mWorkerPrivate can be safely released on the main thread.
  if (!mWorkerPrivate || NS_IsMainThread()) {
    return;
  }
  // But as a backstop, do proxy the release to the main thread.
  NS_ReleaseOnMainThread(
      "RemoteWorkerChild::WorkerPrivateAccessibleState::mWorkerPrivate",
      mWorkerPrivate.forget());
}

}  // namespace mozilla::dom::remoteworker
