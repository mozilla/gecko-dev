/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
});

/**
 * ModelFilesView
 */
export class ModelFilesView extends MozLitElement {
  static properties = {
    models: { type: Array },
  };

  static events = {
    delete: "MlModelDelete",
  };

  constructor() {
    super();
    this.models = [];
  }

  dispatch(event, detail) {
    this.dispatchEvent(
      new CustomEvent(event, { detail, bubbles: true, composed: true })
    );
  }

  /**
   * Convert a timestamp to a string
   *
   * @param {number} ts
   * @returns {string} - The string representation of the timestamp
   */
  ts2str(ts) {
    return ts ? new Date(ts).toLocaleString() : "-";
  }

  /**
   * Handle the delete model click event and remove the model from the list
   *
   * @param {mlmodel} model
   */
  handleDeleteModelClick(model) {
    this.dispatch(ModelFilesView.events.delete, {
      model: model.name,
      revision: model.revision,
    });
  }

  removeModel(model) {
    this.models = this.models.filter(modelToCheck => {
      return (
        modelToCheck.name !== model.name &&
        modelToCheck.revision != model.revision
      );
    });
  }

  /**
   * Render the rows of the table
   *
   * @param {Array} files
   * @returns {Array} - Array of lit-html TemplateResult rows
   */
  renderRows(files) {
    const rows = files.map(file => {
      const size = parseInt(
        file.headers.fileSize || file.headers["Content-Length"] || 0
      );

      return html`
        <tr>
          <td>${file.path}</td>
          <td>${lazy.DownloadUtils.getTransferTotal(size)}</td>
          <td>${this.ts2str(file.headers.lastUsed)}</td>
          <td>${this.ts2str(file.headers.lastUpdated)}</td>
        </tr>
      `;
    });

    return rows;
  }

  render() {
    return html`
      <section>
        <link
          rel="stylesheet"
          href="chrome://global/content/model-files-view.css"
        />
        <h2 data-l10n-id="about-inference-models-title"></h2>
        <div class="description">
          <img class="info-icon" src="chrome://global/skin/icons/info.svg" />
          <p data-l10n-id="about-inference-downloads-description"></p>
        </div>
        ${this.models.map((model, i) => {
          return html`
            <div>
              <div class="header">
                <div class="title">
                  ${model.icon
                    ? html`<img class="model-icon" src=${model.icon} />`
                    : ""}
                  <h3>${model.name} (${model.revision})</h3>
                </div>

                <div class="actions">
                  <moz-button
                    type="destructive"
                    @click=${() => {
                      this.handleDeleteModelClick(model);
                    }}
                    iconSrc="chrome://global/skin/icons/delete.svg"
                  ></moz-button>
                </div>
              </div>
              <table>
                <thead>
                  <tr>
                    <th data-l10n-id="about-inference-file"></th>
                    <th data-l10n-id="about-inference-size"></th>
                    <th data-l10n-id="about-inference-last-used"></th>
                    <th data-l10n-id="about-inference-last-updated"></th>
                  </tr>
                </thead>
                <tbody>
                  ${this.renderRows(model.files)}
                  <tr class="total-row">
                    <td data-l10n-id="about-inference-total"></td>
                    <td>
                      ${lazy.DownloadUtils.getTransferTotal(
                        model.metadata.totalSize
                      )}
                    </td>
                    <td>${this.ts2str(model.metadata.lastUsed)}</td>
                    <td>${this.ts2str(model.metadata.updateDate)}</td>
                  </tr>
                </tbody>
              </table>
            </div>
            ${i < this.models.length - 1 ? html`<hr />` : ""}
          `;
        })}
      </section>
    `;
  }
}

customElements.define("model-files-view", ModelFilesView);
