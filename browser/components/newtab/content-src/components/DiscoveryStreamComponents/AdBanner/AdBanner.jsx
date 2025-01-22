/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

export const AdBanner = ({
  spoc,
  dispatch,
  firstVisibleTimestamp,
  row,
  type,
}) => {
  const getDimensions = format => {
    switch (format) {
      case "leaderboard":
        return {
          width: "728",
          height: "90",
        };
      case "billboard":
        return {
          width: "970",
          height: "250",
        };
    }
    return {
      // image will still render with default values
      width: undefined,
      height: undefined,
    };
  };

  const { width: imgWidth, height: imgHeight } = getDimensions(spoc.format);

  const handleDismissClick = () => {
    dispatch(
      ac.AlsoToMain({
        type: at.BLOCK_URL,
        data: [
          {
            block_key: spoc.block_key,
            fetchTimestamp: spoc.fetchTimestamp,
            flight_id: spoc.flight_id,
            format: spoc.format,
            id: spoc.id,
            card_type: "spoc",
            is_pocket_card: true,
            position: row,
            sponsor: spoc.sponsor,
            title: spoc.title,
            url: spoc.url || spoc.shim.url,
            personalization_models: spoc.personalization_models,
            priority: spoc.priority,
            score: spoc.score,
            alt_text: spoc.alt_text,
          },
        ],
      })
    );
  };

  const onLinkClick = () => {
    dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: type.toUpperCase(),
        // Banner ads dont have a position, but a row number
        action_position: row,
        value: {
          card_type: "spoc",
          tile_id: spoc.id,
          ...(spoc.shim?.click ? { shim: spoc.shim.click } : {}),
          fetchTimestamp: spoc.fetchTimestamp,
          firstVisibleTimestamp,
          format: spoc.format,
        },
      })
    );
  };

  // in the default card grid 1 would come before the 1st row of cards and 9 comes after the last row
  // using clamp to make sure its between valid values (1-9)
  const clampedRow = Math.max(1, Math.min(9, row));

  return (
    <aside className="ad-banner-wrapper" style={{ gridRow: clampedRow }}>
      <div className={`ad-banner-inner ${spoc.format}`}>
        <div className="ad-banner-dismiss">
          <button
            className="icon icon-dismiss"
            onClick={handleDismissClick}
            data-l10n-id="newtab-toast-dismiss-button"
          ></button>
        </div>
        <SafeAnchor
          className="ad-banner-link"
          url={spoc.url}
          title={spoc.title}
          onLinkClick={onLinkClick}
          dispatch={dispatch}
        >
          <ImpressionStats
            flightId={spoc.flight_id}
            rows={[
              {
                id: spoc.id,
                card_type: "spoc",
                pos: row,
                recommended_at: spoc.recommended_at,
                received_rank: spoc.received_rank,
                format: spoc.format,
              },
            ]}
            dispatch={dispatch}
            firstVisibleTimestamp={firstVisibleTimestamp}
          />
          <div className="ad-banner-content">
            <img
              src={spoc.raw_image_src}
              alt={spoc.alt_text}
              loading="eager"
              width={imgWidth}
              height={imgHeight}
            />
          </div>
        </SafeAnchor>
        <div className="ad-banner-sponsored">
          <span
            className="ad-banner-sponsored-label"
            data-l10n-id="newtab-topsite-sponsored"
          />
        </div>
      </div>
    </aside>
  );
};
