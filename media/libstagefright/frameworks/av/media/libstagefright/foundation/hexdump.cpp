/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "hexdump"
#include <utils/Log.h>

#include "hexdump.h"

#include "ADebug.h"
#include "AString.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

namespace stagefright {

static void appendIndent(AString *s, int32_t indent) {
    static const char kWhitespace[] =
        "                                        "
        "                                        ";

    CHECK_LT((size_t)indent, sizeof(kWhitespace));

    s->append(kWhitespace, indent);
}

void hexdump(const void *_data, size_t size, size_t indent, AString *appendTo) {
    const uint8_t *data = (const uint8_t *)_data;

    size_t offset = 0;
    while (offset < size) {
        AString line;

        appendIndent(&line, indent);

        char tmp[32];
        sprintf(tmp, "%08lx:  ", (unsigned long)offset);

        line.append(tmp);

        for (size_t i = 0; i < 16; ++i) {
            if (i == 8) {
                line.append(' ');
            }
            if (offset + i >= size) {
                line.append("   ");
            } else {
                sprintf(tmp, "%02x ", data[offset + i]);
                line.append(tmp);
            }
        }

        line.append(' ');

        for (size_t i = 0; i < 16; ++i) {
            if (offset + i >= size) {
                break;
            }

            if (isprint(data[offset + i])) {
                line.append((char)data[offset + i]);
            } else {
                line.append('.');
            }
        }

        if (appendTo != NULL) {
            appendTo->append(line);
            appendTo->append("\n");
        } else {
            ALOGI("%s", line.c_str());
        }

        offset += 16;
    }
}

}  // namespace stagefright

#undef LOG_TAG
