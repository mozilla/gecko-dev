/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState } from "react";
import { useSelector } from "react-redux";
import { DSCard } from "../DSCard/DSCard";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { actionCreators as ac } from "common/Actions.mjs";
const PREF_LISTFEED_TITLE = "discoverystream.contextualContent.listFeedTitle";
const PREF_FAKESPOT_CATEGROY =
  "discoverystream.contextualContent.fakespot.defaultCategoryTitle";
const PREF_FAKESPOT_FOOTER =
  "discoverystream.contextualContent.fakespot.footerCopy";
const PREF_FAKESPOT_CTA_COPY =
  "discoverystream.contextualContent.fakespot.ctaCopy";
const PREF_FAKESPOT_CTA_URL =
  "discoverystream.contextualContent.fakespot.ctaUrl";
const PREF_CONTEXTUAL_CONTENT_SELECTED_FEED =
  "discoverystream.contextualContent.selectedFeed";

function ListFeed({ type, firstVisibleTimestamp, recs, categories, dispatch }) {
  const [selectedFakespotFeed, setSelectedFakespotFeed] = useState("");
  const prefs = useSelector(state => state.Prefs.values);
  const listFeedTitle = prefs[PREF_LISTFEED_TITLE];
  const categoryTitle = prefs[PREF_FAKESPOT_CATEGROY];
  const footerCopy = prefs[PREF_FAKESPOT_FOOTER];
  const ctaCopy = prefs[PREF_FAKESPOT_CTA_COPY];
  const ctaUrl = prefs[PREF_FAKESPOT_CTA_URL];

  const isFakespot =
    prefs[PREF_CONTEXTUAL_CONTENT_SELECTED_FEED] === "fakespot";
  // Todo: need to remove ads while using default recommendations, remove this line once API has been updated.
  let listFeedRecs = selectedFakespotFeed
    ? recs.filter(rec => rec.category === selectedFakespotFeed)
    : recs;

  function handleCtaClick() {
    dispatch(
      ac.OnlyToMain({
        type: "FAKESPOT_CTA_CLICK",
      })
    );
  }

  function handleChange(e) {
    setSelectedFakespotFeed(e.target.value);
    dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "FAKESPOT_CATEGORY",
        value: {
          category: e.target.value || "",
        },
      })
    );
  }

  const contextMenuOptions = ["FakespotDismiss", "AboutFakespot"];

  const { length: listLength } = listFeedRecs;
  // determine if the list should take up all availible height or not
  const fullList = listLength >= 5;
  return (
    listLength > 0 && (
      <div
        className={`list-feed ${fullList ? "full-height" : ""} ${
          listLength > 2 ? "span-2" : "span-1"
        }`}
      >
        <div className="list-feed-inner-wrapper">
          {isFakespot ? (
            <div className="fakespot-heading">
              <div className="dropdown-wrapper">
                <select
                  className="fakespot-dropdown"
                  name="fakespot-categories"
                  value={selectedFakespotFeed}
                  onChange={handleChange}
                >
                  <option value="">
                    {categoryTitle || "Holiday Gift Guide"}
                  </option>
                  {categories.map(category => (
                    <option key={category} value={category}>
                      {category}
                    </option>
                  ))}
                </select>
                <div className="context-menu-wrapper">
                  <ContextMenuButton>
                    <LinkMenu
                      dispatch={dispatch}
                      options={contextMenuOptions}
                      shouldSendImpressionStats={true}
                      site={{ url: "https://www.fakespot.com" }}
                    />
                  </ContextMenuButton>
                </div>
              </div>
              <p className="fakespot-desc">{listFeedTitle}</p>
            </div>
          ) : (
            <h1 className="list-feed-title" id="list-feed-title">
              <span className="icon icon-newsfeed"></span>
              {listFeedTitle}
            </h1>
          )}
          <div
            className="list-feed-content"
            role="menu"
            aria-labelledby="list-feed-title"
          >
            {listFeedRecs.slice(0, 5).map((rec, index) => {
              if (!rec || rec.placeholder) {
                return (
                  <DSCard
                    key={`list-card-${index}`}
                    placeholder={true}
                    isListCard={true}
                  />
                );
              }
              return (
                <DSCard
                  key={`list-card-${index}`}
                  pos={index}
                  flightId={rec.flight_id}
                  image_src={rec.image_src}
                  raw_image_src={rec.raw_image_src}
                  word_count={rec.word_count}
                  time_to_read={rec.time_to_read}
                  title={rec.title}
                  topic={rec.topic}
                  excerpt={rec.excerpt}
                  url={rec.url}
                  id={rec.id}
                  shim={rec.shim}
                  type={type}
                  context={rec.context}
                  sponsor={rec.sponsor}
                  sponsored_by_override={rec.sponsored_by_override}
                  dispatch={dispatch}
                  source={rec.domain}
                  publisher={rec.publisher}
                  pocket_id={rec.pocket_id}
                  context_type={rec.context_type}
                  bookmarkGuid={rec.bookmarkGuid}
                  firstVisibleTimestamp={firstVisibleTimestamp}
                  corpus_item_id={rec.corpus_item_id}
                  scheduled_corpus_item_id={rec.scheduled_corpus_item_id}
                  recommended_at={rec.recommended_at}
                  received_rank={rec.received_rank}
                  isListCard={true}
                  isFakespot={isFakespot}
                  category={rec.category}
                />
              );
            })}
            {isFakespot && (
              <div className="fakespot-footer">
                <p>{footerCopy}</p>
                <SafeAnchor
                  className="fakespot-cta"
                  url={ctaUrl}
                  referrer={""}
                  onLinkClick={handleCtaClick}
                  dispatch={dispatch}
                >
                  {ctaCopy}
                </SafeAnchor>
              </div>
            )}
          </div>
        </div>
      </div>
    )
  );
}

export { ListFeed };
