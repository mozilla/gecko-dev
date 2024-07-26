/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useCallback, useEffect, useRef, useState } from "react";
import { useDispatch, useSelector } from "react-redux";
import { ModalOverlayWrapper } from "content-src/components/ModalOverlay/ModalOverlay";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

// TODO: move strings to newtab.ftl once strings have been approved
const TOPIC_LABELS = {
  "newtab-topic-business": "Business",
  "newtab-topic-arts": "Entertainment",
  "newtab-topic-food": "Food",
  "newtab-topic-health": "Health",
  "newtab-topic-finance": "Money",
  "newtab-topic-government": "Politics",
  "newtab-topic-sports": "Sports",
  "newtab-topic-tech": "Tech",
  "newtab-topic-travel": "Travel",
  "newtab-topic-education": "Science",
  "newtab-topic-society": "Life Hacks",
};

const EMOJI_LABELS = {
  business: "💼",
  arts: "🎭",
  food: "🍕",
  health: "🩺",
  finance: "💰",
  government: "🏛️",
  sports: "⚽️",
  tech: "💻",
  travel: "✈️",
  education: "🧪",
  society: "💡",
};

function TopicSelection() {
  const dispatch = useDispatch();
  const inputRef = useRef(null);
  const modalRef = useRef(null);
  const checkboxWrapperRef = useRef(null);
  const topics = useSelector(
    state => state.Prefs.values["discoverystream.topicSelection.topics"]
  ).split(", ");
  const selectedTopics = useSelector(
    state => state.Prefs.values["discoverystream.topicSelection.selectedTopics"]
  );
  const topicsHaveBeenPreviouslySet = useSelector(
    state =>
      state.Prefs.values[
        "discoverystream.topicSelection.hasBeenUpdatedPreviously"
      ]
  );

  function isFirstSave() {
    // Only return true if the user has not previous set prefs
    // and the selected topics pref is empty
    if (selectedTopics === "" && !topicsHaveBeenPreviouslySet) {
      dispatch(
        ac.SetPref(
          "discoverystream.topicSelection.hasBeenUpdatedPreviously",
          true
        )
      );
      return true;
    }

    return false;
  }

  const suggestedTopics = useSelector(
    state =>
      state.Prefs.values["discoverystream.topicSelection.suggestedTopics"]
  ).split(", ");

  // TODO: only show suggested topics during the first run
  // if selectedTopics is empty - default to using the suggestedTopics as a starting value
  const [topicsToSelect, setTopicsToSelect] = useState(
    selectedTopics ? selectedTopics.split(", ") : suggestedTopics
  );

  function handleModalClose() {
    dispatch(ac.OnlyToMain({ type: at.TOPIC_SELECTION_USER_DISMISS }));
    dispatch(ac.AlsoToMain({ type: at.TOPIC_SELECTION_SPOTLIGHT_TOGGLE }));
  }

  // when component mounts, set focus to input
  useEffect(() => {
    inputRef?.current?.focus();
  }, [inputRef]);

  const handleFocus = useCallback(e => {
    // this list will have to be updated with other reusable components that get used inside of this modal
    const tabbableElements = modalRef.current.querySelectorAll(
      'a[href], button, moz-button, input[tabindex="0"]'
    );
    const [firstTabableEl] = tabbableElements;
    const lastTabbableEl = tabbableElements[tabbableElements.length - 1];

    let isTabPressed = e.key === "Tab" || e.keyCode === 9;
    let isArrowPressed = e.key === "ArrowUp" || e.key === "ArrowDown";

    if (isTabPressed) {
      if (e.shiftKey) {
        if (document.activeElement === firstTabableEl) {
          lastTabbableEl.focus();
          e.preventDefault();
        }
      } else if (document.activeElement === lastTabbableEl) {
        firstTabableEl.focus();
        e.preventDefault();
      }
    } else if (
      isArrowPressed &&
      checkboxWrapperRef.current.contains(document.activeElement)
    ) {
      const checkboxElements =
        checkboxWrapperRef.current.querySelectorAll("input");
      const [firstInput] = checkboxElements;
      const lastInput = checkboxElements[checkboxElements.length - 1];
      const inputArr = Array.from(checkboxElements);
      const currentIndex = inputArr.indexOf(document.activeElement);
      let nextEl;
      if (e.key === "ArrowUp") {
        nextEl =
          document.activeElement === firstInput
            ? lastInput
            : checkboxElements[currentIndex - 1];
      } else if (e.key === "ArrowDown") {
        nextEl =
          document.activeElement === lastInput
            ? firstInput
            : checkboxElements[currentIndex + 1];
      }
      nextEl.tabIndex = 0;
      document.activeElement.tabIndex = -1;
      nextEl.focus();
    }
  }, []);

  useEffect(() => {
    const ref = modalRef.current;
    ref.addEventListener("keydown", handleFocus);

    inputRef.current.tabIndex = 0;

    return () => {
      ref.removeEventListener("keydown", handleFocus);
    };
  }, [handleFocus]);

  function handleChange(e) {
    const topic = e.target.name;
    const isChecked = e.target.checked;
    if (isChecked) {
      setTopicsToSelect([...topicsToSelect, topic]);
    } else {
      const updatedTopics = topicsToSelect.filter(t => t !== topic);
      setTopicsToSelect(updatedTopics);
    }
  }

  function handleSubmit() {
    const topicsString = topicsToSelect.join(", ");
    dispatch(
      ac.OnlyToMain({
        type: at.TOPIC_SELECTION_USER_SAVE,
        data: {
          topics: topicsString,
          previous_topics: selectedTopics,
          first_save: isFirstSave(),
        },
      })
    );

    dispatch(
      ac.SetPref("discoverystream.topicSelection.selectedTopics", topicsString)
    );
    dispatch(ac.AlsoToMain({ type: at.TOPIC_SELECTION_SPOTLIGHT_TOGGLE }));
  }

  return (
    <ModalOverlayWrapper
      onClose={handleModalClose}
      innerClassName="topic-selection-container"
    >
      <div className="topic-selection-form" ref={modalRef}>
        <button
          className="dismiss-button"
          title="dismiss"
          onClick={handleModalClose}
        />
        <h1 className="title">Select topics you care about</h1>
        <p className="subtitle">
          Tell us what you are interested in and we’ll recommend you great
          stories!
        </p>
        <div className="topic-list" ref={checkboxWrapperRef}>
          {topics.map((topic, i) => {
            const checked = topicsToSelect.includes(topic);
            return (
              <label className={`topic-item`} key={topic}>
                <input
                  type="checkbox"
                  id={topic}
                  name={topic}
                  ref={i === 0 ? inputRef : null}
                  onChange={handleChange}
                  checked={checked}
                  aria-checked={checked}
                  tabIndex={-1}
                />
                <div className={`topic-custom-checkbox`}>
                  <span className="topic-icon">{EMOJI_LABELS[`${topic}`]}</span>
                  <span className="topic-checked" />
                </div>
                <span className="topic-item-label">
                  {TOPIC_LABELS[`newtab-topic-${topic}`]}
                </span>
              </label>
            );
          })}
        </div>
        <div className="modal-footer">
          <a href="https://support.mozilla.org/en-US/kb/pocket-recommendations-firefox-new-tab">
            How we protect your data and privacy
          </a>
          <moz-button-group className="button-group">
            <moz-button label="Remove topics" onClick={handleModalClose} />
            <moz-button
              label="Save topics"
              type="primary"
              onClick={handleSubmit}
            />
          </moz-button-group>
        </div>
      </div>
    </ModalOverlayWrapper>
  );
}

export { TopicSelection };
