/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const info = new Error().fileName.split("#")[1]?.split(":");
if (info) {
  const bug = info.shift().split("_")[0];
  const interventions =
    info
      .map(type => {
        switch (type) {
          case "css":
            return "CSS";
          case "ua":
            return "user-agent string";
        }
        return "";
      })
      .filter(n => n)
      .join(" and ") || "interventions";
  console.info(
    `Custom ${interventions} being applied for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`
  );
}
