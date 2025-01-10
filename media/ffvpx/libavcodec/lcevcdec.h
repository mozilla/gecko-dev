/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Stubs for lcevcdec.{c,h} */
#ifndef AVCODEC_LCEVCDEC_H
#define AVCODEC_LCEVCDEC_H

#include "config_components.h"

#include <stdint.h>
typedef uintptr_t LCEVC_DecoderHandle;

typedef struct FFLCEVCContext {
    LCEVC_DecoderHandle decoder;
    int initialized;
} FFLCEVCContext;

struct AVFrame;

static int ff_lcevc_alloc(FFLCEVCContext **plcevc) {
    return 0;
}

static int ff_lcevc_process(void *logctx, struct AVFrame *frame) {
    return 0;
}

static void ff_lcevc_unref(void *opaque) {}

#endif /* AVCODEC_LCEVCDEC_H */
