/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import { Localized } from "./MSLocalized";

export const Loader = () => {
  return (
    <button className="primary">
      <div className="loaderContainer">
        <span className="loader" />
      </div>
    </button>
  );
};

export const InstallButton = props => {
  // determine if the addon is already installed so the state is
  // consistent on refresh or navigation
  const [installing, setInstalling] = useState(false);
  const [installComplete, setInstallComplete] = useState(false);

  const defaultInstallLabel = {
    string_id: "amo-picker-install-button-label",
  };

  function getDefaultInstallCompleteLabel(addonType = "") {
    let defaultInstallCompleteLabel;
    if (addonType && addonType === "theme") {
      defaultInstallCompleteLabel = {
        string_id: "return-to-amo-theme-install-complete-label",
      };
    } else if (addonType && addonType === "extension") {
      defaultInstallCompleteLabel = {
        string_id: "return-to-amo-extension-install-complete-label",
      };
    } else {
      defaultInstallCompleteLabel = {
        string_id: "amo-picker-install-complete-label",
      };
    }
    return defaultInstallCompleteLabel;
  }

  useEffect(() => {
    setInstallComplete(props.installedAddons?.includes(props.addonId));
  }, [props.addonId, props.installedAddons]);

  let buttonLabel = installComplete
    ? props.install_complete_label ||
      getDefaultInstallCompleteLabel(props.addonType)
    : props.install_label || defaultInstallLabel;

  function onClick(event) {
    props.handleAction(event);
    // Replace the label with the spinner
    setInstalling(true);

    window.AWEnsureAddonInstalled(props.addonId).then(value => {
      if (value === "complete") {
        // Set the label to "Installed"
        setInstallComplete(true);
      }
      // Whether the addon installs or not, we want to remove the spinner
      setInstalling(false);
    });
  }

  return (
    <div className="install-button-wrapper">
      {installing ? (
        <Loader />
      ) : (
        <Localized text={buttonLabel}>
          <button
            id={`install-button-${props.addonId}`}
            value={props.index}
            onClick={onClick}
            disabled={installComplete}
            className="primary"
            data-l10n-args={JSON.stringify({
              "addon-name": props.addonName ?? "",
            })}
          />
        </Localized>
      )}
    </div>
  );
};
