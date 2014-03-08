/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 * Copyright (c) 2014, Mozilla
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 ** Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 ** Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 ** Neither the name of Google nor the names of its contributors may
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMP_ENTRYPOINTS_h_
#define GMP_ENTRYPOINTS_h_

#include "gmp-errors.h"
#include "gmp-platform.h"

/* C functions exposed by Gecko Media Plugin shared library. */

// GMPInit
// Called once after plugin library is loaded, before GMPGetAPI or GMPShutdown are called.
typedef GMPErr (*GMPInitFunc)(const GMPPlatformAPI* aPlatformAPI);

// GMPGetAPI
// aAPIName: Name of API being requested.
// aHostAPI: A host API object. Owned by host, becomes invalid whenever the API object
//           being requested from the plugin becomes invalid on the host side. Plugin
//           should not attempt to delete.
// aPluginAPI: Plugin API object being requested from plugin. Ownership defined by the
//             API in question.
typedef GMPErr (*GMPGetAPIFunc)(const char* aAPIName, void* aHostAPI, void** aPluginAPI);

// GMPShutdown
// Called once before exiting process (unloading library).
typedef void   (*GMPShutdownFunc)(void);

#endif // GMP_ENTRYPOINTS_h_
