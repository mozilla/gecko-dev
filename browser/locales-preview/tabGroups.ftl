# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

## Variables:
##  $tabCount (Number): the number of tabs that are affected by the action.

tab-context-unnamed-group =
    .label = Unnamed group

tab-context-move-tab-to-new-group =
    .label =
        { $tabCount ->
            [1] Add Tab to New Group
           *[other] Add Tabs to New Group
        }
    .accesskey = G
tab-context-move-tab-to-group =
    .label =
        { $tabCount ->
            [1] Add Tab to Group
           *[other] Add Tabs to Group
        }
    .accesskey = G

tab-group-editor-action-new-tab =
  .label = New tab in group
tab-group-editor-action-new-window =
  .label = Move group to new window
tab-group-editor-action-save =
  .label = Save and close group
tab-group-editor-action-ungroup =
  .label = Ungroup tabs
tab-group-editor-action-delete =
  .label = Delete group
tab-group-editor-done =
  .label = Done
  .accessKey = D

##

# Variables:
#  $groupCount (Number): the number of tab groups that are affected by the action.
tab-context-ungroup-tab =
  .label =
      { $groupCount ->
          [1] Remove from Group
         *[other] Remove from Groups
      }
  .accesskey = R
