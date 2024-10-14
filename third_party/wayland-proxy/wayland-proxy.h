/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _wayland_proxy_h_
#define _wayland_proxy_h_

#include <poll.h>
#include <vector>
#include <fcntl.h>
#include <atomic>
#include <memory>

class ProxiedConnection;

typedef void (*CompositorCrashHandler)();

#define WAYLAND_PROXY_ENABLED                       (1 << 0)
#define WAYLAND_PROXY_DISABLED                      (1 << 1)
#define WAYLAND_PROXY_RUN_FAILED                    (1 << 2)
#define WAYLAND_PROXY_TERMINATED                    (1 << 3)
#define WAYLAND_PROXY_CONNECTION_ADDED              (1 << 4)
#define WAYLAND_PROXY_CONNECTION_REMOVED            (1 << 5)
#define WAYLAND_PROXY_APP_TERMINATED                (1 << 6)
#define WAYLAND_PROXY_APP_CONNECTION_FAILED         (1 << 7)
#define WAYLAND_PROXY_COMPOSITOR_ATTACHED           (1 << 8)
#define WAYLAND_PROXY_COMPOSITOR_CONNECTION_FAILED  (1 << 9)
#define WAYLAND_PROXY_COMPOSITOR_SOCKET_FAILED      (1 << 10)

class WaylandProxy {
 public:
  static std::unique_ptr<WaylandProxy> Create();

  // Launch an application with Wayland proxy set
  bool RunChildApplication(char* argv[]);

  // Run proxy as part of already running application
  // and set Wayland proxy display for it.
  bool RunThread();

  // Set original Wayland display env variable and clear
  // proxy display file.
  void RestoreWaylandDisplay();

  static void SetVerbose(bool aVerbose);
  static void SetCompositorCrashHandler(CompositorCrashHandler aCrashHandler);
  static void CompositorCrashed();
  static void AddState(unsigned aState);
  static const char* GetState();

  ~WaylandProxy();

 private:
  bool Init();
  void Run();

  void SetWaylandProxyDisplay();
  static void* RunProxyThread(WaylandProxy* aProxy);
  bool CheckWaylandDisplay(const char* aWaylandDisplay);

  bool SetupWaylandDisplays();
  bool StartProxyServer();
  bool IsChildAppTerminated();

  bool PollConnections();
  bool ProcessConnections();

  void Info(const char* aFormat, ...);
  void Warning(const char* aOperation);
  void Error(const char* aOperation);
  void ErrorPlain(const char* aFormat, ...);

  void CheckCompositor();

 private:
  // List of all Compositor <-> Application connections
  std::vector<std::unique_ptr<ProxiedConnection>> mConnections;
  int mProxyServerSocket = -1;
  pid_t mApplicationPID = 0;
  std::atomic<bool> mThreadRunning = false;
  pthread_t mThread;

  // sockaddr_un has hardcoded max len of sun_path
  static constexpr int sMaxDisplayNameLen = 108;
  // Name of Wayland display provided by compositor
  char mWaylandDisplay[sMaxDisplayNameLen];
  // Name of Wayland display provided by us
  char mWaylandProxy[sMaxDisplayNameLen];

  static CompositorCrashHandler sCompositorCrashHandler;
  static std::atomic<unsigned> sProxyStateFlags;
};

#endif  // _wayland_proxy_h_
