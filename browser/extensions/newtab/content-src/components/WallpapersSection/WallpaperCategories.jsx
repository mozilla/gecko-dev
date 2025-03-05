/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { connect } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
// eslint-disable-next-line no-shadow
import { CSSTransition } from "react-transition-group";

const PREF_WALLPAPER_UPLOADED_PREVIOUSLY =
  "newtabWallpapers.customWallpaper.uploadedPreviously";

// Returns a function will not be continuously triggered when called. The
// function will be triggered if called again after `wait` milliseconds.
function debounce(func, wait) {
  let timer;
  return (...args) => {
    if (timer) {
      return;
    }

    let wakeUp = () => {
      timer = null;
    };

    timer = setTimeout(wakeUp, wait);
    func.apply(this, args);
  };
}

export class _WallpaperCategories extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleColorInput = this.handleColorInput.bind(this);
    this.debouncedHandleChange = debounce(this.handleChange.bind(this), 999);
    this.handleChange = this.handleChange.bind(this);
    this.handleReset = this.handleReset.bind(this);
    this.handleCategory = this.handleCategory.bind(this);
    this.handleUpload = this.handleUpload.bind(this);
    this.handleBack = this.handleBack.bind(this);
    this.getRGBColors = this.getRGBColors.bind(this);
    this.prefersHighContrastQuery = null;
    this.prefersDarkQuery = null;
    this.categoryRef = []; // store references for wallpaper category list
    this.wallpaperRef = []; // store reference for wallpaper selection list
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

  componentDidUpdate(prevProps) {
    // Walllpaper category subpanel should close when parent menu is closed
    if (
      this.props.exitEventFired &&
      this.props.exitEventFired !== prevProps.exitEventFired
    ) {
      this.handleBack();
    }
  }

  handleColorInput(event) {
    let { id } = event.target;
    // Set ID to include hex value of custom color
    id = `solid-color-picker-${event.target.value}`;
    const rgbColors = this.getRGBColors(event.target.value);

    // Set background color to custom color
    event.target.style.backgroundColor = `rgb(${rgbColors.toString()})`;
    this.setState({ customHexValue: event.target.style.backgroundColor });

    // Setting this now so when we remove v1 we don't have to migrate v1 values.
    this.props.setPref("newtabWallpapers.wallpaper", id);
  }

  // Note: There's a separate event (debouncedHandleChange) that fires the handleChange
  // event but is delayed so that it doesn't fire multiple events when a user
  // is selecting a custom color background
  handleChange(event) {
    let { id } = event.target;

    // Set ID to include hex value of custom color
    if (id === "solid-color-picker") {
      id = `solid-color-picker-${event.target.value}`;
    }

    this.props.setPref("newtabWallpapers.wallpaper", id);

    this.handleUserEvent(at.WALLPAPER_CLICK, {
      selected_wallpaper: id,
      had_previous_wallpaper: !!this.props.activeWallpaper,
    });
  }

  // function implementing arrow navigation for wallpaper category selection
  handleCategoryKeyDown(event, category) {
    const getIndex = this.categoryRef.findIndex(cat => cat.id === category);
    if (getIndex === -1) {
      return; // prevents errors if wallpaper index isn't found when navigating with arrow keys
    }

    const isRTL = document.dir === "rtl"; // returns true is page language is right-to-left
    let eventKey = event.key;

    if (eventKey === "ArrowRight" || eventKey === "ArrowLeft") {
      if (isRTL) {
        eventKey = eventKey === "ArrowRight" ? "ArrowLeft" : "ArrowRight";
      }
    }

    let nextIndex = getIndex;

    if (eventKey === "ArrowRight") {
      nextIndex =
        getIndex + 1 < this.categoryRef.length ? getIndex + 1 : getIndex;
    } else if (eventKey === "ArrowLeft") {
      nextIndex = getIndex - 1 >= 0 ? getIndex - 1 : getIndex;
    }

    this.categoryRef[nextIndex].focus();
  }

  // function implementing arrow navigation for wallpaper selection
  handleWallpaperKeyDown(event, title) {
    if (event.key === "Tab") {
      if (event.shiftKey) {
        event.preventDefault();
        this.backToMenuButton?.focus();
      } else {
        event.preventDefault(); // prevent tabbing within wallpaper selection. We should only be using the Tab key to tab between groups
      }
      return;
    }

    const isRTL = document.dir === "rtl"; // returns true if page language is right-to-left
    let eventKey = event.key;

    if (eventKey === "ArrowRight" || eventKey === "ArrowLeft") {
      if (isRTL) {
        eventKey = eventKey === "ArrowRight" ? "ArrowLeft" : "ArrowRight";
      }
    }

    const getIndex = this.wallpaperRef.findIndex(
      wallpaper => wallpaper.id === title
    );

    if (getIndex === -1) {
      return; // prevents errors if wallpaper index isn't found when navigating with arrow keys
    }

    // the set layout of columns per row for the wallpaper selection
    const columnCount = 3;
    let nextIndex = getIndex;

    if (eventKey === "ArrowRight") {
      nextIndex =
        getIndex + 1 < this.wallpaperRef.length ? getIndex + 1 : getIndex;
    } else if (eventKey === "ArrowLeft") {
      nextIndex = getIndex - 1 >= 0 ? getIndex - 1 : getIndex;
    } else if (eventKey === "ArrowDown") {
      nextIndex =
        getIndex + columnCount < this.wallpaperRef.length
          ? getIndex + columnCount
          : getIndex;
    } else if (eventKey === "ArrowUp") {
      nextIndex =
        getIndex - columnCount >= 0 ? getIndex - columnCount : getIndex;
    }

    this.wallpaperRef[nextIndex].tabIndex = 0;
    this.wallpaperRef[getIndex].tabIndex = -1;
    this.wallpaperRef[nextIndex].focus();
    this.wallpaperRef[nextIndex].click();
  }

  handleReset() {
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
      case "abstracts":
        fluent_id = "newtab-wallpaper-category-title-abstract";
        break;
      case "celestial":
        fluent_id = "newtab-wallpaper-category-title-celestial";
        break;
      case "photographs":
        fluent_id = "newtab-wallpaper-category-title-photographs";
        break;
      case "solid-colors":
        fluent_id = "newtab-wallpaper-category-title-colors";
    }

    this.setState({ activeCategoryFluentID: fluent_id });
  };

  // Custom wallpaper image upload
  async handleUpload() {
    // TODO: Bug 1943663: Add telemetry

    // TODO: Bug 1947813: Add image upload error states/UI

    // TODO: Once Bug 1947813 has landed, we may need a separate event
    // for selecting previously uploaded wallpaper, rather than uploading a new one.
    // The plan would be to reuse at.WALLPAPER_CLICK for this use case
    const uploadedPreviously =
      this.props.Prefs.values[PREF_WALLPAPER_UPLOADED_PREVIOUSLY];

    this.handleUserEvent(at.WALLPAPER_UPLOAD, {
      had_uploaded_previously: !!uploadedPreviously,
      had_previous_wallpaper: !!this.props.activeWallpaper,
    });

    this.props.setPref(PREF_WALLPAPER_UPLOADED_PREVIOUSLY, true);

    // Create a file input since category buttons are radio inputs
    const fileInput = document.createElement("input");
    fileInput.type = "file";
    fileInput.accept = "image/*"; // only allow image files

    fileInput.onchange = async event => {
      const [file] = event.target.files;

      if (file) {
        this.props.dispatch(
          ac.OnlyToMain({
            type: at.WALLPAPER_UPLOAD,
            data: file,
          })
        );
      }
    };

    fileInput.click();
  }

  handleBack() {
    this.setState({ activeCategory: null });
    this.categoryRef[0]?.focus();
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
    let filteredWallpapers = wallpaperList.filter(
      wallpaper => wallpaper.category === activeCategory
    );

    function reduceColorsToFitCustomColorInput(arr) {
      // Reduce the amount of custom colors to make space for the custom color picker
      while (arr.length % 3 !== 2) {
        arr.pop();
      }
      return arr;
    }

    let categorySectionClassname = "category wallpaper-list";
    if (prefs["newtabWallpapers.v2.enabled"]) {
      categorySectionClassname += " ignore-color-mode";
    }

    let wallpaperCustomSolidColorHex = null;

    const selectedWallpaper = prefs["newtabWallpapers.wallpaper"];

    // User has previous selected a custom color
    if (selectedWallpaper.includes("solid-color-picker")) {
      this.setState({ showColorPicker: true });
      const regex = /#([a-fA-F0-9]{6})/;
      [wallpaperCustomSolidColorHex] = selectedWallpaper.match(regex);
    }

    // Enable custom color select if pref'ed on
    this.setState({
      showColorPicker: prefs["newtabWallpapers.customColor.enabled"],
    });

    // Remove last item of solid colors to make space for custom color picker
    if (
      prefs["newtabWallpapers.customColor.enabled"] &&
      activeCategory === "solid-colors"
    ) {
      filteredWallpapers =
        reduceColorsToFitCustomColorInput(filteredWallpapers);
    }

    let colorPickerInput =
      showColorPicker && activeCategory === "solid-colors" ? (
        <div className="theme-custom-color-picker">
          <input
            onInput={this.handleColorInput}
            onChange={this.debouncedHandleChange}
            onClick={() => this.setActiveId("solid-color-picker")} //
            type="color"
            name={`wallpaper-solid-color-picker`}
            id="solid-color-picker"
            // aria-checked is not applicable for input[type="color"] elements
            aria-current={this.state.activeId === "solid-color-picker"}
            // If nothing selected, default to Zilla Green
            value={wallpaperCustomSolidColorHex || "#00d230"}
            className={`wallpaper-input
              ${this.state.activeId === "solid-color-picker" ? "active" : ""}`}
          />
          <label
            htmlFor="solid-color-picker"
            data-l10n-id="newtab-wallpaper-custom-color"
          ></label>
        </div>
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
        <div
          role="grid"
          aria-label="Wallpaper category selection. Use arrow keys to navigate."
        >
          <fieldset className="category-list">
            {categories.map((category, index) => {
              const filteredList = wallpaperList.filter(
                wallpaper => wallpaper.category === category
              );
              const activeWallpaperObj =
                activeWallpaper &&
                filteredList.find(wp => wp.title === activeWallpaper);
              const thumbnail = activeWallpaperObj || filteredList[0];
              let fluent_id;
              switch (category) {
                case "abstracts":
                  fluent_id = "newtab-wallpaper-category-title-abstract";
                  break;
                case "celestial":
                  fluent_id = "newtab-wallpaper-category-title-celestial";
                  break;
                case "custom-wallpaper":
                  fluent_id = "newtab-wallpaper-upload-image";
                  break;
                case "photographs":
                  fluent_id = "newtab-wallpaper-category-title-photographs";
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
                    ref={el => {
                      if (el) {
                        this.categoryRef[index] = el;
                      }
                    }}
                    id={category}
                    style={style}
                    type="radio"
                    onKeyDown={e => this.handleCategoryKeyDown(e, category)}
                    // Add overrides for custom wallpaper upload UI
                    onClick={
                      category !== "custom-wallpaper"
                        ? this.handleCategory
                        : this.handleUpload
                    }
                    className={
                      category !== "custom-wallpaper"
                        ? `wallpaper-input`
                        : `wallpaper-input theme-custom-wallpaper`
                    }
                    tabIndex={index === 0 ? 0 : -1}
                  />
                  <label htmlFor={category} data-l10n-id={fluent_id}>
                    {fluent_id}
                  </label>
                </div>
              );
            })}
          </fieldset>
        </div>

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
              ref={el => {
                this.backToMenuButton = el;
              }}
            />
            <div
              role="grid"
              aria-label="Wallpaper selection. Use arrow keys to navigate."
            >
              <fieldset>
                {filteredWallpapers.map(
                  (
                    { title, theme, fluent_id, solid_color, wallpaperUrl },
                    index
                  ) => {
                    let style = {};
                    if (wallpaperUrl) {
                      style.backgroundImage = `url(${wallpaperUrl})`;
                    } else {
                      style.backgroundColor = solid_color || "";
                    }
                    return (
                      <>
                        <input
                          ref={el => {
                            if (el) {
                              this.wallpaperRef[index] = el;
                            }
                          }}
                          onChange={this.handleChange}
                          onKeyDown={e => this.handleWallpaperKeyDown(e, title)}
                          style={style}
                          type="radio"
                          name={`wallpaper-${title}`}
                          id={title}
                          value={title}
                          checked={title === activeWallpaper}
                          aria-checked={title === activeWallpaper}
                          className={`wallpaper-input theme-${theme} ${this.state.activeId === title ? "active" : ""}`}
                          onClick={() => this.setActiveId(title)} //
                          tabIndex={index === 0 ? 0 : -1} //the first wallpaper in the array will have a tabindex of 0 so we can tab into it. The rest will have a tabindex of -1
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
            </div>
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
