/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __FORKSERVER_H_
#define __FORKSERVER_H_

#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "base/process_util.h"
#include "mozilla/ipc/MiniTransceiver.h"

namespace mozilla {
namespace ipc {

class ForkServer {
 public:
  ForkServer(int* aArgc, char*** aArgv);
  ~ForkServer() = default;

  void InitProcess(int* aArgc, char*** aArgv);

  static bool RunForkServer(int* aArgc, char*** aArgv);

 private:
  bool HandleMessages();
  bool HandleForkNewSubprocess(UniquePtr<IPC::Message> aMessage);
  void HandleWaitPid(UniquePtr<IPC::Message> aMessage);

  UniqueFileHandle mIpcFd;
  UniquePtr<MiniTransceiver> mTcver;

  int* mArgc;
  char*** mArgv;
};

enum {
  Msg_ForkNewSubprocess__ID = 0x7f0,  // a random picked number
  Reply_ForkNewSubprocess__ID,
  Msg_SubprocessExecInfo__ID,
  Msg_WaitPid__ID,
  Reply_WaitPid__ID,
};

}  // namespace ipc
}  // namespace mozilla

#endif  // __FORKSERVER_H_
