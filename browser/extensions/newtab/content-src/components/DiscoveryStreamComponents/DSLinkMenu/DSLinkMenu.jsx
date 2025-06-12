/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { actionCreators as ac } from "common/Actions.mjs";
import React from "react";
import { connect } from "react-redux";

export class _DSLinkMenu extends React.PureComponent {
  render() {
    const { index, dispatch } = this.props;
    let TOP_STORIES_CONTEXT_MENU_OPTIONS;
    const PREF_REPORT_ADS_ENABLED = "discoverystream.reportAds.enabled";
    const prefs = this.props.Prefs.values;
    const showAdsReporting = prefs[PREF_REPORT_ADS_ENABLED];
    const isSpoc = this.props.card_type === "spoc";

    if (isSpoc) {
      TOP_STORIES_CONTEXT_MENU_OPTIONS = [
        "BlockUrl",
        ...(showAdsReporting ? ["ReportAd"] : []),
        "ManageSponsoredContent",
        "OurSponsorsAndYourPrivacy",
      ];
    } else {
      TOP_STORIES_CONTEXT_MENU_OPTIONS = [
        "CheckBookmark",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
        ...(this.props.section ? ["ReportContent"] : []),
      ];
    }

    const type = this.props.type || "DISCOVERY_STREAM";
    const title = this.props.title || this.props.source;

    return (
      <div className="context-menu-position-container">
        <ContextMenuButton
          tooltip={"newtab-menu-content-tooltip"}
          tooltipArgs={{ title }}
          onUpdate={this.props.onMenuUpdate}
        >
          <LinkMenu
            dispatch={dispatch}
            index={index}
            source={type.toUpperCase()}
            onShow={this.props.onMenuShow}
            options={TOP_STORIES_CONTEXT_MENU_OPTIONS}
            shouldSendImpressionStats={true}
            userEvent={ac.DiscoveryStreamUserEvent}
            site={{
              referrer: "https://getpocket.com/recommendations",
              title: this.props.title,
              type: this.props.type,
              url: this.props.url,
              guid: this.props.id,
              pocket_id: this.props.pocket_id,
              card_type: this.props.card_type,
              shim: this.props.shim,
              bookmarkGuid: this.props.bookmarkGuid,
              flight_id: this.props.flightId,
              tile_id: this.props.tile_id,
              recommendation_id: this.props.recommendation_id,
              corpus_item_id: this.props.corpus_item_id,
              scheduled_corpus_item_id: this.props.scheduled_corpus_item_id,
              firstVisibleTimestamp: this.props.firstVisibleTimestamp,
              recommended_at: this.props.recommended_at,
              received_rank: this.props.received_rank,
              topic: this.props.topic,
              is_list_card: this.props.is_list_card,
              position: index,
              ...(this.props.format ? { format: this.props.format } : {}),
              ...(this.props.section
                ? {
                    section: this.props.section,
                    section_position: this.props.section_position,
                    is_section_followed: this.props.is_section_followed,
                  }
                : {}),
            }}
          />
        </ContextMenuButton>
      </div>
    );
  }
}

export const DSLinkMenu = connect(state => ({
  Prefs: state.Prefs,
}))(_DSLinkMenu);
