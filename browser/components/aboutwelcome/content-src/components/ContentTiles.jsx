/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
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

const TILE_STYLES = [
  "marginBlock",
  "marginInline",
  "paddingBlock",
  "paddingInline",
];

export const ContentTiles = props => {
  const { content } = props;
  const [expandedTileIndex, setExpandedTileIndex] = useState(null);
  // State for header that toggles showing and hiding all tiles, if applicable
  const [tilesHeaderExpanded, setTilesHeaderExpanded] = useState(false);
  const { tiles } = content;
  if (!tiles) {
    return null;
  }

  // eslint-disable-next-line react-hooks/rules-of-hooks
  useEffect(() => {
    // Run once when ContentTiles mounts to prefill activeMultiSelect
    if (!props.activeMultiSelect) {
      const newActiveMultiSelect = [];
      const tilesArray = Array.isArray(tiles) ? tiles : [tiles];

      tilesArray.forEach(tile => {
        if (tile.type !== "multiselect" || !tile.data) {
          return;
        }
        tile.data.forEach(({ id, defaultValue }) => {
          if (defaultValue && id) {
            newActiveMultiSelect.push(id);
          }
        });
      });
      props.setActiveMultiSelect(newActiveMultiSelect);
    }
  }, [tiles]); // eslint-disable-line react-hooks/exhaustive-deps

  const toggleTile = (index, tile) => {
    const tileId = `${tile.type}${tile.id ? "_" : ""}${tile.id ?? ""}_header`;
    setExpandedTileIndex(prevIndex => (prevIndex === index ? null : index));
    AboutWelcomeUtils.sendActionTelemetry(props.messageId, tileId);
  };

  const toggleTiles = () => {
    setTilesHeaderExpanded(prev => !prev);
    AboutWelcomeUtils.sendActionTelemetry(
      props.messageId,
      "content_tiles_header"
    );
  };

  const renderContentTile = (tile, index = 0) => {
    const isExpanded = expandedTileIndex === index;
    const { header, title, subtitle } = tile;

    return (
      <div
        key={index}
        className={`content-tile ${header ? "has-header" : ""}`}
        style={AboutWelcomeUtils.getValidStyle(tile.style, TILE_STYLES)}
      >
        {header?.title && (
          <button
            className="tile-header secondary"
            onClick={() => toggleTile(index, tile)}
            aria-expanded={isExpanded}
            aria-controls={`tile-content-${index}`}
            style={AboutWelcomeUtils.getValidStyle(header.style, HEADER_STYLES)}
          >
            <div className="header-text-container">
              <Localized text={header.title}>
                <span className="header-title" />
              </Localized>
              {header.subtitle && (
                <Localized text={header.subtitle}>
                  <span className="header-subtitle" />
                </Localized>
              )}
            </div>
            <div className="arrow-icon"></div>
          </button>
        )}
        {(title || subtitle) && (
          <div
            className="tile-title-container"
            id={`tile-title-container-${index}`}
          >
            {title && (
              <Localized text={title}>
                {/* H1 content is provided by Localized */}
                {/* eslint-disable-next-line jsx-a11y/heading-has-content */}
                <h1 className="tile-title" id={`content-tile-title-${index}`} />
              </Localized>
            )}

            {subtitle && (
              <Localized text={subtitle}>
                <p className="tile-subtitle" />
              </Localized>
            )}
          </div>
        )}
        {isExpanded || !header ? (
          <div className="tile-content" id={`tile-content-${index}`}>
            {tile.type === "addons-picker" && tile.data && (
              <AddonsPicker
                content={{ tiles: tile }}
                installedAddons={props.installedAddons}
                message_id={props.messageId}
                handleAction={props.handleAction}
                layout={content.position}
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

  const renderContentTiles = () => {
    if (Array.isArray(tiles)) {
      return (
        <div id="content-tiles-container">
          {tiles.map((tile, index) => renderContentTile(tile, index))}
        </div>
      );
    }
    // If tiles is not an array render the tile alone without a container
    return renderContentTile(tiles, 0);
  };

  if (content.tiles_header) {
    return (
      <React.Fragment>
        <button
          className="content-tiles-header secondary"
          onClick={toggleTiles}
          aria-expanded={tilesHeaderExpanded}
          aria-controls={`content-tiles-container`}
        >
          <Localized text={content.tiles_header.title}>
            <span className="header-title" />
          </Localized>
          <div className="arrow-icon"></div>
        </button>
        {tilesHeaderExpanded && renderContentTiles()}
      </React.Fragment>
    );
  }
  return renderContentTiles(tiles);
};
