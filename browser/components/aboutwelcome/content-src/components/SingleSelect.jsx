/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect } from "react";

import { Localized } from "./MSLocalized";

// This component was formerly "Themes" and continues to support theme and
// wallpaper pickers.
export const SingleSelect = ({
  activeSingleSelect,
  activeTheme,
  content,
  handleAction,
  setActiveSingleSelect,
}) => {
  const category = content.tiles?.category?.type || content.tiles?.type;
  const isSingleSelect = category === "single-select";
  // When screen renders for first time or user navigates back, update state to
  // check default option.
  useEffect(() => {
    if (isSingleSelect && !activeSingleSelect) {
      let newActiveSingleSelect =
        content.tiles?.selected || content.tiles.data[0].id;
      setActiveSingleSelect(newActiveSingleSelect);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const getIconStyles = (icon = {}) => {
    const CONFIGURABLE_STYLES = [
      "background",
      "borderRadius",
      "height",
      "marginBlock",
      "marginInline",
      "paddingBlock",
      "paddingInline",
      "width",
    ];
    let styles = {};
    Object.keys(icon).forEach(styleProp => {
      if (CONFIGURABLE_STYLES.includes(styleProp)) {
        styles[styleProp] = icon[styleProp];
      }
    });
    return styles;
  };

  return (
    <div className="tiles-single-select-container">
      <div>
        <fieldset className={`tiles-single-select-section ${category}`}>
          <Localized text={content.subtitle}>
            <legend className="sr-only" />
          </Localized>
          {content.tiles.data.map(
            ({
              description,
              icon,
              id,
              label = "",
              theme,
              tooltip,
              type = "",
            }) => {
              const value = id || theme;
              const selected =
                (theme && theme === activeTheme) ||
                (isSingleSelect && activeSingleSelect === value);
              const valOrObj = val => (typeof val === "object" ? val : {});

              const handleClick = evt => {
                if (isSingleSelect) {
                  setActiveSingleSelect(value);
                }
                handleAction(evt);
              };

              const handleKeyDown = evt => {
                if (evt.key === "Enter" || evt.keyCode === 13) {
                  // Set target value to the input inside of the selected label
                  evt.currentTarget.value = value;
                  handleClick(evt);
                }
              };

              return (
                <Localized
                  key={value + (isSingleSelect ? "" : label)}
                  text={valOrObj(tooltip)}
                >
                  {/* eslint-disable jsx-a11y/label-has-associated-control */}
                  {/* eslint-disable jsx-a11y/no-noninteractive-element-interactions */}
                  <label
                    className={`select-item ${type}`}
                    title={value}
                    onKeyDown={e => handleKeyDown(e)}
                  >
                    <Localized text={valOrObj(description)}>
                      <input
                        type="radio"
                        value={value}
                        name={category === "theme" ? "theme" : id}
                        checked={selected}
                        className="sr-only input"
                        onClick={e => handleClick(e)}
                      />
                    </Localized>
                    <div
                      className={`icon ${selected ? " selected" : ""} ${value}`}
                      style={getIconStyles(icon)}
                    />
                    <Localized text={label}>
                      <div className="text" />
                    </Localized>
                  </label>
                </Localized>
              );
            }
          )}
        </fieldset>
      </div>
    </div>
  );
};
