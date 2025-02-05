/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxPolicyPGO_h
#define mozilla_SandboxPolicyPGO_h

namespace mozilla {

static const char SandboxPolicyPGO[] = R"SANDBOX_LITERAL(
  (define pgoDataDir (param "PGO_DATA_DIR"))

  (allow file-write-data (subpath pgoDataDir))
  (allow file-write-create (subpath pgoDataDir))
)SANDBOX_LITERAL";

}  // namespace mozilla

#endif  // mozilla_SandboxPolicyPGO_h
