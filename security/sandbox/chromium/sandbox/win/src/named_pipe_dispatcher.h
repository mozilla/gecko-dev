// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__
#define SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles named pipe related IPC calls.
class NamedPipeDispatcher : public Dispatcher {
 public:
  explicit NamedPipeDispatcher(PolicyBase* policy_base);
  ~NamedPipeDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to CreateNamedPipeW() in the
  // target.
  bool CreateNamedPipe(IPCInfo* ipc,
                       base::string16* name,
                       uint32 open_mode,
                       uint32 pipe_mode,
                       uint32 max_instances,
                       uint32 out_buffer_size,
                       uint32 in_buffer_size,
                       uint32 default_timeout);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(NamedPipeDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__
