/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState } from "react";
import { Localized } from "./MSLocalized";
import { AddonsPicker } from "./AddonsPicker";
import { SingleSelect } from "./SingleSelect";
import { MobileDownloads } from "./MobileDownloads";
import { MultiSelect } from "./MultiSelect";
import { EmbeddedMigrationWizard } from "./EmbeddedMigrationWizard";
import { ActionChecklist } from "./ActionChecklist";
import { EmbeddedBrowser } from "./EmbeddedBrowser";
import { AboutWelcomeUtils } from "../lib/aboutwelcome-utils.mjs";

const HEADER_STYLES = [
  "backgroundColor",
  "border",
  "padding",
  "margin",
  "width",
  "height",
];

export const ContentTiles = props => {
  const { content } = props;
  const [expandedTileIndex, setExpandedTileIndex] = useState(null);
  const { tiles } = content;
  if (!tiles) {
    return null;
  }

  const toggleTile = (index, tile) => {
    const tileId = `${tile.type}${tile.id ? "_" : ""}${tile.id ?? ""}_header`;
    setExpandedTileIndex(prevIndex => (prevIndex === index ? null : index));
    AboutWelcomeUtils.sendActionTelemetry(props.messageId, tileId);
  };

  const renderContentTile = (tile, index = 0) => {
    const isExpanded = expandedTileIndex === index;
    const { header } = tile;

    return (
      <div key={index} className="content-tile">
        {header?.title && (
          <button
            className="tile-header"
            onClick={() => toggleTile(index, tile)}
            aria-expanded={isExpanded}
            aria-controls={`tile-content-${index}`}
            style={AboutWelcomeUtils.getValidStyle(header.style, HEADER_STYLES)}
          >
            <Localized text={header}>
              <span className="header-title">{header.title}</span>
            </Localized>
            <Localized text={header}>
              {header.subtitle && (
                <span className="header-subtitle">{header.subtitle}</span>
              )}
            </Localized>
          </button>
        )}
        {isExpanded || !header ? (
          <div className="tile-content" id={`tile-content-${index}`}>
            {tile.type === "addons-picker" && tile.data && (
              <AddonsPicker
                content={{ tiles: tile }}
                installedAddons={props.installedAddons}
                message_id={props.messageId}
                handleAction={props.handleAction}
              />
            )}
            {["theme", "single-select"].includes(tile.type) && tile.data && (
              <SingleSelect
                content={{ tiles: tile }}
                activeTheme={props.activeTheme}
                handleAction={props.handleAction}
                activeSingleSelect={props.activeSingleSelect}
                setActiveSingleSelect={props.setActiveSingleSelect}
              />
            )}
            {tile.type === "mobile_downloads" && tile.data && (
              <MobileDownloads
                data={tile.data}
                handleAction={props.handleAction}
              />
            )}
            {tile.type === "multiselect" && tile.data && (
              <MultiSelect
                content={{ tiles: tile }}
                screenMultiSelects={props.screenMultiSelects}
                setScreenMultiSelects={props.setScreenMultiSelects}
                activeMultiSelect={props.activeMultiSelect}
                setActiveMultiSelect={props.setActiveMultiSelect}
              />
            )}
            {tile.type === "migration-wizard" && (
              <EmbeddedMigrationWizard
                handleAction={props.handleAction}
                content={{ tiles: tile }}
              />
            )}
            {tile.type === "action_checklist" && tile.data && (
              <ActionChecklist content={content} message_id={props.messageId} />
            )}
            {tile.type === "embedded_browser" && tile.data?.url && (
              <EmbeddedBrowser url={tile.data.url} style={tile.data.style} />
            )}
          </div>
        ) : null}
      </div>
    );
  };

  if (Array.isArray(content.tiles)) {
    return (
      <div className="content-tiles-container">
        {content.tiles.map((tile, index) => renderContentTile(tile, index))}
      </div>
    );
  }
  // If tiles is not an array render the tile alone without a container
  return renderContentTile(tiles, 0);
};
