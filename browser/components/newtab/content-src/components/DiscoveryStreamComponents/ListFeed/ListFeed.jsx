/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { useDispatch } from "react-redux";
import { DSCard } from "../DSCard/DSCard";

function ListFeed({ type, firstVisibleTimestamp, recs }) {
  const dispatch = useDispatch();
  return (
    <div className="list-feed">
      <div className="list-feed-inner-wrapper">
        <h1 className="list-feed-title" id="list-feed-title">
          <span className="icon icon-trending"></span>
          Popular
        </h1>
        <div
          className="list-feed-content"
          role="menu"
          aria-labelledby="list-feed-title"
        >
          {recs.map(rec => (
            <DSCard
              key={`list-card-${rec.id}`}
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
          ))}
        </div>
      </div>
    </div>
  );
}

export { ListFeed };
