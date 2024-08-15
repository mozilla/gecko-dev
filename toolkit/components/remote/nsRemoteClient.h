/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_NSREMOTECLIENT_H_
#define TOOLKIT_COMPONENTS_REMOTE_NSREMOTECLIENT_H_

#include "nscore.h"

/**
 * Pure-virtual common base class for remoting implementations.
 */

class nsRemoteClient {
 public:
  virtual ~nsRemoteClient() = default;

  /**
   * Initializes the client
   */
  virtual nsresult Init() = 0;

  /**
   * Send a complete command line to a running instance.
   *
   * @param aProgram This is the preferred program that we want to use
   * for this particular command.
   *
   * @param aProfile This allows you to specify a particular server
   * running under a named profile.  If it is not specified the
   * profile is not checked.
   *
   * @param argc The number of command-line arguments.
   *
   * @param argv The command-line arguments.
   *
   * @param aRaise Whether the target instance's window should be brought to the
   * foreground. The actual effect of this is platform-dependent; see comments
   * in platform-specific implementations for further information.
   */
  virtual nsresult SendCommandLine(const char* aProgram, const char* aProfile,
                                   int32_t argc, const char** argv,
                                   bool aRaise) = 0;
};

#endif  // TOOLKIT_COMPONENTS_REMOTE_NSREMOTECLIENT_H_
