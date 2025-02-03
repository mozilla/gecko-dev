/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pkcs11.h"

// The build system builds the rust library builtins as a static library
// called builtins_static. On macOS and Windows, that static library can
// be linked with an empty file and turned into a shared library with the
// function C_GetFunctionList exposed.
// Unfortunately, on Linux, exposing the C_GetFunctionList in the static
// library doesn't work for some unknown reason. As a workaround, this file
// declares its own C_GetFunctionList that can be exposed in the shared
// library. It then calls the function BUILTINSC_GetFunctionList exposed
// (internally to the linkage in question) by builtins. This enables
// the build system to ultimately turn builtins into a shared library
// that exposes a C_GetFunctionList function, meaning it can be used as a
// PKCS#11 module.

extern "C" {

CK_RV BUILTINSC_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList);

CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList) {
  return BUILTINSC_GetFunctionList(ppFunctionList);
}
}
