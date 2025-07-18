/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Pref="media.webspeech.recognition.enable",
 LegacyFactoryFunction=webkitSpeechGrammarList,
 Exposed=Window]
interface SpeechGrammarList {
    constructor();

    readonly attribute unsigned long length;
    [Throws]
    getter SpeechGrammar item(unsigned long index);
    [Throws]
    undefined addFromURI(DOMString src, optional float weight);
    [Throws]
    undefined addFromString(DOMString string, optional float weight);
};
