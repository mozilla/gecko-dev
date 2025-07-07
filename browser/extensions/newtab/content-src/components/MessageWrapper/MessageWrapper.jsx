/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useCallback, useEffect, useState } from "react";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { useSelector } from "react-redux";
import { useIntersectionObserver } from "../../lib/utils";

// Note: MessageWrapper emits events via submitGleanPingForPing() in the OMC messaging-system.
// If a feature is triggered outside of this flow (e.g., the Mobile Download QR Promo),
// it should emit New Tab-specific Glean events independently.

function MessageWrapper({ children, dispatch, hiddenOverride, onDismiss }) {
  const message = useSelector(state => state.Messages);
  const [isIntersecting, setIsIntersecting] = useState(false);
  const [tabIsVisible, setTabIsVisible] = useState(
    () =>
      typeof document !== "undefined" && document.visibilityState === "visible"
  );
  const [hasRun, setHasRun] = useState();

  const handleIntersection = useCallback(() => {
    setIsIntersecting(true);
    // only send impression if messageId is defined and tab is visible
    if (tabIsVisible && message.messageData.id && !hasRun) {
      setHasRun(true);
      dispatch(
        ac.AlsoToMain({
          type: at.MESSAGE_IMPRESSION,
          data: message.messageData,
        })
      );
    }
  }, [dispatch, message, tabIsVisible, hasRun]);

  useEffect(() => {
    // we dont want to dispatch this action unless the current tab is open and visible
    if (message.isVisible && tabIsVisible) {
      dispatch(
        ac.AlsoToMain({
          type: at.MESSAGE_NOTIFY_VISIBILITY,
          data: true,
        })
      );
    }
  }, [message, dispatch, tabIsVisible]);

  useEffect(() => {
    const handleVisibilityChange = () => {
      setTabIsVisible(document.visibilityState === "visible");
    };

    document.addEventListener("visibilitychange", handleVisibilityChange);
    return () => {
      document.removeEventListener("visibilitychange", handleVisibilityChange);
    };
  }, []);

  const ref = useIntersectionObserver(handleIntersection);

  const handleClose = useCallback(() => {
    const action = {
      type: at.MESSAGE_TOGGLE_VISIBILITY,
      data: false, //isVisible
    };
    if (message.portID) {
      dispatch(ac.OnlyToOneContent(action, message.portID));
    } else {
      dispatch(ac.AlsoToMain(action));
    }
    dispatch(
      ac.AlsoToMain({
        type: at.MESSAGE_NOTIFY_VISIBILITY,
        data: false,
      })
    );
    onDismiss?.();
  }, [dispatch, message, onDismiss]);

  function handleDismiss() {
    const { id } = message.messageData;
    if (id) {
      dispatch(
        ac.OnlyToMain({
          type: at.MESSAGE_DISMISS,
          data: { message: message.messageData },
        })
      );
    }
    handleClose();
  }

  function handleBlock() {
    const { id } = message.messageData;
    if (id) {
      dispatch(
        ac.OnlyToMain({
          type: at.MESSAGE_BLOCK,
          data: id,
        })
      );
    }
  }

  function handleClick(elementId) {
    const { id } = message.messageData;
    if (id) {
      dispatch(
        ac.OnlyToMain({
          type: at.MESSAGE_CLICK,
          data: { message: message.messageData, source: elementId || "" },
        })
      );
    }
  }

  if (!message || (!hiddenOverride && !message.isVisible)) {
    return null;
  }

  // only display the message if `isVisible` is true
  return (
    <div
      ref={el => {
        ref.current = [el];
      }}
      className="message-wrapper"
    >
      {React.cloneElement(children, {
        isIntersecting,
        handleDismiss,
        handleClick,
        handleBlock,
        handleClose,
      })}
    </div>
  );
}

export { MessageWrapper };
