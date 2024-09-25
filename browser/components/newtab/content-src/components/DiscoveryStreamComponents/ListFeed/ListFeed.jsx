/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { useDispatch, useSelector } from "react-redux";
import { DSCard } from "../DSCard/DSCard";
const PREF_CONTEXTUAL_CONTENT_LISTFEED_TITLE =
  "discoverystream.contextualContent.listFeedTitle";

function ListFeed({ type, firstVisibleTimestamp, recs }) {
  const dispatch = useDispatch();
  const listFeedTitle = useSelector(state => state.Prefs.values)[
    PREF_CONTEXTUAL_CONTENT_LISTFEED_TITLE
  ];
  // Todo: need to remove ads while using default recommendations, remove this line once API has been updated.
  const listFeedRecs = recs.filter(rec => !rec.flight_id).slice(0, 5);
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
          <h1 className="list-feed-title" id="list-feed-title">
            <span className="icon icon-trending"></span>
            {listFeedTitle}
          </h1>
          <div
            className="list-feed-content"
            role="menu"
            aria-labelledby="list-feed-title"
          >
            {listFeedRecs.map((rec, index) => {
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
                  pos={rec.pos}
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
                  fetchTimestamp={rec.fetchTimestamp}
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
                  recommendation_id={rec.recommendation_id}
                  firstVisibleTimestamp={firstVisibleTimestamp}
                  scheduled_corpus_item_id={rec.scheduled_corpus_item_id}
                  recommended_at={rec.recommended_at}
                  received_rank={rec.received_rank}
                  isListCard={true}
                />
              );
            })}
          </div>
        </div>
      </div>
    )
  );
}

export { ListFeed };
