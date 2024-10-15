/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function Logo() {
  return (
    <h1 className="logo-and-wordmark-wrapper">
      <div
        className="logo-and-wordmark"
        role="img"
        data-l10n-id="newtab-logo-and-wordmark"
      >
        <div className="logo" />
        <div className="wordmark" />
      </div>
    </h1>
  );
}

export { Logo };
