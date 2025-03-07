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

#else

  pref("pdfjs.enableUpdatedAddImage", true);

  #if defined(EARLY_BETA_OR_EARLIER)
    pref("pdfjs.enableHighlightFloatingButton", true);

    // Enable adding a signature in the PDF viewer.
    pref("pdfjs.enableSignatureEditor", true);
  #else
    pref("pdfjs.enableHighlightFloatingButton", false);
    pref("pdfjs.enableSignatureEditor", false);
  #endif

  pref("pdfjs.enableAltTextForEnglish", false);

#endif
