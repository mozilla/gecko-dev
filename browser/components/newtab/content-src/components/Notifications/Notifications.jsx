/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useCallback, useEffect } from "react";
import { useSelector } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { ThumbsUpToast } from "./Toasts/ThumbsUpToast";
import { ThumbsDownToast } from "./Toasts/ThumbsDownToast";

function Notifications({ dispatch }) {
  const toastQueue = useSelector(state => state.Notifications.toastQueue);
  const toastCounter = useSelector(state => state.Notifications.toastCounter);

  const onDismissClick = useCallback(() => {
    dispatch(
      ac.OnlyToOneContent(
        {
          type: at.HIDE_TOAST_MESSAGE,
          data: {
            showNotifications: false,
          },
        },
        "ActivityStream:Content"
      )
    );
  }, [dispatch]);

  const getToast = useCallback(() => {
    // Note: This architecture could expand to support multiple toast notifications at once
    const latestToastItem = toastQueue[toastQueue.length - 1];

    switch (latestToastItem) {
      case "thumbsDownToast":
        return (
          <ThumbsDownToast onDismissClick={onDismissClick} key={toastCounter} />
        );
      case "thumbsUpToast":
        return (
          <ThumbsUpToast onDismissClick={onDismissClick} key={toastCounter} />
        );
      default:
        throw new Error("No toast found");
    }
  }, [onDismissClick, toastCounter, toastQueue]);

  useEffect(() => {
    getToast();
  }, [toastQueue, getToast]);

  return (
    <div className="notification-wrapper">
      <ul className="notification-feed">{getToast()}</ul>
    </div>
  );
}

export { Notifications };
