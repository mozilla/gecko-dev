/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.mjs";
import { FeatureHighlight } from "./FeatureHighlight";
import React, { useCallback, useEffect } from "react";

const FEATURE_ID = "FEATURE_FOLLOW_SECTION_BUTTON";

export function FollowSectionButtonHighlight({
  arrowPosition,
  position,
  verticalPosition,
  dispatch,
  handleDismiss,
  handleBlock,
  isIntersecting,
}) {
  const onDismiss = useCallback(() => {
    // This event is emitted manually because the feature may be triggered outside the OMC flow,
    // and may not be captured by the messaging-system’s automatic reporting.
    dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "FEATURE_HIGHLIGHT_DISMISS",
        source: "FEATURE_HIGHLIGHT",
        value: FEATURE_ID,
      })
    );

    handleDismiss();
    handleBlock();
  }, [dispatch, handleDismiss, handleBlock]);

  useEffect(() => {
    if (isIntersecting) {
      // This event is emitted manually because the feature may be triggered outside the OMC flow,
      // and may not be captured by the messaging-system’s automatic reporting.
      dispatch(
        ac.DiscoveryStreamUserEvent({
          event: "FEATURE_HIGHLIGHT_IMPRESSION",
          source: "FEATURE_HIGHLIGHT",
          value: FEATURE_ID,
        })
      );
    }
  }, [dispatch, isIntersecting]);

  return (
    <div className="follow-section-button-highlight">
      <FeatureHighlight
        position={position}
        arrowPosition={arrowPosition}
        verticalPosition={verticalPosition}
        feature={FEATURE_ID}
        dispatch={dispatch}
        message={
          <div className="follow-section-button-highlight-content">
            <img
              src="chrome://browser/content/asrouter/assets/smiling-fox-icon.svg"
              data-l10n-id="newtab-download-mobile-highlight-image"
              width="24"
              height="24"
              alt=""
            />
            <div className="follow-section-button-highlight-copy">
              <p
                className="title"
                data-l10n-id="newtab-section-follow-highlight-title"
              />
              <p
                className="subtitle"
                data-l10n-id="newtab-section-follow-highlight-subtitle"
              />
            </div>
          </div>
        }
        openedOverride={true}
        showButtonIcon={false}
        dismissCallback={onDismiss}
        outsideClickCallback={handleDismiss}
      />
    </div>
  );
}
