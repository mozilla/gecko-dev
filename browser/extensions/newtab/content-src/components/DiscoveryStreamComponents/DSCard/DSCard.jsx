/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { DSImage } from "../DSImage/DSImage.jsx";
import { DSLinkMenu } from "../DSLinkMenu/DSLinkMenu";
import { ImpressionStats } from "../../DiscoveryStreamImpressionStats/ImpressionStats";
import { getActiveCardSize } from "../../../lib/utils";
import React from "react";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import {
  DSContextFooter,
  SponsorLabel,
  DSMessageFooter,
} from "../DSContextFooter/DSContextFooter.jsx";
import { DSThumbsUpDownButtons } from "../DSThumbsUpDownButtons/DSThumbsUpDownButtons.jsx";
import { FluentOrText } from "../../FluentOrText/FluentOrText.jsx";
import { connect } from "react-redux";
import { LinkMenuOptions } from "content-src/lib/link-menu-options";
const READING_WPM = 220;

/**
 * READ TIME FROM WORD COUNT
 * @param {int} wordCount number of words in an article
 * @returns {int} number of words per minute in minutes
 */
export function readTimeFromWordCount(wordCount) {
  if (!wordCount) {
    return false;
  }
  return Math.ceil(parseInt(wordCount, 10) / READING_WPM);
}

export const DSSource = ({
  source,
  timeToRead,
  newSponsoredLabel,
  context,
  sponsor,
  sponsored_by_override,
  icon_src,
}) => {
  // First try to display sponsored label or time to read here.
  if (newSponsoredLabel) {
    // If we can display something for spocs, do so.
    if (sponsored_by_override || sponsor || context) {
      return (
        <SponsorLabel
          context={context}
          sponsor={sponsor}
          sponsored_by_override={sponsored_by_override}
          newSponsoredLabel="new-sponsored-label"
        />
      );
    }
  }

  // If we are not a spoc, and can display a time to read value.
  if (source && timeToRead) {
    return (
      <p className="source clamp time-to-read">
        <FluentOrText
          message={{
            id: `newtab-label-source-read-time`,
            values: { source, timeToRead },
          }}
        />
      </p>
    );
  }

  // Otherwise display a default source.
  return (
    <div className="source-wrapper">
      {icon_src && <img src={icon_src} height="16" width="16" alt="" />}
      <p className="source clamp">{source}</p>
    </div>
  );
};

export const DefaultMeta = ({
  source,
  title,
  excerpt,
  timeToRead,
  newSponsoredLabel,
  context,
  context_type,
  sponsor,
  sponsored_by_override,
  saveToPocketCard,
  ctaButtonVariant,
  dispatch,
  spocMessageVariant,
  mayHaveSectionsCards,
  mayHaveThumbsUpDown,
  onThumbsUpClick,
  onThumbsDownClick,
  isListCard,
  state,
  format,
  topic,
  isSectionsCard,
  showTopics,
  icon_src,
  refinedCardsLayout,
}) => {
  const shouldHaveThumbs =
    !isListCard &&
    format !== "rectangle" &&
    mayHaveSectionsCards &&
    mayHaveThumbsUpDown;
  const shouldHaveFooterSection =
    isSectionsCard && (shouldHaveThumbs || showTopics);

  return (
    <div className="meta">
      <div className="info-wrap">
        {ctaButtonVariant !== "variant-b" &&
          format !== "rectangle" &&
          !refinedCardsLayout && (
            <DSSource
              source={source}
              timeToRead={timeToRead}
              newSponsoredLabel={newSponsoredLabel}
              context={context}
              sponsor={sponsor}
              sponsored_by_override={sponsored_by_override}
              icon_src={icon_src}
            />
          )}
        <h3 className="title clamp">
          {format === "rectangle" ? "Sponsored" : title}
        </h3>
        {format === "rectangle" ? (
          <p className="excerpt clamp">
            Sponsored content supports our mission to build a better web.
          </p>
        ) : (
          excerpt && <p className="excerpt clamp">{excerpt}</p>
        )}
      </div>
      {!isListCard &&
        format !== "rectangle" &&
        !mayHaveSectionsCards &&
        mayHaveThumbsUpDown &&
        !refinedCardsLayout && (
          <DSThumbsUpDownButtons
            onThumbsDownClick={onThumbsDownClick}
            onThumbsUpClick={onThumbsUpClick}
            sponsor={sponsor}
            isThumbsDownActive={state.isThumbsDownActive}
            isThumbsUpActive={state.isThumbsUpActive}
          />
        )}
      {(shouldHaveFooterSection || refinedCardsLayout) && (
        <div className="sections-card-footer">
          {refinedCardsLayout &&
            format !== "rectangle" &&
            format !== "spoc" && (
              <DSSource
                source={source}
                timeToRead={timeToRead}
                newSponsoredLabel={newSponsoredLabel}
                context={context}
                sponsor={sponsor}
                sponsored_by_override={sponsored_by_override}
                icon_src={icon_src}
              />
            )}
          {(shouldHaveThumbs || refinedCardsLayout) && (
            <DSThumbsUpDownButtons
              onThumbsDownClick={onThumbsDownClick}
              onThumbsUpClick={onThumbsUpClick}
              sponsor={sponsor}
              isThumbsDownActive={state.isThumbsDownActive}
              isThumbsUpActive={state.isThumbsUpActive}
            />
          )}
          {showTopics && (
            <span
              className="ds-card-topic"
              data-l10n-id={`newtab-topic-label-${topic}`}
            />
          )}
        </div>
      )}
      {!newSponsoredLabel && (
        <DSContextFooter
          context_type={context_type}
          context={context}
          sponsor={sponsor}
          sponsored_by_override={sponsored_by_override}
          cta_button_variant={ctaButtonVariant}
          source={source}
          dispatch={dispatch}
          spocMessageVariant={spocMessageVariant}
          mayHaveSectionsCards={mayHaveSectionsCards}
        />
      )}
      {/* Sponsored label is normally in the way of any message.
          newSponsoredLabel cards sponsored label is moved to just under the thumbnail,
          so we can display both, so we specifically don't pass in context. */}
      {newSponsoredLabel && (
        <DSMessageFooter
          context_type={context_type}
          context={null}
          saveToPocketCard={saveToPocketCard}
        />
      )}
    </div>
  );
};

export class _DSCard extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onLinkClick = this.onLinkClick.bind(this);
    this.doesLinkTopicMatchSelectedTopic =
      this.doesLinkTopicMatchSelectedTopic.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);
    this.onThumbsUpClick = this.onThumbsUpClick.bind(this);
    this.onThumbsDownClick = this.onThumbsDownClick.bind(this);
    const refinedCardsLayout =
      this.props.Prefs.values["discoverystream.refinedCardsLayout.enabled"];

    this.setContextMenuButtonHostRef = element => {
      this.contextMenuButtonHostElement = element;
    };
    this.setPlaceholderRef = element => {
      this.placeholderElement = element;
    };

    this.state = {
      isSeen: false,
      isThumbsUpActive: false,
      isThumbsDownActive: false,
    };

    // If this is for the about:home startup cache, then we always want
    // to render the DSCard, regardless of whether or not its been seen.
    if (props.App.isForStartupCache.App) {
      this.state.isSeen = true;
    }

    // We want to choose the optimal thumbnail for the underlying DSImage, but
    // want to do it in a performant way. The breakpoints used in the
    // CSS of the page are, unfortuntely, not easy to retrieve without
    // causing a style flush. To avoid that, we hardcode them here.
    //
    // The values chosen here were the dimensions of the card thumbnails as
    // computed by getBoundingClientRect() for each type of viewport width
    // across both high-density and normal-density displays.
    this.dsImageSizes = [
      {
        mediaMatcher: "(min-width: 1122px)",
        width: 296,
        height: 148,
      },

      {
        mediaMatcher: "(min-width: 866px)",
        width: 218,
        height: 109,
      },

      {
        mediaMatcher: "(max-width: 610px)",
        width: 202,
        height: 101,
      },
    ];

    this.standardCardImageSizes = [
      {
        mediaMatcher: "default",
        width: 296,
        height: 148,
      },
    ];

    this.listCardImageSizes = [
      {
        mediaMatcher: "(min-width: 1122px)",
        width: 75,
        height: 75,
      },
      {
        mediaMatcher: "default",
        width: 50,
        height: 50,
      },
    ];

    this.sectionsCardImagesSizes = {
      small: {
        width: 100,
        height: 120,
      },
      medium: {
        width: 300,
        height: refinedCardsLayout ? 172 : 150,
      },
      large: {
        width: 265,
        height: 265,
      },
    };

    this.sectionsColumnMediaMatcher = {
      1: "default",
      2: "(min-width: 724px)",
      3: "(min-width: 1122px)",
      4: "(min-width: 1390px)",
    };
  }

  getSectionImageSize(column, size) {
    const cardImageSize = {
      mediaMatcher: this.sectionsColumnMediaMatcher[column],
      width: this.sectionsCardImagesSizes[size].width,
      height: this.sectionsCardImagesSizes[size].height,
    };
    return cardImageSize;
  }

  doesLinkTopicMatchSelectedTopic() {
    // Edge case for clicking on a card when topic selections have not be set
    if (!this.props.selectedTopics) {
      return "not-set";
    }

    // Edge case the topic of the card is not one of the available topics
    if (!this.props.availableTopics.includes(this.props.topic)) {
      return "topic-not-selectable";
    }

    if (this.props.selectedTopics.includes(this.props.topic)) {
      return "true";
    }

    return "false";
  }

  onLinkClick() {
    const matchesSelectedTopic = this.doesLinkTopicMatchSelectedTopic();
    if (this.props.dispatch) {
      if (this.props.isFakespot) {
        this.props.dispatch(
          ac.DiscoveryStreamUserEvent({
            event: "FAKESPOT_CLICK",
            value: {
              product_id: this.props.id,
              category: this.props.category || "",
            },
          })
        );
      } else {
        this.props.dispatch(
          ac.DiscoveryStreamUserEvent({
            event: "CLICK",
            source: this.props.type.toUpperCase(),
            action_position: this.props.pos,
            value: {
              event_source: "card",
              card_type: this.props.flightId ? "spoc" : "organic",
              recommendation_id: this.props.recommendation_id,
              tile_id: this.props.id,
              ...(this.props.shim && this.props.shim.click
                ? { shim: this.props.shim.click }
                : {}),
              fetchTimestamp: this.props.fetchTimestamp,
              firstVisibleTimestamp: this.props.firstVisibleTimestamp,
              corpus_item_id: this.props.corpus_item_id,
              scheduled_corpus_item_id: this.props.scheduled_corpus_item_id,
              recommended_at: this.props.recommended_at,
              received_rank: this.props.received_rank,
              topic: this.props.topic,
              features: this.props.features,
              matches_selected_topic: matchesSelectedTopic,
              selected_topics: this.props.selectedTopics,
              is_list_card: this.props.isListCard,
              ...(this.props.format
                ? { format: this.props.format }
                : {
                    format: getActiveCardSize(
                      window.innerWidth,
                      this.props.sectionsClassNames,
                      this.props.section,
                      this.props.flightId
                    ),
                  }),
              ...(this.props.section
                ? {
                    section: this.props.section,
                    section_position: this.props.sectionPosition,
                    is_section_followed: this.props.sectionFollowed,
                  }
                : {}),
            },
          })
        );

        this.props.dispatch(
          ac.ImpressionStats({
            source: this.props.type.toUpperCase(),
            click: 0,
            window_inner_width: this.props.windowObj.innerWidth,
            window_inner_height: this.props.windowObj.innerHeight,
            tiles: [
              {
                id: this.props.id,
                pos: this.props.pos,
                ...(this.props.shim && this.props.shim.click
                  ? { shim: this.props.shim.click }
                  : {}),
                type: this.props.flightId ? "spoc" : "organic",
                recommendation_id: this.props.recommendation_id,
                topic: this.props.topic,
                selected_topics: this.props.selectedTopics,
                is_list_card: this.props.isListCard,
                ...(this.props.format
                  ? { format: this.props.format }
                  : {
                      format: getActiveCardSize(
                        window.innerWidth,
                        this.props.sectionsClassNames,
                        this.props.section,
                        this.props.flightId
                      ),
                    }),
                ...(this.props.section
                  ? {
                      section: this.props.section,
                      section_position: this.props.sectionPosition,
                      is_section_followed: this.props.sectionFollowed,
                    }
                  : {}),
              },
            ],
          })
        );
      }
    }
  }

  onThumbsUpClick(event) {
    event.stopPropagation();
    event.preventDefault();

    // Toggle active state for thumbs up button to show CSS animation
    const currentState = this.state.isThumbsUpActive;

    // If thumbs up has been clicked already, do nothing.
    if (currentState) {
      return;
    }

    this.setState({ isThumbsUpActive: !currentState });

    // Record thumbs up telemetry event
    this.props.dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "POCKET_THUMBS_UP",
        source: "THUMBS_UI",
        value: {
          recommendation_id: this.props.recommendation_id,
          tile_id: this.props.id,
          corpus_item_id: this.props.corpus_item_id,
          scheduled_corpus_item_id: this.props.scheduled_corpus_item_id,
          recommended_at: this.props.recommended_at,
          received_rank: this.props.received_rank,
          thumbs_up: true,
          thumbs_down: false,
          topic: this.props.topic,
          format: getActiveCardSize(
            window.innerWidth,
            this.props.sectionsClassNames,
            this.props.section,
            false // (thumbs up/down only exist on organic content)
          ),
          ...(this.props.section
            ? {
                section: this.props.section,
                section_position: this.props.sectionPosition,
                is_section_followed: this.props.sectionFollowed,
              }
            : {}),
        },
      })
    );

    // Show Toast
    this.props.dispatch(
      ac.OnlyToOneContent(
        {
          type: at.SHOW_TOAST_MESSAGE,
          data: {
            showNotifications: true,
            toastId: "thumbsUpToast",
          },
        },
        "ActivityStream:Content"
      )
    );
  }

  onThumbsDownClick(event) {
    event.stopPropagation();
    event.preventDefault();

    // Toggle active state for thumbs down button to show CSS animation
    const currentState = this.state.isThumbsDownActive;
    this.setState({ isThumbsDownActive: !currentState });

    // Run dismiss event after 0.5 second delay
    if (
      this.props.dispatch &&
      this.props.type &&
      this.props.id &&
      this.props.url
    ) {
      const index = this.props.pos;
      const source = this.props.type.toUpperCase();
      const spocData = {
        url: this.props.url,
        guid: this.props.id,
        type: "CardGrid",
        card_type: "organic",
        recommendation_id: this.props.recommendation_id,
        tile_id: this.props.id,
        corpus_item_id: this.props.corpus_item_id,
        scheduled_corpus_item_id: this.props.scheduled_corpus_item_id,
        recommended_at: this.props.recommended_at,
        received_rank: this.props.received_rank,
      };
      const blockUrlOption = LinkMenuOptions.BlockUrl(spocData, index, source);

      const { action, impression, userEvent } = blockUrlOption;

      setTimeout(() => {
        this.props.dispatch(action);

        this.props.dispatch(
          ac.DiscoveryStreamUserEvent({
            event: userEvent,
            source,
            action_position: index,
          })
        );
      }, 500);

      if (impression) {
        this.props.dispatch(impression);
      }

      // Record thumbs down telemetry event
      this.props.dispatch(
        ac.DiscoveryStreamUserEvent({
          event: "POCKET_THUMBS_DOWN",
          source: "THUMBS_UI",
          value: {
            recommendation_id: this.props.recommendation_id,
            tile_id: this.props.id,
            corpus_item_id: this.props.corpus_item_id,
            scheduled_corpus_item_id: this.props.scheduled_corpus_item_id,
            recommended_at: this.props.recommended_at,
            received_rank: this.props.received_rank,
            thumbs_up: false,
            thumbs_down: true,
            topic: this.props.topic,
            format: getActiveCardSize(
              window.innerWidth,
              this.props.sectionsClassNames,
              this.props.section,
              false // (thumbs up/down only exist on organic content)
            ),
            ...(this.props.section
              ? {
                  section: this.props.section,
                  section_position: this.props.sectionPosition,
                  is_section_followed: this.props.sectionFollowed,
                }
              : {}),
          },
        })
      );

      // Show Toast
      this.props.dispatch(
        ac.OnlyToOneContent(
          {
            type: at.SHOW_TOAST_MESSAGE,
            data: {
              showNotifications: true,
              toastId: "thumbsDownToast",
            },
          },
          "ActivityStream:Content"
        )
      );
    }
  }

  onMenuUpdate(showContextMenu) {
    if (!showContextMenu) {
      const dsLinkMenuHostDiv = this.contextMenuButtonHostElement;
      if (dsLinkMenuHostDiv) {
        dsLinkMenuHostDiv.classList.remove("active", "last-item");
      }
    }
  }

  async onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonHostElement;
    if (dsLinkMenuHostDiv) {
      // Force translation so we can be sure it's ready before measuring.
      await this.props.windowObj.document.l10n.translateFragment(
        dsLinkMenuHostDiv
      );
      if (this.props.windowObj.scrollMaxX > 0) {
        dsLinkMenuHostDiv.classList.add("last-item");
      }
      dsLinkMenuHostDiv.classList.add("active");
    }
  }

  onSeen(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.placeholderElement) {
          this.observer.unobserve(this.placeholderElement);
        }

        // Stop observing since element has been seen
        this.setState({
          isSeen: true,
        });
      }
    }
  }

  onIdleCallback() {
    if (!this.state.isSeen) {
      if (this.observer && this.placeholderElement) {
        this.observer.unobserve(this.placeholderElement);
      }
      this.setState({
        isSeen: true,
      });
    }
  }

  componentDidMount() {
    this.idleCallbackId = this.props.windowObj.requestIdleCallback(
      this.onIdleCallback.bind(this)
    );
    if (this.placeholderElement) {
      this.observer = new IntersectionObserver(this.onSeen.bind(this));
      this.observer.observe(this.placeholderElement);
    }
  }

  componentWillUnmount() {
    // Remove observer on unmount
    if (this.observer && this.placeholderElement) {
      this.observer.unobserve(this.placeholderElement);
    }
    if (this.idleCallbackId) {
      this.props.windowObj.cancelIdleCallback(this.idleCallbackId);
    }
  }

  render() {
    const {
      isRecentSave,
      DiscoveryStream,
      Prefs,
      saveToPocketCard,
      isListCard,
      isFakespot,
      mayHaveSectionsCards,
      format,
      alt_text,
    } = this.props;

    const refinedCardsLayout =
      Prefs.values["discoverystream.refinedCardsLayout.enabled"];
    const refinedCardsClassName = refinedCardsLayout ? `refined-cards` : ``;

    if (this.props.placeholder || !this.state.isSeen) {
      // placeholder-seen is used to ensure the loading animation is only used if the card is visible.
      const placeholderClassName = this.state.isSeen ? `placeholder-seen` : ``;
      let placeholderElements = (
        <>
          <div className="placeholder-image placeholder-fill" />
          <div className="placeholder-label placeholder-fill" />
          <div className="placeholder-header placeholder-fill" />
          <div className="placeholder-description placeholder-fill" />
        </>
      );

      if (refinedCardsLayout) {
        placeholderElements = (
          <>
            <div className="placeholder-image placeholder-fill" />
            <div className="placeholder-description placeholder-fill" />
            <div className="placeholder-header placeholder-fill" />
          </>
        );
      }
      return (
        <div
          className={`ds-card placeholder ${placeholderClassName} ${
            isListCard ? "list-card-placeholder" : ""
          } ${refinedCardsClassName}`}
          ref={this.setPlaceholderRef}
        >
          {placeholderElements}
        </div>
      );
    }

    let source = this.props.source || this.props.publisher;
    if (!source) {
      try {
        source = new URL(this.props.url).hostname;
      } catch (e) {}
    }

    const {
      pocketButtonEnabled,
      hideDescriptions,
      compactImages,
      imageGradient,
      newSponsoredLabel,
      titleLines = 3,
      descLines = 3,
      readTime: displayReadTime,
    } = DiscoveryStream;

    const layoutsVariantAEnabled = Prefs.values["newtabLayouts.variant-a"];
    const layoutsVariantBEnabled = Prefs.values["newtabLayouts.variant-b"];
    const sectionsEnabled = Prefs.values["discoverystream.sections.enabled"];
    const layoutsVariantAorB = layoutsVariantAEnabled || layoutsVariantBEnabled;

    const smartCrop = Prefs.values["images.smart"];
    const faviconEnabled =
      Prefs.values["discoverystream.publisherFavicon.enabled"];
    const excerpt = !hideDescriptions ? this.props.excerpt : "";

    let timeToRead;
    if (displayReadTime) {
      timeToRead =
        this.props.time_to_read || readTimeFromWordCount(this.props.word_count);
    }

    const ctaButtonEnabled = this.props.ctaButtonSponsors?.includes(
      this.props.sponsor?.toLowerCase()
    );
    let ctaButtonVariant = "";
    if (ctaButtonEnabled) {
      ctaButtonVariant = this.props.ctaButtonVariant;
    }
    let ctaButtonVariantClassName = ctaButtonVariant;

    const ctaButtonClassName = ctaButtonEnabled ? `ds-card-cta-button` : ``;
    const compactImagesClassName = compactImages ? `ds-card-compact-image` : ``;
    const imageGradientClassName = imageGradient
      ? `ds-card-image-gradient`
      : ``;
    const listCardClassName = isListCard ? `list-feed-card` : ``;
    const fakespotClassName = isFakespot ? `fakespot` : ``;
    const sectionsCardsClassName = [
      mayHaveSectionsCards ? `sections-card-ui` : ``,
      this.props.sectionsClassNames,
    ].join(" ");
    const sectionsCardsImageSizes = this.props.sectionsCardImageSizes;
    const titleLinesName = `ds-card-title-lines-${titleLines}`;
    const descLinesClassName = `ds-card-desc-lines-${descLines}`;
    const isMediumRectangle = format === "rectangle";
    const spocFormatClassName = isMediumRectangle ? `ds-spoc-rectangle` : ``;

    let sizes = [];
    if (!isMediumRectangle) {
      sizes = this.dsImageSizes;
      if (sectionsEnabled) {
        sizes = [
          this.getSectionImageSize("4", sectionsCardsImageSizes["4"]),
          this.getSectionImageSize("3", sectionsCardsImageSizes["3"]),
          this.getSectionImageSize("2", sectionsCardsImageSizes["2"]),
          this.getSectionImageSize("1", sectionsCardsImageSizes["1"]),
        ];
      } else if (layoutsVariantAorB) {
        sizes = this.standardCardImageSizes;
      }
      if (isListCard) {
        sizes = this.listCardImageSizes;
      }
    }

    return (
      <article
        className={`ds-card ${listCardClassName} ${fakespotClassName} ${sectionsCardsClassName} ${compactImagesClassName} ${imageGradientClassName} ${titleLinesName} ${descLinesClassName} ${spocFormatClassName} ${ctaButtonClassName} ${ctaButtonVariantClassName} ${refinedCardsClassName}`}
        ref={this.setContextMenuButtonHostRef}
        data-position-one={this.props["data-position-one"]}
        data-position-two={this.props["data-position-one"]}
        data-position-three={this.props["data-position-one"]}
        data-position-four={this.props["data-position-one"]}
      >
        <SafeAnchor
          className="ds-card-link"
          dispatch={this.props.dispatch}
          onLinkClick={!this.props.placeholder ? this.onLinkClick : undefined}
          url={this.props.url}
          title={this.props.title}
        >
          {this.props.showTopics &&
            !this.props.mayHaveSectionsCards &&
            this.props.topic &&
            !isListCard &&
            !refinedCardsLayout && (
              <span
                className="ds-card-topic"
                data-l10n-id={`newtab-topic-label-${this.props.topic}`}
              />
            )}
          <div className="img-wrapper">
            <DSImage
              extraClassNames="img"
              source={this.props.image_src}
              rawSource={this.props.raw_image_src}
              sizes={sizes}
              url={this.props.url}
              title={this.props.title}
              isRecentSave={isRecentSave}
              alt_text={alt_text}
              smartCrop={smartCrop}
            />
          </div>
          <ImpressionStats
            flightId={this.props.flightId}
            rows={[
              {
                id: this.props.id,
                pos: this.props.pos,
                ...(this.props.shim && this.props.shim.impression
                  ? { shim: this.props.shim.impression }
                  : {}),
                recommendation_id: this.props.recommendation_id,
                fetchTimestamp: this.props.fetchTimestamp,
                corpus_item_id: this.props.corpus_item_id,
                scheduled_corpus_item_id: this.props.scheduled_corpus_item_id,
                recommended_at: this.props.recommended_at,
                received_rank: this.props.received_rank,
                topic: this.props.topic,
                features: this.props.features,
                is_list_card: isListCard,
                ...(format ? { format } : {}),
                isFakespot,
                category: this.props.category,
                ...(this.props.section
                  ? {
                      section: this.props.section,
                      section_position: this.props.sectionPosition,
                      is_section_followed: this.props.sectionFollowed,
                    }
                  : {}),
                ...(!format && this.props.section
                  ? // Note: sectionsCardsClassName is passed to ImpressionStats.jsx in order to calculate format
                    { class_names: sectionsCardsClassName }
                  : {}),
              },
            ]}
            dispatch={this.props.dispatch}
            isFakespot={isFakespot}
            source={this.props.type}
            firstVisibleTimestamp={this.props.firstVisibleTimestamp}
          />

          {ctaButtonVariant === "variant-b" && (
            <div className="cta-header">Shop Now</div>
          )}
          {isFakespot ? (
            <div className="meta">
              <div className="info-wrap">
                <h3 className="title clamp">{this.props.title}</h3>
              </div>
            </div>
          ) : (
            <DefaultMeta
              source={source}
              title={this.props.title}
              excerpt={excerpt}
              newSponsoredLabel={newSponsoredLabel}
              timeToRead={timeToRead}
              context={this.props.context}
              context_type={this.props.context_type}
              sponsor={this.props.sponsor}
              sponsored_by_override={this.props.sponsored_by_override}
              saveToPocketCard={saveToPocketCard}
              ctaButtonVariant={ctaButtonVariant}
              dispatch={this.props.dispatch}
              spocMessageVariant={this.props.spocMessageVariant}
              mayHaveThumbsUpDown={this.props.mayHaveThumbsUpDown}
              mayHaveSectionsCards={this.props.mayHaveSectionsCards}
              onThumbsUpClick={this.onThumbsUpClick}
              onThumbsDownClick={this.onThumbsDownClick}
              state={this.state}
              isListCard={isListCard}
              showTopics={!refinedCardsLayout && this.props.showTopics}
              isSectionsCard={
                this.props.mayHaveSectionsCards &&
                this.props.topic &&
                !isListCard
              }
              format={format}
              topic={this.props.topic}
              icon_src={faviconEnabled && this.props.icon_src}
              refinedCardsLayout={refinedCardsLayout}
            />
          )}
        </SafeAnchor>
        <div className="card-stp-button-hover-background">
          <div className="card-stp-button-position-wrapper">
            {!isFakespot && (
              <DSLinkMenu
                id={this.props.id}
                index={this.props.pos}
                dispatch={this.props.dispatch}
                url={this.props.url}
                title={this.props.title}
                source={source}
                type={this.props.type}
                card_type={this.props.flightId ? "spoc" : "organic"}
                pocket_id={this.props.pocket_id}
                shim={this.props.shim}
                bookmarkGuid={this.props.bookmarkGuid}
                flightId={
                  !this.props.is_collection ? this.props.flightId : undefined
                }
                showPrivacyInfo={!!this.props.flightId}
                onMenuUpdate={this.onMenuUpdate}
                onMenuShow={this.onMenuShow}
                saveToPocketCard={saveToPocketCard}
                pocket_button_enabled={pocketButtonEnabled}
                isRecentSave={isRecentSave}
                recommendation_id={this.props.recommendation_id}
                tile_id={this.props.id}
                block_key={this.props.id}
                corpus_item_id={this.props.corpus_item_id}
                scheduled_corpus_item_id={this.props.scheduled_corpus_item_id}
                recommended_at={this.props.recommended_at}
                received_rank={this.props.received_rank}
                is_list_card={this.props.isListCard}
                section={this.props.section}
                section_position={this.props.sectionPosition}
                is_section_followed={this.props.sectionFollowed}
                fetchTimestamp={this.props.fetchTimestamp}
                firstVisibleTimestamp={this.props.firstVisibleTimestamp}
                format={
                  format
                    ? format
                    : getActiveCardSize(
                        window.innerWidth,
                        this.props.sectionsClassNames,
                        this.props.section,
                        this.props.flightId
                      )
                }
                isSectionsCard={this.props.mayHaveSectionsCards}
                topic={this.props.topic}
                selected_topics={this.props.selected_topics}
              />
            )}
          </div>
        </div>
      </article>
    );
  }
}

_DSCard.defaultProps = {
  windowObj: window, // Added to support unit tests
};

export const DSCard = connect(state => ({
  App: state.App,
  DiscoveryStream: state.DiscoveryStream,
  Prefs: state.Prefs,
}))(_DSCard);

export const PlaceholderDSCard = () => <DSCard placeholder={true} />;
