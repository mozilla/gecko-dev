/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ErrorBoundary } from "content-src/components/ErrorBoundary/ErrorBoundary";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";
import { SponsoredContentHighlight } from "../DiscoveryStreamComponents/FeatureHighlight/SponsoredContentHighlight";
import React from "react";
import { connect } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

/**
 * A section that can collapse. As of bug 1710937, it can no longer collapse.
 * See bug 1727365 for follow-up work to simplify this component.
 */
export class _CollapsibleSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onBodyMount = this.onBodyMount.bind(this);
    this.onMenuButtonMouseEnter = this.onMenuButtonMouseEnter.bind(this);
    this.onMenuButtonMouseLeave = this.onMenuButtonMouseLeave.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.setContextMenuButtonRef = this.setContextMenuButtonRef.bind(this);
    this.handleTopicSelectionButtonClick =
      this.handleTopicSelectionButtonClick.bind(this);
    this.state = {
      menuButtonHover: false,
      showContextMenu: false,
    };
  }

  setContextMenuButtonRef(element) {
    this.contextMenuButtonRef = element;
  }

  onBodyMount(node) {
    this.sectionBody = node;
  }

  onMenuButtonMouseEnter() {
    this.setState({ menuButtonHover: true });
  }

  onMenuButtonMouseLeave() {
    this.setState({ menuButtonHover: false });
  }

  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }

  handleTopicSelectionButtonClick() {
    const maybeDisplay =
      this.props.Prefs.values[
        "discoverystream.topicSelection.onboarding.maybeDisplay"
      ];

    this.props.dispatch(ac.OnlyToMain({ type: at.TOPIC_SELECTION_USER_OPEN }));

    if (maybeDisplay) {
      // if still part of onboarding, remove user from onboarding flow
      this.props.dispatch(
        ac.SetPref(
          "discoverystream.topicSelection.onboarding.maybeDisplay",
          false
        )
      );
    }
    this.props.dispatch(
      ac.BroadcastToContent({ type: at.TOPIC_SELECTION_SPOTLIGHT_OPEN })
    );
  }

  render() {
    const { isAnimating, maxHeight, menuButtonHover, showContextMenu } =
      this.state;
    const {
      id,
      collapsed,
      title,
      subTitle,
      mayHaveSponsoredStories,
      mayHaveTopicsSelection,
      sectionsEnabled,
    } = this.props;
    const active = menuButtonHover || showContextMenu;
    let bodyStyle;
    if (isAnimating && !collapsed) {
      bodyStyle = { maxHeight };
    } else if (!isAnimating && collapsed) {
      bodyStyle = { display: "none" };
    }
    let titleStyle;
    if (this.props.hideTitle) {
      titleStyle = { visibility: "hidden" };
    }
    const hasSubtitleClassName = subTitle ? `has-subtitle` : ``;
    const hasBeenUpdatedPreviously =
      this.props.Prefs.values[
        "discoverystream.topicSelection.hasBeenUpdatedPreviously"
      ];
    const selectedTopics =
      this.props.Prefs.values["discoverystream.topicSelection.selectedTopics"];
    const topicsHaveBeenPreviouslySet =
      hasBeenUpdatedPreviously || selectedTopics;
    return (
      <section
        className={`collapsible-section ${this.props.className}${
          active ? " active" : ""
        }`}
        // Note: data-section-id is used for web extension api tests in mozilla central
        data-section-id={id}
      >
        {!sectionsEnabled && (
          <div className="section-top-bar">
            <h2
              className={`section-title-container ${hasSubtitleClassName}`}
              style={titleStyle}
            >
              <span className="section-title">
                <FluentOrText message={title} />
              </span>
              {subTitle && (
                <span className="section-sub-title">
                  <FluentOrText message={subTitle} />
                </span>
              )}
              {mayHaveSponsoredStories &&
                this.props.spocMessageVariant === "variant-a" && (
                  <SponsoredContentHighlight
                    position="inset-block-start inset-inline-start"
                    dispatch={this.props.dispatch}
                  />
                )}
            </h2>
            {mayHaveTopicsSelection && (
              <div className="button-topic-selection">
                <moz-button
                  data-l10n-id={
                    topicsHaveBeenPreviouslySet
                      ? "newtab-topic-selection-button-update-interests"
                      : "newtab-topic-selection-button-pick-interests"
                  }
                  type={topicsHaveBeenPreviouslySet ? "default" : "primary"}
                  onClick={this.handleTopicSelectionButtonClick}
                />
              </div>
            )}
          </div>
        )}
        <ErrorBoundary className="section-body-fallback">
          <div ref={this.onBodyMount} style={bodyStyle}>
            {this.props.children}
          </div>
        </ErrorBoundary>
      </section>
    );
  }
}

_CollapsibleSection.defaultProps = {
  document: globalThis.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden",
  },
};

export const CollapsibleSection = connect(state => ({
  Prefs: state.Prefs,
}))(_CollapsibleSection);
