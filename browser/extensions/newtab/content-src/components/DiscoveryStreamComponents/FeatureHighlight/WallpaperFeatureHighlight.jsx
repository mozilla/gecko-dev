/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FeatureHighlight } from "./FeatureHighlight";
import React, { useCallback } from "react";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

export function WallpaperFeatureHighlight({
  position,
  dispatch,
  handleDismiss,
  handleClose,
  handleClick,
}) {
  const onToggleClick = useCallback(
    elementId => {
      dispatch({ type: at.SHOW_PERSONALIZE });
      dispatch(ac.UserEvent({ event: "SHOW_PERSONALIZE" }));
      handleClick(elementId);
      handleDismiss();
    },
    [dispatch, handleDismiss, handleClick]
  );

  return (
    <div className="wallpaper-feature-highlight">
      <FeatureHighlight
        position={position}
        data-l10n-id="feature-highlight-wallpaper"
        feature="FEATURE_HIGHLIGHT_WALLPAPER"
        dispatch={dispatch}
        message={
          <div className="wallpaper-feature-highlight-content">
            <img
              src="chrome://newtab/content/data/content/assets/custom-wp-highlight.png"
              alt=""
              width="320"
              height="195"
            />
            <p className="title" data-l10n-id="newtab-custom-wallpaper-title" />
            <p
              className="subtitle"
              data-l10n-id="newtab-custom-wallpaper-subtitle"
            />
            <span className="button-wrapper">
              <moz-button
                type="default"
                onClick={() => onToggleClick("open-customize-menu")}
                data-l10n-id="newtab-custom-wallpaper-cta"
              />
            </span>
          </div>
        }
        toggle={<div className="icon icon-help"></div>}
        openedOverride={true}
        showButtonIcon={false}
        dismissCallback={handleDismiss}
        outsideClickCallback={handleClose}
      />
    </div>
  );
}
