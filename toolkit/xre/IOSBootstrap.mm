/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoView/IOSBootstrap.h"
#include "GeckoView/GeckoViewSwiftSupport.h"

#include "mozilla/Bootstrap.h"
#include "mozilla/DarwinObjectPtr.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/widget/GeckoViewSupport.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "XREChildData.h"

#include "application.ini.h"

static id<SwiftGeckoViewRuntime> gRuntime;

id<SwiftGeckoViewRuntime> mozilla::widget::GetSwiftRuntime() {
  return gRuntime;
}

static id<GeckoProcessExtension> gCurrentProcessExtension;

id<GeckoProcessExtension> mozilla::widget::GetCurrentProcessExtension() {
  return gCurrentProcessExtension;
}

int MainProcessInit(int aArgc, char** aArgv,
                    id<SwiftGeckoViewRuntime> aRuntime) {
  auto bootstrap = mozilla::GetBootstrap();
  if (bootstrap.isErr()) {
    printf_stderr("Couldn't load XPCOM.\n");
    return 255;
  }

  gRuntime = [aRuntime retain];

  mozilla::BootstrapConfig config;
  config.appData = &sAppData;
  config.appDataPath = nullptr;

  return bootstrap.inspect()->XRE_main(aArgc, aArgv, config);
}

static void HandleBootstrapMessage(xpc_object_t aEvent);

void ChildProcessInit(xpc_connection_t aXpcConnection,
                      id<GeckoProcessExtension> aProcess,
                      id<SwiftGeckoViewRuntime> aRuntime) {
  gCurrentProcessExtension = aProcess;
  gRuntime = [aRuntime retain];
  static std::atomic<bool> geckoViewStarted = false;

  xpc_connection_set_event_handler(aXpcConnection, [](xpc_object_t aEvent) {
    xpc_type_t type = xpc_get_type(aEvent);
    if (type != XPC_TYPE_DICTIONARY) {
      NSLog(@"[%d] Received unexpected XPC event type: %s\n", getpid(),
            xpc_type_get_name(type));
      if (!geckoViewStarted && type == XPC_TYPE_ERROR &&
          (aEvent == XPC_ERROR_CONNECTION_INVALID ||
           aEvent == XPC_ERROR_TERMINATION_IMMINENT)) {
        // FIXME: handle this more gracefully?
        MOZ_CRASH("Received XPC error event before bootstrap event");
      }
      return;
    }

    const char* messageName = xpc_dictionary_get_string(aEvent, "message-name");
    if (!messageName) {
      NSLog(@"[%d] No message name specified in XPC message", getpid());
      return;
    }

    if (!strcmp(messageName, "bootstrap")) {
      HandleBootstrapMessage(aEvent);
      // Errors on the XPC channel no longer indicate we should shut down.
      geckoViewStarted = true;
    } else {
      NS_WARNING(nsPrintfCString("Unknown XPC message: %s", messageName).get());
    }
  });

  xpc_connection_activate(aXpcConnection);
}

static int ChildProcessInitImpl(int aArgc, char** aArgv) {
  auto bootstrap = mozilla::GetBootstrap();
  if (bootstrap.isErr()) {
    printf_stderr("Couldn't load XPCOM.\n");
    return 255;
  }
  // Check for the absolute minimum number of args we need to move
  // forward here. We expect the last arg to be the child process type,
  // and the second-last argument to be the gecko child id.
  if (aArgc < 2) {
    return 3;
  }

  // Set the process type. We don't remove the arg here as that will be
  // done later in common code.
  mozilla::SetGeckoProcessType(aArgv[aArgc - 1]);

  XREChildData childData;

  mozilla::SetGeckoChildID(aArgv[aArgc - 2]);

  nsresult rv =
      bootstrap.inspect()->XRE_InitChildProcess(aArgc - 2, aArgv, &childData);

  return NS_FAILED(rv);
}

void HandleBootstrapMessage(xpc_object_t aEvent) {
  // Set up stdout and stderr if they were provided.
  int fd = xpc_dictionary_dup_fd(aEvent, "stdout");
  if (fd != -1) {
    MOZ_ASSERT(fd != STDOUT_FILENO);
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }
  fd = xpc_dictionary_dup_fd(aEvent, "stderr");
  if (fd != -1) {
    MOZ_ASSERT(fd != STDERR_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
  }

  // Immediately send a reply with our pid and mach task port.
  auto reply = mozilla::AdoptDarwinObject(xpc_dictionary_create_reply(aEvent));
  xpc_dictionary_set_int64(reply.get(), "pid", getpid());
  xpc_dictionary_set_mach_send(reply.get(), "task", mach_task_self());
  xpc_connection_send_message(xpc_dictionary_get_remote_connection(aEvent),
                              reply.get());

  // Load any environment variable overrides set by the parent process.
  xpc_object_t newEnviron = xpc_dictionary_get_dictionary(aEvent, "environ");
  xpc_dictionary_apply(newEnviron, [](const char* key, xpc_object_t value) {
    setenv(key, xpc_string_get_string_ptr(value), 1);
    return true;
  });

  xpc_object_t fds = xpc_dictionary_get_array(aEvent, "fds");
  if (!fds) {
    MOZ_CRASH("fds array not specified");
    return;
  }

  size_t num_fds = xpc_array_get_count(fds);
  std::vector<mozilla::UniqueFileHandle> files;
  files.reserve(num_fds);
  for (size_t i = 0; i < num_fds; ++i) {
    files.emplace_back(xpc_array_dup_fd(fds, i));
  }

  mozilla::geckoargs::SetPassedFileHandles(std::move(files));

  xpc_object_t sendRightsArray = xpc_dictionary_get_array(aEvent, "sendRights");
  if (!sendRightsArray) {
    MOZ_CRASH("sendRights array not specified");
    return;
  }

  size_t num_rights = xpc_array_get_count(sendRightsArray);
  std::vector<mozilla::UniqueMachSendRight> sendRights;
  sendRights.reserve(num_rights);
  for (size_t i = 0; i < num_rights; ++i) {
    // NOTE: As iOS doesn't expose an xpc_array_set_mach_send method, the
    // port is wrapped with a single-key dictionary.
    xpc_object_t sendRightWrapper =
        xpc_array_get_dictionary(sendRightsArray, i);
    if (!sendRightWrapper) {
      MOZ_CRASH("invalid sendRights array");
      continue;
    }
    sendRights.emplace_back(
        xpc_dictionary_copy_mach_send(sendRightWrapper, "port"));
  }

  mozilla::geckoargs::SetPassedMachSendRights(std::move(sendRights));

  // Populate a new argv array with our argument list from IPC.
  xpc_object_t args = xpc_dictionary_get_array(aEvent, "argv");
  if (!args) {
    MOZ_CRASH("argv array not specified");
    return;
  }

  int argc = static_cast<int>(xpc_array_get_count(args));
  char** argv = new char*[argc + 1];
  for (int i = 0; i < argc; ++i) {
    argv[i] = strdup(xpc_array_get_string(args, i));
  }
  argv[argc] = nullptr;

  dispatch_async(dispatch_get_main_queue(),
                 [argc, argv] { _exit(ChildProcessInitImpl(argc, argv)); });
}
