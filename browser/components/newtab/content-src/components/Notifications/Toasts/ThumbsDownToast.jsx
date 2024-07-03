/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

function ThumbsDownToast({ onDismissClick }) {
  return (
    <div className="notification-feed-item is-success">
      <div className="icon icon-check-filled icon-themed"></div>
      <div
        className="notification-feed-item-text"
        data-l10n-id="newtab-toast-thumbs-up-or-down"
      ></div>
      <button
        onClick={onDismissClick}
        className="icon icon-dismiss"
        data-l10n-id="newtab-toast-dismiss-button"
      ></button>
    </div>
  );
}

export { ThumbsDownToast };
