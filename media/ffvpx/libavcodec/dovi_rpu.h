/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Stubs for dovi_rpu.{c,h} */

enum {
    FF_DOVI_WRAP_T35        = 1 << 1, ///< wrap inside T.35+EMDF
};

typedef struct AVCtx AVContext;

typedef struct AVDOVIDecoderConfigurationRecord {
    uint8_t dv_profile;
} AVDOVIDecoderConfigurationRecord;

typedef struct DOVICtx {
  int dv_profile;
  void* logctx;
  int operating_point;
  AVDOVIDecoderConfigurationRecord cfg;
  #define FF_DOVI_AUTOMATIC -1
    int enable;
} DOVIContext;

typedef struct AVDOVIMetadata {
} AVDOVIMetadata;

static void ff_dovi_ctx_unref(DOVIContext* ctx) {}
static void ff_dovi_update_cfg(DOVIContext* ctx,
                               AVDOVIDecoderConfigurationRecord* record) {}
static int ff_dovi_rpu_parse(DOVIContext* ctx, uint8_t* buf, size_t len,
                             int err_recognition) {
  return 0;
}

static int ff_dovi_attach_side_data(DOVIContext* ctx, AVFrame* frame) {
  return 0;
}

static int ff_dovi_configure(DOVIContext *s, AVCodecContext *avctx) {
  return 0;
}

static int ff_dovi_rpu_generate(DOVIContext *s, const AVDOVIMetadata *metadata,
                                int flags, uint8_t **out_rpu, int *out_size) {
  return 0;
}
