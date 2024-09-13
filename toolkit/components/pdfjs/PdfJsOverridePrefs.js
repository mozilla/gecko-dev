/* Copyright 2024 Mozilla Foundation
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

#if defined(ANDROID)

  // Editing PDFs is not supported on mobile
  pref("pdfjs.annotationEditorMode", -1);

  // Enable the floating PDF.js toolbar on GeckoView (bug 1829366)
  pref("pdfjs.enableFloatingToolbar", true);

#else

  #if defined(EARLY_BETA_OR_EARLIER)
    pref("pdfjs.enableHighlightFloatingButton", true);
  #else
    pref("pdfjs.enableHighlightFloatingButton", false);
  #endif

  #if defined(XP_WIN)
    pref("pdfjs.enableHWA", true);
  #endif

#endif
