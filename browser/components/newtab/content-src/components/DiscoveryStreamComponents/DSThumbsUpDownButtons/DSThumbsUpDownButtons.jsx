/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function DSThumbsUpDownButtons({
  sponsor,
  onThumbsUpClick,
  onThumbsDownClick,
  isThumbsUpActive,
  isThumbsDownActive,
}) {
  return (
    <div className="card-stp-thumbs-buttons-wrapper">
      {/* Only show to non-sponsored content */}
      {!sponsor && (
        <div className="card-stp-thumbs-buttons">
          <button
            onClick={onThumbsUpClick}
            className={`card-stp-thumbs-button icon icon-thumbs-up ${
              isThumbsUpActive ? "is-active" : null
            }`}
            data-l10n-id="newtab-pocket-thumbs-up-tooltip"
          ></button>
          <button
            onClick={onThumbsDownClick}
            className={`card-stp-thumbs-button icon icon-thumbs-down ${
              isThumbsDownActive ? "is-active" : null
            }`}
            data-l10n-id="newtab-pocket-thumbs-down-tooltip"
          ></button>
        </div>
      )}
    </div>
  );
}

export { DSThumbsUpDownButtons };
