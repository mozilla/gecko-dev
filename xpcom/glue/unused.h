/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_unused_h
#define mozilla_unused_h

#include "nscore.h"

namespace mozilla {

//
// Suppress GCC warnings about unused return values with
//   unused << SomeFuncDeclaredWarnUnusedReturnValue();
//
struct NS_COM_GLUE unused_t
{
};

extern const unused_t NS_COM_GLUE unused;

template<typename T>
inline void
operator<<(const unused_t& /*unused*/, const T& /*unused*/)
{
}

}

#endif // mozilla_unused_h
