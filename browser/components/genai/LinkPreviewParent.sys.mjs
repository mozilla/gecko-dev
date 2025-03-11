/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * JSWindowActor to pass data between LinkPreview singleton and content pages.
 */
export class LinkPreviewParent extends JSWindowActorParent {
  async fetchPageData(url) {
    return this.sendQuery("LinkPreview:FetchPageData", { url });
  }
}
