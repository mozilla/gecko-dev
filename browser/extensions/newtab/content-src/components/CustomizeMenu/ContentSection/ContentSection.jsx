/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { actionCreators as ac } from "common/Actions.mjs";
import { SectionsMgmtPanel } from "../SectionsMgmtPanel/SectionsMgmtPanel";
import { WallpaperCategories } from "../../WallpaperCategories/WallpaperCategories";

export class ContentSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onPreferenceSelect = this.onPreferenceSelect.bind(this);

    // Refs are necessary for dynamically measuring drawer heights for slide animations
    this.topSitesDrawerRef = React.createRef();
    this.pocketDrawerRef = React.createRef();
  }

  inputUserEvent(eventSource, eventValue) {
    this.props.dispatch(
      ac.UserEvent({
        event: "PREF_CHANGED",
        source: eventSource,
        value: { status: eventValue, menu_source: "CUSTOMIZE_MENU" },
      })
    );
  }

  onPreferenceSelect(e) {
    // eventSource: WEATHER | TOP_SITES | TOP_STORIES
    const { preference, eventSource } = e.target.dataset;
    let value;
    if (e.target.nodeName === "SELECT") {
      value = parseInt(e.target.value, 10);
    } else if (e.target.nodeName === "INPUT") {
      value = e.target.checked;
      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    } else if (e.target.nodeName === "MOZ-TOGGLE") {
      value = e.target.pressed;
      if (eventSource) {
        this.inputUserEvent(eventSource, value);
      }
    }
    this.props.setPref(preference, value);
  }

  componentDidMount() {
    this.setDrawerMargins();
  }

  componentDidUpdate() {
    this.setDrawerMargins();
  }

  setDrawerMargins() {
    this.setDrawerMargin(
      `TOP_SITES`,
      this.props.enabledSections.topSitesEnabled
    );
    this.setDrawerMargin(
      `TOP_STORIES`,
      this.props.enabledSections.pocketEnabled
    );
  }

  setDrawerMargin(drawerID, isOpen) {
    let drawerRef;

    if (drawerID === `TOP_SITES`) {
      drawerRef = this.topSitesDrawerRef.current;
    } else if (drawerID === `TOP_STORIES`) {
      drawerRef = this.pocketDrawerRef.current;
    } else {
      return;
    }

    if (drawerRef) {
      let drawerHeight =
        parseFloat(window.getComputedStyle(drawerRef)?.height) || 0;

      if (isOpen) {
        drawerRef.style.marginTop = "var(--space-large)";
      } else {
        drawerRef.style.marginTop = `-${drawerHeight + 3}px`;
      }
    }
  }

  render() {
    const {
      enabledSections,
      pocketRegion,
      mayHaveInferredPersonalization,
      mayHaveRecentSaves,
      mayHaveWeather,
      mayHaveTrendingSearch,
      openPreferences,
      wallpapersEnabled,
      activeWallpaper,
      setPref,
      mayHaveTopicSections,
      exitEventFired,
    } = this.props;
    const {
      topSitesEnabled,
      pocketEnabled,
      weatherEnabled,
      trendingSearchEnabled,
      showInferredPersonalizationEnabled,
      showRecentSavesEnabled,
      topSitesRowsCount,
    } = enabledSections;

    return (
      <div className="home-section">
        {wallpapersEnabled && (
          <>
            <div className="wallpapers-section">
              <WallpaperCategories
                setPref={setPref}
                activeWallpaper={activeWallpaper}
                exitEventFired={exitEventFired}
              />
            </div>
            <span className="divider" role="separator"></span>
          </>
        )}
        <div className="settings-toggles">
          {mayHaveWeather && (
            <div id="weather-section" className="section">
              <moz-toggle
                id="weather-toggle"
                pressed={weatherEnabled || null}
                onToggle={this.onPreferenceSelect}
                data-preference="showWeather"
                data-eventSource="WEATHER"
                data-l10n-id="newtab-custom-weather-toggle"
              />
            </div>
          )}

          {mayHaveTrendingSearch && (
            <div id="trending-search-section" className="section">
              <moz-toggle
                id="trending-search-toggle"
                pressed={trendingSearchEnabled || null}
                onToggle={this.onPreferenceSelect}
                data-preference="trendingSearch.enabled"
                data-eventSource="TRENDING_SEARCH"
                data-l10n-id="newtab-custom-trending-search-toggle"
              />
            </div>
          )}

          <div id="shortcuts-section" className="section">
            <moz-toggle
              id="shortcuts-toggle"
              pressed={topSitesEnabled || null}
              onToggle={this.onPreferenceSelect}
              data-preference="feeds.topsites"
              data-eventSource="TOP_SITES"
              data-l10n-id="newtab-custom-shortcuts-toggle"
            >
              <div slot="nested">
                <div className="more-info-top-wrapper">
                  <div
                    className="more-information"
                    ref={this.topSitesDrawerRef}
                  >
                    <select
                      id="row-selector"
                      className="selector"
                      name="row-count"
                      data-preference="topSitesRows"
                      value={topSitesRowsCount}
                      onChange={this.onPreferenceSelect}
                      disabled={!topSitesEnabled}
                      aria-labelledby="custom-shortcuts-title"
                    >
                      <option
                        value="1"
                        data-l10n-id="newtab-custom-row-selector"
                        data-l10n-args='{"num": 1}'
                      />
                      <option
                        value="2"
                        data-l10n-id="newtab-custom-row-selector"
                        data-l10n-args='{"num": 2}'
                      />
                      <option
                        value="3"
                        data-l10n-id="newtab-custom-row-selector"
                        data-l10n-args='{"num": 3}'
                      />
                      <option
                        value="4"
                        data-l10n-id="newtab-custom-row-selector"
                        data-l10n-args='{"num": 4}'
                      />
                    </select>
                  </div>
                </div>
              </div>
            </moz-toggle>
          </div>

          {pocketRegion && (
            <div id="pocket-section" className="section">
              <moz-toggle
                id="pocket-toggle"
                pressed={pocketEnabled || null}
                onToggle={this.onPreferenceSelect}
                aria-describedby="custom-pocket-subtitle"
                data-preference="feeds.section.topstories"
                data-eventSource="TOP_STORIES"
                data-l10n-id="newtab-custom-stories-toggle"
              >
                <div slot="nested">
                  {(mayHaveRecentSaves ||
                    mayHaveInferredPersonalization ||
                    mayHaveTopicSections) && (
                    <div className="more-info-pocket-wrapper">
                      <div
                        className="more-information"
                        ref={this.pocketDrawerRef}
                      >
                        {mayHaveInferredPersonalization && (
                          <div className="check-wrapper" role="presentation">
                            <input
                              id="inferred-personalization"
                              className="customize-menu-checkbox"
                              disabled={!pocketEnabled}
                              checked={showInferredPersonalizationEnabled}
                              type="checkbox"
                              onChange={this.onPreferenceSelect}
                              data-preference="discoverystream.sections.personalization.inferred.user.enabled"
                              data-eventSource="INFERRED_PERSONALIZATION"
                            />
                            <label
                              className="customize-menu-checkbox-label"
                              htmlFor="inferred-personalization"
                            >
                              Recommendations inferred from your activity with
                              the feed
                            </label>
                          </div>
                        )}
                        {mayHaveTopicSections && (
                          <SectionsMgmtPanel exitEventFired={exitEventFired} />
                        )}
                        {mayHaveRecentSaves && (
                          <div className="check-wrapper" role="presentation">
                            <input
                              id="recent-saves-pocket"
                              className="customize-menu-checkbox"
                              disabled={!pocketEnabled}
                              checked={showRecentSavesEnabled}
                              type="checkbox"
                              onChange={this.onPreferenceSelect}
                              data-preference="showRecentSaves"
                              data-eventSource="POCKET_RECENT_SAVES"
                            />
                            <label
                              className="customize-menu-checkbox-label"
                              htmlFor="recent-saves-pocket"
                              data-l10n-id="newtab-custom-pocket-show-recent-saves"
                            />
                          </div>
                        )}
                      </div>
                    </div>
                  )}
                </div>
              </moz-toggle>
            </div>
          )}
        </div>

        <span className="divider" role="separator"></span>

        <div>
          <button
            id="settings-link"
            className="external-link"
            onClick={openPreferences}
            data-l10n-id="newtab-custom-settings"
          />
        </div>
      </div>
    );
  }
}
