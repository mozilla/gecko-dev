/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState } from "react";
import { useSelector } from "react-redux";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";

function TrendingSearchesVarA() {
  const [showTrends, setShowTrends] = useState(true);

  const onArrowClick = () => {
    setShowTrends(!showTrends);
  };

  const resultsObject = useSelector(state => state.TrendingSearch);
  const searchResults = resultsObject.suggestions;

  return (
    <section className="trending-searches-pill-wrapper">
      <div className="trending-searches-title-wrapper">
        <span className="trending-searches-icon icon icon-arrow-trending"></span>
        <h2 className="trending-searches-title">Trending on Google</h2>
        <div className="close-open-trending-searches">
          <moz-button
            iconsrc={`chrome://global/skin/icons/arrow-${showTrends ? "up" : "down"}.svg`}
            onClick={onArrowClick}
            className={`icon icon-arrowhead-up`}
            type="icon ghost"
          ></moz-button>
        </div>
      </div>
      {showTrends && (
        <ul className="trending-searches-list">
          {searchResults.map((result, index) => {
            return (
              <li key={index} className="trending-search-item">
                <SafeAnchor url="">{result.lowerCaseSuggestion}</SafeAnchor>
              </li>
            );
          })}
        </ul>
      )}
    </section>
  );
}

export { TrendingSearchesVarA };
