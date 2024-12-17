/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { actionCreators as ac } from "common/Actions.mjs";
import React from "react";

export class DSLinkMenu extends React.PureComponent {
  render() {
    const { index, dispatch } = this.props;
    let pocketMenuOptions = [];
    let TOP_STORIES_CONTEXT_MENU_OPTIONS = [
      "OpenInNewWindow",
      "OpenInPrivateWindow",
    ];
    if (!this.props.isRecentSave) {
      if (this.props.pocket_button_enabled) {
        pocketMenuOptions = this.props.saveToPocketCard
          ? ["CheckDeleteFromPocket"]
          : ["CheckSavedToPocket"];
      }
      // Override pocketMenuOptions to add Save to Pocket btn link to all section cards
      if (this.props.isSectionsCard) {
        pocketMenuOptions = ["CheckSavedToPocket"];
      }
      TOP_STORIES_CONTEXT_MENU_OPTIONS = [
        "CheckBookmark",
        "CheckArchiveFromPocket",
        ...pocketMenuOptions,
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
        ...(this.props.showPrivacyInfo ? ["ShowPrivacyInfo"] : []),
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
              recommended_at: this.props.recommended_at,
              received_rank: this.props.received_rank,
              is_list_card: this.props.is_list_card,
              ...(this.props.format ? { format: this.props.format } : {}),
              ...(this.props.section
                ? {
                    section: this.props.section,
                    section_position: this.props.section_position,
                    is_secton_followed: this.props.is_secton_followed,
                  }
                : {}),
            }}
          />
        </ContextMenuButton>
      </div>
    );
  }
}
