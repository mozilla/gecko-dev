/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// https://html.spec.whatwg.org/multipage/interaction.html#the-commandevent-interface
[Exposed=Window,Func="mozilla::dom::CommandEvent::IsCallerChromeOrCommandForEnabled"]
interface CommandEvent : Event {
  [Pref="dom.element.commandfor.enabled"]
  constructor(DOMString type, optional CommandEventInit eventInitDict = {});

  readonly attribute DOMString command;

  [Pref="dom.element.commandfor.enabled"]
  readonly attribute Element? source;
};

dictionary CommandEventInit : EventInit {
    Element? source = null;
    DOMString command = "";
};
