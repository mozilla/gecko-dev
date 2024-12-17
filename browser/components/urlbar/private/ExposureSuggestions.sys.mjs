/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SuggestProvider } from "resource:///modules/urlbar/private/SuggestFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

/**
 * A feature for exposure suggestions.
 */
export class ExposureSuggestions extends SuggestProvider {
  get shouldEnable() {
    return !!this.exposureSuggestionTypes.size;
  }

  get enablingPreferences() {
    return ["quicksuggest.exposureSuggestionTypes"];
  }

  get rustSuggestionTypes() {
    return ["Exposure"];
  }

  getRustProviderConstraints() {
    return {
      exposureSuggestionTypes: [...this.exposureSuggestionTypes],
    };
  }

  getSuggestionTelemetryType() {
    return "exposure";
  }

  get exposureSuggestionTypes() {
    // UrlbarPrefs converts this pref to a `Set` of type strings.
    return lazy.UrlbarPrefs.get("quicksuggest.exposureSuggestionTypes");
  }

  async makeResult(queryContext, suggestion, _searchString) {
    // It doesn't really matter what kind of result we return since it won't be
    // shown. Use a dynamic result since that kind of makes sense and there are
    // no requirements for its payload other than `dynamicType`.
    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.DYNAMIC,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          // Include `exposureSuggestionType` so tests can verify a suggestion
          // of the expected type is returned. We don't use it otherwise.
          exposureSuggestionType: suggestion.suggestionType,
          dynamicType: "exposure",
        }
      ),
      {
        // Exposure suggestions should always be hidden, and it's assumed that
        // exposure telemetry should be recorded for them, so as a convenience
        // set `exposureTelemetry` here. Otherwise experiments would need to set
        // the corresponding Nimbus variables properly. (They can still do that,
        // it's just not required.)
        exposureTelemetry: lazy.UrlbarUtils.EXPOSURE_TELEMETRY.HIDDEN,
      }
    );
  }
}
