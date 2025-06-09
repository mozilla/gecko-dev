/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState } from "react";
import { useDispatch, useSelector } from "react-redux";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { LinkMenu } from "../../LinkMenu/LinkMenu";

const PREF_TRENDING_VARIANT = "trendingSearch.variant";

function TrendingSearches() {
  const [showContextMenu, setShowContextMenu] = useState(false);
  // The keyboard access parameter is passed down to LinkMenu component
  // that uses it to focus on the first context menu option for accessibility.
  const [isKeyboardAccess, setIsKeyboardAccess] = useState(false);
  const dispatch = useDispatch();
  const { TrendingSearch, Prefs } = useSelector(state => state);
  const { values: prefs } = Prefs;
  const { suggestions, collapsed } = TrendingSearch;
  const variant = prefs[PREF_TRENDING_VARIANT];

  const TRENDING_SEARCH_CONTEXT_MENU_OPTIONS = [
    "TrendingSearchLearnMore",
    "TrendingSearchDismiss",
  ];

  function onArrowClick() {
    dispatch(
      ac.AlsoToMain({
        type: at.TRENDING_SEARCH_TOGGLE_COLLAPSE,
        data: !collapsed,
      })
    );
  }

  const toggleContextMenu = isKeyBoard => {
    setShowContextMenu(!showContextMenu);
    setIsKeyboardAccess(isKeyBoard);
  };

  function onContextMenuClick(e) {
    e.preventDefault();
    toggleContextMenu(false);
  }

  function onContextMenuKeyDown(e) {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      toggleContextMenu(true);
    }
  }

  function onUpdate() {
    setShowContextMenu(!showContextMenu);
  }

  if (!suggestions?.length) {
    return null;
  } else if (variant === "a") {
    return (
      <section className="trending-searches-pill-wrapper">
        <div className="trending-searches-title-wrapper">
          <span className="trending-searches-icon icon icon-arrow-trending"></span>
          <h2
            className="trending-searches-title"
            data-l10n-id="newtab-trending-searches-trending-on-google"
          ></h2>
          <div className="close-open-trending-searches">
            <moz-button
              iconsrc={`chrome://global/skin/icons/arrow-${collapsed ? "down" : "up"}.svg`}
              onClick={onArrowClick}
              className={`icon icon-arrowhead-up`}
              type="icon ghost"
              data-l10n-id={`newtab-trending-searches-${collapsed ? "hide" : "show"}-trending`}
            ></moz-button>
          </div>
        </div>
        {!collapsed && (
          <ul className="trending-searches-list">
            {suggestions.map((result, index) => {
              return (
                <li key={index} className="trending-search-item">
                  <SafeAnchor url="">{result.lowerCaseSuggestion}</SafeAnchor>
                </li>
              );
            })}
          </ul>
        )}
      </section>
    );
  } else if (variant === "b") {
    return (
      <div className="trending-searches-list-view">
        <div className="trending-searches-list-view-header">
          <h3 data-l10n-id="newtab-trending-searches-trending-on-google"></h3>
          <div className="trending-searches-context-menu-wrapper">
            <div
              className={`trending-searches-context-menu ${showContextMenu ? "context-menu-open" : ""}`}
            >
              <moz-button
                type="icon ghost"
                size="default"
                data-l10n-id="newtab-menu-section-tooltip"
                iconsrc="chrome://global/skin/icons/more.svg"
                onClick={onContextMenuClick}
                onKeyDown={onContextMenuKeyDown}
              />
              {showContextMenu && (
                <LinkMenu
                  onUpdate={onUpdate}
                  dispatch={dispatch}
                  keyboardAccess={isKeyboardAccess}
                  options={TRENDING_SEARCH_CONTEXT_MENU_OPTIONS}
                  site={{
                    url: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/trending-searches-new-tab",
                  }}
                />
              )}
            </div>
          </div>
        </div>
        <ul className="trending-searches-list-items">
          {suggestions.slice(0, 6).map(result => (
            <li key={result.suggestion} className="trending-searches-list-item">
              <SafeAnchor url="" title={result.suggestion}>
                <span className="trending-searches-icon icon icon-arrow-trending"></span>
                {result.lowerCaseSuggestion}
              </SafeAnchor>
            </li>
          ))}
        </ul>
      </div>
    );
  }
}

export { TrendingSearches };
