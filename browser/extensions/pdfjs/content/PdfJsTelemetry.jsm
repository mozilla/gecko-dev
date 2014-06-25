/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* jshint esnext:true */

'use strict';

this.EXPORTED_SYMBOLS = ['PdfJsTelemetry'];

const Cu = Components.utils;
Cu.import('resource://gre/modules/Services.jsm');

this.PdfJsTelemetry = {
  onViewerIsUsed: function () {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_USED");
    histogram.add(true);
  },
  onFallback: function () {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_FALLBACK_SHOWN");
    histogram.add(true);
  },
  onDocumentSize: function (size) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_DOCUMENT_SIZE_KB");
    histogram.add(size / 1024);
  },
  onDocumentVersion: function (versionId) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_DOCUMENT_VERSION");
    histogram.add(versionId);
  },
  onDocumentGenerator: function (generatorId) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_DOCUMENT_GENERATOR");
    histogram.add(generatorId);
  },
  onForm: function (isAcroform) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_FORM");
    histogram.add(isAcroform);
  },
  onStreamType: function (streamTypeId) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_STREAM_TYPES");
    histogram.add(streamTypeId);
  },
  onTimeToView: function (ms) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_TIME_TO_VIEW_MS");
    histogram.add(ms);
  }
};
