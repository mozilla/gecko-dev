/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Temporary implementation for zucchini_main.cc. The final patch in the stack
// of patches provides the true implementation, which requires that we have
// already vendored all necessary dependencies.

#include <cstdio>
#include <cstring>

#include "components/zucchini/crc32.h"

int main() {
    uint8_t buffer[] = "Hello World!";
    uint32_t crc32 = zucchini::CalculateCrc32(buffer, buffer + sizeof(buffer));
    printf("CRC32(%s) = %u\n", buffer, crc32);
    return 0;
}
