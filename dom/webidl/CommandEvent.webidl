/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// https://html.spec.whatwg.org/multipage/interaction.html#the-commandevent-interface
[Exposed=Window,Func="mozilla::dom::CommandEvent::IsCallerChromeOrCommandForEnabled"]
interface CommandEvent : Event {
  // TODO(keithamus): Spec is `DOMString` but internal CommandEvent is `DOMString?`
  readonly attribute DOMString? command;

  // TODO(keithamus): Add remaining interface (source, constructor) from spec CommandEvent
};
