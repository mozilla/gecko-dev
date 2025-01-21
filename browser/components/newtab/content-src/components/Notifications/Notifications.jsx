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

  /**
   * Syncs {@link toastQueue} array so it can be used to
   * remove the toasts wrapper if there are none after a
   * toast is auto-hidden (animated out) via CSS.
   */
  const syncHiddenToastData = useCallback(() => {
    const toastId = toastQueue[toastQueue.length - 1];
    const queuedToasts = [...toastQueue].slice(1);
    dispatch(
      ac.OnlyToOneContent(
        {
          type: at.HIDE_TOAST_MESSAGE,
          data: {
            toastQueue: queuedToasts,
            toastCounter: queuedToasts.length,
            toastId,
            showNotifications: false,
          },
        },
        "ActivityStream:Content"
      )
    );
  }, [dispatch, toastQueue]);

  const getToast = useCallback(() => {
    // Note: This architecture could expand to support multiple toast notifications at once
    const latestToastItem = toastQueue[toastQueue.length - 1];

    switch (latestToastItem) {
      case "thumbsDownToast":
        return (
          <ThumbsDownToast
            onDismissClick={syncHiddenToastData}
            onAnimationEnd={syncHiddenToastData}
            key={toastCounter}
          />
        );
      case "thumbsUpToast":
        return (
          <ThumbsUpToast
            onDismissClick={syncHiddenToastData}
            onAnimationEnd={syncHiddenToastData}
            key={toastCounter}
          />
        );
      default:
        throw new Error("No toast found");
    }
  }, [syncHiddenToastData, toastCounter, toastQueue]);

  useEffect(() => {
    getToast();
  }, [toastQueue, getToast]);

  return toastQueue.length ? (
    <div className="notification-wrapper">
      <ul className="notification-feed">{getToast()}</ul>
    </div>
  ) : (
    ""
  );
}

export { Notifications };
