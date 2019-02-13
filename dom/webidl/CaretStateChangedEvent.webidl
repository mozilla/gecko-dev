/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum CaretChangedReason {
  "visibilitychange",
  "updateposition",
  "longpressonemptycontent",
  "taponcaret",
  "presscaret",
  "releasecaret"
};

dictionary CaretStateChangedEventInit : EventInit {
  boolean collapsed = true;
  DOMRectReadOnly? boundingClientRect = null;
  CaretChangedReason reason = "visibilitychange";
  boolean caretVisible = false;
  boolean selectionVisible = false;
};

[Constructor(DOMString type, optional CaretStateChangedEventInit eventInit),
 ChromeOnly]
interface CaretStateChangedEvent : Event {
  readonly attribute boolean collapsed;
  readonly attribute DOMRectReadOnly? boundingClientRect;
  readonly attribute CaretChangedReason reason;
  readonly attribute boolean caretVisible;
  readonly attribute boolean selectionVisible;
};
