/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { clearASTs } from "./utils/ast";
import getScopes, { clearScopes } from "./getScopes";
import { setSource, clearSources } from "./sources";
import mapExpression from "./mapExpression";

import { workerHandler } from "../../../../shared/worker-utils";

function clearAllHelpersForSources(sourceIds) {
  clearASTs(sourceIds);
  clearScopes(sourceIds);
  clearSources(sourceIds);
}

self.onmessage = workerHandler({
  getScopes,
  clearSources: clearAllHelpersForSources,
  mapExpression,
  setSource,
});
