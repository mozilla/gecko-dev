/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import React, { Component } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../actions/index";
import {
  getActiveSearch,
  getSelectedSource,
  getIsCurrentThreadPaused,
  getSelectedSourceTextContent,
  getSearchOptions,
} from "../../selectors/index";

import { searchKeys } from "../../constants";
import { scrollList } from "../../utils/result-list";
import { createLocation } from "../../utils/location";

import SearchInput from "../shared/SearchInput";

const { PluralForm } = require("resource://devtools/shared/plural-form.js");
const { debounce } = require("resource://devtools/shared/debounce.js");
import {
  clearSearch,
  find,
  findNext,
  findPrev,
} from "../../utils/editor/index";
import { isFulfilled } from "../../utils/async-value";

function getSearchShortcut() {
  return L10N.getStr("sourceSearch.search.key2");
}

class SearchInFileBar extends Component {
  constructor(props) {
    super(props);
    this.state = {
      query: "",
      selectedResultIndex: 0,
      results: {
        matches: [],
        matchIndex: -1,
        count: 0,
        index: -1,
      },
      inputFocused: false,
    };
  }

  static get propTypes() {
    return {
      closeFileSearch: PropTypes.func.isRequired,
      editor: PropTypes.object,
      modifiers: PropTypes.object.isRequired,
      searchInFileEnabled: PropTypes.bool.isRequired,
      selectedSourceTextContent: PropTypes.object,
      selectedSource: PropTypes.object.isRequired,
      setActiveSearch: PropTypes.func.isRequired,
      querySearchWorker: PropTypes.func.isRequired,
      selectLocation: PropTypes.func.isRequired,
      isPaused: PropTypes.bool.isRequired,
    };
  }

  componentWillUnmount() {
    const { shortcuts } = this.context;

    shortcuts.off(getSearchShortcut(), this.toggleSearch);
    shortcuts.off("Escape", this.onEscape);

    this.doSearch.cancel();
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { query } = this.state;
    // Trigger a search to update the search results ...
    if (
      // if there is a search query and ...
      (query &&
        // the file search bar is toggled open or ...
        ((!this.props.searchInFileEnabled && nextProps.searchInFileEnabled) ||
          // a new source is selected.
          this.props.selectedSource.id !== nextProps.selectedSource.id)) ||
      // the source content changes
      this.props.selectedSourceTextContent !==
        nextProps.selectedSourceTextContent
    ) {
      // Do not scroll to the search location, if we just switched to a new source
      // and debugger is already paused on a selected line.
      this.doSearch(query, !nextProps.isPaused);
    }
  }

  componentDidMount() {
    // overwrite this.doSearch with debounced version to
    // reduce frequency of queries
    this.doSearch = debounce(this.doSearch, 100);
    const { shortcuts } = this.context;

    shortcuts.on(getSearchShortcut(), this.toggleSearch);
    shortcuts.on("Escape", this.onEscape);
  }

  componentDidUpdate() {
    if (this.refs.resultList && this.refs.resultList.refs) {
      scrollList(this.refs.resultList.refs, this.state.selectedResultIndex);
    }
  }

  onEscape = e => {
    this.closeSearch(e);
  };

  clearSearch = () => {
    const { editor } = this.props;
    if (!editor) {
      return;
    }
    editor.clearSearchMatches();
    editor.removePositionContentMarker("active-selection-marker");
  };

  closeSearch = e => {
    const { closeFileSearch, editor, searchInFileEnabled } = this.props;
    this.clearSearch();
    if (editor && searchInFileEnabled) {
      closeFileSearch();
      e.stopPropagation();
      e.preventDefault();
    }
    this.setState({ inputFocused: false });
  };

  toggleSearch = e => {
    e.stopPropagation();
    e.preventDefault();
    const { editor, searchInFileEnabled, setActiveSearch } = this.props;

    // Set inputFocused to false, so that search query is highlighted whenever search shortcut is used, even if the input already has focus.
    this.setState({ inputFocused: false });

    if (!searchInFileEnabled) {
      setActiveSearch("file");
    }

    if (searchInFileEnabled && editor) {
      const selectedText = editor.getSelectedText();
      const query = selectedText || this.state.query;

      if (query !== "") {
        this.setState({ query, inputFocused: true });
        this.doSearch(query);
      } else {
        this.setState({ query: "", inputFocused: true });
      }
    }
  };

  doSearch = async (query, shouldScroll = true) => {
    const { editor, modifiers, selectedSourceTextContent } = this.props;
    if (
      !editor ||
      !selectedSourceTextContent ||
      !isFulfilled(selectedSourceTextContent) ||
      !modifiers
    ) {
      return;
    }
    const selectedContent = selectedSourceTextContent.value;

    const ctx = { editor, cm: editor.codeMirror };

    if (!query) {
      clearSearch(ctx);
      return;
    }

    let text;
    if (selectedContent.type === "wasm") {
      text = editor.renderWasmText(selectedContent).join("\n");
    } else {
      text = selectedContent.value;
    }

    const matches = await this.props.querySearchWorker(query, text, modifiers);
    const results = find(ctx, query, true, modifiers, {
      shouldScroll,
    });
    this.setSearchResults(results, matches);
  };

  traverseResults = (e, reverse = false) => {
    e.stopPropagation();
    e.preventDefault();
    const { editor } = this.props;

    if (!editor) {
      return;
    }

    const ctx = { editor, cm: editor.codeMirror };

    const { modifiers } = this.props;
    const { query } = this.state;
    const { matches } = this.state.results;

    if (query === "" && !this.props.searchInFileEnabled) {
      this.props.setActiveSearch("file");
    }

    if (modifiers) {
      const findArgs = [ctx, query, true, modifiers];
      const results = reverse ? findPrev(...findArgs) : findNext(...findArgs);
      this.setSearchResults(results, matches);
    }
  };

  /**
   * Update the state with the results and matches from the search.
   * The cursor location is also set for CM6.
   * @param {Object} results
   * @param {Array} matches
   * @returns
   */
  setSearchResults(results, matches) {
    if (!results) {
      this.setState({
        results: {
          matches,
          matchIndex: 0,
          count: matches.length,
          index: -1,
        },
      });
      return;
    }
    const { ch, line } = results;
    let matchContent = "";
    const matchIndex = matches.findIndex(elm => {
      if (elm.line === line && elm.ch === ch) {
        matchContent = elm.match;
        return true;
      }
      return false;
    });

    this.setCursorLocation(line, ch, matchContent);
    this.setState({
      results: {
        matches,
        matchIndex,
        count: matches.length,
        index: ch,
      },
    });
  }

  /**
   * CodeMirror event handler, called whenever the cursor moves
   * for user-driven or programatic reasons.
   * @param {Number} line
   * @param {Number} ch
   * @param {Number} matchCount
   */
  setCursorLocation = (line, ch, matchContent) => {
    this.props.selectLocation(
      createLocation({
        source: this.props.selectedSource,
        line: line + 1,
        column: ch + matchContent.length,
      }),
      {
        // Reset the context, so that we don't switch to original
        // while moving the cursor within a bundle
        keepContext: false,

        // Avoid highlighting the selected line
        highlight: false,

        // This is mostly for displaying the correct location
        // in the footer, so this should not scroll.
        scroll: false,
      }
    );
  };

  // Handlers
  onChange = e => {
    this.setState({ query: e.target.value });

    return this.doSearch(e.target.value);
  };

  onFocus = () => {
    this.setState({ inputFocused: true });
  };

  onBlur = () => {
    this.setState({ inputFocused: false });
  };

  onKeyDown = e => {
    if (e.key !== "Enter" && e.key !== "F3") {
      return;
    }

    e.preventDefault();
    this.traverseResults(e, e.shiftKey);
  };

  onHistoryScroll = query => {
    this.setState({ query });
    this.doSearch(query);
  };

  // Renderers
  buildSummaryMsg() {
    const {
      query,
      results: { matchIndex, count, index },
    } = this.state;

    if (query.trim() == "") {
      return "";
    }

    if (count == 0) {
      return L10N.getStr("editor.noResultsFound");
    }

    if (index == -1) {
      const resultsSummaryString = L10N.getStr("sourceSearch.resultsSummary1");
      return PluralForm.get(count, resultsSummaryString).replace("#1", count);
    }

    const searchResultsString = L10N.getStr("editor.searchResults1");
    return PluralForm.get(count, searchResultsString)
      .replace("#1", count)
      .replace("%d", matchIndex + 1);
  }

  shouldShowErrorEmoji() {
    const {
      query,
      results: { count },
    } = this.state;
    return !!query && !count;
  }

  render() {
    const { searchInFileEnabled } = this.props;
    const {
      results: { count },
    } = this.state;

    if (!searchInFileEnabled) {
      return div(null);
    }
    return div(
      {
        className: "search-bar",
      },
      React.createElement(SearchInput, {
        query: this.state.query,
        count,
        placeholder: L10N.getStr("sourceSearch.search.placeholder2"),
        summaryMsg: this.buildSummaryMsg(),
        isLoading: false,
        onChange: this.onChange,
        onFocus: this.onFocus,
        onBlur: this.onBlur,
        showErrorEmoji: this.shouldShowErrorEmoji(),
        onKeyDown: this.onKeyDown,
        onHistoryScroll: this.onHistoryScroll,
        handleNext: e => this.traverseResults(e, false),
        handlePrev: e => this.traverseResults(e, true),
        shouldFocus: this.state.inputFocused,
        showClose: true,
        showExcludePatterns: false,
        handleClose: this.closeSearch,
        showSearchModifiers: true,
        searchKey: searchKeys.FILE_SEARCH,
        onToggleSearchModifier: () => this.doSearch(this.state.query),
      })
    );
  }
}

SearchInFileBar.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => {
  return {
    searchInFileEnabled: getActiveSearch(state) === "file",
    selectedSource: getSelectedSource(state),
    isPaused: getIsCurrentThreadPaused(state),
    selectedSourceTextContent: getSelectedSourceTextContent(state),
    modifiers: getSearchOptions(state, "file-search"),
  };
};

export default connect(mapStateToProps, {
  setFileSearchQuery: actions.setFileSearchQuery,
  setActiveSearch: actions.setActiveSearch,
  closeFileSearch: actions.closeFileSearch,
  querySearchWorker: actions.querySearchWorker,
  selectLocation: actions.selectLocation,
})(SearchInFileBar);
