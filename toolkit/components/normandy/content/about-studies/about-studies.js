/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global classnames PropTypes React ReactDOM */

/**
 * Shorthand for creating elements (to avoid using a JSX preprocessor)
 */
const r = React.createElement;

/**
 * Dispatches a page event to the privileged frame script for this tab.
 * @param {String} action
 * @param {Object} data
 */
function sendPageEvent(action, data) {
  const event = new CustomEvent("ShieldPageEvent", { bubbles: true, detail: { action, data } });
  document.dispatchEvent(event);
}

/**
 * Handle basic layout and routing within about:studies.
 */
class AboutStudies extends React.Component {
  constructor(props) {
    super(props);

    this.remoteValueNameMap = {
      AddonStudyList: "addonStudies",
      PreferenceStudyList: "prefStudies",
      ShieldLearnMoreHref: "learnMoreHref",
      StudiesEnabled: "studiesEnabled",
      ShieldTranslations: "translations",
    };

    this.state = {};
    for (const stateName of Object.values(this.remoteValueNameMap)) {
      this.state[stateName] = null;
    }
  }

  componentWillMount() {
    for (const remoteName of Object.keys(this.remoteValueNameMap)) {
      document.addEventListener(`ReceiveRemoteValue:${remoteName}`, this);
      sendPageEvent(`GetRemoteValue:${remoteName}`);
    }
  }

  componentWillUnmount() {
    for (const remoteName of Object.keys(this.remoteValueNameMap)) {
      document.removeEventListener(`ReceiveRemoteValue:${remoteName}`, this);
    }
  }

  /** Event handle to receive remote values from documentAddEventListener */
  handleEvent({ type, detail: value }) {
    const prefix = "ReceiveRemoteValue:";
    if (type.startsWith(prefix)) {
      const name = type.substring(prefix.length);
      this.setState({ [this.remoteValueNameMap[name]]: value });
    }
  }

  render() {
    const { translations, learnMoreHref, studiesEnabled, addonStudies, prefStudies } = this.state;

    // Wait for all values to be loaded before rendering. Some of the values may
    // be falsey, so an explicit null check is needed.
    if (Object.values(this.state).some(v => v === null)) {
      return null;
    }

    return (
      r("div", { className: "about-studies-container main-content" },
        r(WhatsThisBox, { translations, learnMoreHref, studiesEnabled }),
        r(StudyList, { translations, addonStudies, prefStudies }),
      )
    );
  }
}

/**
 * Explains the contents of the page, and offers a way to learn more and update preferences.
 */
class WhatsThisBox extends React.Component {
  handleUpdateClick() {
    sendPageEvent("NavigateToDataPreferences");
  }

  render() {
    const { learnMoreHref, studiesEnabled, translations } = this.props;

    return (
      r("div", { className: "info-box" },
        r("div", { className: "info-box-content" },
          r("span", {},
            studiesEnabled ? translations.enabledList : translations.disabledList,
          ),
          r("a", { id: "shield-studies-learn-more", href: learnMoreHref }, translations.learnMore),

          r("button", { id: "shield-studies-update-preferences", onClick: this.handleUpdateClick },
            r("div", { className: "button-box" },
              navigator.platform.includes("Win") ? translations.updateButtonWin : translations.updateButtonUnix
            ),
          )
        )
      )
    );
  }
}

/**
 * Shows a list of studies, with an option to end in-progress ones.
 */
class StudyList extends React.Component {
  render() {
    const { addonStudies, prefStudies, translations } = this.props;

    if (!addonStudies.length && !prefStudies.length) {
      return r("p", { className: "study-list-info" }, translations.noStudies);
    }

    const activeStudies = [];
    const inactiveStudies = [];

    // Since we are modifying the study objects, it is polite to make copies
    for (const study of addonStudies) {
      const clonedStudy = Object.assign({}, study, {type: "addon", sortDate: study.studyStartDate});
      if (study.active) {
        activeStudies.push(clonedStudy);
      } else {
        inactiveStudies.push(clonedStudy);
      }
    }

    for (const study of prefStudies) {
      const clonedStudy = Object.assign({}, study, {type: "pref", sortDate: new Date(study.lastSeen)});
      if (study.expired) {
        inactiveStudies.push(clonedStudy);
      } else {
        activeStudies.push(clonedStudy);
      }
    }

    activeStudies.sort((a, b) => b.sortDate - a.sortDate);
    inactiveStudies.sort((a, b) => b.sortDate - a.sortDate);

    return (
      r("div", {},
        r("h2", {}, translations.activeStudiesList),
        r("ul", { className: "study-list active-study-list" },
          activeStudies.map(study => (
            study.type === "addon"
            ? r(AddonStudyListItem, { key: study.name, study, translations })
            : r(PreferenceStudyListItem, { key: study.name, study, translations })
          )),
        ),
        r("h2", {}, translations.completedStudiesList),
        r("ul", { className: "study-list inactive-study-list" },
          inactiveStudies.map(study => (
            study.type === "addon"
            ? r(AddonStudyListItem, { key: study.name, study, translations })
            : r(PreferenceStudyListItem, { key: study.name, study, translations })
          )),
        ),
      )
    );
  }
}
StudyList.propTypes = {
  addonStudies: PropTypes.array.isRequired,
  translations: PropTypes.object.isRequired,
};

/**
 * Details about an individual add-on study, with an option to end it if it is active.
 */
class AddonStudyListItem extends React.Component {
  constructor(props) {
    super(props);
    this.handleClickRemove = this.handleClickRemove.bind(this);
  }

  handleClickRemove() {
    sendPageEvent("RemoveAddonStudy", {
      recipeId: this.props.study.recipeId,
      reason: "individual-opt-out",
    });
  }

  render() {
    const { study, translations } = this.props;
    return (
      r("li", {
        className: classnames("study addon-study", { disabled: !study.active }),
        "data-study-name": study.name,
      },
        r("div", { className: "study-icon" },
          study.name.replace(/-?add-?on-?/, "").replace(/-?study-?/, "").slice(0, 1)
        ),
        r("div", { className: "study-details" },
          r("div", { className: "study-header" },
            r("span", { className: "study-name" }, study.name),
            r("span", {}, "\u2022"), // &bullet;
            r("span", { className: "study-status" }, study.active ? translations.activeStatus : translations.completeStatus),
          ),
          r("div", { className: "study-description" },
            study.description
          ),
        ),
        r("div", { className: "study-actions" },
          study.active &&
          r("button", { className: "remove-button", onClick: this.handleClickRemove },
            r("div", { className: "button-box" },
              translations.removeButton
            ),
          )
        ),
      )
    );
  }
}
AddonStudyListItem.propTypes = {
  study: PropTypes.shape({
    recipeId: PropTypes.number.isRequired,
    name: PropTypes.string.isRequired,
    active: PropTypes.bool.isRequired,
    description: PropTypes.string.isRequired,
  }).isRequired,
  translations: PropTypes.object.isRequired,
};

/**
 * Details about an individual preference study, with an option to end it if it is active.
 */
class PreferenceStudyListItem extends React.Component {
  constructor(props) {
    super(props);
    this.handleClickRemove = this.handleClickRemove.bind(this);
  }

  handleClickRemove() {
    sendPageEvent("RemovePreferenceStudy", {
      experimentName: this.props.study.name,
      reason: "individual-opt-out",
    });
  }

  render() {
    const { study, translations } = this.props;

    // Sanitize the values by setting them as the text content of an element,
    // and then getting the HTML representation of that text. This will have the
    // browser safely sanitize them. Use outerHTML to also include the <code>
    // element in the string.
    const sanitizer = document.createElement("code");
    sanitizer.textContent = study.preferenceName;
    const sanitizedPreferenceName = sanitizer.outerHTML;
    sanitizer.textContent = study.preferenceValue;
    const sanitizedPreferenceValue = sanitizer.outerHTML;
    const description = translations.preferenceStudyDescription
      .replace(/%(?:1\$)?S/, sanitizedPreferenceName)
      .replace(/%(?:2\$)?S/, sanitizedPreferenceValue);

    return (
      r("li", {
        className: classnames("study pref-study", { disabled: study.expired }),
        "data-study-name": study.name,
      },
        r("div", { className: "study-icon" },
          study.name.replace(/-?pref-?(flip|study)-?/, "").replace(/-?study-?/, "").slice(0, 1)
        ),
        r("div", { className: "study-details" },
          r("div", { className: "study-header" },
            r("span", { className: "study-name" }, study.name),
            r("span", {}, "\u2022"), // &bullet;
            r("span", { className: "study-status" }, study.expired ? translations.completeStatus : translations.activeStatus),
          ),
          r("div", { className: "study-description", dangerouslySetInnerHTML: { __html: description }}),
        ),
        r("div", { className: "study-actions" },
          !study.expired &&
          r("button", { className: "remove-button", onClick: this.handleClickRemove },
            r("div", { className: "button-box" },
              translations.removeButton
            ),
          )
        ),
      )
    );
  }
}
PreferenceStudyListItem.propTypes = {
  study: PropTypes.shape({
    name: PropTypes.string.isRequired,
    expired: PropTypes.bool.isRequired,
    preferenceName: PropTypes.string.isRequired,
    preferenceValue: PropTypes.oneOf(PropTypes.string, PropTypes.bool, PropTypes.number).isRequired,
  }).isRequired,
  translations: PropTypes.object.isRequired,
};

ReactDOM.render(r(AboutStudies), document.getElementById("app"));
