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
  refinedCardsLayout,
}) {
  let thumbsButtons = (
    <>
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
    </>
  );

  if (refinedCardsLayout) {
    thumbsButtons = (
      <>
        <moz-button
          iconsrc="chrome://global/skin/icons/thumbs-up-20.svg"
          onClick={onThumbsUpClick}
          className={`card-stp-thumbs-button icon icon-thumbs-up refined-layout ${
            isThumbsUpActive ? "is-active" : null
          }`}
          data-l10n-id="newtab-pocket-thumbs-up-tooltip"
          type="icon ghost"
        ></moz-button>
        <moz-button
          iconsrc="chrome://global/skin/icons/thumbs-down-20.svg"
          onClick={onThumbsDownClick}
          className={`card-stp-thumbs-button icon icon-thumbs-down ${
            isThumbsDownActive ? "is-active" : null
          }`}
          data-l10n-id="newtab-pocket-thumbs-down-tooltip"
          type="icon ghost"
        ></moz-button>
      </>
    );
  }
  return (
    <div className="card-stp-thumbs-buttons-wrapper">
      {/* Only show to non-sponsored content */}
      {!sponsor && (
        <div className="card-stp-thumbs-buttons">{thumbsButtons}</div>
      )}
    </div>
  );
}

export { DSThumbsUpDownButtons };
