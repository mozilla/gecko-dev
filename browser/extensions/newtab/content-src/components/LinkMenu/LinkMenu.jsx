/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.mjs";
import { connect } from "react-redux";
import { ContextMenu } from "content-src/components/ContextMenu/ContextMenu";
import { LinkMenuOptions } from "content-src/lib/link-menu-options";
import React from "react";

const DEFAULT_SITE_MENU_OPTIONS = [
  "CheckPinTopSite",
  "EditTopSite",
  "Separator",
  "OpenInNewWindow",
  "OpenInPrivateWindow",
  "Separator",
  "BlockUrl",
];

export class _LinkMenu extends React.PureComponent {
  getOptions() {
    const { props } = this;
    const {
      site,
      index,
      source,
      isPrivateBrowsingEnabled,
      siteInfo,
      platform,
      dispatch,
      options,
      shouldSendImpressionStats,
      userEvent = ac.UserEvent,
    } = props;

    // Handle special case of default site
    const propOptions =
      site.isDefault && !site.searchTopSite && !site.sponsored_position
        ? DEFAULT_SITE_MENU_OPTIONS
        : options;

    const linkMenuOptions = propOptions
      .map(o =>
        LinkMenuOptions[o](
          site,
          index,
          source,
          isPrivateBrowsingEnabled,
          siteInfo,
          platform
        )
      )
      .map(option => {
        const { action, impression, id, type, userEvent: eventName } = option;
        if (!type && id) {
          option.onClick = (event = {}) => {
            const { ctrlKey, metaKey, shiftKey, button } = event;
            // Only send along event info if there's something non-default to send
            if (ctrlKey || metaKey || shiftKey || button === 1) {
              action.data = Object.assign(
                {
                  event: { ctrlKey, metaKey, shiftKey, button },
                },
                action.data
              );
            }
            dispatch(action);
            if (eventName) {
              let value;
              // Bug 1958135: Pass additional info to ac.OPEN_NEW_WINDOW event
              if (action.type === "OPEN_NEW_WINDOW") {
                const {
                  card_type,
                  corpus_item_id,
                  event_source,
                  fetchTimestamp,
                  firstVisibleTimestamp,
                  format,
                  is_list_card,
                  is_section_followed,
                  received_rank,
                  recommendation_id,
                  recommended_at,
                  scheduled_corpus_item_id,
                  section_position,
                  section,
                  selected_topics,
                  tile_id,
                  topic,
                } = action.data;

                value = {
                  card_type,
                  corpus_item_id,
                  event_source,
                  fetchTimestamp,
                  firstVisibleTimestamp,
                  format,
                  is_list_card,
                  received_rank,
                  recommendation_id,
                  recommended_at,
                  scheduled_corpus_item_id,
                  ...(section
                    ? { is_section_followed, section_position, section }
                    : {}),
                  selected_topics: selected_topics ? selected_topics : "",
                  tile_id,
                  topic,
                };
              } else {
                value = { card_type: site.flight_id ? "spoc" : "organic" };
              }
              const userEventData = Object.assign(
                {
                  event: eventName,
                  source,
                  action_position: index,
                  value,
                },
                siteInfo
              );
              dispatch(userEvent(userEventData));
              if (impression && shouldSendImpressionStats) {
                dispatch(impression);
              }
            }
          };
        }
        return option;
      });

    // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.
    linkMenuOptions[0].first = true;
    linkMenuOptions[linkMenuOptions.length - 1].last = true;
    return linkMenuOptions;
  }

  render() {
    return (
      <ContextMenu
        onUpdate={this.props.onUpdate}
        onShow={this.props.onShow}
        options={this.getOptions()}
        keyboardAccess={this.props.keyboardAccess}
      />
    );
  }
}

const getState = state => ({
  isPrivateBrowsingEnabled: state.Prefs.values.isPrivateBrowsingEnabled,
  platform: state.Prefs.values.platform,
});
export const LinkMenu = connect(getState)(_LinkMenu);
