/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import React, { useCallback } from "react";
import { DSEmptyState } from "../DSEmptyState/DSEmptyState";
import { DSCard, PlaceholderDSCard } from "../DSCard/DSCard";
import { useSelector } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { useIntersectionObserver } from "../../../lib/hooks";
import { SectionContextMenu } from "../SectionContextMenu/SectionContextMenu";

// Prefs
const PREF_SECTIONS_CARDS_ENABLED = "discoverystream.sections.cards.enabled";
const PREF_SECTIONS_CARDS_THUMBS_UP_DOWN_ENABLED =
  "discoverystream.sections.cards.thumbsUpDown.enabled";
const PREF_SECTIONS_PERSONALIZATION_ENABLED =
  "discoverystream.sections.personalization.enabled";
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
  const prefs = useSelector(state => state.Prefs.values);
  const showTopics = prefs[PREF_TOPICS_ENABLED];
  const mayHaveSectionsCards = prefs[PREF_SECTIONS_CARDS_ENABLED];
  const mayHaveSectionsCardsThumbsUpDown =
    prefs[PREF_SECTIONS_CARDS_THUMBS_UP_DOWN_ENABLED];
  const mayHaveThumbsUpDown = prefs[PREF_THUMBS_UP_DOWN_ENABLED];
  const selectedTopics = prefs[PREF_TOPICS_SELECTED];
  const availableTopics = prefs[PREF_TOPICS_AVAILABLE];
  const mayHaveSectionsContextMenu =
    prefs[PREF_SECTIONS_PERSONALIZATION_ENABLED];
  const { saveToPocketCard } = useSelector(state => state.DiscoveryStream);

  const handleIntersection = useCallback(
    el => {
      dispatch(
        ac.AlsoToMain({
          type: at.CARD_SECTION_IMPRESSION,
          data: {
            section: el.id,
            section_position: el.dataset.sectionPosition,
          },
        })
      );
    },
    [dispatch]
  );

  // Ref to hold all of the section elements
  const sectionRefs = useIntersectionObserver(handleIntersection);

  // Handle a render before feed has been fetched by displaying nothing
  if (!data) {
    return null;
  }

  // Only show thumbs up/down buttons if both default thumbs and sections thumbs prefs are enabled
  const mayHaveCombinedThumbsUpDown =
    mayHaveSectionsCardsThumbsUpDown && mayHaveThumbsUpDown;
  const { sections } = data;
  const isEmpty = sections.length === 0;

  function getLayoutData(responsiveLayout, index) {
    let layoutData = {
      classNames: [],
    };

    responsiveLayout.forEach(layout => {
      layout.tiles.forEach((tile, tileIndex) => {
        if (tile.position === index) {
          layoutData.classNames.push(`col-${layout.columnCount}-${tile.size}`);
          layoutData.classNames.push(
            `col-${layout.columnCount}-position-${tileIndex}`
          );
        }
      });
    });

    return layoutData;
  }

  // function to determine amount of tiles shown per section per viewport
  function getMaxTiles(responsiveLayouts) {
    return responsiveLayouts
      .flatMap(responsiveLayout => responsiveLayout)
      .reduce((acc, t) => {
        acc[t.columnCount] = t.tiles.length;

        // Update maxTile if current tile count is greater
        if (!acc.maxTile || t.tiles.length > acc.maxTile) {
          acc.maxTile = t.tiles.length;
        }
        return acc;
      }, {});
  }

  return isEmpty ? (
    <div className="ds-card-grid empty">
      <DSEmptyState status={data.status} dispatch={dispatch} feed={feed} />
    </div>
  ) : (
    <div className="ds-section-wrapper">
      {sections.map((section, sectionIndex) => {
        const { sectionKey, title, subtitle } = section;
        const { responsiveLayouts } = section.layout;
        const { maxTile } = getMaxTiles(responsiveLayouts);
        const displaySections = section.data.slice(0, maxTile);
        const isSectionEmpty = !displaySections?.length;
        const shouldShowLabels =
          sectionKey === "top_stories_section" && showTopics;

        if (isSectionEmpty) {
          return null;
        }

        return (
          <section
            key={sectionKey}
            id={sectionKey}
            className="ds-section"
            data-section-position={sectionIndex}
            ref={el => {
              sectionRefs.current[sectionIndex] = el;
            }}
          >
            <div className="section-heading">
              <h2 className="section-title">{title}</h2>
              {subtitle && <p className="section-subtitle">{subtitle}</p>}
              {mayHaveSectionsContextMenu && (
                <SectionContextMenu
                  dispatch={dispatch}
                  index={sectionIndex}
                  title={title}
                  type={type}
                />
              )}
            </div>
            <div className="ds-section-grid ds-card-grid">
              {section.data.slice(0, maxTile).map((rec, index) => {
                const { classNames } = getLayoutData(responsiveLayouts, index);

                if (!rec || rec.placeholder) {
                  return <PlaceholderDSCard key={`dscard-${index}`} />;
                }

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
                    corpus_item_id={rec.corpus_item_id}
                    scheduled_corpus_item_id={rec.scheduled_corpus_item_id}
                    recommended_at={rec.recommended_at}
                    received_rank={rec.received_rank}
                    format={rec.format}
                    alt_text={rec.alt_text}
                    mayHaveThumbsUpDown={mayHaveCombinedThumbsUpDown}
                    mayHaveSectionsCards={mayHaveSectionsCards}
                    showTopics={shouldShowLabels}
                    selectedTopics={selectedTopics}
                    availableTopics={availableTopics}
                    is_collection={is_collection}
                    saveToPocketCard={saveToPocketCard}
                    ctaButtonSponsors={ctaButtonSponsors}
                    ctaButtonVariant={ctaButtonVariant}
                    spocMessageVariant={spocMessageVariant}
                    sectionsClassNames={classNames.join(" ")}
                    section={sectionKey}
                    sectionPosition={sectionIndex}
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
