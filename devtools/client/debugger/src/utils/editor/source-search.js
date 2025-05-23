/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import buildQuery from "../build-query";
import { markerTypes } from "../../constants";

/**
 * @memberof utils/source-search
 * @static
 */
function SearchState() {
  this.posFrom = this.posTo = this.query = null;
  this.overlay = null;
  this.results = [];
}

/**
 * @memberof utils/source-search
 * @static
 */
function getSearchState(cm) {
  const state = cm.state.search || (cm.state.search = new SearchState());
  return state;
}

function isWhitespace(query) {
  return !query.match(/\S/);
}

export function getMatchIndex(count, currentIndex, rev) {
  if (!rev) {
    if (currentIndex == count - 1) {
      return 0;
    }

    return currentIndex + 1;
  }

  if (currentIndex == 0) {
    return count - 1;
  }

  return currentIndex - 1;
}

/**
 * If there's a saved search, selects the next results.
 * Otherwise, creates a new search and selects the first
 * result.
 *
 * @memberof utils/source-search
 * @static
 */
function doSearch(
  ctx,
  rev,
  query,
  keepSelection,
  modifiers,
  { shouldScroll = true }
) {
  const { editor } = ctx;
  if (!query || isWhitespace(query)) {
    editor.clearSearchMatches();
    return null;
  }
  const regexQuery = buildQuery(query, modifiers, {
    ignoreSpaces: true,
    // regex must be global for the overlay
    isGlobal: true,
  });

  if (editor.searchState.query?.toString() !== regexQuery.toString()) {
    editor.highlightSearchMatches(regexQuery, "cm-highlight");
  }
  const cursor = editor.getNextSearchCursor(rev);
  if (!cursor) {
    return null;
  }
  editor.setPositionContentMarker({
    id: markerTypes.ACTIVE_SELECTION_MARKER,
    positionClassName: "cm-matchhighlight",
    positions: [{ from: cursor.from, to: cursor.to }],
  });
  if (shouldScroll) {
    editor.scrollToPosition(cursor.from);
  }
  return editor.getPositionFromSearchCursor(cursor);
}

export function searchSourceForHighlight(
  ctx,
  rev,
  query,
  keepSelection,
  modifiers
) {
  const { editor } = ctx;
  if (!query || isWhitespace(query)) {
    editor.clearSearchMatches();
    return;
  }

  const regexQuery = buildQuery(query, modifiers, {
    ignoreSpaces: true,
    // regex must be global for the overlay
    isGlobal: true,
  });

  if (editor.searchState.query?.toString() !== regexQuery.toString()) {
    editor.highlightSearchMatches(regexQuery, "cm-highlight");
  }
}

/**
 * Remove overlay.
 *
 * @memberof utils/source-search
 * @static
 */
export function removeOverlay(ctx) {
  const state = getSearchState(ctx.cm);
  ctx.cm.removeOverlay(state.overlay);
  const { line, ch } = ctx.cm.getCursor();
  ctx.cm.doc.setSelection({ line, ch }, { line, ch }, { scroll: false });
}

/**
 * Clears the currently saved search.
 *
 * @memberof utils/source-search
 * @static
 */
export function clearSearch(ctx) {
  const { editor } = ctx;
  editor.clearSearchMatches();
  editor.removePositionContentMarker("active-selection-marker");
}

/**
 * Starts a new search.
 *
 * @memberof utils/source-search
 * @static
 */
export function find(ctx, query, keepSelection, modifiers, options) {
  clearSearch(ctx);
  return doSearch(ctx, false, query, keepSelection, modifiers, options);
}

/**
 * Finds the next item based on the currently saved search.
 *
 * @memberof utils/source-search
 * @static
 */
export function findNext(ctx, query, keepSelection, modifiers) {
  return doSearch(ctx, false, query, keepSelection, modifiers, {});
}

/**
 * Finds the previous item based on the currently saved search.
 *
 * @memberof utils/source-search
 * @static
 */
export function findPrev(ctx, query, keepSelection, modifiers) {
  return doSearch(ctx, true, query, keepSelection, modifiers, {});
}

export { buildQuery };
