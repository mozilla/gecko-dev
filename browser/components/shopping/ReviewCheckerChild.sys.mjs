/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ShoppingSidebarChild } from "resource:///actors/ShoppingSidebarChild.sys.mjs";

/**
 * The ReviewCheckerChild will get the current URL from the parent
 * and will request data to update the sidebar UI if that URL is a
 * product or display the current opt-in or empty state.
 */
export class ReviewCheckerChild extends ShoppingSidebarChild {
  // TODO: Move common methods into a helper and switch this to just
  // extend RemotePageChild in Bug 1933539.
}
