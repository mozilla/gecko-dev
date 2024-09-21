/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SelectableProfileService } from "resource:///modules/profiles/SelectableProfileService.sys.mjs";

export class ProfilesParent extends JSWindowActorParent {
  async receiveMessage(message) {
    switch (message.name) {
      case "Profiles:GetEditProfileContent": {
        // Make sure SelectableProfileService is initialized
        await SelectableProfileService.init();
        let currentProfile = SelectableProfileService.currentProfile;
        let profiles = await SelectableProfileService.getAllProfiles();
        return {
          currentProfile: currentProfile.toObject(),
          profiles: profiles.map(p => p.toObject()),
        };
      }
      case "Profiles:UpdateProfileName": {
        let profileObj = message.data;
        SelectableProfileService.currentProfile.name = profileObj.name;
        break;
      }
    }
    return null;
  }
}
