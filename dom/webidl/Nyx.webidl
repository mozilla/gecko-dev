/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Interface for interacting with the Nyx snapshot fuzzing engine.
 *
 * Various functions useful for snapshot fuzzing that are enabled
 * only in --enable-snapshot-fuzzing builds, because they may be dangerous to
 * enable on untrusted pages.
*/

[Pref="fuzzing.snapshot.enabled",
 Exposed=Window]
namespace Nyx {
  /**
   * Use nyx_put for logging during Nyx fuzzing.
   */

  undefined log(DOMString aMsg);
  /**
   * Determine if Nyx is enabled for the specified fuzzer.
   */
  boolean isEnabled(DOMString aFuzzerName);


  /**
   * Determine if Nyx is in replay mode.
   */
  boolean isReplay();

  /**
   * Determine if Nyx is in started.
   */
  boolean isStarted();

  /**
   * Start Nyx.
   */
  undefined start();

  /**
   * Stop Nyx.
   */
  undefined release();

  /**
   * Get raw data from Nyx buffer.
   */
  [Throws, NewObject]
  ArrayBuffer getRawData();
};
