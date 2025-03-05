/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=(Window,Worker)]
interface ProgressEvent : Event
{
  constructor(DOMString type, optional ProgressEventInit eventInitDict = {});

  readonly attribute boolean lengthComputable;
  readonly attribute double loaded;
  readonly attribute double total;
};

dictionary ProgressEventInit : EventInit
{
  boolean lengthComputable = false;
  double loaded = 0;
  double total = 0;
};
