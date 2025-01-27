/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://html.spec.whatwg.org/#broadcastchannel
 */

[Exposed=(Window,Worker)]
interface BroadcastChannel : EventTarget {
  [Throws]
  constructor(DOMString channel);

  // This function is for simplifying tests that need to coordinate between
  // top-level and third-party documents from the same origin.
  // This will throw if called outside of automation.
  [ChromeOnly,Throws]
  static BroadcastChannel unpartitionedTestingChannel(DOMString channel);

  readonly attribute DOMString name;

  [Throws]
  undefined postMessage(any message);

  undefined close();

  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};
