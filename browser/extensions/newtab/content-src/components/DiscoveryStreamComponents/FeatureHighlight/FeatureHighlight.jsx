/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useCallback, useRef, useEffect } from "react";
import { actionCreators as ac } from "common/Actions.mjs";

export function FeatureHighlight({
  message,
  icon,
  toggle,
  arrowPosition = "",
  position = "top-left",
  verticalPosition = "",
  title,
  ariaLabel,
  feature = "FEATURE_HIGHLIGHT_DEFAULT",
  dispatch = () => {},
  windowObj = global,
  openedOverride = false,
  showButtonIcon = true,
  dismissCallback = () => {},
  outsideClickCallback = () => {},
}) {
  const [opened, setOpened] = useState(openedOverride);
  const ref = useRef(null);

  useEffect(() => {
    const handleOutsideClick = e => {
      if (!ref?.current?.contains(e.target)) {
        setOpened(false);
        outsideClickCallback();
      }
    };

    const handleKeyDown = e => {
      if (e.key === "Escape") {
        outsideClickCallback();
      }
    };

    windowObj.document.addEventListener("click", handleOutsideClick);
    windowObj.document.addEventListener("keydown", handleKeyDown);
    return () => {
      windowObj.document.removeEventListener("click", handleOutsideClick);
      windowObj.document.removeEventListener("keydown", handleKeyDown);
    };
  }, [windowObj, outsideClickCallback]);

  const onToggleClick = useCallback(() => {
    if (!opened) {
      dispatch(
        ac.DiscoveryStreamUserEvent({
          event: "CLICK",
          source: "FEATURE_HIGHLIGHT",
          value: {
            feature,
          },
        })
      );
    }
    setOpened(!opened);
  }, [dispatch, feature, opened]);

  const onDismissClick = useCallback(() => {
    setOpened(false);
    dismissCallback();
  }, [dismissCallback]);

  const hideButtonClass = showButtonIcon ? `` : `isHidden`;
  const openedClassname = opened ? `opened` : `closed`;
  return (
    <div ref={ref} className={`feature-highlight ${verticalPosition}`}>
      <button
        title={title}
        aria-haspopup="true"
        aria-label={ariaLabel}
        className={`toggle-button ${hideButtonClass}`}
        onClick={onToggleClick}
      >
        {toggle}
      </button>
      <div
        className={`feature-highlight-modal ${position} ${arrowPosition} ${openedClassname}`}
      >
        <div className="message-icon">{icon}</div>
        <p className="content-wrapper">{message}</p>
        <button
          data-l10n-id="feature-highlight-dismiss-button"
          className="icon icon-dismiss"
          onClick={onDismissClick}
        ></button>
      </div>
    </div>
  );
}
