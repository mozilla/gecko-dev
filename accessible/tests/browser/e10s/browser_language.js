/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  "e10s/doc_language.html",
  async function (browser, docAcc) {
    is(docAcc.language, "en", "Document language correct");
    const inheritEn = findAccessibleChildByID(docAcc, "inheritEn");
    is(inheritEn.language, "en", "inheritEn language correct");
    is(inheritEn.firstChild.language, "en", "inheritEn leaf language correct");
    const imgInheritEn = findAccessibleChildByID(docAcc, "imgInheritEn");
    is(imgInheritEn.language, "en", "imgInheritEn language correct");
    const de = findAccessibleChildByID(docAcc, "de");
    is(de.language, "de", "de language correct");
    const inheritDe = findAccessibleChildByID(docAcc, "inheritDe");
    is(inheritDe.language, "de", "inheritDe language correct");
    is(inheritDe.firstChild.language, "de", "inheritDe leaf language correct");
    is(inheritDe.getChildAt(1).language, "es", "es leaf language correct");
    const imgInheritDe = findAccessibleChildByID(docAcc, "imgInheritDe");
    is(imgInheritDe.language, "de", "imgInheritDe language correct");
    const fr = findAccessibleChildByID(docAcc, "fr");
    is(fr.language, "fr", "fr language correct");
    const imgFr = findAccessibleChildByID(docAcc, "imgFr");
    is(imgFr.language, "fr", "imgFr language correct");
    const radioFr = findAccessibleChildByID(docAcc, "radioFr");
    is(radioFr.language, "fr", "radioFr language correct");
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);
