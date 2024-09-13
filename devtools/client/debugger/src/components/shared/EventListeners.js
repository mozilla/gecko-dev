/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import {
  div,
  input,
  li,
  ul,
  span,
  button,
  form,
  label,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../actions/index";
import {
  getActiveEventListeners,
  getEventListenerBreakpointTypes,
  getEventListenerExpanded,
} from "../../selectors/index";

import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("resource://devtools/client/shared/classnames.js");
const isOSX = Services.appinfo.OS == "Darwin";

class EventListeners extends Component {
  state = {
    searchText: "",
  };

  static get propTypes() {
    return {
      panelKey: PropTypes.string.isRequired,
      activeEventListeners: PropTypes.array.isRequired,
      addEventListenerExpanded: PropTypes.func.isRequired,
      addEventListeners: PropTypes.func.isRequired,
      categories: PropTypes.array.isRequired,
      expandedCategories: PropTypes.array.isRequired,
      removeEventListenerExpanded: PropTypes.func.isRequired,
      removeEventListeners: PropTypes.func.isRequired,
    };
  }

  hasMatch(eventOrCategoryName, searchText) {
    const lowercaseEventOrCategoryName = eventOrCategoryName.toLowerCase();
    const lowercaseSearchText = searchText.toLowerCase();

    return lowercaseEventOrCategoryName.includes(lowercaseSearchText);
  }

  getSearchResults() {
    const { searchText } = this.state;
    const { categories } = this.props;
    const searchResults = categories.reduce((results, cat, index) => {
      const category = categories[index];

      if (this.hasMatch(category.name, searchText)) {
        results[category.name] = category.events;
      } else {
        results[category.name] = category.events.filter(event =>
          this.hasMatch(event.name, searchText)
        );
      }

      return results;
    }, {});

    return searchResults;
  }

  onCategoryToggle(category) {
    const {
      expandedCategories,
      removeEventListenerExpanded,
      addEventListenerExpanded,
    } = this.props;

    if (expandedCategories.includes(category)) {
      removeEventListenerExpanded(this.props.panelKey, category);
    } else {
      addEventListenerExpanded(this.props.panelKey, category);
    }
  }

  onCategoryClick(category, isChecked) {
    const { addEventListeners, removeEventListeners } = this.props;
    const eventsIds = category.events.map(event => event.id);

    if (isChecked) {
      addEventListeners(this.props.panelKey, eventsIds);
    } else {
      removeEventListeners(this.props.panelKey, eventsIds);
    }
  }

  async onCategoryCtrlClick(category) {
    const {
      categories,
      activeEventListeners,
      addEventListeners,
      removeEventListeners,
    } = this.props;

    const eventsIdsToAdd = category.events
      .map(event => event.id)
      .filter(id => !activeEventListeners.includes(id));
    if (eventsIdsToAdd.length) {
      await addEventListeners(this.props.panelKey, eventsIdsToAdd);
    }

    const eventsIdsToRemove = [];
    for (const cat of categories) {
      if (cat == category) {
        continue;
      }
      for (const event of cat.events) {
        if (activeEventListeners.includes(event.id)) {
          eventsIdsToRemove.push(event.id);
        }
      }
    }

    if (eventsIdsToRemove.length) {
      await removeEventListeners(this.props.panelKey, eventsIdsToRemove);
    }
  }

  onEventTypeClick(eventId, isChecked) {
    const { addEventListeners, removeEventListeners } = this.props;
    if (isChecked) {
      addEventListeners(this.props.panelKey, [eventId]);
    } else {
      removeEventListeners(this.props.panelKey, [eventId]);
    }
  }

  async onEventTypeCtrlClick(eventId) {
    const {
      categories,
      activeEventListeners,
      addEventListeners,
      removeEventListeners,
    } = this.props;

    if (!activeEventListeners.includes(eventId)) {
      await addEventListeners(this.props.panelKey, [eventId]);
    }

    const eventsIdsToRemove = [];
    for (const cat of categories) {
      for (const event of cat.events) {
        if (event.id != eventId && activeEventListeners.includes(event.id)) {
          eventsIdsToRemove.push(event.id);
        }
      }
    }

    if (eventsIdsToRemove.length) {
      await removeEventListeners(this.props.panelKey, eventsIdsToRemove);
    }
  }

  onInputChange = event => {
    this.setState({ searchText: event.currentTarget.value });
  };

  onKeyDown = event => {
    if (event.key === "Escape") {
      this.setState({ searchText: "" });
    }
  };

  renderSearchInput() {
    const { searchText } = this.state;
    const placeholder = L10N.getStr("eventListenersHeader1.placeholder");
    return form(
      {
        className: "event-search-form",
        onSubmit: e => e.preventDefault(),
      },
      input({
        className: "event-search-input",
        placeholder,
        value: searchText,
        onChange: this.onInputChange,
        onKeyDown: this.onKeyDown,
      })
    );
  }

  renderClearSearchButton() {
    const { searchText } = this.state;

    if (!searchText) {
      return null;
    }
    return button({
      onClick: () =>
        this.setState({
          searchText: "",
        }),
      className: "devtools-searchinput-clear",
    });
  }

  renderCategoriesList() {
    const { categories } = this.props;
    return ul(
      {
        className: "event-listeners-list",
      },
      categories.map((category, index) => {
        return li(
          {
            className: "event-listener-group",
            key: index,
          },
          this.renderCategoryHeading(category),
          this.renderCategoryListing(category)
        );
      })
    );
  }

  renderSearchResultsList() {
    const searchResults = this.getSearchResults();
    return ul(
      {
        className: "event-search-results-list",
      },
      Object.keys(searchResults).map(category => {
        return searchResults[category].map(event => {
          return this.renderListenerEvent(event, category);
        });
      })
    );
  }

  renderCategoryHeading(category) {
    const { activeEventListeners, expandedCategories } = this.props;
    const { events } = category;

    const expanded = expandedCategories.includes(category.name);
    const checked = events.every(({ id }) => activeEventListeners.includes(id));
    const indeterminate =
      !checked && events.some(({ id }) => activeEventListeners.includes(id));

    return div(
      {
        className: "event-listener-header",
        onMouseOver: () => {
          this.props.highlightEventListeners(
            this.props.panelKey,
            category.events.map(e => e.id)
          );
        },
        onMouseOut: () => {
          this.props.highlightEventListeners(this.props.panelKey, []);
        },
      },
      button(
        {
          className: "event-listener-expand",
          onClick: () => this.onCategoryToggle(category.name),
        },
        React.createElement(AccessibleImage, {
          className: classnames("arrow", {
            expanded,
          }),
        })
      ),
      label(
        {
          className: "event-listener-label",
          onMouseDown: e => {
            const isAccelKey = (isOSX && e.metaKey) || (!isOSX && e.ctrlKey);
            if (isAccelKey) {
              // Hack a non-sense of React.
              // Even if we prevent default and stop propagation,
              // React will still trigger the `onChange` handler with the `checked` attribute set to false
              // which would lead to disable the category we are willing to enable here
              e.target.ignoreNextChange = true;

              this.onCategoryCtrlClick(category);
              e.preventDefault();
              e.stopPropagation();
            }
          },
        },
        input({
          type: "checkbox",
          value: category.name,
          onChange: e => {
            if (e.target.ignoreNextChange) {
              delete e.target.ignoreNextChange;
              return;
            }
            this.onCategoryClick(
              category,
              // Clicking an indeterminate checkbox should always have the
              // effect of disabling any selected items.
              indeterminate ? false : e.target.checked
            );
          },
          checked,
          ref: el => el && (el.indeterminate = indeterminate),
        }),
        span(
          {
            className: "event-listener-category",
          },
          category.name
        )
      )
    );
  }

  renderCategoryListing(category) {
    const { expandedCategories } = this.props;

    const expanded = expandedCategories.includes(category.name);
    if (!expanded) {
      return null;
    }
    return ul(
      null,
      category.events.map(event => {
        return this.renderListenerEvent(event, category.name);
      })
    );
  }

  renderCategory(category) {
    return span(
      {
        className: "category-label",
      },
      category,
      " \u25B8 "
    );
  }

  renderListenerEvent(event, category) {
    const { activeEventListeners } = this.props;
    const { searchText } = this.state;
    return li(
      {
        className: "event-listener-event",
        key: event.id,
        onMouseOver: () => {
          this.props.highlightEventListeners(this.props.panelKey, [event.id]);
        },
        onMouseOut: () => {
          this.props.highlightEventListeners(this.props.panelKey, []);
        },
      },
      label(
        {
          className: "event-listener-label",
          onMouseDown: e => {
            const isAccelKey = (isOSX && e.metaKey) || (!isOSX && e.ctrlKey);
            if (isAccelKey) {
              // Hack a non-sense of React.
              // Even if we prevent default and stop propagation,
              // React will still trigger the `onChange` handler with the `checked` attribute set to false
              // which would lead to disable the category we are willing to enable here
              e.target.ignoreNextChange = true;
              this.onEventTypeCtrlClick(event.id);
              e.preventDefault();
              e.stopPropagation();
            }
          },
        },
        input({
          type: "checkbox",
          value: event.id,
          onChange: e => {
            if (e.target.ignoreNextChange) {
              delete e.target.ignoreNextChange;
              return;
            }
            this.onEventTypeClick(event.id, e.target.checked);
          },
          checked: activeEventListeners.includes(event.id),
        }),
        span(
          {
            className: "event-listener-name",
          },
          searchText ? this.renderCategory(category) : null,
          event.name
        )
      )
    );
  }

  render() {
    const { searchText } = this.state;
    return div(
      {
        className: "event-listeners",
      },
      div(
        {
          className: "event-search-container",
        },
        this.renderSearchInput(),
        this.renderClearSearchButton()
      ),
      div(
        {
          className: "event-listeners-content",
        },
        searchText
          ? this.renderSearchResultsList()
          : this.renderCategoriesList()
      )
    );
  }
}

const mapStateToProps = (state, props) => ({
  categories: getEventListenerBreakpointTypes(state, props.panelKey),
  activeEventListeners: getActiveEventListeners(state, props.panelKey),
  expandedCategories: getEventListenerExpanded(state, props.panelKey),
});

export default connect(mapStateToProps, {
  addEventListeners: actions.addEventListenerBreakpoints,
  removeEventListeners: actions.removeEventListenerBreakpoints,
  addEventListenerExpanded: actions.addEventListenerExpanded,
  removeEventListenerExpanded: actions.removeEventListenerExpanded,
  highlightEventListeners: actions.highlightEventListeners,
})(EventListeners);
