/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SelectableProfileService } from "resource:///modules/profiles/SelectableProfileService.sys.mjs";

export class ProfilesParent extends JSWindowActorParent {
  actorCreated() {
    // init() just in case
    SelectableProfileService.init();
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "Profiles:GetEditProfileContent": {
        let currentProfile = SelectableProfileService.currentProfile;
        return currentProfile.toObject();
      }
    }
    return null;
  }
}
