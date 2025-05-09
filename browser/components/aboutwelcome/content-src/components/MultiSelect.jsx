/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useCallback, useMemo, useRef } from "react";
import { Localized, CONFIGURABLE_STYLES } from "./MSLocalized";
import { AboutWelcomeUtils } from "../lib/aboutwelcome-utils.mjs";

const MULTI_SELECT_STYLES = [
  ...CONFIGURABLE_STYLES,
  "flexDirection",
  "flexWrap",
  "flexFlow",
  "flexGrow",
  "flexShrink",
  "justifyContent",
  "alignItems",
  "gap",
];

const TILE_STYLES = [
  "marginBlock",
  "marginInline",
  "paddingBlock",
  "paddingInline",
];

// Do not include styles applied at the content tile level
for (let i = MULTI_SELECT_STYLES.length - 1; i >= 0; i--) {
  if (TILE_STYLES.includes(MULTI_SELECT_STYLES[i])) {
    MULTI_SELECT_STYLES.splice(i, 1);
  }
}

const MULTI_SELECT_ICON_STYLES = [
  ...CONFIGURABLE_STYLES,
  "width",
  "height",
  "background",
  "backgroundColor",
  "backgroundImage",
  "backgroundSize",
  "backgroundPosition",
  "backgroundRepeat",
  "backgroundOrigin",
  "backgroundClip",
  "border",
  "borderRadius",
  "appearance",
  "fill",
  "stroke",
  "outline",
  "outlineOffset",
  "boxShadow",
];

export const MultiSelect = ({
  content,
  screenMultiSelects,
  setScreenMultiSelects,
  activeMultiSelect,
  setActiveMultiSelect,
  multiSelectId,
}) => {
  const { data, multiSelectItemDesign } = content.tiles;

  const isPicker = multiSelectItemDesign === "picker";
  const refs = useRef({});

  const handleChange = useCallback(() => {
    const newActiveMultiSelect = [];
    Object.keys(refs.current).forEach(key => {
      if (refs.current[key]?.checked) {
        newActiveMultiSelect.push(key);
      }
    });
    setActiveMultiSelect(newActiveMultiSelect, multiSelectId);
  }, [setActiveMultiSelect, multiSelectId]);

  const items = useMemo(
    () => {
      function getOrderedIds() {
        if (screenMultiSelects) {
          return screenMultiSelects;
        }
        let orderedIds = data
          .map(item => ({
            id: item.id,
            rank: item.randomize ? Math.random() : NaN,
          }))
          .sort((a, b) => b.rank - a.rank)
          .map(({ id }) => id);
        setScreenMultiSelects(orderedIds, multiSelectId);
        return orderedIds;
      }
      return getOrderedIds().map(id => data.find(item => item.id === id));
    },
    [] // eslint-disable-line react-hooks/exhaustive-deps
  );

  const containerStyle = useMemo(
    () =>
      AboutWelcomeUtils.getValidStyle(
        content.tiles.style,
        MULTI_SELECT_STYLES,
        true
      ),
    [content.tiles.style]
  );

  const PickerIcon = ({ emoji, bgColor, isChecked }) => {
    return (
      <span
        className={`picker-icon ${isChecked ? "picker-checked" : ""}`}
        style={{
          ...(!isChecked && bgColor && { backgroundColor: bgColor }),
        }}
      >
        {!isChecked && emoji ? emoji : ""}
      </span>
    );
  };

  // This handles interaction for when the user is clicking on or keyboard-interacting
  // with the container element when using the picker design. It is required
  // for appropriate accessibility.
  const handleCheckboxContainerInteraction = e => {
    if (!isPicker) {
      return;
    }

    if (e.type === "keydown") {
      // Prevent scroll on space presses
      if (e.key === " ") {
        e.preventDefault();
      }

      // Only handle space and enter keypresses
      if (e.key !== " " && e.key !== "Enter") {
        return;
      }
    }

    const container = e.currentTarget;
    // Manually flip the hidden checkbox since handleChange relies on it
    const checkbox = container.querySelector('input[type="checkbox"]');
    checkbox.checked = !checkbox.checked;

    // Manually call handleChange to update the multiselect state
    handleChange();
  };

  // When screen renders for first time, update state
  // with checkbox ids that has defaultvalue true
  useEffect(() => {
    if (!activeMultiSelect) {
      let newActiveMultiSelect = [];
      items.forEach(({ id, defaultValue }) => {
        if (defaultValue && id) {
          newActiveMultiSelect.push(id);
        }
      });
      setActiveMultiSelect(newActiveMultiSelect, multiSelectId);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return (
    <div
      className={`multi-select-container ${multiSelectItemDesign || ""}`}
      style={containerStyle}
      role={
        items.some(({ type, group }) => type === "radio" && group)
          ? "radiogroup"
          : "group"
      }
      aria-labelledby="multi-stage-multi-select-label"
    >
      {content.tiles.label ? (
        <Localized text={content.tiles.label}>
          <h2 id="multi-stage-multi-select-label" />
        </Localized>
      ) : null}
      {items.map(
        ({
          id,
          label,
          description,
          icon,
          type = "checkbox",
          group,
          style,
          pickerEmoji,
          pickerEmojiBackgroundColor,
        }) => (
          <div
            key={id + label}
            className="checkbox-container multi-select-item"
            style={AboutWelcomeUtils.getValidStyle(style, MULTI_SELECT_STYLES)}
            tabIndex={isPicker ? "0" : null}
            onClick={isPicker ? handleCheckboxContainerInteraction : null}
            onKeyDown={isPicker ? handleCheckboxContainerInteraction : null}
            role={isPicker ? "checkbox" : null}
            aria-checked={isPicker ? activeMultiSelect?.includes(id) : null}
          >
            <input
              type={type} // checkbox or radio
              id={id}
              value={id}
              name={group}
              checked={activeMultiSelect?.includes(id)}
              style={AboutWelcomeUtils.getValidStyle(
                icon?.style,
                MULTI_SELECT_ICON_STYLES
              )}
              onChange={handleChange}
              ref={el => (refs.current[id] = el)}
              aria-describedby={description ? `${id}-description` : null}
              aria-labelledby={description ? `${id}-label` : null}
              tabIndex={isPicker ? "-1" : "0"}
            />
            {isPicker && (
              <PickerIcon
                emoji={pickerEmoji}
                bgColor={pickerEmojiBackgroundColor}
                isChecked={activeMultiSelect?.includes(id)}
              />
            )}
            {label ? (
              <Localized text={label}>
                <label id={`${id}-label`} htmlFor={id}></label>
              </Localized>
            ) : null}
            {description ? (
              <Localized text={description}>
                <p id={`${id}-description`}></p>
              </Localized>
            ) : null}
          </div>
        )
      )}
    </div>
  );
};
