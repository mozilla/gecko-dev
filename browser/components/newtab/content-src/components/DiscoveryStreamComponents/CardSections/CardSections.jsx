/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import React, { useMemo } from "react";
import { DSEmptyState } from "../DSEmptyState/DSEmptyState";
import { DSCard } from "../DSCard/DSCard";
import { useSelector } from "react-redux";

// Prefs
const PREF_SECTIONS_CARDS_ENABLED = "discoverystream.sections.cards.enabled";
const PREF_TOPICS_ENABLED = "discoverystream.topicLabels.enabled";
const PREF_TOPICS_SELECTED = "discoverystream.topicSelection.selectedTopics";
const PREF_TOPICS_AVAILABLE = "discoverystream.topicSelection.topics";
const PREF_THUMBS_UP_DOWN_ENABLED = "discoverystream.thumbsUpDown.enabled";

function CardSections({
  data,
  feed,
  dispatch,
  type,
  firstVisibleTimestamp,
  is_collection,
  spocMessageVariant,
  ctaButtonVariant,
  ctaButtonSponsors,
}) {
  // const prefs = this.props.Prefs.values;
  const { recommendations, sections } = data;
  const isEmpty = recommendations?.length === 0 || !sections;
  const sortedSections = sections?.sort(
    (a, b) => a.receivedRank - b.receivedRank
  );

  const prefs = useSelector(state => state.Prefs.values);
  const { saveToPocketCard } = useSelector(state => state.DiscoveryStream);
  const showTopics = prefs[PREF_TOPICS_ENABLED];
  const mayHaveSectionsCards = prefs[PREF_SECTIONS_CARDS_ENABLED];
  const mayHaveThumbsUpDown = prefs[PREF_THUMBS_UP_DOWN_ENABLED];
  const selectedTopics = prefs[PREF_TOPICS_SELECTED];
  const availableTopics = prefs[PREF_TOPICS_AVAILABLE];

  // useMemo to only get sorted recs when the data prop changes
  const sortedRecs = useMemo(() => {
    return data.recommendations.reduce((acc, recommendation) => {
      const { section } = recommendation;
      acc[section] = acc[section] || [];
      acc[section].push(recommendation);
      return acc;
    }, {});
  }, [data]);
  // Handle a render before feed has been fetched by displaying nothing
  if (!data) {
    return null;
  }

  return isEmpty ? (
    <div className="ds-card-grid empty">
      <DSEmptyState status={data.status} dispatch={dispatch} feed={feed} />
    </div>
  ) : (
    <div className="ds-section-wrapper">
      {sortedSections.map(section => {
        const { sectionKey, title, subtitle } = section;
        return (
          <section key={sectionKey} className="ds-section">
            <div className="section-heading">
              <h2 className="section-title">{title}</h2>
              {subtitle && <p className="section-subtitle">{subtitle}</p>}
            </div>
            <div className="ds-section-grid ds-card-grid">
              {sortedRecs[sectionKey].slice(0, 4).map(rec => {
                return (
                  <DSCard
                    key={`dscard-${rec.id}`}
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
                    format={rec.format}
                    alt_text={rec.alt_text}
                    mayHaveThumbsUpDown={mayHaveThumbsUpDown}
                    mayHaveSectionsCards={mayHaveSectionsCards}
                    showTopics={showTopics}
                    selectedTopics={selectedTopics}
                    availableTopics={availableTopics}
                    is_collection={is_collection}
                    ctaButtonSponsors={ctaButtonSponsors}
                    ctaButtonVariant={ctaButtonVariant}
                    spocMessageVariant={spocMessageVariant}
                    saveToPocketCard={saveToPocketCard}
                  />
                );
              })}
            </div>
          </section>
        );
      })}
    </div>
  );
}

export { CardSections };
