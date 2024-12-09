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

  const autoTriggerAllowed = itemAction => {
    // Currently only enabled for sidebar experiment prefs
    const allowedActions = ["SET_PREF"];
    const allowedPrefs = [
      "sidebar.revamp",
      "sidebar.verticalTabs",
      "sidebar.visibility",
    ];
    const checkAction = action => {
      if (!allowedActions.includes(action.type)) {
        return false;
      }
      if (
        action.type === "SET_PREF" &&
        !allowedPrefs.includes(action.data?.pref.name)
      ) {
        return false;
      }
      return true;
    };
    if (itemAction.type === "MULTI_ACTION") {
      // Only allow autoTrigger if all actions are allowed
      return !itemAction.data.actions.some(action => !checkAction(action));
    }
    return checkAction(itemAction);
  };

  // When screen renders for first time or user navigates back, update state to
  // check default option.
  useEffect(() => {
    if (isSingleSelect && !activeSingleSelect) {
      let newActiveSingleSelect =
        content.tiles?.selected || content.tiles?.data[0].id;
      setActiveSingleSelect(newActiveSingleSelect);
      let selectedTile = content.tiles?.data.find(
        opt => opt.id === newActiveSingleSelect
      );
      // If applicable, automatically trigger the action for the default
      // selected tile.
      if (
        isSingleSelect &&
        content.tiles?.autoTrigger &&
        autoTriggerAllowed(selectedTile?.action)
      ) {
        handleAction({ currentTarget: { value: selectedTile.id } });
      }
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
              flair,
            }) => {
              const value = id || theme;
              let inputName = "select-item";
              if (!isSingleSelect) {
                inputName = category === "theme" ? "theme" : id; // unique names per item are currently used in the wallpaper picker
              }
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
                    style={icon?.width ? { minWidth: icon.width } : {}}
                  >
                    {flair ? (
                      <Localized text={valOrObj(flair.text)}>
                        <span className="flair"></span>
                      </Localized>
                    ) : (
                      ""
                    )}
                    <Localized text={valOrObj(description)}>
                      <input
                        type="radio"
                        value={value}
                        name={inputName}
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
