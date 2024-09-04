/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/
 *
 * https://drafts.csswg.org/css-view-transitions-1/#the-domtransition-interface
 */

[Exposed=Window, Pref="dom.viewTransitions.enabled"]
interface ViewTransition {
  readonly attribute Promise<undefined> updateCallbackDone;
  readonly attribute Promise<undefined> ready;
  readonly attribute Promise<undefined> finished;
  undefined skipTransition();
};
