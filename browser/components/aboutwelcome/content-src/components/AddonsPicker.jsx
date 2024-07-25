/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import { AboutWelcomeUtils } from "../lib/aboutwelcome-utils.mjs";
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

  const defaultInstallLabel = { string_id: "amo-picker-install-button-label" };
  const defaultInstallCompleteLabel = {
    string_id: "amo-picker-install-complete-label",
  };

  useEffect(() => {
    setInstallComplete(props.installedAddons?.includes(props.addonId));
  }, [props.addonId, props.installedAddons]);

  let buttonLabel = installComplete
    ? props.install_complete_label || defaultInstallCompleteLabel
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
          />
        </Localized>
      )}
    </div>
  );
};

export const AddonsPicker = props => {
  const { content, installedAddons } = props;

  if (!content) {
    return null;
  }

  function handleAction(event) {
    const { message_id } = props;
    let { action, source_id } = content.tiles.data[event.currentTarget.value];
    let { type, data } = action;

    if (type === "INSTALL_ADDON_FROM_URL") {
      if (!data) {
        return;
      }
    }

    AboutWelcomeUtils.handleUserAction({ type, data });
    AboutWelcomeUtils.sendActionTelemetry(message_id, source_id);
  }

  return (
    <div className={"addons-picker-container"}>
      {content.tiles.data.map(
        (
          {
            id,
            name,
            type,
            description,
            icon,
            install_label,
            install_complete_label,
          },
          index
        ) =>
          name ? (
            <div key={id} className="addon-container">
              <div className="rtamo-icon">
                <img
                  className={`${
                    type === "theme" ? "rtamo-theme-icon" : "brand-logo"
                  }`}
                  src={icon}
                  role="presentation"
                  alt=""
                />
              </div>
              <div className="addon-details">
                <Localized text={name}>
                  <div className="addon-title" />
                </Localized>
                <Localized text={description}>
                  <div className="addon-description" />
                </Localized>
              </div>
              <InstallButton
                key={id}
                addonId={id}
                handleAction={handleAction}
                index={index}
                installedAddons={installedAddons}
                install_label={install_label}
                install_complete_label={install_complete_label}
              />
            </div>
          ) : null
      )}
    </div>
  );
};
