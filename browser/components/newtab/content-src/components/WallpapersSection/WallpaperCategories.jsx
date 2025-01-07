/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { connect } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
// eslint-disable-next-line no-shadow
import { CSSTransition } from "react-transition-group";

export class _WallpaperCategories extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.handleReset = this.handleReset.bind(this);
    this.handleCategory = this.handleCategory.bind(this);
    this.handleBack = this.handleBack.bind(this);
    this.getRGBColors = this.getRGBColors.bind(this);
    this.prefersHighContrastQuery = null;
    this.prefersDarkQuery = null;
    this.state = {
      activeCategory: null,
      activeCategoryFluentID: null,
      showColorPicker: false,
      inputType: "radio",
      activeId: null,
    };
  }

  componentDidMount() {
    this.prefersDarkQuery = globalThis.matchMedia(
      "(prefers-color-scheme: dark)"
    );
  }

  handleChange(event) {
    let { id } = event.target;

    if (id === "solid-color-picker") {
      id = `solid-color-picker-${event.target.value}`;
      const rgbColors = this.getRGBColors(event.target.value);
      event.target.style.backgroundColor = `rgb(${rgbColors.toString()})`;
      event.target.checked = true;
      this.setState({ customHexValue: event.target.style.backgroundColor });
    }

    this.props.setPref("newtabWallpapers.wallpaper-light", id);
    this.props.setPref("newtabWallpapers.wallpaper-dark", id);
    // Setting this now so when we remove v1 we don't have to migrate v1 values.
    this.props.setPref("newtabWallpapers.wallpaper", id);

    this.handleUserEvent(at.WALLPAPER_CLICK, {
      selected_wallpaper: id,
      had_previous_wallpaper: !!this.props.activeWallpaper,
    });
  }
  handleReset() {
    this.props.setPref("newtabWallpapers.wallpaper-light", "");
    this.props.setPref("newtabWallpapers.wallpaper-dark", "");
    // Setting this now so when we remove v1 we don't have to migrate v1 values.
    this.props.setPref("newtabWallpapers.wallpaper", "");
    this.handleUserEvent(at.WALLPAPER_CLICK, {
      selected_wallpaper: "none",
      had_previous_wallpaper: !!this.props.activeWallpaper,
    });
  }

  handleCategory = event => {
    this.setState({ activeCategory: event.target.id });
    this.handleUserEvent(at.WALLPAPER_CATEGORY_CLICK, event.target.id);

    let fluent_id;
    switch (event.target.id) {
      case "photographs":
        fluent_id = "newtab-wallpaper-category-title-photographs";
        break;
      case "abstracts":
        fluent_id = "newtab-wallpaper-category-title-abstract";
        break;
      case "solid-colors":
        fluent_id = "newtab-wallpaper-category-title-colors";
    }

    this.setState({ activeCategoryFluentID: fluent_id });
  };

  handleBack() {
    this.setState({ activeCategory: null });
  }

  // Record user interaction when changing wallpaper and reseting wallpaper to default
  handleUserEvent(type, data) {
    this.props.dispatch(ac.OnlyToMain({ type, data }));
  }

  setActiveId = id => {
    this.setState({ activeId: id }); // Set the active ID
  };

  getRGBColors(input) {
    if (input.length !== 7) {
      return [];
    }

    const r = parseInt(input.substr(1, 2), 16);
    const g = parseInt(input.substr(3, 2), 16);
    const b = parseInt(input.substr(5, 2), 16);

    return [r, g, b];
  }

  render() {
    const prefs = this.props.Prefs.values;
    const { wallpaperList, categories } = this.props.Wallpapers;
    const { activeWallpaper } = this.props;
    const { activeCategory, showColorPicker } = this.state;
    const { activeCategoryFluentID } = this.state;
    const filteredWallpapers = wallpaperList.filter(
      wallpaper => wallpaper.category === activeCategory
    );

    let categorySectionClassname = "category wallpaper-list";
    if (prefs["newtabWallpapers.v2.enabled"]) {
      categorySectionClassname += " ignore-color-mode";
    }

    let wallpaperCustomSolidColorHex = null;

    const wallpaperLight = prefs["newtabWallpapers.wallpaper-light"];
    const wallpaperDark = prefs["newtabWallpapers.wallpaper-dark"];

    // User has previous selected a custom color
    if (
      wallpaperLight.includes("solid-color-picker") &&
      wallpaperDark.includes("solid-color-picker")
    ) {
      this.setState({ showColorPicker: true });
      const regex = /#([a-fA-F0-9]{6})/;
      [wallpaperCustomSolidColorHex] = wallpaperLight.match(regex);
    }

    // Enable custom color select if preffed on
    if (prefs["newtabWallpapers.customColor.enabled"]) {
      this.setState({ showColorPicker: true });
    }

    let colorPickerInput =
      showColorPicker && activeCategory === "solid-colors" ? (
        <>
          <input
            onChange={this.handleChange}
            onClick={() => this.setActiveId("solid-color-picker")} //
            type="color"
            name={`wallpaper-solid-color-picker`}
            id="solid-color-picker"
            // aria-checked is not applicable for input[type="color"] elements
            aria-current={this.state.activeId === "solid-color-picker"}
            // If nothing selected, default to Zilla Green
            value={wallpaperCustomSolidColorHex || "#00d230"}
            className={`wallpaper-input theme-solid-color-picker 
              ${this.state.activeId === "solid-color-picker" ? "active" : ""}`}
          />
          <label
            htmlFor="solid-color-picker"
            className="sr-only"
            // TODO: Add Fluent string
            // data-l10n-id={fluent_id}
          >
            Solid Color Picker
          </label>
        </>
      ) : (
        ""
      );

    return (
      <div>
        <div className="category-header">
          <h2 data-l10n-id="newtab-wallpaper-title"></h2>
          <button
            className="wallpapers-reset"
            onClick={this.handleReset}
            data-l10n-id="newtab-wallpaper-reset"
          />
        </div>
        <fieldset className="category-list">
          {categories.map(category => {
            const filteredList = wallpaperList.filter(
              wallpaper => wallpaper.category === category
            );
            const activeWallpaperObj =
              activeWallpaper &&
              filteredList.find(wp => wp.title === activeWallpaper);
            const thumbnail = activeWallpaperObj || filteredList[0];

            let fluent_id;
            switch (category) {
              case "photographs":
                fluent_id = "newtab-wallpaper-category-title-photographs";
                break;
              case "abstracts":
                fluent_id = "newtab-wallpaper-category-title-abstract";
                break;
              case "solid-colors":
                fluent_id = "newtab-wallpaper-category-title-colors";
            }

            let style = {};
            if (thumbnail?.wallpaperUrl) {
              style.backgroundImage = `url(${thumbnail.wallpaperUrl})`;
            } else {
              style.backgroundColor = thumbnail?.solid_color || "";
            }

            return (
              <div key={category}>
                <input
                  id={category}
                  style={style}
                  type="radio"
                  onClick={this.handleCategory}
                  className="wallpaper-input"
                />
                <label htmlFor={category} data-l10n-id={fluent_id}>
                  {fluent_id}
                </label>
              </div>
            );
          })}
        </fieldset>

        <CSSTransition
          in={!!activeCategory}
          timeout={300}
          classNames="wallpaper-list"
          unmountOnExit={true}
        >
          <section className={categorySectionClassname}>
            <button
              className="arrow-button"
              data-l10n-id={activeCategoryFluentID}
              onClick={this.handleBack}
            />
            <fieldset>
              {filteredWallpapers.map(
                ({ title, theme, fluent_id, solid_color, wallpaperUrl }) => {
                  let style = {};

                  if (wallpaperUrl) {
                    style.backgroundImage = `url(${wallpaperUrl})`;
                  } else {
                    style.backgroundColor = solid_color || "";
                  }
                  return (
                    <>
                      <input
                        onChange={this.handleChange}
                        style={style}
                        type="radio"
                        name={`wallpaper-${title}`}
                        id={title}
                        value={title}
                        checked={title === activeWallpaper}
                        aria-checked={title === activeWallpaper}
                        className={`wallpaper-input theme-${theme} ${this.state.activeId === title ? "active" : ""}`}
                        onClick={() => this.setActiveId(title)} //
                      />
                      <label
                        htmlFor={title}
                        className="sr-only"
                        data-l10n-id={fluent_id}
                      >
                        {fluent_id}
                      </label>
                    </>
                  );
                }
              )}
              {colorPickerInput}
            </fieldset>
          </section>
        </CSSTransition>
      </div>
    );
  }
}

export const WallpaperCategories = connect(state => {
  return {
    Wallpapers: state.Wallpapers,
    Prefs: state.Prefs,
  };
})(_WallpaperCategories);
