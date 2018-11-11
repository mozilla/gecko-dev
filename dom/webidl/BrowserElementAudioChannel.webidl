/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.mozBrowserFramesEnabled",
 ChromeOnly]
interface BrowserElementAudioChannel : EventTarget {
  readonly attribute AudioChannel name;

  // This event is dispatched when this audiochannel is actually in used by the
  // app or one of the sub-iframes.
  attribute EventHandler onactivestatechanged;

  [Throws]
  DOMRequest getVolume();

  [Throws]
  DOMRequest setVolume(float aVolume);

  [Throws]
  DOMRequest getMuted();

  [Throws]
  DOMRequest setMuted(boolean aMuted);

  [Throws]
  DOMRequest isActive();
};

partial interface BrowserElementPrivileged {
  [Pure, Cached, Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  readonly attribute sequence<BrowserElementAudioChannel> allowedAudioChannels;

  /**
   * Mutes all audio in this browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void mute();

  /**
   * Unmutes all audio in this browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void unmute();

  /**
   * Obtains whether or not the browser is muted.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getMuted();

  /**
   * Sets the volume for the browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void setVolume(float volume);

  /**
   * Gets the volume for the browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getVolume();
};
