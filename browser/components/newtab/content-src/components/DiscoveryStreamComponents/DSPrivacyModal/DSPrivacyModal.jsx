/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { actionCreators as ac } from "common/Actions.jsm";
import { SafeAnchor } from "../SafeAnchor/SafeAnchor";
import { ModalOverlayWrapper } from "content-src/asrouter/components/ModalOverlay/ModalOverlay";

export class DSPrivacyModal extends React.PureComponent {
  constructor(props) {
    super(props);
    this.closeModal = this.closeModal.bind(this);
    this.onLinkClick = this.onLinkClick.bind(this);
  }

  onLinkClick(event) {
    this.props.dispatch(
      ac.UserEvent({
        event: "CLICK_PRIVACY_INFO",
        source: "DS_PRIVACY_MODAL",
      })
    );
  }

  closeModal() {
    this.props.dispatch({
      type: `HIDE_PRIVACY_INFO`,
      data: {},
    });
  }

  render() {
    return (
      <ModalOverlayWrapper
        onClose={this.closeModal}
        innerClassName="ds-privacy-modal"
      >
        <div className="privacy-notice">
          <h3 data-l10n-id="newtab-privacy-modal-header" />
          <p data-l10n-id="newtab-privacy-modal-paragraph" />
          <SafeAnchor
            onLinkClick={this.onLinkClick}
            url="https://www.mozilla.org/en-US/privacy/firefox/"
          >
            <span data-l10n-id="newtab-privacy-modal-link" />
          </SafeAnchor>
        </div>
        <section className="actions">
          <button
            className="done"
            type="submit"
            onClick={this.closeModal}
            data-l10n-id="newtab-privacy-modal-button-done"
          />
        </section>
      </ModalOverlayWrapper>
    );
  }
}
