/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { useDispatch, useSelector } from "react-redux";
import { actionCreators as ac } from "common/Actions.mjs";
const PREF_FOLLOWED_SECTIONS = "discoverystream.sections.following";

/**
 * Shows a list of recommended topics with visual indication whether
 * the user follows some of the topics (active, blue, selected topics)
 * or is yet to do so (neutrally-coloured topics with a "plus" button).
 *
 * @returns {React.Element}
 */
function InlineTopicSelection() {
  const dispatch = useDispatch();
  const prefs = useSelector(state => state.Prefs.values);
  const following = prefs[PREF_FOLLOWED_SECTIONS]
    ? prefs[PREF_FOLLOWED_SECTIONS].split(", ")
    : [];
  // Stub out topics, will replace with server topics
  const topics = [
    { label: "Politics", id: "government" },
    { label: "Sports", id: "sports" },
    { label: "Life Hacks", id: "society" },
    { label: "Food", id: "food" },
    { label: "Tech", id: "tech" },
    { label: "Travel", id: "travel" },
    { label: "Health", id: "health" },
    { label: "Money", id: "finance" },
    { label: "Science", id: "education-science" },
    { label: "Home & Garden", id: "home" },
    { label: "Entertainment", id: "arts" },
  ];

  // Updates user preferences as they follow or unfollow topics
  // by selecting them from the list
  function handleChange(e) {
    const { name: topic, checked } = e.target;
    let updatedTopics = following;
    if (checked) {
      updatedTopics = updatedTopics.length
        ? [...updatedTopics, topic]
        : [topic];
    } else {
      updatedTopics = updatedTopics.filter(t => t !== topic);
    }
    dispatch(ac.SetPref(PREF_FOLLOWED_SECTIONS, updatedTopics.join(", ")));
  }

  return (
    <section className="inline-selection-wrapper">
      {/* Will replace copy here to copy sent from over the server */}
      <h2>Follow topics to personalize your feed</h2>
      <p className="inline-selection-copy">
        We will bring you personalized content, all while respecting your
        privacy. You'll have powerful control over what content you see and what
        you don't.
      </p>
      <ul className="topic-list">
        {topics.map(topic => {
          const checked = following.includes(topic.id);
          return (
            <li key={topic.id}>
              <label>
                <input
                  type="checkbox"
                  id={topic.id}
                  name={topic.id}
                  checked={checked}
                  aria-checked={checked}
                  onChange={handleChange}
                  tabIndex={-1}
                />
                <span className="topic-item-label">{topic.label}</span>
                <div
                  className={`topic-item-icon icon ${checked ? "icon-check-filled" : "icon-add-circle-fill"}`}
                ></div>
              </label>
            </li>
          );
        })}
      </ul>
      <p>
        <a
          href={prefs["support.url"]}
          data-l10n-id="newtab-topic-selection-privacy-link"
        />
      </p>
    </section>
  );
}

export { InlineTopicSelection };
