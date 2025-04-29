/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webidl.spec.whatwg.org/
 */

typedef (ArrayBufferView or ArrayBuffer) BufferSource;

// Should be replaced with `ArrayBuffer or SharedArrayBuffer`. See bug 1838639.
typedef ([AllowShared] ArrayBuffer or [AllowShared] ArrayBufferView) AllowSharedBufferSource;
